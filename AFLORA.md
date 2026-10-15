# FLoRa 1.3.2 with AFLoRa

This branch (`flora-1.3.2-straturi`) builds our simulator in layers, so that every difference
from the original can be traced and switched off:

1. **FLoRa 1.3.2** (florasim/flora, tag `v1.3.2`, commit `454633f`), unchanged. It needs INET 4.6.0
   and OMNeT++ 6.2 or later.
2. **AFLoRa v0.9** (lluiscas/AFLoRa, commit `9b1a049`; L. Casals, C. Gomez, R. Vidal, "The SF12
   Well in LoRaWAN: Problem and End-Device-Based Solutions", *Sensors* 21(19):6478, 2021). This file
   describes this layer.
3. Our changes (ADRopt, FEC, NbTrans and the corrections from our reviews), from commit `783e3ea`
   on, described in `OURS.md`; `simulations/ours.ini` switches them on.

AFLoRa is based on FLoRa 0.8 and the INET 3 packet API, so it could not be applied as a patch. Each
mechanism was rewritten on FLoRa 1.3.2, one commit per mechanism.

## Rule

Every AFLoRa mechanism that changes behaviour has a parameter whose default keeps FLoRa 1.3.2's
behaviour. `simulations/aflora.ini` switches all of them to AFLoRa's values (include it from a
scenario ini). Statistics are only added: every FLoRa output is still recorded.

## What was ported

| Mechanism | Where | Parameter | FLoRa 1.3.2 | AFLoRa | Commit |
|---|---|---|---|---|---|
| Radio parameters and the device side of ADR live in the MAC | `LoRaMac`, `SimpleLoRaApp` | `initialLoRaTP/CF/SF/BW/CR`, `initialUseHeader`, `evaluateADRinNode` moved from `SimpleLoRaApp` to `LoRaMac` | app | MAC (same values) | `28aa598` |
| LoRaWAN frame control and options in the MAC frame | `LoRaMacFrame.msg` | none | none | `MType`, `ADR`, `ADRACKReq`, `ACK`, `FOpsLen`, `FPending`, `options` | `28aa598` |
| LinkADRReq carried in the frame options (FOpts) | `NetworkServerApp` to `LoRaMac` | none | in the app packet | in the MAC frame | `28aa598` |
| ADRACKReq counter and SF backoff | `LoRaMac` | none | counted when a packet is sent | counted when RX2 closes without a downlink | `28aa598` |
| Data types of the app packet | `LoRaAppPacket.msg` | none | `DATA = 3`, `TXCONFIG = 4` | `UCONF_DATA_UP = 3`, `UCONF_DATA_DW`, `CONF_DATA_UP`, `CONF_DATA_DW`, `TXCONFIG = 8` | `28aa598` |
| Confirmed uplinks, ACK, retransmissions | `SimpleLoRaApp`, `LoRaMac`, `NetworkServerApp` | `confirmedTx`, `maxRetry`, `sfModePar` | none | ACK in the next window; up to 8 transmissions, 1 to 3 s apart | `22bfd5c` |
| SF on retransmissions | `LoRaMac` | `sfModePar` | none | `SFM0` one step up every two retries, `SFM1` unchanged, `SFM2` circular, `SFM3` like `SFM0` plus one step down per ACK | `22bfd5c` |
| A packet counts once per sequence number | `NetworkServerApp` (`isPacketNew`) | none | no effect without retransmissions | counted once | `22bfd5c` |
| Downlink TX power set by the server | `NetworkServerApp`, `LoRaTransmitter` | `initialLoRaTP` | fixed 14 dBm | 14 dBm by default | `22bfd5c`, `37a787d` |
| End-device duty cycle | `LoRaMac` | `dutyCycleEnabled` | none | 1% of the airtime of 20 B at CR 4/8, from the start of the frame | `0f20b06` |
| MAC transmission queue | `LoRaMac` | `maxQueueSize` | unlimited | unlimited (AFLoRa's example scenarios used 1) | `0f20b06` |
| App spacing at SF11 | `LoRaMac::getTimeOff` | follows `dutyCycleEnabled` | 85.6064 s | 98.7136 s (low data rate optimisation) | `0f20b06` |
| Receive windows | `LoRaMac` | `receiveDelay1`, `receiveWindow1`, `receiveDelay2`, `receiveWindow2` | RX1 1 to 2 s, RX2 3 to 4 s | RX1 1 to 1.2 s, RX2 2 to 2.2 s | `8485ef5` |
| Server wait for gateway copies | `NetworkServerApp` | `gatewayWaitTime` | 1.2 s | 1.002 s | `8485ef5` |
| Airtime formula | `LoRaTransmitter` | `airtimeFormula` | `"flora"` | `"aflora"`: Semtech formula with low data rate optimisation at SF11 and SF12 | `0c70ea5` |
| Gateway downlink duty-cycle table | `LoRaGWMac` | `dutyCycleTable` | `"flora"` | `"aflora"`: SF8 1.07008 s, SF10 3.62496 s | `1b51de7` |
| Statistics | `LoRaMac`, `NetworkServerApp`, `LoRaReceiver`, `LoRaGWMac`, `PacketForwarder` | none | none | ACK and retry counters, dropped packets, per-type frame counts, DER per MAC frame, collision vectors | `22bfd5c`, `0f20b06`, `1bfa415` |

## Differences from AFLoRa itself

- **Random stream.** AFLoRa draws an unused `intuniform(0, 2)` when the MAC initializes. That draw
  would shift the random stream of every simulation, so it was left out; random numbers are drawn
  only for actual retries.
- **Commanded TX power.** It is truncated to whole dBm, as FLoRa 1.x's `SimpleLoRaApp::setTP(int)`
  did. AFLoRa kept it as a double.
- **ADR backoff counter.** It advances when RX2 closes without a downlink, as in AFLoRa. A downlink
  that answers the 96th uplink therefore cancels the SF step that FLoRa would already have taken.
- **Queue.** The queue belongs to the MAC, as in AFLoRa. The NIC's queue module stays, but the MAC
  drains it at once, so only that module's own statistics change.
- **When waiting frames start.** They also start when the MAC returns to IDLE, as in FLoRa. AFLoRa
  started them only at the end of the off-time, at the retry timer or when a new packet arrived. The
  results are the same whenever the off-time outlasts the receive windows, which AFLoRa's tables
  always ensure.
- **App to MAC information.** It goes through `LoRaTag` (`mType` was added there), not through
  `LoRaMacControlInfo` as in AFLoRa.
- **Not ported:**
  - the empty `sendJoinRequest` (dead code that leaks a packet);
  - AFLoRa's removal of `finalSF`, `finalTP` and the SF/TP vectors from the app (they are kept);
  - AFLoRa's example simulations.
- **`LoRa_GW_CountPacketsNodes`.** AFLoRa compared the signal name with `"LoRA_AppPacketSent"`, so
  the counter stayed 0. Here it counts the packets.

## Limitations that come from AFLoRa (kept)

- The end-device duty cycle uses a fixed table: 20 B at CR 4/8, whatever the actual frame is.
- A confirmed uplink gets no ADR: the server answers it with an ACK only.
- A downlink without the ACK bit, while a confirmed uplink waits, leaves that uplink waiting. This
  server never sends one.
- Both receive windows use the uplink's channel and SF. LoRaWAN's RX2 parameters (869.525 MHz, SF12)
  are not modelled.

## Build

With OMNeT++ 6.2.0 on the path and INET 4.6.0 in `../inet4.6`:

```bash
make makefiles INET_DIR=../inet4.6
make MODE=release -j8
```

## Status

Every commit compiles against INET 4.6.0 without warnings. Nothing has been simulated yet. The first
checks to run:

1. The FLoRa example (`simulations/omnetpp.ini`) on commit `454633f` and on this layer with default
   parameters. Results should be identical apart from the added statistics and the NIC queue's own
   statistics.
2. The same example with `include aflora.ini`.
