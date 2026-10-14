//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

//based on Ieee802154NarrowbandMac

package flora.LoRa;

import inet.linklayer.csmaca.CsmaCaMac;

module LoRaMac extends CsmaCaMac
{
    parameters:
        bitrate = 250bps;
        @class(LoRaMac);
        @signal[LoRa_MacFrameSent](type=long); // AFLoRa: every transmitted frame, value = MType
        @statistic[LoRa_MacFrameSent](source=LoRa_MacFrameSent; record=count);

        // AFLoRa: the radio parameters and the end-device side of ADR (ADRACKReq, backoff,
        // LinkADRReq) belong to the MAC. Moved here from SimpleLoRaApp, same names and defaults.
        double initialLoRaTP @unit(dBm) = default(14dBm);
        double initialLoRaCF @unit(Hz) = default(868MHz);
        int initialLoRaSF = default(12);
        double initialLoRaBW @unit(Hz) = default(125kHz);
        int initialLoRaCR = default(4);
        bool initialUseHeader = default(true);
        bool evaluateADRinNode = default(false);

        // AFLoRa: confirmed uplinks (SimpleLoRaApp.confirmedTx). Without an ACK in RX1/RX2 the
        // frame is sent again after a random 1..3 s, up to maxRetry transmissions in all (fixed
        // at 8 in AFLoRa). sfModePar: SF on retransmissions, SFM0 one step up every two retries
        // (LoRaWAN), SFM1 unchanged, SFM2 circular SF7..SF12, SFM3 like SFM0 plus one step down
        // per ACK.
        int maxRetry = default(8);
        string sfModePar = default("SFM0");

        // AFLoRa: end-device duty cycle (always on in AFLoRa, absent in FLoRa). After each frame
        // the MAC waits getTimeOff() before sending again: 1% of the airtime of a 20 B frame at
        // CR 4/8, counted from the start of the frame. While it is on, SimpleLoRaApp also spaces
        // its packets with AFLoRa's values (85.6 s became 98.7 s at SF11).
        bool dutyCycleEnabled = default(false);
        // AFLoRa: frames waiting in the MAC, the one in flight included. A packet that arrives
        // when the queue is full is dropped (numDroppedUpPackets). -1 = no limit (AFLoRa and
        // FLoRa default).
        int maxQueueSize = default(-1);

        // Receive windows, measured from the end of the uplink. FLoRa: RX1 listens from 1 s to
        // 2 s and RX2 from 3 s to 4 s. AFLoRa: RX1 from 1 s and RX2 from 2 s (RECEIVE_DELAY2 of
        // LoRaWAN), 0.2 s each. A window only has to contain the start of a downlink; the
        // reception then runs to its end. Use together with NetworkServerApp.gatewayWaitTime,
        // which decides when the answer leaves.
        double receiveDelay1 @unit(s) = default(1s);
        double receiveWindow1 @unit(s) = default(1s);
        double receiveDelay2 @unit(s) = default(3s);
        double receiveWindow2 @unit(s) = default(1s);
    gates:
        input upperMgmtIn;
        output upperMgmtOut;
}
