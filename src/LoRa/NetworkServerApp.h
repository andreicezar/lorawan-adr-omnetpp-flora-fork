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

#ifndef __LORANETWORK_NETWORKSERVERAPP_H_
#define __LORANETWORK_NETWORKSERVERAPP_H_

#include <omnetpp.h>
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"
#include <vector>
#include <tuple>
#include <algorithm>
#include "inet/common/INETDefs.h"

#include "LoRaMacControlInfo_m.h"
#include "LoRaMacFrame_m.h"
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "../LoRaApp/LoRaAppPacket_m.h"
#include <list>

namespace flora {

class knownNode
{
public:
    MacAddress srcAddr;
    int framesFromLastADRCommand;
    int lastSeqNoProcessed;
    int numberOfSentADRPackets;
    std::list<double> adrListSNIR;
    // AFLoRa
    int lastSeqProcessed;    // last sequence number counted as a new packet
    int lastSentSeqNumber;   // downlink frame counter
    cOutVector *historyAllSNIR;
    cOutVector *historyAllRSSI;
    cOutVector *receivedSeqNumber;
    cOutVector *calculatedSNRmargin;
};

class knownGW
{
public:
    L3Address ipAddr;
};

class receivedPacket
{
public:
    Packet* rcvdPacket = nullptr;
    cMessage* endOfWaiting = nullptr;
    std::vector<std::tuple<L3Address, double, double>> possibleGateways; // <address, sinr, rssi>
};

class NetworkServerApp : public cSimpleModule, cListener
{
  public:
    using cIListener::finish;
  protected:
    std::vector<knownNode> knownNodes;
    std::vector<knownGW> knownGateways;
    std::vector<receivedPacket> receivedPackets;
    int localPort = -1, destPort = -1;
    std::vector<std::tuple<MacAddress, int>> recvdPackets;
    // state
    UdpSocket socket;
    cMessage *selfMsg = nullptr;
    int totalReceivedPackets;
    std::string adrMethod;
    double adrDeviceMargin;
    std::map<int, int> numReceivedPerNode;
    // AFLoRa
    int totalReceivedFrames1GW = 0;   // frames that passed the duplicate filter (every gateway copy)
    int totalRcvdConfFrames1GW = 0;
    int totalRcvdUnconfFrames1GW = 0;
    cOutVector rcvdPkConfd;      // confirmed packets received (new ones)
    cOutVector rcvdPkUnConfd;    // unconfirmed packets received (new ones)
    cOutVector rcvdFrConfd;      // confirmed frames received
    cOutVector rcvdFrUnConfd;    // unconfirmed frames received
    double iniTP = 14;           // dBm
    double TPnow = 14;           // dBm, TX power of the downlinks
    // ours: device address and sequence number of each uplink copy that arrives, is rejected as
    // old, is merged with a copy from another gateway, or is accepted as a new packet
    cOutVector traceRxAddr;
    cOutVector traceRxSeq;
    cOutVector traceRejectOldSeqAddr;
    cOutVector traceRejectOldSeqSeq;
    cOutVector traceMergeDuplicateAddr;
    cOutVector traceMergeDuplicateSeq;
    cOutVector traceAcceptAddr;
    cOutVector traceAcceptSeq;

  protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    void processLoraMACPacket(Packet *pk);
    void startUDP();
    void setSocketOptions();
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    bool isPacketProcessed(const Ptr<const LoRaMacFrame> &);
    void updateKnownNodes(Packet* pkt);
    void addPktToProcessingTable(Packet* pkt);
    void processScheduledPacket(cMessage* selfMsg);
    void evaluateADR(Packet *pkt, L3Address pickedGateway, double SNIRinGW, double RSSIinGW);
    // AFLoRa
    bool isPacketNew(const Ptr<const LoRaMacFrame> &frame);
    void ackmentPacket(Packet *pkt, L3Address pickedGateway, double SNIRinGW, double RSSIinGW);
    void unConfPacket(Packet *pkt, L3Address pickedGateway, double SNIRinGW, double RSSIinGW);
    void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details) override;
    bool evaluateADRinServer;

    cHistogram receivedRSSI;
  public:
    simsignal_t LoRa_ServerPacketReceived;
    int counterOfSentPacketsFromNodes = 0;
    int counterOfSentPacketsFromNodesPerSF[6];
    int counterUniqueReceivedPackets = 0;
    int counterUniqueReceivedPacketsPerSF[6];
};
} //namespace inet
#endif
