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

//
// TODO generated message class
//

import inet.common.INETDefs;
import inet.common.Units;
import inet.linklayer.common.MacAddress;
import inet.common.packet.chunk.Chunk;

cplusplus {{
using namespace inet;
}}

namespace flora;

// AFLoRa (Casals, Gomez, Vidal, Sensors 21(19):6478, 2021): MAC commands travel in the
// frame options (FOpts). Only LinkADRReq is modelled.
enum MacCmdId
{
    LINK_ADR_REQ_ID = 3;
    MAC_CMD_ID_NOT_USED = -1;
}

enum MacCmdLen
{
    NO_OPTIONS = 0;
    LINK_ADR_REQ_LEN = 5;
    LINK_ADR_ANS_LEN = 2;
    MAC_CMD_LEN_NOT_USED = -1;
}

// AFLoRa: the LinkADRReq content, moved here from LoRaAppPacket.msg.
class LoRaOptions {
    int Cid @enum(MacCmdId) = MAC_CMD_ID_NOT_USED;
    double LoRaTP = -1;
    int LoRaSF = -1;
    double LoRaBW = -1;
}

class LoRaMacFrame extends inet::FieldsChunk {
    inet::MacAddress transmitterAddress;
    inet::MacAddress receiverAddress;

    int MType;                  // AFLoRa: message type, an AppPacketType value (UCONF_DATA_UP, ...)
    int sequenceNumber;
    bool ADR;                   // AFLoRa: FCtrl ADR bit
    bool ADRACKReq;             // AFLoRa: FCtrl ADRACKReq bit
    bool ACK;                   // AFLoRa: FCtrl ACK bit
    int FOpsLen = NO_OPTIONS;   // AFLoRa: FCtrl FOptsLen
    bool FPending;              // AFLoRa: FCtrl FPending bit
    double LoRaTP;
    inet::Hz LoRaCF;
    int LoRaSF;
    inet::Hz LoRaBW;
    int LoRaCR;
    bool LoRaUseHeader;
    double RSSI;
    double SNIR;
    simtime_t tmstm;            // AFLoRa
    LoRaOptions options;        // AFLoRa: FOpts
}
