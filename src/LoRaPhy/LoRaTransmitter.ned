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

package flora.LoRaPhy;

import inet.physicallayer.wireless.common.base.packetlevel.NarrowbandTransmitterBase;

module LoRaTransmitter extends NarrowbandTransmitterBase
{
    parameters:
        @signal[LoRaTransmissionCreated](type=bool); // optional
        @statistic[LoRaTransmissionCreated](source=LoRaTransmissionCreated; record=count);
        modulation = default("LoRaModulation");
        // Airtime of a frame (of the payload length chosen by airtimePayload, in both formulas).
        // "flora": FLoRa's formula, integer division before ceil(), no low data rate
        // optimisation, and 8 payload symbols counted twice. "aflora": AFLoRa's correction, the
        // Semtech formula (real ceil(), low data rate optimisation at SF11-12, CRC on, explicit
        // header).
        string airtimeFormula @enum("flora", "aflora") = default("flora");
        // Ours: the payload length the airtime is computed for. "fixed": FLoRa's and AFLoRa's
        // 20 B from a device and 15 B from a gateway, whatever the frame. "frame": the frame as
        // built, MAC header plus app payload (from a device: CsmaCaMac's 17 B header plus
        // dataSize; LoRaWAN would be 13 B with the MIC, ns-3 counts 9 B).
        string airtimePayload @enum("fixed", "frame") = default("fixed");
        @class(LoRaTransmitter);
}