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

package flora.LoRa;
import inet.applications.contract.IApp;

simple NetworkServerApp like IApp
{
    @signal[LoRa_ServerPacketReceived](type=bool); // optional
    @statistic[LoRa_ServerPacketReceived](source=LoRa_ServerPacketReceived; record=count);
    int localPort = default(-1);  // local port (-1: use ephemeral port)
    string localAddress = default("");
    int destPort = default(-1);
    bool evaluateADRinServer = default(false);
    int headerLength @unit(B) = default(8B);
    // AFLoRa: TX power of the downlinks (ADR commands and ACKs); FLoRa used a fixed 14 dBm.
    double initialLoRaTP @unit(dBm) = default(14dBm);
    // How long the server collects the copies of an uplink from the gateways before it
    // processes it and answers; the answer leaves at once, so it must start inside the device's
    // RX1 window (LoRaMac.receiveDelay1/receiveWindow1). FLoRa: 1.2 s; AFLoRa: 1.002 s, for its
    // 0.2 s windows.
    double gatewayWaitTime @unit(s) = default(1.2s);

    string adrMethod = default("max");
    double adrDeviceMargin = default(15);

    gates:
    output socketOut @labels(UdpControlInfo/up);
    input socketIn @labels(UdpControlInfo/down);

}