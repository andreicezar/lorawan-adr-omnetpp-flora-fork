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

#ifndef __LORA_OMNET_SIMPLELORAAPP_H_
#define __LORA_OMNET_SIMPLELORAAPP_H_

#include <omnetpp.h>
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/lifecycle/LifecycleOperation.h"

#include "LoRaAppPacket_m.h"
#include "LoRa/LoRaMacControlInfo_m.h"
#include "LoRa/LoRaRadio.h"
#include "LoRa/LoRaMac.h"

using namespace omnetpp;
using namespace inet;

namespace flora {

/**
 * TODO - Generated class
 */
class SimpleLoRaApp : public cSimpleModule, public ILifecycle
{
    protected:
        void initialize(int stage) override;
        void finish() override;
        int numInitStages() const override { return NUM_INIT_STAGES; }
        void handleMessage(cMessage *msg) override;
        virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;

        void handleMessageFromLowerLayer(cMessage *msg);
        std::pair<double,double> generateUniformCircleCoordinates(double radius, double gatewayX, double gatewayY);
        void sendDownMgmtPacket();
        // AFLoRa: data uplinks (sendJoinRequest sent data in FLoRa).
        void sendConfData();
        void sendUnConfData();
        void sendData(int msgType, const char *name);
        bool useACK = false;   // AFLoRa: confirmedTx

        int numberOfPacketsToSend;
        int sentPackets;
        int receivedADRCommands;
        int receivedPackets;   // AFLoRa: every downlink delivered to the app
        int lastSentMeasurement;
        simtime_t timeToFirstPacket;
        simtime_t timeToNextPacket;

        cMessage *configureLoRaParameters;
        cMessage *sendMeasurements;

        //history of sent packets;
        cOutVector sfVector;
        cOutVector tpVector;

        // AFLoRa: the MAC controls the radio parameters and the device side of ADR; the app only
        // reads them for its statistics.
        LoRaMac *loRaMac = nullptr;

    public:
        SimpleLoRaApp() {}
        simsignal_t LoRa_AppPacketSent;

};

}

#endif
