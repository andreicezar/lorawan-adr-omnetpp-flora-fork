#ifndef __LORAMAC_H
#define __LORAMAC_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/linklayer/contract/IMacProtocol.h"
#include "inet/linklayer/base/MacProtocolBase.h"
#include "inet/common/FSMA.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "LoRaMacControlInfo_m.h"
#include "LoRaMacFrame_m.h"
#include "../LoRaApp/LoRaAppPacket_m.h"   // AFLoRa: AppPacketType values used as the frame MType
#include "inet/common/Protocol.h"
#include "inet/queueing/contract/IActivePacketSink.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "inet/linklayer/contract/IMacProtocol.h"

#include "LoRaRadio.h"

namespace flora {

using namespace physicallayer;

/**
 * Based on CSMA class
 */

class LoRaMac : public MacProtocolBase, public IMacProtocol, public queueing::IActivePacketSink
{
  public:
    using cIListener::finish;
    using MacProtocolBase::receiveSignal;
  protected:
    /**
     * @name Configuration parameters
     */
    //@{
    MacAddress address;
    bool useAck = true;
    double bitrate = NaN;
    int headerLength = -1;
    int ackLength = -1;
    simtime_t ackTimeout = -1;
    simtime_t slotTime = -1;
    simtime_t sifsTime = -1;
    simtime_t difsTime = -1;
    simtime_t waitDelay1Time = -1;
    simtime_t listening1Time = -1;
    simtime_t waitDelay2Time = -1;
    simtime_t listening2Time = -1;
    int maxQueueSize = -1;
    int retryLimit = -1;
    int cwMin = -1;
    int cwMax = -1;
    int cwMulticast = -1;
    int sequenceNumber = 0;

    // AFLoRa: the MAC owns the radio parameters (kept in the LoRaRadio members, which the PHY
    // reads) and the end-device side of ADR.
    LoRaRadio *loRaRadio = nullptr;
    bool evaluateADRinNode = false;
    int ADR_ACK_CNT = 0;
    int ADR_ACK_LIMIT = 64;
    int ADR_ACK_DELAY = 32;
    bool sendNextPacketWithADRACKReq = false;
    int sequenceNumberRemote = 0;

    // AFLoRa: confirmed uplinks. The frame waiting for its ACK stays in currentTxFrame and is
    // sent again after a random 1..3 s, up to maxRetry transmissions in all.
    bool ACKPending = false;
    bool retry = false;          // the retransmission timer has fired
    int maxRetry = 8;
    int countRetry = 0;
    simtime_t acktimeoutTime = -1;
    // AFLoRa: SF management on retransmissions of confirmed uplinks
    enum SFModes {
        SFM0,   // SF as defined in the LoRaWAN specification: one step up every two retries
        SFM1,   // SF unchanged
        SFM2,   // circular: SF7 -> ... -> SF12 -> SF7 -> ...
        SFM3,   // one step up every two retries, one step down when an ACK arrives
    };
    SFModes sfMode = SFM0;
    std::string sfModePar;
    //@}

    /** End of the Short Inter-Frame Time period */
    cMessage *endSifs = nullptr;

    /** End of the Data Inter-Frame Time period */
    cMessage *endDifs = nullptr;

    /** End of the backoff period */
    cMessage *endBackoff = nullptr;

    /** End of the ack timeout */
    cMessage *endAckTimeout = nullptr;

    /**
     * @name CsmaCaMac state variables
     * Various state information checked and modified according to the state machine.
     */
    //@{
    enum State {
        IDLE,
        TRANSMIT,
        WAIT_DELAY_1,
        LISTENING_1,
        RECEIVING_1,
        WAIT_DELAY_2,
        LISTENING_2,
        RECEIVING_2,
    };

    IRadio *radio = nullptr;
    IRadio::TransmissionState transmissionState = IRadio::TRANSMISSION_STATE_UNDEFINED;
    IRadio::ReceptionState receptionState = IRadio::RECEPTION_STATE_UNDEFINED;

    Fsm fsm;

    /** Remaining backoff period in seconds */
    simtime_t backoffPeriod = -1;

    /** Number of frame retransmission attempts. */
    int retryCounter = -1;

    /** Messages received from upper layer and to be transmitted later */
    cPacketQueue transmissionQueue;

    /** Passive queue module to request messages from */
    cPacketQueue *queueModule = nullptr;
    //@}

    /** @name Timer messages */
    //@{
    /** Timeout after the transmission of a Data frame */
    cMessage *endTransmission = nullptr;

    /** Timeout after the reception of a Data frame */
    cMessage *endReception = nullptr;

    /** Timeout after the reception of a Data frame */
    cMessage *droppedPacket = nullptr;

    /** End of the Delay_1 */
    cMessage *endDelay_1 = nullptr;

    /** End of the Listening_1 */
    cMessage *endListening_1 = nullptr;

    /** End of the Delay_2 */
    cMessage *endDelay_2 = nullptr;

    /** End of the Listening_2 */
    cMessage *endListening_2 = nullptr;

    /** Radio state change self message. Currently this is optimized away and sent directly */
    cMessage *mediumStateChange = nullptr;

    /** AFLoRa: end of the ACK timeout, retransmission of a confirmed uplink */
    cMessage *retransmission = nullptr;
    //@}

    /** @name Statistics */
    //@{
    long numRetry;
    long numSentWithoutRetry;
    long numGivenUp;
    long numCollision;
    long numSent;
    long numReceived;
    long numSentBroadcast;
    long numReceivedBroadcast;
    long numADRCommandReceived;   // AFLoRa
    // AFLoRa
    long numAckReceivedNoRetry;
    long numSentConFrames;
    long numSentUnconFrames;
    long numAckReceived;
    long numAckSent;
    long numAckNotReceived;
    cOutVector sfSentCount;        // SF of every transmitted frame
    cOutVector tpSentCount;        // TX power of every transmitted frame
    cOutVector reqSendPkConfd;     // confirmed packets the app asked to send
    cOutVector reqSendPkUnConfd;   // unconfirmed packets the app asked to send
    cOutVector rcvdAckPk;          // ACKs received
    simsignal_t LoRa_MacFrameSent;
    //@}

  public:
    // AFLoRa: current radio parameters, as the MAC sets them.
    virtual double getPresentTP();
    virtual int getPresentSF();
    virtual inet::units::values::Hz getPresentCF();
    virtual inet::units::values::Hz getPresentBW();
    virtual int getPresentCR();
    // Spacing the app keeps between two packets (AFLoRa moved this table from SimpleLoRaApp).
    virtual double getTimeOff();

    /**
     * @name Construction functions
     */
    //@{
    virtual ~LoRaMac();
    //@}
    virtual MacAddress getAddress();
    virtual queueing::IPassivePacketSource *getProvider(const cGate *gate) override;
    virtual void handleCanPullPacketChanged(const cGate *gate) override;
    virtual void handlePullPacketProcessed(Packet *packet, const cGate *gate, bool successful) override;

  protected:
    /**
     * @name Initialization functions
     */
    //@{
    /** @brief Initialization of the module and its variables */
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void configureNetworkInterface() override;
    //@}

    /**
     * @name Message handing functions
     * @brief Functions called from other classes to notify about state changes and to handle messages.
     */
    //@{
    virtual void handleSelfMessage(cMessage *msg) override;
    virtual void handleUpperPacket(Packet *packet) override;
    virtual void handleLowerPacket(Packet *packet) override;
    virtual void handleWithFsm(cMessage *msg);

    virtual void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details) override;

    virtual Packet *encapsulate(Packet *msg);
    virtual Packet *decapsulate(Packet *frame);
    //@}

    // OperationalBase:
    virtual void handleStartOperation(LifecycleOperation *operation) override {}    //TODO implementation
    virtual void handleStopOperation(LifecycleOperation *operation) override {}    //TODO implementation
    virtual void handleCrashOperation(LifecycleOperation *operation) override {}    //TODO implementation

    /**
     * @name Frame transmission functions
     */
    //@{
    virtual void sendDataFrame(Packet *frameToSend);
    virtual void sendAckFrame();
    //virtual void sendJoinFrame();
    //@}

    /**
     * @name Utility functions
     */
    //@{
    virtual void finishCurrentTransmission();
    virtual Packet *getCurrentTransmission();

    virtual bool isReceiving();
    virtual bool isAck(const Ptr<const LoRaMacFrame> &frame);
    virtual bool isBroadcast(const Ptr<const LoRaMacFrame> & msg);
    virtual bool isForUs(const Ptr<const LoRaMacFrame> &msg);

    void turnOnReceiver(void);
    void turnOffReceiver(void);
    virtual void processUpperPacket();

    // AFLoRa
    /** Downlink for this device: frame control and MAC commands (LinkADRReq). */
    virtual void manageFrame(const Ptr<const LoRaMacFrame> &frame);
    /** RX2 closed without a downlink: retransmission of a confirmed uplink, ADRACKReq counter and ADR backoff. */
    virtual void setRetransmission();
    virtual void increaseSFIfPossible();
    /** True while the MAC must not start a transmission (AFLoRa txDeferred). */
    virtual bool isTxDeferred() const;
    /** Sets the SF of the frame waiting for its ACK (header and tag). */
    virtual void setCurrentTxFrameSF(int sf);
    //@}
};

} // namespace inet

#endif // ifndef __LORAMAC_H
