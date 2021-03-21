/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgCommon.h"
#include "ptpClockContext.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#ifndef IN_RANGE
#define IN_RANGE(num, min, max) ((num) >= (min) && (num) <= (max))
#endif

//
// PtpMsgCommon implementation
//

void ptpMsgCommonCtor(MsgCommonRef msg)
{
    assert(msg);
    msg->_version = PTP_VERSION;
    msg->_messageType = PTP_SYNC_MESSAGE;
    msg->_sequenceId = 0;
    msg->_messageLength = 0;
    msg->_logMeanMsgInterval = 0;
    msg->_correctionField = 0;
    msg->_domainNumber = 0;

    ptpPortIdentityCtor(&(msg->_sourcePortIdentity));
}

// Default constructor. Creates MsgCommon from given payload
void ptpMsgCommonPayloadCtor(MsgCommonRef msg, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);
    assert(payloadSize >= sizeof(MsgCommon));
    (void)payloadSize; //unused in release builds
    memcpy(msg, payload, sizeof(MsgCommon));
    msg->_messageType = msg->_messageType;

    // convert properties from network order to machine order
    msg->_correctionField = ptpNtohlls(msg->_correctionField);
    msg->_messageLength = ptpNtohsu(msg->_messageLength);
    msg->_sequenceId = ptpNtohsu(msg->_sequenceId);

    ptpPortIdentityFromNetworkOrder(&(msg->_sourcePortIdentity));
}

// destructor
void ptpMsgCommonDtor(MsgCommonRef msg)
{
    if (!msg)
        return;

    ptpPortIdentityDtor(&(msg->_sourcePortIdentity));
}

void ptpMsgCommonCalculate(MsgDelayRespRef delayResponse, NetworkPortRef port)
{
    assert(delayResponse);
    assert(port);
    ptpMutexLock(ptpNetPortGetProcessingLock(port));

    if (ePortStateSlave == ptpNetPortGetState(port)) {

        ClockCtxRef clock = ptpNetPortGetClock(port);

        uint16_t delayResponsSeqId = ptpMsgCommonGetSequenceId(ptpMsgDelayRespGetBase(delayResponse));
        MsgDelayReqRef delayRequest = ptpNetPortGetCurrentDelayReq(port, delayResponsSeqId);
        MsgSyncRef sync = NULL;
        MsgFollowUpRef followUp = NULL;

        if (delayRequest != NULL) {
            sync = ptpNetPortGetCurrentSync(port, ptpMsgCommonGetPortIdentity(ptpMsgDelayRespGetBase(delayResponse)), ptpMsgDelayReqGetMatcingFollowUpSeqId(delayRequest));
            followUp = ptpNetPortGetCurrentFollowUp(port, ptpMsgCommonGetPortIdentity(ptpMsgDelayRespGetBase(delayResponse)), ptpMsgDelayReqGetMatcingFollowUpSeqId(delayRequest));
        }

        if (sync && followUp && delayRequest && delayResponse) {
            // Ensure our last values are from the same "group" of
            // sync/followup/delay_req/delay_resp messages from the
            // same clock.
            PortIdentityRef syncPortId = ptpMsgCommonGetPortIdentity(ptpMsgSyncGetBase(sync));

            if (ptpPortIdentityHasSameClocks(ptpNetPortGetPortIdentity(port), ptpMsgCommonGetPortIdentity(ptpMsgDelayReqGetBase(delayRequest)))
                && ptpPortIdentityHasSameClocks(syncPortId, ptpMsgCommonGetPortIdentity(ptpMsgDelayRespGetBase(delayResponse)))
                && ptpPortIdentityHasSameClocks(syncPortId, ptpMsgCommonGetPortIdentity(ptpMsgFollowUpGetBase(followUp)))) {

                NanosecondTs t1 = TIMESTAMP_TO_NANOSECONDS(ptpMsgFollowUpGetPreciseOriginTimestamp(followUp)); // time of the sync message from the Master
                NanosecondTs t2 = TIMESTAMP_TO_NANOSECONDS(ptpMsgSyncGetRxTimestamp(sync)); // time of the sync message as it is received at the Slave
                NanosecondTs t3 = TIMESTAMP_TO_NANOSECONDS(ptpMsgDelayReqGetTxTimestamp(delayRequest)); // time when we sent the delay request message from the Slave
                NanosecondTs t4 = TIMESTAMP_TO_NANOSECONDS(ptpMsgDelayRespGetRequestReceiptTimestamp(delayResponse)); // time of the delay request message when received at the Master.

                int64_t followUpCorrection = ptpMsgCommonGetCorrectionField(ptpMsgFollowUpGetBase(followUp));
                int64_t delayResponseCorrection = ptpMsgCommonGetCorrectionField(ptpMsgDelayRespGetBase(delayResponse));

                // The correction fileds are in 2^16 so correct them by shifting
                // right 16 bits and ignore the fractional part
                followUpCorrection /= 65536;
                delayResponseCorrection /= 65536;

                // Adjust t1 and t4 by the correction field
                t1 += followUpCorrection;
                t4 += delayResponseCorrection;

                PTP_LOG_DEBUG("follow up correction\t: %f ms", NANOSECONDS_TO_MILLISECONDS((float)followUpCorrection));
                PTP_LOG_DEBUG("delay resp. correction\t: %f ms", NANOSECONDS_TO_MILLISECONDS((float)delayResponseCorrection));

                PTP_LOG_DEBUG("t1 (SYNC snd from master)\t: %lf ms", NANOSECONDS_TO_MILLISECONDS(t1));
                PTP_LOG_DEBUG("t2 (SYNC rcvd on slave)  \t: %lf ms", NANOSECONDS_TO_MILLISECONDS(t2));
                PTP_LOG_DEBUG("t3 (DREQ snd from slave) \t: %lf ms", NANOSECONDS_TO_MILLISECONDS(t3));
                PTP_LOG_DEBUG("t4 (DREQ rcvd on master) \t: %lf ms", NANOSECONDS_TO_MILLISECONDS(t4));

                // Ensure that  t4 >= t1 and t3 >= t2
                if (t4 >= t1 && t3 >= t2) {

                    //
                    // Roundtrip time calculation details:
                    //

                    // master to slave delay: (t2 - t1)
                    // slave to master delay: (t4 - t3)
                    // one way delay		: ((t2  t1) + (t4  t3))/2
                    // is the same as		: ((t4 - t1) - (t3 - t2))/2

                    NanosecondTs delay = ((t2 - t1) + (t4 - t3)) / 2;
                    Boolean delayIsNegative = false;

                    PTP_LOG_DEBUG("calculated link delay\t: %lf ms", NANOSECONDS_TO_MILLISECONDS(delay));

                    // negative delay is not allowed - this usually indicates some issue in the timestamping code
                    if (delay < 0) {
                        PTP_LOG_STATUS("negative link delay [%lf] detected - assuming 0 ms link delay", delay);
                        delay = 0;
                        delayIsNegative = true;
                    }

                    //
                    // check if <= 80 ms
                    // it has to be below 80ms
                    //

                    if (delay <= MAX_ROUNDTRIP_TIME_NS) {

                        PTP_LOG_DEBUG("delay (%lf ms) is below %lf ms, number of correct synchronizations = %d",
                            NANOSECONDS_TO_MILLISECONDS(delay), NANOSECONDS_TO_MILLISECONDS(MAX_ROUNDTRIP_TIME_NS), ptpNetPortGetGoodSyncCount(port));

                        // Do this for the 2nd successful sync that has a less than
                        // check value
                        if (ptpNetPortGetGoodSyncCount(port) > 1) {

                            // local time is calculated as a mean value (time in the middle of interval between <t2;t3>
                            NanosecondTs localTime = (t2 + t3) / 2;

                            // remote time is calculated as a mean value (time in the middle of interval between <t1;t4>
                            NanosecondTs remoteTime = (t4 + t1) / 2;

                            // fix initial values if we are here for the 1st time
                            if (ptpNetPortGetGoodSyncCount(port) == 2) {

                                ptpNetPortSetLastLocaltime(port, localTime);
                                ptpNetPortSetLastRemoteTime(port, remoteTime);
                            }

                            // delta between two consecutive measurements of local and remote time
                            NanosecondTs localTimeDelta = localTime - ptpNetPortGetLastLocalTime(port);
                            NanosecondTs remoteTimeDelta = remoteTime - ptpNetPortGetLastRemoteTime(port);

                            // keep the current local/remote time
                            ptpNetPortSetLastLocaltime(port, localTime);
                            ptpNetPortSetLastRemoteTime(port, remoteTime);

                            // how much difference between remote/local time can we accept?
                            NanosecondTs remoteDeltaLimit = remoteTimeDelta / 100;

                            // upper limit
                            NanosecondTs remoteDeltaUpper = remoteTimeDelta + remoteDeltaLimit;

                            // lower limit
                            NanosecondTs remoteDeltaLower = remoteTimeDelta - remoteDeltaLimit;

                            PTP_LOG_DEBUG("out of range diffs\t: %d", ptpNetPortGetBadDiffCount(port));
                            PTP_LOG_DEBUG("local time delta\t: %f ms", localTimeDelta / 1000000.0);
                            PTP_LOG_DEBUG("remote delta upper\t: %f ms", remoteDeltaUpper / 1000000.0);
                            PTP_LOG_DEBUG("remote delta lower\t: %f ms", remoteDeltaLower / 1000000.0);

                            // Limit the precision of our floating point values
                            int kLimit = 100;
                            localTimeDelta = ceil(localTimeDelta * kLimit) / kLimit;
                            remoteDeltaUpper = ceil(remoteDeltaUpper * kLimit) / kLimit;
                            remoteDeltaLower = ceil(remoteDeltaLower * kLimit) / kLimit;

                            // Limit to 5 bad diffs initially to quickly lock into
                            // master time then increase the amout to stabilize the
                            // offests. Keep this check in place otherwise we loose
                            // too many.

                            // If good or too many bads
                            if (IN_RANGE(localTimeDelta, remoteDeltaLower, remoteDeltaUpper) || (ptpNetPortGetBadDiffCount(port) > ptpNetPortGetMaxBadDiffCount(port))) {

                                ptpNetPortSetBadDiffCount(port, 0);

                                if ((ptpNetPortGetSuccessDiffCount(port) > PTP_MAX_SUCCESS_CNT) && (ptpNetPortGetSuccessDiffCount(port) < PTP_MAX_BAD_DIFF_CNT)) {
                                    ptpNetPortSetMaxBadDiffCount(port, ptpNetPortGetMaxBadDiffCount(port) * 2);
                                    PTP_LOG_DEBUG("Incremented sMaxBadDiffCount to %d", ptpNetPortGetMaxBadDiffCount(port));
                                }

                                if (ptpNetPortGetSuccessDiffCount(port) < PTP_MAX_BAD_DIFF_CNT) {
                                    ptpNetPortSetSuccessDiffCount(port, ptpNetPortGetSuccessDiffCount(port) + 1);
                                    PTP_LOG_DEBUG("Incremented sSuccessDiffCount to %d", ptpNetPortGetSuccessDiffCount(port));
                                }

                                //Update our delay when it is positive.
                                if(false == delayIsNegative){
                                    ptpNetPortSetLinkDelay(port, delay);
                                }

                                //
                                // calculate ratio between master clock and local clock (this is based on a difference between two consecutive
                                // measurements of delay compensated remote time T1 and local time T2
                                //

                                NanosecondTs masterToLocalRatio = ptpClockCtxCalcmasterToLocalRatio(clock, t1 + ptpNetPortGetLinkDelay(port), t2);

                                // only proceed if we got a valid clock ratio:
                                if (masterToLocalRatio != NEGATIVE_TIME_JUMP) {

                                    // Update information on local status structure.
                                    int32_t scaledLastGmFreqChange = (int32_t)((1.0 / masterToLocalRatio - 1.0) * (1ULL << 41));

                                    // phase of our clock ratio
                                    ScaledNs scaledLastGmPhaseChange;
                                    ptpScaledNsCtor(&scaledLastGmPhaseChange);
                                    ptpScaledNsSetNanosecondsLsb(&scaledLastGmPhaseChange, (uint64_t)ptpFollowUpTlvGetRateOffset(ptpMsgGetFollowUpTlv(followUp)));

                                    ptpFollowUpTlvSetScaledLastGmFreqChange(ptpClockCtxGetFollowUTlvRx(clock), scaledLastGmFreqChange);
                                    ptpFollowUpTlvAssignScaledLastGmPhaseChange(ptpClockCtxGetFollowUTlvRx(clock), &scaledLastGmPhaseChange);

                                    // The sync_count counts the number of sync messages received
                                    // that influence the time on the device. Since adjustments are only
                                    // made in the PTP_SLAVE state, increment it here
                                    ptpNetPortIncSyncCount(port);

                                    uint64_t grandMasterId = 0;
                                    ClockIdentityRef identity = NULL;

                                    if (ePortStateMaster == ptpNetPortGetState(port)) {
                                        identity = ptpPortIdentityGetClockIdentity(ptpNetPortGetPortIdentity(port));
                                    } else {
                                        identity = ptpClockCtxGetGrandmasterClockIdentity(clock);
                                    }

                                    grandMasterId = ptpClockIdentityGet(identity);

                                    // this is the offset between our local clock and the the master clock.
                                    NanosecondTs masterToLocalPhase = (t2 - t1) - ptpNetPortGetLinkDelay(port);

                                    // synchronization is complete, update synchronization context
                                    ptpClockCtxSyncComplete(
                                        clock,
                                        masterToLocalPhase,
                                        masterToLocalRatio,
                                        t2,
                                        ptpNetPortGetSyncCount(port),
                                        ptpNetPortGetState(port),
                                        grandMasterId);

                                    // Restart the SYNC_RECEIPT timer
                                    ptpNetPortStartSyncReceiptTimer(port, (uint64_t)(SYNC_RECEIPT_TIMEOUT_MULTIPLIER * ((double)pow((double)2, ptpNetPortGetSyncInterval(port)) * PTP_AVERAGE_MSG_RECV_INTERVAL)));

                                    uint16_t lastGmTimeBaseIndicator = ptpNetPortGetLastGmTimeBaseIndicator(port);

                                    if ((lastGmTimeBaseIndicator > 0) && (ptpFollowUpTlvGetGmTimeBaseIndicator(ptpMsgGetFollowUpTlv(followUp)) != lastGmTimeBaseIndicator)) {
                                        PTP_LOG_STATUS("Sync discontinuity");
                                    }

                                    ptpNetPortSetLastGmTimeBaseIndicator(port, ptpFollowUpTlvGetGmTimeBaseIndicator(ptpMsgGetFollowUpTlv(followUp)));
                                }
                            } else {
                                ptpNetPortSetBadDiffCount(port, ptpNetPortGetBadDiffCount(port) + 1);
                            }
                        }

                        ptpNetPortSetGoodSyncCount(port, ptpNetPortGetGoodSyncCount(port) + 1);
                    }

                    LastTimestamps lastTs;

                    lastTs._t1 = t1;
                    lastTs._t2 = t2;
                    lastTs._t3 = t3;
                    lastTs._t4 = t4;

                    ptpNetPortSetLastTimestamps(port, &lastTs);

                } else {

                    if (t4 < t1)
                        PTP_LOG_STATUS("condition (t4 > t1) IS FALSE!");
                    if (t3 < t2)
                        PTP_LOG_STATUS("condition (t3 > t2) IS FALSE!");
                }
            }
        } else {
            uint16_t syncSeqId = sync ? ptpMsgCommonGetSequenceId(ptpMsgSyncGetBase(sync)) : 0;
            uint16_t delayRequestSeqId = delayRequest ? ptpMsgCommonGetSequenceId(ptpMsgDelayReqGetBase(delayRequest)) : 0;
            PTP_LOG_STATUS("=============================================");
            PTP_LOG_STATUS("sync/fu:%d delayReq(Res):%d", syncSeqId, delayRequestSeqId);
            PTP_LOG_STATUS("=============================================");
        }
    }

    ptpMutexUnlock(ptpNetPortGetProcessingLock(port));
}

void ptpMsgCommonAssign(MsgCommonRef msg, const MsgCommonRef other)
{
    assert(msg && other);

    if (msg != other) {
        memcpy(msg, other, sizeof(MsgCommon));
    }
}

unsigned char* ptpMsgCommonGetFlags(MsgCommonRef msg)
{
    assert(msg);
    return msg->_flags;
}

// Gets the sequenceId value within a ptp message
uint16_t ptpMsgCommonGetSequenceId(MsgCommonRef msg)
{
    assert(msg);
    return msg->_sequenceId;
}

// Sets the sequence ID value to the PTP message.
void ptpMsgCommonSetSequenceId(MsgCommonRef msg, uint16_t seq)
{
    assert(msg);
    msg->_sequenceId = seq;
}

// Gets the MessageType field within the PTP message.
MessageType ptpMsgCommonGetMessageType(MsgCommonRef msg)
{
    assert(msg);
    return (MessageType)((msg->_messageType) & 0x0F);
}

uint8_t ptpMsgCommonGetTransportSpecific(MsgCommonRef msg)
{
    assert(msg);
    return ((msg->_messageType) >> 4) & 0x0F;
}

// Check if message type is event
Boolean ptpMsgCommonIsEvent(MsgCommonRef msg)
{
    assert(msg);
    return ((msg->_messageType >> 3) & 0x1) == 0;
}

// Gets the correctionField value in a Little-Endian format.
int64_t ptpMsgCommonGetCorrectionField(MsgCommonRef msg)
{
    assert(msg);
    return msg->_correctionField;
}

// Sets the correction field. It expects the host format.
void ptpMsgCommonSetCorrectionField(MsgCommonRef msg, long long correctionAmount)
{
    assert(msg);
    msg->_correctionField = correctionAmount;
}

PortIdentityRef ptpMsgCommonGetPortIdentity(MsgCommonRef msg)
{
    assert(msg);
    return &(msg->_sourcePortIdentity);
}

// Sets PortIdentity value
void ptpMsgCommonAssignPortIdentity(MsgCommonRef msg, PortIdentityRef identity)
{
    assert(msg);
    ptpPortIdentityAssign(&(msg->_sourcePortIdentity), identity);
}

// Generic interface for processing PTP message
void ptpMsgCommonProcessMessage(MsgCommonRef msg, TimestampRef ingressTime, NetworkPortRef port)
{
    // overriden in child classes
    assert(msg);

    switch (ptpMsgCommonGetMessageType(msg)) {

    case PTP_SYNC_MESSAGE:
        ptpMsgSyncProcess((MsgSyncRef)msg, port);
        break;

    case PTP_DELAY_REQ_MESSAGE:
        ptpMsgDelayReqProcess((MsgDelayReqRef)msg, ingressTime, port);
        break;

    case PTP_FOLLOWUP_MESSAGE:
        ptpMsgFollowUpProcess((MsgFollowUpRef)msg, port);
        break;

    case PTP_DELAY_RESP_MESSAGE:
        ptpMsgDelayRespProcess((MsgDelayRespRef)msg, port);
        break;

    case PTP_ANNC_MESSAGE:
        ptpMsgAnnounceProcess((MsgAnnounceRef)msg, port);
        break;

    case PTP_SIGNALLING_MESSAGE:
        ptpMsgSignallingProcess((MsgSignallingRef)msg);
        break;

    case PTP_MANAGEMENT_MESSAGE:
        break;
    }

    // release message
    ptpRefRelease((void*)msg);
}

// Builds PTP common header
void ptpMsgCommonBuildHeader(MsgCommonRef msg, uint8_t* buf)
{
    assert(msg);

    MsgCommonRef outMsg = (MsgCommonRef)buf;
    memcpy(outMsg, msg, sizeof(MsgCommon));

    // add transport specific flag to message type (0x10)
    outMsg->_messageType = outMsg->_messageType | 0x10;
    outMsg->_correctionField = ptpHtonlls(outMsg->_correctionField);
    outMsg->_messageLength = ptpHtonsu(outMsg->_messageLength);
    outMsg->_sequenceId = ptpHtonsu(outMsg->_sequenceId);

    // Per Apple Vendor PTP Profile 2017 - force two way and unicast
    outMsg->_flags[0] |= 6;

    ptpPortIdentityToNetworkOrder(&(outMsg->_sourcePortIdentity));
}

void ptpMsgCommonDumpHeader(MsgCommonRef msg)
{
    assert(msg);

    uint16_t portNumber = ptpPortIdentityGetPortNumber(&(msg->_sourcePortIdentity));

    PTP_LOG_VERBOSE("message type\t\t: %d", ptpMsgCommonGetMessageType(msg));
    PTP_LOG_VERBOSE("ptp version\t\t: %d", (msg->_version & 0xF));
    PTP_LOG_VERBOSE("message length\t: %d", msg->_messageLength);
    PTP_LOG_VERBOSE("domain number\t: %d", msg->_domainNumber);
    PTP_LOG_VERBOSE("flags\t\t: 0x%04x", msg->_flags);
    PTP_LOG_VERBOSE("correction field\t: %lld", msg->_correctionField);
    PTP_LOG_VERBOSE("source port id (clk id)\t: %016llx", ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(&(msg->_sourcePortIdentity))));
    PTP_LOG_VERBOSE("source port id (port #)\t: %d", portNumber);
    PTP_LOG_VERBOSE("sequence id\t\t: %d", msg->_sequenceId);
    PTP_LOG_VERBOSE("control field\t: %d", msg->_control);
    PTP_LOG_VERBOSE("log message interval\t: %d", msg->_logMeanMsgInterval);
}

// Allocate new message object from provided raw data
MsgCommonRef ptpMsgCommonBuildPTPMessage(const uint8_t* buf, size_t size, const LinkLayerAddressRef remote, const NetworkPortRef port, const ClockIdentityRef expectedClockIdentity, const TimestampRef ingressTime)
{
    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };

    MsgCommonRef msg;
    MsgCommon msgCommon;
    ptpMsgCommonPayloadCtor(&msgCommon, buf, size);
    msg = &msgCommon;

    unsigned char transportSpecific = ptpMsgCommonGetTransportSpecific(msg);

    if (1 != transportSpecific) {
        PTP_LOG_EXCEPTION("Received message with unsupported transportSpecific type=%d", transportSpecific);
        return NULL;
    }

    MessageType messageType = ptpMsgCommonGetMessageType(msg);
    uint64_t clockIdentity64 = ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(ptpMsgCommonGetPortIdentity(msg)));

    // if the received message has a different clock identity than the one that we are currently
    // listening to, let's ignore such message...
    // (the only exception is ANNOUNCE message which has to be processed always)
    if (expectedClockIdentity) {

        if (ptpClockIdentityNotEqualTo(ptpPortIdentityGetClockIdentity(ptpMsgCommonGetPortIdentity(msg)), expectedClockIdentity)) {
            if (messageType != PTP_ANNC_MESSAGE) {

                PTP_LOG_STATUS("Received message with unexpected clock identity (%016llx) instead of expected identity (%016llx), ignoring!",
                    clockIdentity64, ptpClockIdentityGetUInt64(expectedClockIdentity));

                return NULL;
            }
        }
    }

    //
    // Handle message type
    //

    switch (messageType) {

    case PTP_SYNC_MESSAGE: {
        // make sure we have enough data
        if (size < sizeof(MsgSyncPayload)) {
            PTP_LOG_ERROR("Sync message length is invalid (%d instead of %d)", size, sizeof(MsgSyncPayload));
            return NULL;
        }

        MsgSyncRef syncMessage = ptpRefMsgSyncAlloc();
        ptpMsgSyncPayloadCtor(syncMessage, ingressTime, buf, size);
        msg = ptpMsgSyncGetBase(syncMessage);

        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received Sync message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");

        PTP_LOG_VERBOSE("Received sync  originTimestamp: %lf s", TIMESTAMP_TO_SECONDS(ptpMsgSyncGetOriginTimestamp(syncMessage)));
        PTP_LOG_VERBOSE("Received sync  correctionField: %d", ptpMsgSyncGetBase(syncMessage)->_correctionField);
    } break;

    case PTP_FOLLOWUP_MESSAGE: {
        // make sure we have enough data
        if (size < sizeof(MsgFollowUp)) {
            PTP_LOG_ERROR("FollowUp message length is invalid (%d instead of %d)", size, sizeof(MsgFollowUp));
            return NULL;
        }

        MsgFollowUpRef followupMessage = ptpRefMsgFollowUpAlloc();
        ptpMsgFollowUpPayloadCtor(followupMessage, buf, size);
        msg = ptpMsgFollowUpGetBase(followupMessage);
        ptpNetPortSetCurrentFollowUp(port, followupMessage);

        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received Follow Up message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");

        PTP_LOG_VERBOSE("Received Follow Up  preciseOriginTimestamp: %lf s", TIMESTAMP_TO_SECONDS(&(followupMessage->_preciseOriginTimestamp)));
        PTP_LOG_VERBOSE("Received Follow Up  correctionField       : %d", ptpMsgFollowUpGetBase(followupMessage)->_correctionField);
    } break;

    case PTP_DELAY_REQ_MESSAGE: {
        // make sure we have enough data
        if (size < sizeof(MsgDelayReqPayload)) {
            PTP_LOG_ERROR("DelayRequest message length is invalid (%d instead of %d)", size, sizeof(MsgDelayReqPayload));
            return NULL;
        }

        MsgDelayReqRef delayReq = ptpRefMsgDelayReqAlloc();
        ptpMsgDelayReqPayloadCtor(delayReq, buf, size);
        msg = ptpMsgDelayReqGetBase(delayReq);

        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received Delay Req message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");
    } break;

    case PTP_DELAY_RESP_MESSAGE: {
        // make sure we have enough data
        if (size < sizeof(MsgDelayResp)) {
            PTP_LOG_ERROR("DelayResponse message length is invalid (%d instead of %d)", size, sizeof(MsgDelayResp));
            return NULL;
        }

        // create delay response object out of the received payload
        MsgDelayRespRef responseMessage = ptpRefMsgDelayRespAlloc();
        ptpMsgDelayRespPayloadCtor(responseMessage, buf, size);
        msg = ptpMsgDelayRespGetBase(responseMessage);

        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received Delay Resp message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");

        PTP_LOG_VERBOSE("requestReceiptTimestamp\t: %lf s", TIMESTAMP_TO_SECONDS(&(responseMessage->_requestReceiptTimestamp)));
        PTP_LOG_VERBOSE("correctionField\t: %lld", msg->_correctionField);
    } break;

    case PTP_ANNC_MESSAGE: {
        // make sure we have enough data
        if (size < sizeof(MsgAnnouncePayload)) {
            PTP_LOG_ERROR("Announce message length is invalid (%d instead of %d)", size, sizeof(MsgAnnouncePayload));
            return NULL;
        }

        MsgAnnounceRef announceMessage = ptpRefMsgAnnounceAlloc();
        ptpMsgAnnouncePayloadCtor(announceMessage, ingressTime, buf, size);
        msg = ptpMsgAnnounceGetBase(announceMessage);

        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received Announce message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");
    } break;

    case PTP_SIGNALLING_MESSAGE: {
        // make sure we have enough data
        if (size < sizeof(MsgSignalling)) {
            PTP_LOG_ERROR("Signalling message length is invalid (%d instead of %d)", size, sizeof(MsgSignalling));
            return NULL;
        }

        MsgSignallingRef signallingMsg = ptpRefMsgSignallingAlloc();
        ptpMsgSignallingPayloadCtor(signallingMsg, buf, size);
        msg = ptpMsgSignallingGetBase(signallingMsg);

        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received Signalling message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");
    } break;

    default: {
        PTP_LOG_DEBUG("==================================================================");
        PTP_LOG_STATUS("Received unsupported message (seq = %d, ingress time = %lf s, clk = %016llx (%d)) from %s", msg->_sequenceId, TIMESTAMP_TO_SECONDS(ingressTime),
            clockIdentity64, ptpPortIdentityGetPortNumber(ptpMsgCommonGetPortIdentity(msg)), ptpLinkLayerAddressString(remote, addrStr, sizeof(addrStr)));
        PTP_LOG_DEBUG("==================================================================");
        return NULL;
    }
    }

    ptpMsgCommonDumpHeader(msg);

    //
    // add address of the sender of this packet to the socket map
    //

    if (ptpNetPortAddSockAddrMap(port, &(msg->_sourcePortIdentity), remote)) {

        char remoteStr[ADDRESS_STRING_LENGTH] = { 0 };
        ptpLinkLayerAddressString(remote, remoteStr, sizeof(remoteStr));

        PTP_LOG_VERBOSE("Added msg port %d, remote address: %s", ptpPortIdentityGetPortNumber(&(msg->_sourcePortIdentity)), remoteStr);
    }

    return msg;
}
