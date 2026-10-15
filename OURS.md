# Our changes on FLoRa 1.3.2 with AFLoRa

Layer 3 of the branch `flora-1.3.2-straturi` (layers 1 and 2, FLoRa 1.3.2 and AFLoRa, are described
in `AFLORA.md`). It holds what we added to FLoRa for the ADRopt study, ported from our earlier fork
(based on FLoRa 1.1.0 and INET 4.4).

## Rule

The same as for AFLoRa. Every change that alters behaviour has a parameter whose default keeps the
behaviour of the layers below. `simulations/ours.ini` switches on the ones that apply to every
scenario; scenario-specific values (the path-loss reference, fading) stay in the scenario ini.
Statistics are only added.

## Ported so far

| Change | Where | Parameter | Default (layers 1-2) | Ours | Commit |
|---|---|---|---|---|---|
| Path loss at the reference distance | `LoRaLogNormalShadowing` | `PL_d0_db` | 127.41 dB, hardcoded | any value; our scenarios use 103.139 dB with `d0` = 100 m (ns-3's log-distance reference) | `783e3ea` |
| The same value in `computeRange` (range filter) | `LoRaLogNormalShadowing` | `PL_d0_db` | 127.41 dB | `PL_d0_db` | `783e3ea` |
| Rayleigh fading | `LoRaLogNormalShadowing` | `enableRayleighFading` | off | received power times an Exp(1) draw per frame | `783e3ea` |
| Fixed-SNR mode | `LoRaLogNormalShadowing` | `fixedMeanSnrDb`, `fixedTxPowerDbm` | off (`nan`) | path loss set so that the mean SNR above the -117 dBm noise floor is `fixedMeanSnrDb` | `783e3ea` |
| Payload length of the airtime | `LoRaTransmitter` | `airtimePayload` | `"fixed"`: 20 B from a device, 15 B from a gateway | `"frame"`: the frame as built, MAC header plus app payload | `a689283` |
| Trace vectors | `LoRaMac`, `LoRaReceiver`, `LoRaGWMac`, `PacketForwarder`, `NetworkServerApp` | none | none | `trace_*`: device address and sequence number of each uplink at every layer | `1a35e70` |

## Differences from our earlier fork

- **`PL_d0_db` default.** It is FLoRa's 127.41 dB. The old fork had changed the default to 103.139 dB,
  so an ini that relied on that default must now set the value.
- **`computeRange`.** It uses `PL_d0_db`; the old fork kept a fixed 103.139 dB there.
- **Debug output.** None. The old fork wrote one `EV_WARN` line per path-loss computation and
  `TRACE_DEBUG` lines in the receiver.
- **`dataSize`.** It keeps FLoRa's default of 10 B. The old fork changed it to 15 B, which our
  scenarios also set explicitly.
- **The server's `accept` trace.** It is recorded once per sequence number (AFLoRa's `isPacketNew`);
  the old fork recorded every processed packet. Without retransmissions the two are the same.
- **Device MAC header.** It stays CsmaCaMac's 17 B. With `airtimePayload = "frame"` a 15 B payload is
  therefore 32 B on air, as in the old fork; LoRaWAN would give 28 B (13 B with the MIC) and ns-3
  counts 24 B (9 B, no MIC). `**.loRaNodes[*].LoRaNic.mac.headerLength` changes it.

## Not ported yet

- ADRopt (network server), with its traces;
- the FEC encoder and decoder;
- NbTrans, carried in the LinkADRReq options;
- multi-channel hopping;
- ADR-TTN as its own method, next to FLoRa's `max` and `avg`;
- the paper's power rule;
- the device duty cycle computed from the real airtime (AFLoRa's fixed table lets frames longer than
  20 B exceed 1%);
- the serializers and the implicit chunk serialization;
- the review corrections that concern these.

The scenarios that need them still run on the old fork. None of them is active in the PHY scenarios
of article 1 (BASE, COLLISION, CAPTURE), which this layer can already run.

## Status

Every commit compiles against INET 4.6.0 without warnings. Nothing has been simulated yet. With
default parameters this layer must reproduce layer 2, and therefore FLoRa 1.3.2, apart from the
added statistics.
