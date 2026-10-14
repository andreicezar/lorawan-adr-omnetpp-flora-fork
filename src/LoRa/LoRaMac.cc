//
// Copyright (C) 2016 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "inet/linklayer/common/UserPriority.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/linklayer/csmaca/CsmaCaMac.h"
#include "LoRaMac.h"
#include "LoRaTagInfo_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"


namespace flora {

Define_Module(LoRaMac);

LoRaMac::~LoRaMac()
{
    cancelAndDelete(endTransmission);
    cancelAndDelete(endReception);
    cancelAndDelete(droppedPacket);
    cancelAndDelete(endDelay_1);
    cancelAndDelete(endListening_1);
    cancelAndDelete(endDelay_2);
    cancelAndDelete(endListening_2);
    cancelAndDelete(mediumStateChange);
    cancelAndDelete(retransmission);
    cancelAndDelete(endDCTimeOff);
}

/****************************************************************
 * Initialization functions.
 */
void LoRaMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        EV << "Initializing stage 0\n";

        //maxQueueSize = par("maxQueueSize");
        headerLength = par("headerLength");
        ackLength = par("ackLength");
        ackTimeout = par("ackTimeout");
        retryLimit = par("retryLimit");

        // Receive windows (FLoRa: 1 s + 1 s, then 1 s + 1 s; AFLoRa: RX1 at 1 s and RX2 at 2 s,
        // 0.2 s each). waitDelay2Time is the gap between the end of RX1 and the start of RX2.
        waitDelay1Time = par("receiveDelay1");
        listening1Time = par("receiveWindow1");
        waitDelay2Time = par("receiveDelay2").doubleValue() - par("receiveDelay1").doubleValue() - par("receiveWindow1").doubleValue();
        listening2Time = par("receiveWindow2");
        if (waitDelay2Time < 0)
            throw cRuntimeError("receiveDelay2 must not start before RX1 ends (receiveDelay1 + receiveWindow1)");

        const char *addressString = par("address");
        if (!strcmp(addressString, "auto")) {
            // assign automatic address
            address = MacAddress::generateAutoAddress();
            // change module parameter from "auto" to concrete address
            par("address").setStringValue(address.str().c_str());
        }
        else
            address.setAddress(addressString);

        // subscribe for the information of the carrier sense
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        radioModule->subscribe(IRadio::receptionStateChangedSignal, this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radioModule->subscribe(LoRaRadio::droppedPacket, this);
        radio = check_and_cast<IRadio *>(radioModule);

        // AFLoRa: the MAC sets the radio parameters (SimpleLoRaApp did it in FLoRa).
        loRaRadio = check_and_cast<LoRaRadio *>(radio);
        loRaRadio->loRaTP = par("initialLoRaTP").doubleValue();
        loRaRadio->loRaCF = units::values::Hz(par("initialLoRaCF").doubleValue());
        loRaRadio->loRaSF = par("initialLoRaSF");
        loRaRadio->loRaBW = units::values::Hz(par("initialLoRaBW").doubleValue());
        loRaRadio->loRaCR = par("initialLoRaCR");
        loRaRadio->loRaUseHeader = par("initialUseHeader");
        evaluateADRinNode = par("evaluateADRinNode");
        sequenceNumberRemote = 0;

        // AFLoRa: confirmed uplinks
        maxRetry = par("maxRetry");
        countRetry = 0;
        ACKPending = false;
        retry = false;
        sfModePar = par("sfModePar").stdstringValue();
        if (sfModePar == "SFM0")
            sfMode = SFM0;
        else if (sfModePar == "SFM1")
            sfMode = SFM1;
        else if (sfModePar == "SFM2")
            sfMode = SFM2;
        else if (sfModePar == "SFM3")
            sfMode = SFM3;
        else
            throw cRuntimeError("Unknown sfModePar '%s' (SFM0, SFM1, SFM2 or SFM3)", sfModePar.c_str());
        LoRa_MacFrameSent = registerSignal("LoRa_MacFrameSent");

        // AFLoRa: end-device duty cycle and MAC queue
        dutyCycleEnabled = par("dutyCycleEnabled");
        dcTimeOff = false;
        maxQueueSize = par("maxQueueSize");

        // initialize self messages
        endTransmission = new cMessage("Transmission");
        endReception = new cMessage("Reception");
        droppedPacket = new cMessage("Dropped Packet");
        endDelay_1 = new cMessage("Delay_1");
        endListening_1 = new cMessage("Listening_1");
        endDelay_2 = new cMessage("Delay_2");
        endListening_2 = new cMessage("Listening_2");
        mediumStateChange = new cMessage("MediumStateChange");
        retransmission = new cMessage("Retransmission");
        endDCTimeOff = new cMessage("EndDCTimeOff");

        // set up internal queue
        txQueue = getQueue(gate(upperLayerInGateId));//check_and_cast<queueing::IPacketQueue *>(getSubmodule("queue"));
        // AFLoRa: the MAC takes every packet from txQueue into its own queue, which counts the
        // frame in flight (FLoRa pulled a packet only when idle; with maxQueueSize = -1 and the
        // duty cycle off the order and timing are the same).
        transmissionQueue.setName("transmissionQueue");

        // state variables
        fsm.setName("LoRaMac State Machine");
        backoffPeriod = -1;
        retryCounter = 0;

        // sequence number for messages
        sequenceNumber = 0;

        // statistics
        numRetry = 0;
        numSentWithoutRetry = 0;
        numGivenUp = 0;
        numCollision = 0;
        numSent = 0;
        numReceived = 0;
        numSentBroadcast = 0;
        numReceivedBroadcast = 0;
        numADRCommandReceived = 0;
        numAckReceivedNoRetry = 0;
        numSentConFrames = 0;
        numSentUnconFrames = 0;
        numAckReceived = 0;
        numAckSent = 0;
        numAckNotReceived = 0;
        numDroppedUpPackets = 0;
        nodeDroppedDC.setName("Node Dropped DC");
        sfSentCount.setName("SF node count");
        tpSentCount.setName("TP tx frame");
        reqSendPkConfd.setName("Req send con pk");
        reqSendPkUnConfd.setName("Req send uncon pk");
        rcvdAckPk.setName("Recvd ACK pk");

        // initialize watches
        WATCH(fsm);
        WATCH(backoffPeriod);
        WATCH(retryCounter);
        WATCH(numRetry);
        WATCH(numSentWithoutRetry);
        WATCH(numGivenUp);
        WATCH(numCollision);
        WATCH(numSent);
        WATCH(numReceived);
        WATCH(numSentBroadcast);
        WATCH(numReceivedBroadcast);
        WATCH(numADRCommandReceived);
        WATCH(numAckReceived);
        WATCH(numAckSent);
        WATCH(numAckNotReceived);
        WATCH(loRaRadio->loRaSF);
        WATCH(loRaRadio->loRaTP);
    }
    else if (stage == INITSTAGE_LINK_LAYER)
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
}

void LoRaMac::finish()
{
    recordScalar("numRetry", numRetry);
    recordScalar("numSentWithoutRetry", numSentWithoutRetry);
    recordScalar("numGivenUp", numGivenUp);
    //recordScalar("numCollision", numCollision);
    recordScalar("numSent", numSent);
    recordScalar("numReceived", numReceived);
    recordScalar("numSentBroadcast", numSentBroadcast);
    recordScalar("numReceivedBroadcast", numReceivedBroadcast);
    recordScalar("numADRCommandReceived", numADRCommandReceived);
    // AFLoRa
    recordScalar("numAckReceivedNoRetry", numAckReceivedNoRetry);
    recordScalar("numSentConFrames", numSentConFrames);
    recordScalar("numSentUnconFrames", numSentUnconFrames);
    recordScalar("numAckReceived", numAckReceived);
    recordScalar("numAckSent", numAckSent);
    recordScalar("numAckNotReceived", numAckNotReceived);
    recordScalar("numDroppedUpPackets", numDroppedUpPackets);
    recordScalar("numPckTxPending", getQueueLength() > 0 ? 1 : 0);
}

void LoRaMac::configureNetworkInterface()
{
    //NetworkInterface *e = new NetworkInterface(this);

    // data rate
    networkInterface->setDatarate(bitrate);
    networkInterface->setMacAddress(address);

    // capabilities
    //interfaceEntry->setMtu(par("mtu"));
    networkInterface->setMtu(std::numeric_limits<int>::quiet_NaN());
    networkInterface->setMulticast(true);
    networkInterface->setBroadcast(true);
    networkInterface->setPointToPoint(false);
}

/****************************************************************
 * Message handling functions.
 */
void LoRaMac::handleSelfMessage(cMessage *msg)
{
    EV << "received self message: " << msg << endl;
    // AFLoRa: end of the duty-cycle off-time, or the retry timer of the frame waiting for its
    // ACK; either may let the next frame go (AFLoRa's Idle-Transmit-Pending transition).
    if (msg == endDCTimeOff) {
        dcTimeOff = false;
        startNextTransmission();
        return;
    }
    if (msg == retransmission) {
        retry = true;
        startNextTransmission();
        return;
    }
    handleWithFsm(msg);
}
#if 0
void LoRaMac::handleUpperPacket(cMessage *msg)
{
    if(fsm.getState() != IDLE) {
            error("Wrong, it should not happen erroneous state: %s", fsm.getStateName());
    }
    auto pkt = check_and_cast<Packet *>(msg);

    pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::lora);
//    LoRaMacControlInfo *cInfo = check_and_cast<LoRaMacControlInfo *>(msg->getControlInfo());
    auto pktEncap = encapsulate(pkt);

    const auto &frame = pktEncap->peekAtFront<LoRaMacFrame>();

    EV << "frame " << pktEncap << " received from higher layer, receiver = " << frame->getReceiverAddress() << endl;

    txQueue->enqueuePacket(pktEncap);
    if (fsm.getState() != IDLE)
        EV << "deferring upper message transmission in " << fsm.getStateName() << " state\n";
    else {
        popTxQueue();
        handleWithFsm(currentTxFrame);
    }
}
#endif
void LoRaMac::handleUpperPacket(Packet *packet)
{
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);

    EV << "frame " << packet << " received from higher layer " << endl;
    // AFLoRa: packets the app asked to send, by type
    if (simTime() >= getSimulation()->getWarmupPeriod()) {
        auto loraTag = packet->findTag<LoRaTag>();
        int mType = loraTag ? loraTag->getMType() : UCONF_DATA_UP;
        if (mType == CONF_DATA_UP)
            reqSendPkConfd.record(1);
        if (mType == UCONF_DATA_UP)
            reqSendPkUnConfd.record(1);
    }
    // AFLoRa: a packet arriving when the MAC queue is full is dropped.
    if (maxQueueSize >= 0 && getQueueLength() >= maxQueueSize) {
        if (simTime() >= getSimulation()->getWarmupPeriod()) {
            numDroppedUpPackets++;
            nodeDroppedDC.record(1);
        }
        EV << "Dropped Up frame. Transmission Queue is full \n";
        delete packet;
        return;
    }
    auto pktEncap = encapsulate(packet);
    const auto &frame = pktEncap->peekAtFront<LoRaMacFrame>();
    if (frame == nullptr)
        throw cRuntimeError("Header LoRaMacFrame not found");

    transmissionQueue.insert(pktEncap);
    startNextTransmission();
}


void LoRaMac::handleLowerPacket(Packet *msg)
{
    if( (fsm.getState() == RECEIVING_1) || (fsm.getState() == RECEIVING_2)) handleWithFsm(msg);
    else delete msg;
}

void LoRaMac::processUpperPacket()
{
    Packet *packet = dequeuePacket();
    handleUpperMessage(packet);
}

queueing::IPassivePacketSource *LoRaMac::getProvider(const cGate *gate)
{
    return (gate->getId() == upperLayerInGateId) ? txQueue.get() : nullptr;
}

void LoRaMac::handleCanPullPacketChanged(const cGate *gate)
{
    Enter_Method("handleCanPullPacketChanged");
    // AFLoRa: every packet goes into the MAC's own queue at once.
    while (!txQueue->isEmpty())
        processUpperPacket();
}

void LoRaMac::handlePullPacketProcessed(Packet *packet, const cGate *gate, bool successful)
{
    Enter_Method("handlePullPacketProcessed");
    throw cRuntimeError("Not supported callback");
}

void LoRaMac::handleWithFsm(cMessage *msg)
{
    Ptr<LoRaMacFrame>frame = nullptr;

    auto pkt = dynamic_cast<Packet *>(msg);
    if (pkt) {
        const auto &chunk = pkt->peekAtFront<Chunk>();
        frame = dynamicPtrCast<LoRaMacFrame>(constPtrCast<Chunk>(chunk));
    }
    { FSMA_Switch(fsm)
    {
        FSMA_State(IDLE)
        {
            FSMA_Enter(FSMA_Delay_Action(turnOffReceiver()));
            FSMA_Event_Transition(Idle-Transmit,
                                  isUpperMessage(msg) && !isTxDeferred(),
                                  TRANSMIT,
            );
        }
        FSMA_State(TRANSMIT)
        {
            FSMA_Enter(FSMA_Delay_Action(sendDataFrame(getCurrentTransmission())));
            FSMA_Event_Transition(Transmit-Wait_Delay_1,
                                  msg == endTransmission,
                                  WAIT_DELAY_1,
                finishCurrentTransmission();
                numSent++;
            );
        }
        FSMA_State(WAIT_DELAY_1)
        {
            FSMA_Enter(FSMA_Delay_Action(turnOffReceiver()));
            FSMA_Event_Transition(Wait_Delay_1-Listening_1,
                                  msg == endDelay_1 || endDelay_1->isScheduled() == false,
                                  LISTENING_1,
            );
        }
        FSMA_State(LISTENING_1)
        {
            FSMA_Enter(FSMA_Delay_Action(turnOnReceiver()));
            FSMA_Event_Transition(Listening_1-Wait_Delay_2,
                                  msg == endListening_1 || endListening_1->isScheduled() == false,
                                  WAIT_DELAY_2,
            );
            FSMA_Event_Transition(Listening_1-Receiving1,
                                  msg == mediumStateChange && isReceiving(),
                                  RECEIVING_1,
            );
        }
        FSMA_State(RECEIVING_1)
        {
            FSMA_Event_Transition(Receive-Unicast-Not-For,
                                  isLowerMessage(msg) && !isForUs(frame),
                                  LISTENING_1,
            );
            FSMA_Event_Transition(Receive-Unicast,
                                  isLowerMessage(msg) && isForUs(frame),
                                  IDLE,
                manageFrame(frame);
                sendUp(decapsulate(pkt));
                numReceived++;
                cancelEvent(endListening_1);
                cancelEvent(endDelay_2);
                cancelEvent(endListening_2);
            );
            FSMA_Event_Transition(Receive-BelowSensitivity,
                                  msg == droppedPacket,
                                  LISTENING_1,
            );

        }
        FSMA_State(WAIT_DELAY_2)
        {
            FSMA_Enter(FSMA_Delay_Action(turnOffReceiver()));
            FSMA_Event_Transition(Wait_Delay_2-Listening_2,
                                  msg == endDelay_2 || endDelay_2->isScheduled() == false,
                                  LISTENING_2,
            );
        }
        FSMA_State(LISTENING_2)
        {
            FSMA_Enter(FSMA_Delay_Action(turnOnReceiver()));
            FSMA_Event_Transition(Listening_2-idle,
                                  msg == endListening_2 || endListening_2->isScheduled() == false,
                                  IDLE,
                setRetransmission();
            );
            FSMA_Event_Transition(Listening_2-Receiving2,
                                  msg == mediumStateChange && isReceiving(),
                                  RECEIVING_2,
            );
        }
        FSMA_State(RECEIVING_2)
        {
            FSMA_Event_Transition(Receive2-Unicast-Not-For,
                                  isLowerMessage(msg) && !isForUs(frame),
                                  LISTENING_2,
            );
            FSMA_Event_Transition(Receive2-Unicast,
                                  isLowerMessage(msg) && isForUs(frame),
                                  IDLE,
                manageFrame(frame);
                sendUp(pkt);
                numReceived++;
                cancelEvent(endListening_2);
            );
            FSMA_Event_Transition(Receive2-BelowSensitivity,
                                  msg == droppedPacket,
                                  LISTENING_2,
            );
        }
    } }
    fsm.executeDelayedActions();

//    if (fsm.getState() == IDLE) {
//        if (isReceiving())
//            handleWithFsm(mediumStateChange);
//        else if (currentTxFrame != nullptr)
//            handleWithFsm(currentTxFrame);
//        else if (!txQueue->isEmpty()) {
//            popTxQueue();
//            handleWithFsm(currentTxFrame);
//        }
//    }

    // Back in IDLE: the next frame goes unless the MAC defers transmissions (AFLoRa: waiting for
    // a retry or for the end of the duty-cycle off-time).
    if (fsm.getState() == IDLE) {
        if (isReceiving())
            handleWithFsm(mediumStateChange);
        else
            startNextTransmission();
    }

    if (endSifs) {
        if (isLowerMessage(msg) && pkt->getOwner() == this && (endSifs->getContextPointer() != pkt))
            delete pkt;
    }
    else {
        if (isLowerMessage(msg) && pkt->getOwner() == this)
            delete pkt;
    }
    getDisplayString().setTagArg("t", 0, fsm.getStateName());
}

void LoRaMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method_Silent();
    if (signalID == IRadio::receptionStateChangedSignal) {
        IRadio::ReceptionState newRadioReceptionState = (IRadio::ReceptionState)value;
        if (receptionState == IRadio::RECEPTION_STATE_RECEIVING) {
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }
        receptionState = newRadioReceptionState;
        handleWithFsm(mediumStateChange);
    }
    else if (signalID == LoRaRadio::droppedPacket) {
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        handleWithFsm(droppedPacket);
    }
    else if (signalID == IRadio::transmissionStateChangedSignal) {
        IRadio::TransmissionState newRadioTransmissionState = (IRadio::TransmissionState)value;
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && newRadioTransmissionState == IRadio::TRANSMISSION_STATE_IDLE) {
            handleWithFsm(endTransmission);
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }
        transmissionState = newRadioTransmissionState;
    }
}

Packet *LoRaMac::encapsulate(Packet *msg)
{
    auto frame = makeShared<LoRaMacFrame>();
    frame->setChunkLength(B(headerLength));
    msg->setArrival(msg->getArrivalModuleId(), msg->getArrivalGateId());
    // AFLoRa: TP, SF, CF, BW, CR and the header flag are set by the MAC, not by the app. The
    // tag carries them on to LoRaRadio, which builds the preamble from it.
    auto tag = msg->addTagIfAbsent<LoRaTag>();
    tag->setPower(mW(math::dBmW2mW(getPresentTP())));
    tag->setCenterFrequency(getPresentCF());
    tag->setSpreadFactor(getPresentSF());
    tag->setBandwidth(getPresentBW());
    tag->setCodeRendundance(getPresentCR());
    tag->setUseHeader(loRaRadio->loRaUseHeader);

    frame->setTransmitterAddress(address);
    frame->setLoRaTP(tag->getPower().get());
    frame->setLoRaCF(tag->getCenterFrequency());
    frame->setLoRaSF(tag->getSpreadFactor());
    frame->setLoRaBW(tag->getBandwidth());
    frame->setLoRaCR(tag->getCodeRendundance());
    frame->setSequenceNumber(sequenceNumber);
    frame->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);

    ++sequenceNumber;
    //frame->setLoRaUseHeader(cInfo->getLoRaUseHeader());
    frame->setLoRaUseHeader(tag->getUseHeader());

    // AFLoRa: frame control. The ADR bit is set when the device runs ADR, and ADRACKReq once
    // ADR_ACK_LIMIT uplinks went without a downlink.
    frame->setMType(tag->getMType());
    frame->setACK(false);
    frame->setFOpsLen(NO_OPTIONS);
    frame->setADRACKReq(false);
    if (evaluateADRinNode) {
        frame->setADR(true);
        if (sendNextPacketWithADRACKReq)
            frame->setADRACKReq(true);
        sendNextPacketWithADRACKReq = false;
    }

    msg->insertAtFront(frame);

    return msg;
}

Packet *LoRaMac::decapsulate(Packet *frame)
{

    auto loraHeader = frame->popAtFront<LoRaMacFrame>();
    frame->addTagIfAbsent<MacAddressInd>()->setSrcAddress(loraHeader->getTransmitterAddress());
    frame->addTagIfAbsent<MacAddressInd>()->setDestAddress(loraHeader->getReceiverAddress());
    frame->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
//    auto payloadProtocol = ProtocolGroup::ethertype.getProtocol(loraHeader->getNetworkProtocol());
//    frame->addTagIfAbsent<DispatchProtocolReq>()->setProtocol(payloadProtocol);
//    frame->addTagIfAbsent<PacketProtocolTag>()->setProtocol(payloadProtocol);

    return frame;
}

/****************************************************************
 * Frame sender functions.
 */
void LoRaMac::sendDataFrame(Packet *frameToSend)
{
    EV << "sending Data frame\n";

    // AFLoRa: a confirmed uplink waits for its ACK; while it does, every new send is a retry.
    const auto &header = frameToSend->peekAtFront<LoRaMacFrame>();
    if (ACKPending) {
        EV << "as a retransmission\n";
        numRetry++;
    }
    else if (header->getMType() == CONF_DATA_UP)
        ACKPending = true;

    radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);

    auto frameCopy = frameToSend->dup();

    //LoRaMacControlInfo *ctrl = new LoRaMacControlInfo();
    //ctrl->setSrc(frameCopy->getTransmitterAddress());
    //ctrl->setDest(frameCopy->getReceiverAddress());
//    frameCopy->setControlInfo(ctrl);
    auto macHeader = frameCopy->peekAtFront<LoRaMacFrame>();

    auto macAddressInd = frameCopy->addTagIfAbsent<MacAddressInd>();
    macAddressInd->setSrcAddress(macHeader->getTransmitterAddress());
    macAddressInd->setDestAddress(macHeader->getReceiverAddress());

    //frameCopy->addTag<PacketProtocolTag>()->setProtocol(&Protocol::lora);

    // AFLoRa: duty-cycle off-time, counted from the start of the frame
    if (dutyCycleEnabled) {
        dcTimeOff = true;
        if (endDCTimeOff->isScheduled())
            cancelEvent(endDCTimeOff);
        scheduleAt(simTime() + getTimeOff(), endDCTimeOff);
    }

    // AFLoRa statistics
    sfSentCount.record(getPresentSF());
    tpSentCount.record(getPresentTP());
    emit(LoRa_MacFrameSent, (intval_t)macHeader->getMType());
    if (macHeader->getMType() == CONF_DATA_UP)
        numSentConFrames++;
    if (macHeader->getMType() == UCONF_DATA_UP)
        numSentUnconFrames++;

    sendDown(frameCopy);
}

void LoRaMac::sendAckFrame()
{
    auto frameToAck = static_cast<Packet *>(endSifs->getContextPointer());
    endSifs->setContextPointer(nullptr);
    auto macHeader = makeShared<CsmaCaMacAckHeader>();
    macHeader->setReceiverAddress(MacAddress(frameToAck->peekAtFront<LoRaMacFrame>()->getTransmitterAddress().getInt()));

    EV << "sending Ack frame\n";
    //auto macHeader = makeShared<CsmaCaMacAckHeader>();
    macHeader->setChunkLength(B(ackLength));
    auto frame = new Packet("CsmaAck");
    frame->insertAtFront(macHeader);
//    frame->addTag<PacketProtocolTag>()->setProtocol(&Protocol::lora);
    radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);

    auto macAddressInd = frame->addTagIfAbsent<MacAddressInd>();
    macAddressInd->setSrcAddress(macHeader->getTransmitterAddress());
    macAddressInd->setDestAddress(macHeader->getReceiverAddress());

    sendDown(frame);
}

/****************************************************************
 * Helper functions.
 */
void LoRaMac::finishCurrentTransmission()
{
    scheduleAt(simTime() + waitDelay1Time, endDelay_1);
    scheduleAt(simTime() + waitDelay1Time + listening1Time, endListening_1);
    scheduleAt(simTime() + waitDelay1Time + listening1Time + waitDelay2Time, endDelay_2);
    scheduleAt(simTime() + waitDelay1Time + listening1Time + waitDelay2Time + listening2Time, endListening_2);
    // AFLoRa: a confirmed uplink is kept until its ACK arrives or the retries run out.
    if (!ACKPending)
        deleteCurrentTxFrame();
    //popTxQueue();
}

Packet *LoRaMac::getCurrentTransmission()
{
    ASSERT(currentTxFrame != nullptr);
    return currentTxFrame;
}

bool LoRaMac::isReceiving()
{
    return radio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING;
}

bool LoRaMac::isAck(const Ptr<const LoRaMacFrame> &frame)
{
    return frame->getACK();   // AFLoRa (FLoRa returned false)
}

bool LoRaMac::isBroadcast(const Ptr<const LoRaMacFrame> &frame)
{
    return frame->getReceiverAddress().isBroadcast();
}

bool LoRaMac::isForUs(const Ptr<const LoRaMacFrame> &frame)
{
    return frame->getReceiverAddress() == address;
}

void LoRaMac::turnOnReceiver()
{
    LoRaRadio *loraRadio;
    loraRadio = check_and_cast<LoRaRadio *>(radio);
    loraRadio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
}

void LoRaMac::turnOffReceiver()
{
    LoRaRadio *loraRadio;
    loraRadio = check_and_cast<LoRaRadio *>(radio);
    loraRadio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
}

MacAddress LoRaMac::getAddress()
{
    return address;
}

/****************************************************************
 * AFLoRa: end-device ADR and radio parameters in the MAC.
 */
void LoRaMac::manageFrame(const Ptr<const LoRaMacFrame> &frame)
{
    sequenceNumberRemote = frame->getSequenceNumber();
    // ACK of the confirmed uplink waiting in currentTxFrame
    if (frame->getACK()) {
        ACKPending = false;
        retry = false;
        if (sfMode == SFM3 && loRaRadio->loRaSF > 7)
            loRaRadio->loRaSF--;
        if (countRetry == 0) {
            if (simTime() >= getSimulation()->getWarmupPeriod())
                numAckReceivedNoRetry++;
        }
        else
            countRetry = 0;
        if (simTime() >= getSimulation()->getWarmupPeriod()) {
            numAckReceived++;
            rcvdAckPk.record(1);
        }
        if (currentTxFrame != nullptr)
            deleteCurrentTxFrame();
        EV << "Received Ack frame\n";
    }
    // Any downlink resets the ADRACKReq counter; the options carry a LinkADRReq.
    if (evaluateADRinNode) {
        ADR_ACK_CNT = 0;
        if (frame->getFOpsLen() != 0) {
            numADRCommandReceived++;
            // Truncated to whole dBm as SimpleLoRaApp::setTP(int) did in FLoRa 1.x (AFLoRa kept
            // the value as a double); kept so that the default behaviour stays FLoRa's.
            if (frame->getOptions().getLoRaTP() != -1)
                loRaRadio->loRaTP = (int) frame->getOptions().getLoRaTP();
            if (frame->getOptions().getLoRaSF() != -1)
                loRaRadio->loRaSF = frame->getOptions().getLoRaSF();
            EV << "New TP " << getPresentTP() << ", new SF " << getPresentSF() << endl;
        }
    }
}

void LoRaMac::setRetransmission()
{
    // No downlink in RX1 nor RX2. A confirmed uplink goes again after a random 1..3 s, with the
    // SF of the chosen mode, until maxRetry transmissions; then it is dropped.
    if (ACKPending) {
        countRetry++;
        if (countRetry < maxRetry) {
            if (sfMode == SFM0 || sfMode == SFM2 || sfMode == SFM3) {
                if (loRaRadio->loRaSF < 12 && (countRetry % 2) == 0) {
                    loRaRadio->loRaSF++;
                    setCurrentTxFrameSF(loRaRadio->loRaSF);
                }
                else if (sfMode == SFM2 && loRaRadio->loRaSF == 12 && (countRetry % 2) == 0) {
                    loRaRadio->loRaSF = 7;
                    setCurrentTxFrameSF(loRaRadio->loRaSF);
                }
            }
            acktimeoutTime = intuniform(1, 3);
            retry = false;
            if (retransmission->isScheduled())
                cancelEvent(retransmission);
            scheduleAt(simTime() + acktimeoutTime, retransmission);
        }
        else {
            retry = false;
            ACKPending = false;
            countRetry = 0;
            if (simTime() >= getSimulation()->getWarmupPeriod()) {
                numAckNotReceived++;
                numGivenUp++;
            }
            if (currentTxFrame != nullptr)
                deleteCurrentTxFrame();
            EV << "Max Retries. Drop frame\n";
        }
    }
    // ADRACKReq after ADR_ACK_LIMIT uplinks without a downlink, one SF step up after
    // ADR_ACK_LIMIT + ADR_ACK_DELAY (FLoRa did this in SimpleLoRaApp when sending).
    if (evaluateADRinNode) {
        ADR_ACK_CNT++;
        if (ADR_ACK_CNT == ADR_ACK_LIMIT)
            sendNextPacketWithADRACKReq = true;
        if (ADR_ACK_CNT >= ADR_ACK_LIMIT + ADR_ACK_DELAY) {
            ADR_ACK_CNT = 0;
            increaseSFIfPossible();
        }
    }
}

void LoRaMac::increaseSFIfPossible()
{
    if (loRaRadio->loRaSF < 12)
        loRaRadio->loRaSF++;
}

bool LoRaMac::isTxDeferred() const
{
    // AFLoRa txDeferred: a frame waiting for its ACK goes again once the retry timer fired and
    // the duty-cycle off-time is over; otherwise only the off-time defers.
    if (ACKPending)
        return !(retry && !dcTimeOff);
    return dcTimeOff;
}

void LoRaMac::startNextTransmission()
{
    if (fsm.getState() != IDLE || isTxDeferred())
        return;
    if (currentTxFrame == nullptr) {
        if (transmissionQueue.isEmpty())
            return;
        currentTxFrame = check_and_cast<Packet *>(transmissionQueue.pop());
    }
    // Idle-Transmit needs a frame from the upper layer; anything else would bring us back here.
    if (!isUpperMessage(currentTxFrame))
        throw cRuntimeError("Model error: queued frame did not arrive from the upper layer");
    handleWithFsm(currentTxFrame);
}

int LoRaMac::getQueueLength() const
{
    return transmissionQueue.getLength() + (currentTxFrame != nullptr ? 1 : 0);
}

void LoRaMac::setCurrentTxFrameSF(int sf)
{
    if (currentTxFrame == nullptr)
        return;
    auto header = currentTxFrame->removeAtFront<LoRaMacFrame>();
    header->setLoRaSF(sf);
    currentTxFrame->insertAtFront(header);
    currentTxFrame->getTagForUpdate<LoRaTag>()->setSpreadFactor(sf);
}

double LoRaMac::getPresentTP()
{
    return loRaRadio->loRaTP;
}

int LoRaMac::getPresentSF()
{
    return loRaRadio->loRaSF;
}

units::values::Hz LoRaMac::getPresentCF()
{
    return loRaRadio->loRaCF;
}

units::values::Hz LoRaMac::getPresentBW()
{
    return loRaRadio->loRaBW;
}

int LoRaMac::getPresentCR()
{
    return loRaRadio->loRaCR;
}

double LoRaMac::getTimeOff()
{
    // 1% duty cycle for 20 B at CR 4/8 (100 times the airtime): the duty-cycle off-time and the
    // shortest spacing SimpleLoRaApp accepts between two packets. FLoRa's SimpleLoRaApp table
    // had 85.6064 s at SF11 (no low data rate optimisation); AFLoRa's MAC uses 98.7136 s. The
    // AFLoRa value applies when the device duty cycle is on.
    switch (loRaRadio->loRaSF) {
        case 7: return 7.808;
        case 8: return 13.9776;
        case 9: return 24.6784;
        case 10: return 49.3568;
        case 11: return dutyCycleEnabled ? 98.7136 : 85.6064;
        case 12: return 171.2128;
        default: return 0;
    }
}

} // namespace inet
