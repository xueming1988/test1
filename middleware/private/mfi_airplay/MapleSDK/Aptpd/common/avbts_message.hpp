/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef AVBTS_MESSAGE_HPP
#define AVBTS_MESSAGE_HPP

#include <list>
#include <algorithm>
#include <memory>

#include "avbts_osnet.hpp"
#include "ieee1588.hpp"

#include <stdint.h>

/** @file **/

#define PTP_CODE_STRING_LENGTH 4		/*!< PTP code string length in bytes */
#define PTP_SUBDOMAIN_NAME_LENGTH 16	/*!< PTP subdomain name lenght in bytes */
#define PTP_FLAGS_LENGTH 2				/*!< PTP flags length in bytes */

#define GPTP_VERSION 2			/*!< GPTP version */
#define PTP_NETWORK_VERSION 1	/*!< PTP Network version */

#define PTP_ETHER 1		/*!< @todo Not used */
#define PTP_DEFAULT 255	/*!< @todo Not used */

#define PTP_COMMON_HDR_OFFSET 0		/*!< PTP common header offset */
#define PTP_COMMON_HDR_LENGTH 34	/*!< PTP common header length in bytes */
#define PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(x) x	/*!< Gets the message type offset on PTP header */
#define PTP_COMMON_HDR_PTP_VERSION(x) x+1		/*!< Gets the ptp version offset on PTP header */
#define PTP_COMMON_HDR_MSG_LENGTH(x) x+2		/*!< Gets the message length offset on PTP header */
#define PTP_COMMON_HDR_DOMAIN_NUMBER(x) x+4		/*!< Gets the domain number offset on PTP header */
#define PTP_COMMON_HDR_FLAGS(x) x+6				/*!< Gets the flags offset on PTP header */
#define PTP_COMMON_HDR_CORRECTION(x) x+8		/*!< Gets the correction field offset on PTP header */
#define PTP_COMMON_HDR_SOURCE_CLOCK_ID(x) x+20	/*!< Gets the source clock id offset on PTP header */
#define PTP_COMMON_HDR_SOURCE_PORT_ID(x) x+28	/*!< Gets the source port id offset on PTP header */
#define PTP_COMMON_HDR_SEQUENCE_ID(x) x+30		/*!< Gets the sequence id offset on PTP header */
#define PTP_COMMON_HDR_CONTROL(x) x+32			/*!< Gets the control offset on PTP header */
#define PTP_COMMON_HDR_LOG_MSG_INTRVL(x) x+33	/*!< Gets the log message interval offset on PTP header */

#define PTP_ANNOUNCE_OFFSET 34							/*!< PTP announce offset */
#define PTP_ANNOUNCE_LENGTH 30							/*!< PTP announce length in bytes */
#define PTP_ANNOUNCE_CURRENT_UTC_OFFSET(x) x+10			/*!< Gets PTP announce current UTC offset */
#define PTP_ANNOUNCE_GRANDMASTER_PRIORITY1(x) x+13		/*!< Gets Grandmaster priority1 offset on announce fields */
#define PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY(x) x+14	/*!< Gets Grandmaster clock quality offset on announce fields */
#define PTP_ANNOUNCE_GRANDMASTER_PRIORITY2(x) x+18		/*!< Gets Grandmasdter priority2 offset on announce fields*/
#define PTP_ANNOUNCE_GRANDMASTER_IDENTITY(x) x+19		/*!< Gets Grandmaster identity offset on announce fields*/
#define PTP_ANNOUNCE_STEPS_REMOVED(x) x+27				/*!< Gets steps removed offset on announce fields*/
#define PTP_ANNOUNCE_TIME_SOURCE(x) x+29				/*!< Gets time source offset on announce fields*/

#define PTP_SYNC_OFFSET 34		/*!< PTP SYNC base offset */
#define PTP_SYNC_LENGTH 10		/*!< PTP SYNC length in bytes */
#define PTP_SYNC_SEC_MS(x) x	/*!< PTP SYNC seconds MSB offset */
#define PTP_SYNC_SEC_LS(x) x+2	/*!< PTP SYNC seconds LSB offset */
#define PTP_SYNC_NSEC(x) x+6	/*!< PTP SYNC nanoseconds offset */

#define PTP_FOLLOWUP_OFFSET 34		/*!< PTP FOLLOWUP base offset */
#define PTP_FOLLOWUP_LENGTH 10		/*!< PTP FOLLOWUP length in bytes */
#define PTP_FOLLOWUP_SEC_MS(x) x	/*!< Gets the followup seconds MSB offset */
#define PTP_FOLLOWUP_SEC_LS(x) x+2	/*!< Gets the followup seconds LSB offset */
#define PTP_FOLLOWUP_NSEC(x) x+6	/*!< Gets tne followup nanoseconds offset */

#define PTP_DELAY_REQ_OFFSET 34		/*!< PTP PDELAY REQUEST base offset */
#define PTP_DELAY_REQ_LENGTH 10		/*!< PTP PDELAY REQUEST length in bytes */
#define PTP_DELAY_REQ_SEC_MS(x) x		/*!< Gets the pdelay request seconds MSB offset */
#define PTP_DELAY_REQ_SEC_LS(x) x+2	/*!< Gets the pdelay request seconds LSB offset */
#define PTP_DELAY_REQ_NSEC(x) x+6		/*!< Gets the pdelay request nanoseconds offset */

#define PTP_DELAY_RESP_OFFSET 34				/*!< DELAY RESPONSE base offset */
#define PTP_DELAY_RESP_LENGTH 20				/*!< DELAY RESPONSE length in bytes */
#define PTP_DELAY_RESP_SEC_MS(x) x				/*!< Gets the delay response seconds MSB offset */
#define PTP_DELAY_RESP_SEC_LS(x) x+2			/*!< Gets the delay response seconds LSB offset */
#define PTP_DELAY_RESP_NSEC(x) x+6				/*!< Gets the delay nanoseconds offset */
#define PTP_DELAY_RESP_REQ_CLOCK_ID(x) x+10	/*!< Gets the delay response request clock id offset */
#define PTP_DELAY_RESP_REQ_PORT_ID(x) x+18		/*!< Gets the delay response request port id offset */

#define PTP_PDELAY_REQ_OFFSET 34		/*!< PTP PDELAY REQUEST base offset */
#define PTP_PDELAY_REQ_LENGTH 20		/*!< PTP PDELAY REQUEST length in bytes */
#define PTP_PDELAY_REQ_SEC_MS(x) x		/*!< Gets the pdelay request seconds MSB offset */
#define PTP_PDELAY_REQ_SEC_LS(x) x+2	/*!< Gets the pdelay request seconds LSB offset */
#define PTP_PDELAY_REQ_NSEC(x) x+6		/*!< Gets the pdelay request nanoseconds offset */

#define PTP_PDELAY_RESP_OFFSET 34				/*!< PDELAY RESPONSE base offset */
#define PTP_PDELAY_RESP_LENGTH 20				/*!< PDELAY RESPONSE length in bytes */
#define PTP_PDELAY_RESP_SEC_MS(x) x				/*!< Gets the pdelay response seconds MSB offset */
#define PTP_PDELAY_RESP_SEC_LS(x) x+2			/*!< Gets the pdelay response seconds LSB offset */
#define PTP_PDELAY_RESP_NSEC(x) x+6				/*!< Gets the pdelay nanoseconds offset */
#define PTP_PDELAY_RESP_REQ_CLOCK_ID(x) x+10	/*!< Gets the pdelay response request clock id offset */
#define PTP_PDELAY_RESP_REQ_PORT_ID(x) x+18		/*!< Gets the pdelay response request port id offset */

#define PTP_PDELAY_FOLLOWUP_OFFSET 34				/*!< PTP PDELAY FOLLOWUP base offset*/
#define PTP_PDELAY_FOLLOWUP_LENGTH 20				/*!< PTP PDELAY FOLLOWUP length in bytes */
#define PTP_PDELAY_FOLLOWUP_SEC_MS(x) x				/*!< Gets the pdelay followup seconds MSB offset*/
#define PTP_PDELAY_FOLLOWUP_SEC_LS(x) x+2			/*!< Gets the pdelay followup seconds LSB offset*/
#define PTP_PDELAY_FOLLOWUP_NSEC(x) x+6				/*!< Gets the pdelay followup nanoseconds offset*/
#define PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID(x) x+10	/*!< Gets the pdelay followup request clock id offset*/
#define PTP_PDELAY_FOLLOWUP_REQ_PORT_ID(x) x+18		/*!< Gets the pdelay followup request port id offset*/

#define PTP_SIGNALLING_OFFSET 34                        /*!< PTP signalling offset */
#define PTP_SIGNALLING_LENGTH 10                        /*!< PTP signalling length in bytes */
#define PTP_SIGNALLING_TARGET_PORT_IDENTITY(x) x        /*!< PTP signalling Tareget Port Identity */

#define PTP_LI_61_BYTE 1		/*!< PTP_LI_61(leap61) byte offset on flags field */
#define PTP_LI_61_BIT 0			/*!< PTP_LI_61(leap61) bit offset on PTP_LI_61 byte*/
#define PTP_LI_59_BYTE 1		/*!< PTP_LI_59(leap59) byte offset on flags field*/
#define PTP_LI_59_BIT 1			/*!< PTP_LI_59(leap59) bit offset on PTP_LI_59 byte*/
#define PTP_ASSIST_BYTE 0		/*!< PTP_ASSIST(two step flag) byte offset on flags field*/
#define PTP_ASSIST_BIT 1		/*!< PTP_ASSIST(two step flag) bit on PTP_ASSIST byte*/
#define PTP_PTPTIMESCALE_BYTE 1	/*!< PTPTIMESCALE byte offset on flags field*/
#define PTP_PTPTIMESCALE_BIT 3	/*!< PTPTIMESCAPE bit offset on PTPTIMESCALE byte*/

#define TX_TIMEOUT_BASE 1000 	/*!< Timeout base in microseconds */
#define TX_TIMEOUT_ITER 6		/*!< Number of timeout iteractions for sending/receiving messages*/

/**
 * @brief Enumeration message type. IEEE 1588-2008 Clause 13.3.2.2
 */
enum MessageType {
	SYNC_MESSAGE = 0,
	DELAY_REQ_MESSAGE = 1,
	PATH_DELAY_REQ_MESSAGE = 2,
	PATH_DELAY_RESP_MESSAGE = 3,
	FOLLOWUP_MESSAGE = 8,
	DELAY_RESP_MESSAGE = 9,
	PATH_DELAY_FOLLOWUP_MESSAGE = 0xA,
	ANNOUNCE_MESSAGE = 0xB,
	SIGNALLING_MESSAGE = 0xC,
	MANAGEMENT_MESSAGE = 0xD,
};

/**
 * @brief Enumeration legacy message type
 */
enum LegacyMessageType {
	SYNC,
	DELAY_REQ,
	FOLLOWUP,
	DELAY_RESP,
	MANAGEMENT,
	MESSAGE_OTHER
};

/**
 * @brief Enumeration multicast type.
 */
enum MulticastType {
	MCAST_NONE,
	MCAST_PDELAY,
	MCAST_TEST_STATUS,
	MCAST_OTHER
};

class PTPMessageId {
	MessageType _messageType;
	uint16_t _sequenceId;
public:
	PTPMessageId() { };
	PTPMessageId(MessageType messageType, uint16_t sequenceId) :
		_messageType(messageType),_sequenceId(sequenceId) { }
	PTPMessageId(const PTPMessageId& a) {
		_messageType = a._messageType;
		_sequenceId = a._sequenceId;
	}

	MessageType getMessageType(void) {
		return _messageType;
	}
	void setMessageType(MessageType messageType) {
		_messageType = messageType;
	}

	uint16_t getSequenceId(void) {
		return _sequenceId;
	}
	void setSequenceId(uint16_t sequenceId) {
		_sequenceId = sequenceId;
	}

	bool operator!=(const PTPMessageId & cmp) const {
		return
			this->_sequenceId != cmp._sequenceId ||
			this->_messageType != cmp._messageType ? true : false;
	}
	bool operator==(const PTPMessageId & cmp)const {
		return
			this->_sequenceId == cmp._sequenceId &&
			this->_messageType == cmp._messageType ? true : false;
	}
};

class PTPMessageFollowUp;

/**
 * @brief Provides the PTPMessage common interface used during building of
 * PTP messages.
 */
class PTPMessageCommon {
protected:
	unsigned char versionPTP;	/*!< PTP version */
	uint16_t versionNetwork;	/*!< Network version */
	MessageType messageType;	/*!< MessageType to be built */

	std::shared_ptr<PortIdentity> sourcePortIdentity;	/*!< PortIdentity from source*/

	uint16_t sequenceId;		/*!< PTP message sequence ID*/
	LegacyMessageType control;	/*!< Control message type of LegacyMessageType */
	unsigned char flags[2];		/*!< PTP flags field */

	uint16_t messageLength;			/*!< PTP message length */
	char logMeanMessageInterval;	/*!< LogMessageInterval (IEEE 1588-2008 table 24)*/
	long long correctionField;		/*!< Correction Field (IEEE 1588-2008 table 21) */
	unsigned char domainNumber;		/*!< PTP domain number */

	Timestamp _timestamp;	/*!< PTP message timestamp */
	unsigned _timestamp_counter_value;	/*!< PTP timestamp counter value */

private:
	static int sMaxBadDiffCount;
	static int sSuccessDiffCount;
	static const int kMaxSuccessCount = 4;
	static const int kMaxBadDiffCount = 10;

protected:
	/**
	 * @brief Default constructor
	 */
	PTPMessageCommon() : 
	 sourcePortIdentity(nullptr), sequenceId(0), correctionField(0)
	{

	}

 public:
 	PTPMessageCommon(const PTPMessageCommon& other)
 	{
 		assign(other);
 	}
	/**
	 * @brief Creates the PTPMessageCommon interface
	 * @param port CommonPort where the message interface is
	 * connected to.
	 */
	PTPMessageCommon(CommonPort *port);
	/**
	 * @brief Destroys PTPMessageCommon interface
	 */
	virtual ~PTPMessageCommon(void);

	PTPMessageCommon& operator=(const PTPMessageCommon& other)
	{
		return assign(other);
	}

	bool operator ==(const PTPMessageCommon& other);

	void MaybePerformCalculations(EtherPort* port);

	PTPMessageCommon& assign(const PTPMessageCommon& other);

	/**
	 * @brief  Gets a pointer to the flags field within the PTP message.
	 * @return Pointer to the flags field
	 */
	unsigned char *getFlags(void) {
		return flags;
	}

	/**
	 * @brief  Gets the sequenceId value within a ptp message
	 * @return Sequence ID value
	 */
	uint16_t getSequenceId(void) {
		return sequenceId;
	}
	/**
	 * @brief  Sets the sequence ID value to the PTP message.
	 * @param  seq Sequence id value to be set.
	 * @return void
	 */
	void setSequenceId(uint16_t seq) {
		sequenceId = seq;
	}

	/**
	 * @brief  Gets the MessageType field within the PTP message.
	 * @return MessageType
	 */
	MessageType getMessageType(void) {
		return messageType;
	}

	/**
	 * @brief Check if message type is event
	 * @return true if an event message
	 */
	bool isEvent( void )
	{
		return (( messageType >> 3) & 0x1 ) == 0;
	}

	/**
	 * @brief Get TX timestamp
	 * @param port used to send message
	 * @param link_speed link speed of message
	 */
	bool getTxTimestamp( EtherPort *port, uint32_t link_speed );

	/**
	 * @brief  Gets the MessageID of the PTP message.
	 * @return MessageId
	 */
	const PTPMessageId getMessageId(void) const {
		return PTPMessageId(messageType, sequenceId);
	}
	/**
	 * @brief  Gets the correctionField value in a Little-Endian format.
	 * @return correctionField
	 * @todo Little endian format could be removed by adding endianess discovery on
	 * compile/run time.
	 */
	long long getCorrectionField(void) {
		return correctionField;
	}
	/**
	 * @brief  Sets the correction field. It expects the host format.
	 * @param  correctionAmount
	 * @return void
	 * @todo Little endian format could be removed by adding endianess discovery on
	 * compile/run time.
	 */
	void setCorrectionField(long long correctionAmount) {
		correctionField = correctionAmount;
	}

	/**
	 * @brief  Gets PortIdentity field
	 * @param  identity [out] Source port identity
	 * @return void
	 */
	void getPortIdentity(std::shared_ptr<PortIdentity> identity);

	std::shared_ptr<PortIdentity> getPortIdentity() const;

	/**
	 * @brief  Sets PortIdentity value
	 * @param  identity [in] Source port identity value to be set.
	 * @return void
	 */
	void setPortIdentity(std::shared_ptr<PortIdentity> identity);

	/**
	 * @brief  Gets the current Timestamp value from the PTP message
	 * @return Current Timestamp value
	 */
	Timestamp getTimestamp(void) {
		return _timestamp;
	}
	/**
	 * @brief  Gets the timestamp counter value set during the RX timestamp method.
	 * @return timestamp counter value
	 */
	uint32_t getTimestampCounterValue(void) {
		return _timestamp_counter_value;;
	}
	/**
	 * @brief  Sets the timestamp value
	 * @param  timestamp [in] Reference to Timestamp value
	 * @return void
	 */
	void setTimestamp(const Timestamp & timestamp) {
		_timestamp = timestamp;
	}

	/**
	 * @brief  Determine whether the message was sent by given communication technology, uuid, and
	 * port id fields
	 * @param  portIdentity PortIdentity value
	 * @return TRUE if sender equals to internal PTPCommon values, FALSE otherwise.
	 */
	bool isSenderEqual(const PortIdentity& portIdentity);

	/**
	 * @brief  Generic interface for processing PTP message
	 * @param  port IEEE1588 port
	 * @return void
	 */
	virtual void processMessage(EtherPort *port);

	/**
	 * @brief  Builds PTP common header
	 * @param  buf [out] PTP message
	 * @return void
	 */
	void buildCommonHeader(uint8_t * buf);

	MessageType GetMessageType(const char *buf)
	{
		unsigned char rawMessage;
		memcpy(&rawMessage,
		 buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       sizeof(rawMessage));
		MessageType messageType = MessageType(rawMessage & 0xF);
		return messageType;
	}

	void logCommonHeader();

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/*Exact fit. No padding*/
#pragma pack(push,1)

#define PATH_TRACE_TLV_TYPE 0x8		/*!< This is the value that indicates the
									  TLV is a path trace TLV, as specified in
									  16.2.7.1 and Table 34 of IEEE Std
									  1588-2008. The value is specified there
									  as PATH_TRACE, whose value is 0x8. */

/**
 * @brief Provides the PathTraceTLV interface
 * The fields of the path TLV shall be as specified in Table 10-8 and in
 * 10.5.4.3.2 through 10.5.4.3.9 from IEEE 802.1AS. This TLV,
 * and its use, are defined in IEEE Std 1588-2008 (see 16.2 and Table 34 of IEEE Std 1588-2008).
 */
class PathTraceTLV {
 private:
	uint16_t tlvType;
	typedef std::list<ClockIdentity> IdentityList;
	IdentityList identityList;
 public:
	/**
	 * @brief Creates the PathTraceTLV interface.
	 * Sets tlvType to PATH_TRACE_TLV_TYPE using network byte order
	 */
	PathTraceTLV() {
		tlvType = PLAT_htons(PATH_TRACE_TLV_TYPE);
	}
	/**
	 * @brief  Parses ClockIdentity from message buffer
	 * @param  buffer [in] Message buffer. It should be at least ::PTP_CLOCK_IDENTITY_LENGTH bytes long.
	 * @param  size [in] Buffer size. Should be the length of the data pointed to by the buffer argument.
	 * @return void
	 */
	void parseClockIdentity(uint8_t *buffer, int size) {
		int length = PLAT_ntohs(*(uint16_t*)buffer);

		buffer += sizeof(uint16_t);
		size -= sizeof(uint16_t);

		if((unsigned)size < (unsigned)length) {
			length = size;
		}
		length /= PTP_CLOCK_IDENTITY_LENGTH;

		for(; length > 0; --length) {
			ClockIdentity add;
			add.set(buffer);
			identityList.push_back(add);
			buffer += PTP_CLOCK_IDENTITY_LENGTH;
		}
	}

	bool operator ==(const PathTraceTLV& other)
	{
		return tlvType == other.tlvType && identityList == other.identityList;
	}

	/**
	 * @brief  Appends new ClockIdentity to internal ClockIdentity list
	 * @param  id ClockIdentity to be appended
	 * @return void
	 */
	void appendClockIdentity(const ClockIdentity& id) {
		identityList.push_back(id);
	}

	/**
	 * @brief  Gets TLV value in a byte string format
	 * @param  byte_str [out] Output byte string
	 * @return void
	 */
	void toByteString(uint8_t * byte_str) {
		IdentityList::iterator iter;
		*((uint16_t *)byte_str) = tlvType;  // tlvType already in network byte order
		byte_str += sizeof(tlvType);
		*((uint16_t *)byte_str) = PLAT_htons
			((uint16_t)identityList.size()*PTP_CLOCK_IDENTITY_LENGTH);
		byte_str += sizeof(uint16_t);
		for
			(iter = identityList.begin();
			 iter != identityList.end(); ++iter) {
			iter->getIdentityString(byte_str);
			byte_str += PTP_CLOCK_IDENTITY_LENGTH;
		}
	}

	/**
	 * @brief  Looks for a specific ClockIdentity on the current TLV
	 * @param  id [in] Desired ClockIdentity
	 * @return TRUE if it has found it, FALSE otherwise.
	 */
	bool has(ClockIdentity *id) {
		return std::find
			(identityList.begin(), identityList.end(), *id) !=
			identityList.end();
	}

	/**
	 * @brief  Gets the total length of TLV.
	 * Total length of TLV is length of type field (UINT16) + length of 'length'
	 * field (UINT16) + length of
	 * identities (each PTP_CLOCK_IDENTITY_LENGTH) in the path
	 * @return Total length
	 */
	int length() {
		return (int)(2*sizeof(uint16_t) + PTP_CLOCK_IDENTITY_LENGTH*identityList.size());
	}

	const std::string GetIdentities()
	{
		std::string ids;
		std::string value;
		std::for_each(identityList.begin(), identityList.end(), [&](const ClockIdentity &id)
			{
				ids += ids.empty() ? id.getString() : ", " + id.getString();
			});
		return ids;
	}

	uint16_t TlvType() const
	{
		return tlvType;
	}
};

/* back to whatever the previous packing mode was */
#pragma pack(pop)

/**
 * @brief Provides the PTPMessageAnnounce interface
 * The PTPMessageAnnounce class is used to create
 * announce messages on the 802.1AS format when building
 * the ptp messages.
 */
class PTPMessageAnnounce:public PTPMessageCommon {
 private:
	uint8_t grandmasterIdentity[PTP_CLOCK_IDENTITY_LENGTH];
	ClockQuality grandmasterClockQuality;

	PathTraceTLV tlv;

	uint16_t currentUtcOffset;
	unsigned char grandmasterPriority1;
	unsigned char grandmasterPriority2;
	ClockQuality clockQuality;
	uint16_t stepsRemoved;
	unsigned char timeSource;

public:
	 PTPMessageAnnounce();
 
	 /**
	  * @brief Creates the PTPMessageAnnounce interface
	  */
	 PTPMessageAnnounce(CommonPort * port);

	 /**
	  * @brief Destroys the PTPMessageAnnounce interface
	  */
	~PTPMessageAnnounce();

	void VerboseLog();

	bool operator ==(const PTPMessageAnnounce& other)
	{
		return 0 == memcmp(grandmasterIdentity, other.grandmasterIdentity, PTP_CLOCK_IDENTITY_LENGTH) &&
			grandmasterClockQuality == other.grandmasterClockQuality &&
			tlv == other.tlv &&
			currentUtcOffset == other.currentUtcOffset &&
			grandmasterPriority1 == other.grandmasterPriority1 &&
			grandmasterPriority2 == other.grandmasterPriority2 &&
			clockQuality == other.clockQuality &&
			stepsRemoved == other.stepsRemoved &&
			timeSource == other.timeSource;
	}

	void clear()
	{
		memset(grandmasterIdentity, 0, PTP_CLOCK_IDENTITY_LENGTH);
		tlv = PathTraceTLV();

		currentUtcOffset = 0;
		grandmasterPriority1 = 0;
		grandmasterPriority2 = 0;
		stepsRemoved = 0;
		timeSource = 0;
	}

	/**
	 * @brief  Compare gramdmaster's capabilities comming on the
	 * announce messages against the current grandmaster capabilities.
	 * @param  msg [in] PTPMessageAnnounce to be compared
	 * @return TRUE if it is better. FALSE otherwise.
	 */
	bool isBetterThan(std::shared_ptr<PTPMessageAnnounce> msg);

	bool isBetterThan(const PTPMessageAnnounce& msg);

	/**
	 * @brief  Gets grandmaster's priority1 value
	 * @return Grandmaster priority1
	 */
	unsigned char getGrandmasterPriority1() const {
		return grandmasterPriority1;
	}

	/**
	 * @brief  Gets grandmaster's priority2 value
	 * @return Grandmaster priority2
	 */
	unsigned char getGrandmasterPriority2() const {
		return grandmasterPriority2;
	}

	/**
	 * @brief  Gets grandmaster clock quality
	 * @return Pointer to a ClockQuality object.
	 */
	const ClockQuality& getGrandmasterClockQuality() const {
		return grandmasterClockQuality;
	}

	/**
	 * @brief  Gets the steps removed value. See IEEE 802.1AS-2011 Clause 10.3.3
	 * @return steps removed value
	 */
	uint16_t getStepsRemoved() {
		return stepsRemoved;
	}

	/**
	 * @brief Gets grandmaster identity value
	 * @param  identity [out] Grandmaster identity
	 * @return void
	 */
	void getGrandmasterIdentity(char *identity) const {
		memcpy(identity, grandmasterIdentity, PTP_CLOCK_IDENTITY_LENGTH);
	}

	/**
	 * @brief  Gets grandmaster's clockIdentity value
	 * @return Grandmaster ClockIdentity
	 */
	ClockIdentity getGrandmasterClockIdentity() {
		ClockIdentity ret;
		ret.set( grandmasterIdentity );
		return ret;
	}

	/**
	 * @brief  Processes PTP message
	 * @param  port EtherPort
	 * @return void
	 */
	void processMessage(EtherPort *port);

	/**
	 * @brief  Assembles PTPMessageAnnounce message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/**
 * @brief Provides a class for building the PTP Sync message
 */
class PTPMessageSync : public PTPMessageCommon {
 private:
	Timestamp originTimestamp;

 public:
	PTPMessageSync();

	/**
	 * @brief Default constructor. Creates PTPMessageSync
	 * @param port EtherPort
	 */
	PTPMessageSync(EtherPort *port);

	/**
	 * @brief Destroys PTPMessageSync interface
	 */
	~PTPMessageSync();

	PTPMessageSync(const PTPMessageSync& other)
	{
		assign(other);
	}

	PTPMessageSync& operator=(const PTPMessageSync& other)
	{
		return assign(other);
	}

	PTPMessageSync& assign(const PTPMessageSync& other);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage(EtherPort *port);

	/**
	 * @brief  Gets origin timestamp value
	 * @return Origin Timestamp
	 */
	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}

	/**
	 * @brief  Assembles PTPMessageSync message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/* Exact fit. No padding*/
#pragma pack(push,1)

/**
 * @brief Provides a scaledNs interface
 * The scaledNs type represents signed values of time and time interval in units of 2e-16 ns.
 */
class scaledNs {
 private:
	int32_t ms;
	uint64_t ls;
 public:
	/**
	 * @brief Builds scaledNs interface
	 */
	scaledNs() {
		ms = 0;
		ls = 0;
	}

	/**
	 * @brief  Gets scaledNs in a byte string format
	 * @param  byte_str [out] scaledNs value
	 * @return void
	 */
	void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}

	/**
	 * @brief  Overloads the operator = for this class
	 * @param  other Value to be attributed to this object's instance.
	 * @return Reference to scaledNs object
	 */
	scaledNs& operator=(const scaledNs& other)
	{
		if (this != &other)
		{
			ms = other.ms;
			ls = other.ls;
		}
		return *this;
	}

	/**
	 * @brief  Set the lowest 64bits from the scaledNs object
	 * @param  lsb Value to be set
	 * @return void
	 */
	void setLSB(uint64_t lsb)
	{
		this->ls = lsb;
	}

	/**
	 * @brief  Set the highest 32bits of the scaledNs object
	 * @param  msb 32-bit signed integer to be set
	 * @return void
	 */
	void setMSB(int32_t msb)
	{
		this->ms = msb;
	}
};

/**
 * @brief Provides a follow-up TLV interface back to the previous packing mode
 */
class FollowUpTLV {
 private:
	uint16_t tlvType;
	uint16_t lengthField;
	uint8_t organizationId[3];
	uint8_t organizationSubType_ms;
	uint16_t organizationSubType_ls;
	int32_t cumulativeScaledRateOffset;
	uint16_t gmTimeBaseIndicator;
	scaledNs scaledLastGmPhaseChange;
	int32_t scaledLastGmFreqChange;
 public:
	/**
	 * @brief Builds the FollowUpTLV interface
	 */
	FollowUpTLV() {
		tlvType = PLAT_htons(0x3);
		lengthField = PLAT_htons(28);
		organizationId[0] = '\x00';
		organizationId[1] = '\x80';
		organizationId[2] = '\xC2';
		organizationSubType_ms = 0;
		organizationSubType_ls = PLAT_htons(1);
		cumulativeScaledRateOffset = PLAT_htonl(0);
		gmTimeBaseIndicator = 0;
		scaledLastGmFreqChange = PLAT_htonl(0);
	}

	/**
	 * @brief  Gets FollowUpTLV information in a byte string format
	 * @param  byte_str [out] FollowUpTLV values
	 */
	void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}

	/**
	 * @brief  Gets the cummulative scaledRateOffset
	 * @return 32 bit signed value with the rate offset information.
	 */
	int32_t getRateOffset() {
		return cumulativeScaledRateOffset;
	}

	/**
	 * @brief Gets the gmTimeBaseIndicator
	 * @return 16 bit unsigned value of the gmTimeBaseIndicator
	 *         information
	 */
	uint16_t getGmTimeBaseIndicator() {
		return gmTimeBaseIndicator;
	}

	/**
	 * @brief  Updates the scaledLastGmFreqChanged private member
	 * @param  val Value to be set
	 * @return void
	 */
	void setScaledLastGmFreqChange(int32_t val)
	{
		scaledLastGmFreqChange = PLAT_htonl(val);
	}

	/**
	 * @brief  Gets the current scaledLastGmFreqChanged value
	 * @return scaledLastGmFreqChange
	 */
	int32_t getScaledLastGmFreqChange(void)
	{
		return scaledLastGmFreqChange;
	}

	/**
	 * @brief  Sets the gmTimeBaseIndicator private member
	 * @param  tbi Value to be set
	 * @return void
	 */
	void setGMTimeBaseIndicator(uint16_t tbi)
	{
		gmTimeBaseIndicator = tbi;
	}

	/**
	 * @brief  Incremets the Time Base Indicator member
	 * @return void
	 */
	void incrementGMTimeBaseIndicator(void)
	{
		++gmTimeBaseIndicator;
	}

	/**
	 * @brief  Gets the current gmTimeBaseIndicator value
	 * @return gmTimeBaseIndicator
	 */
	uint16_t getGMTimeBaseIndicator(void)
	{
		return gmTimeBaseIndicator;
	}

	/**
	 * @brief  Sets the scaledLastGmPhaseChange private member
	 * @param  pc Value to be set
	 * @return void
	 */
	void setScaledLastGmPhaseChange(scaledNs pc)
	{
		scaledLastGmPhaseChange = pc;
	}

	/**
	 * @brief  Gets the scaledLastGmPhaseChange private member value
	 * @return scaledLastGmPhaseChange value
	 */
	scaledNs getScaledLastGmPhaseChange(void)
	{
		return scaledLastGmPhaseChange;
	}
};

/* back to whatever the previous packing mode was */
#pragma pack(pop)

/**
 * @brief Provides a class for a class for building a PTP follow up message
 */
class PTPMessageFollowUp:public PTPMessageCommon {
private:
	Timestamp preciseOriginTimestamp;

	FollowUpTLV tlv;

public:
	PTPMessageFollowUp()
	{ 
		messageType = FOLLOWUP_MESSAGE;
	}
	
	/**
	 * @brief Builds the PTPMessageFollowUP object
	 */
	PTPMessageFollowUp(EtherPort * port);

	PTPMessageFollowUp(const PTPMessageFollowUp& other)
	{
		assign(other);
	}

	PTPMessageFollowUp& operator=(const PTPMessageFollowUp& other)
	{
		return assign(other);
	}

	PTPMessageFollowUp& assign(const PTPMessageFollowUp& other);

	/**
	 * @brief  Assembles PTPMessageFollowUp message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Gets the precise origin timestamp value
	 * @return preciseOriginTimestamp value
	 */
	Timestamp getPreciseOriginTimestamp(void) {
		return preciseOriginTimestamp;
	}

	void MaybePerformCalculations(EtherPort *port, bool frequencyComputeOk);

	bool ComputeFrequencies(EtherPort * port, bool lockSync);
	bool ComputeFrequencies(EtherPort * port);


	/**
	 * @brief  Sets the precis origin timestamp value
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setPreciseOriginTimestamp(Timestamp & timestamp) {
		preciseOriginTimestamp = timestamp;
	}

	/**
	 * @brief  Sets the clock source time interface (802.1AS 9.2)
	 * @param  fup Follow up message
	 * @return void
	 */
	void setClockSourceTime(FollowUpTLV *fup)
	{
		tlv.setGMTimeBaseIndicator(fup->getGMTimeBaseIndicator());
		tlv.setScaledLastGmFreqChange(fup->getScaledLastGmFreqChange());
		tlv.setScaledLastGmPhaseChange(fup->getScaledLastGmPhaseChange());
	}

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/**
 * @brief Provides a class for building the PTP Delay Request message
 */
class PTPMessageDelayReq : public PTPMessageCommon {
 private:
	Timestamp originTimestamp;


 public:
	/**
	 * @brief Destroys the PTPMessageDelayReq object
	 */
	~PTPMessageDelayReq()
	{
	}

	PTPMessageDelayReq()
	{
		messageType = DELAY_REQ_MESSAGE;
	}

	/**
	 * @brief Builds the PTPMessageDelayReq message
	 */
	PTPMessageDelayReq(EtherPort * port);

	PTPMessageDelayReq(const PTPMessagePathDelayReq& other)
	{
		assign(other);
	}
	PTPMessageDelayReq& operator=(const PTPMessageDelayReq& other)
	{
		return assign(other);
	}
	PTPMessageDelayReq& assign(const PTPMessageDelayReq& other);
	
	/**
	 * @brief  Assembles PTPMessageDelayReq message on the IEEE1588Port payload
	 * @param  port etherPort where the message will be assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return bool
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] etherPort
	 * @return void
	 */
	void processMessage(EtherPort * port);

#ifdef APTP
	/**
	 * @brief  Build the common header with tweaks for the Apple Vendor PTP Profile
	 * @param  buf [out] uint8_t
	 * @return void
	 */
	void buildCommonHeader(uint8_t * buf);
#endif

	/**
	 * @brief  Gets origin timestamp value
	 * @return Origin Timestamp
	 */
	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}

	void setOriginTimestamp(const Timestamp& timestamp)
	{
		originTimestamp = timestamp;
	}

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/**
 * @brief Provides a class for building the PTP Delay Response message.
 */
class PTPMessageDelayResp:public PTPMessageCommon {
private:
	std::shared_ptr<PortIdentity> requestingPortIdentity;
	Timestamp requestReceiptTimestamp;
	Timestamp fEgressTime;

public:
	PTPMessageDelayResp() :
		requestingPortIdentity(nullptr)
	{
		messageType = DELAY_RESP_MESSAGE;
	}

	PTPMessageDelayResp(const PTPMessageDelayResp& other)
	{
		assign(other);
	}

	/**
	 * @brief Destroys the PTPMessageDelayResp object
	 */
	~PTPMessageDelayResp();

	/**
	 * @brief Builds the PTPMessageDelayResp object
	 */
	PTPMessageDelayResp(EtherPort * port);

	PTPMessageDelayResp& operator=(const PTPMessageDelayResp& other)
	{
		assign(other);
		return *this;
	}


	PTPMessageDelayResp& assign(const PTPMessageDelayResp& other);

	/**
	 * @brief  Assembles PTPMessageDelayResp message on the IEEE1588Port payload
	 * @param  port EtherPort where the message will be assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return bool
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage(EtherPort * port);

	/**
	 * @brief  Sets the request receipt timestamp
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setRequestReceiptTimestamp(Timestamp timestamp) {
		requestReceiptTimestamp = timestamp;
	}
	/**
	 * @brief  Gets the request receipt timestamp
	 * @return requestReceiptTimestamp
	 */
	Timestamp getRequestReceiptTimestamp(void) {
		return requestReceiptTimestamp;
	}

	/**
	 * @brief  Sets requesting port identity
	 * @param  identity [in] PortIdentity to be set
	 * @return void
	 */
	void RequestingPortIdentity(std::shared_ptr<PortIdentity> identity)
	{
		requestingPortIdentity = identity;
	}

	/**
	 * @brief  Gets requesting port identity
	 * @param  identity [out] Requested PortIdentity
	 * @return void
	 */
	std::shared_ptr<PortIdentity> RequestingPortIdentity() const
	{
		return requestingPortIdentity;
	}

#ifdef APTP
	/**
	 * @brief  Overload of base buildCommonHeader member function
	 * @param  buf [out] Buffer holding the header values.
	 * @return void
	 */
	void buildCommonHeader(uint8_t * buf);
#endif
	
	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/**
 * @brief Provides a class for building the PTP Path Delay Request message
 */
class PTPMessagePathDelayReq : public PTPMessageCommon {
 private:
	Timestamp originTimestamp;

	PTPMessagePathDelayReq() {
		return;
	}
 public:
	/**
	 * @brief Destroys the PTPMessagePathDelayReq object
	 */
	~PTPMessagePathDelayReq() {
	}

	/**
	 * @brief Builds the PTPMessagePathDelayReq message
	 */
	PTPMessagePathDelayReq(EtherPort *port);

	PTPMessagePathDelayReq& operator=(const PTPMessagePathDelayReq& other)
	{
		if (this != &other)
		{
			PTPMessageCommon::assign(other);
			originTimestamp = other.originTimestamp;
		}
		return *this;
	}
	bool operator==(const PTPMessagePathDelayReq& other)
	{
		return originTimestamp == other.originTimestamp &&
		  PTPMessageCommon::operator ==(static_cast<PTPMessageCommon>(other));
	}
	bool operator!=(const PTPMessagePathDelayReq& other)
	{
		return !(*this == other);
	}

	/**
	 * @brief  Assembles PTPMessagePathDelayReq message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage(EtherPort *port);

	/**
	 * @brief  Gets origin timestamp value
	 * @return Origin Timestamp
	 */
	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/**
 * @brief Provides a class for building the PTP Path Delay Response message.
 */
class PTPMessagePathDelayResp:public PTPMessageCommon {
private:
	std::shared_ptr<PortIdentity> requestingPortIdentity;
	Timestamp requestReceiptTimestamp;

	PTPMessagePathDelayResp(void) {
	}
public:
	/**
	 * @brief Destroys the PTPMessagePathDelayResp object
	 */
	~PTPMessagePathDelayResp();
	/**
	 * @brief Builds the PTPMessagePathDelayResp object
	 */
	PTPMessagePathDelayResp( EtherPort *port );

	PTPMessagePathDelayResp& operator=(const PTPMessagePathDelayResp& other)
	{
		if (this != &other)
		{
			PTPMessageCommon::assign(other);
			requestingPortIdentity = other.requestingPortIdentity;
			requestReceiptTimestamp = other.requestReceiptTimestamp;
		}
		return *this;
	}

	bool operator==(const PTPMessagePathDelayResp& other)
	{
		return requestingPortIdentity == other.requestingPortIdentity &&
		  requestReceiptTimestamp == other.requestReceiptTimestamp &&
		  PTPMessageCommon::operator ==(static_cast<PTPMessageCommon>(other));
	}
	bool operator!=(const PTPMessagePathDelayResp& other)
	{
		return !(*this == other);
	}
	/**
	 * @brief  Assembles PTPMessagePathDelayResp message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage( EtherPort *port );

	/**
	 * @brief  Sets the request receipt timestamp
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setRequestReceiptTimestamp(Timestamp timestamp) {
		requestReceiptTimestamp = timestamp;
	}

	/**
	 * @brief  Sets requesting port identity
	 * @param  identity [in] PortIdentity to be set
	 * @return void
	 */
	void setRequestingPortIdentity(std::shared_ptr<PortIdentity> identity);
	/**
	 * @brief  Gets requesting port identity
	 * @param  identity [out] Requested PortIdentity
	 * @return void
	 */
	void getRequestingPortIdentity(std::shared_ptr<PortIdentity> identity);

	/**
	 * @brief  Gets the request receipt timestamp
	 * @return requestReceiptTimestamp
	 */
	Timestamp getRequestReceiptTimestamp(void) {
		return requestReceiptTimestamp;
	}

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/**
 * @brief Provides a class for building the PTP Path Delay Response follow up message.
 */
class PTPMessagePathDelayRespFollowUp:public PTPMessageCommon {
 private:
	Timestamp responseOriginTimestamp;
	std::shared_ptr<PortIdentity> requestingPortIdentity;

	PTPMessagePathDelayRespFollowUp(void) { }

public:
	/**
	 * @brief Builds the PTPMessagePathDelayRespFollowUp object
	 */
	PTPMessagePathDelayRespFollowUp(EtherPort *port);

	/**
	 * @brief Destroys the PTPMessagePathDelayRespFollowUp object
	 */
	~PTPMessagePathDelayRespFollowUp();

	bool operator == (const PTPMessagePathDelayRespFollowUp& other);

	bool operator != (const PTPMessagePathDelayRespFollowUp& other);

	/**
	 * @brief  Assembles PTPMessageRespFollowUp message on the
	 * EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage(EtherPort *port);

	/**
	 * @brief  Sets the response origin timestamp
	 * @param  timestamp Timestamp to be set
	 * @return void
	 */
	void setResponseOriginTimestamp(Timestamp timestamp) {
		responseOriginTimestamp = timestamp;
	}
	/**
	 * @brief  Sets the requesting port identity
	 * @param  identity [in] PortIdentity to be set
	 * @return void
	 */
	void setRequestingPortIdentity(std::shared_ptr<PortIdentity> identity);

	/**
	 * @brief  Gets the response origin timestamp
	 * @return responseOriginTimestamp
	 */
	Timestamp getResponseOriginTimestamp(void) {
		return responseOriginTimestamp;
	}
	/**
	 * @brief Gets the requesting port identity
	 * @return Pointer to requesting PortIdentity object
	 */
	std::shared_ptr<PortIdentity> getRequestingPortIdentity(void) {
		return requestingPortIdentity;
	}

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

/*Exact fit. No padding*/
#pragma pack(push,1)


/**
 * @brief Provides a Signalling Msg Interval Request TLV interface back to the previous
 * packing mode
 */
class SignallingTLV {
 private:
	uint16_t tlvType;
	uint16_t lengthField;
	uint8_t organizationId[3];
	uint8_t organizationSubType_ms;
	uint16_t organizationSubType_ls;
	uint8_t linkDelayInterval;
	uint8_t timeSyncInterval;
	uint8_t announceInterval;
	uint8_t flags;
	uint16_t reserved;
 public:
	/**
	 * @brief Builds the Signalling Msg Interval Request TLV interface
	 */
	SignallingTLV() {
		tlvType = PLAT_htons(0x3);
		lengthField = PLAT_htons(12);
		organizationId[0] = '\x00';
		organizationId[1] = '\x80';
		organizationId[2] = '\xC2';
		organizationSubType_ms = 0;
		organizationSubType_ls = PLAT_htons(2);
		linkDelayInterval = 0;
		timeSyncInterval = 0;
		announceInterval = 0;
		flags = 3;
		reserved = PLAT_htons(0);
	}

	/**
	 * @brief  Gets Msg Interval Request TLV information in a byte
	 *         string format
	 * @param  byte_str [out] Msg Interval Request TLV values
	 */
	void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}

	/**
	 * @brief  Gets the link delay interval.
	 * @return 8 bit signed value of the link delay interval.
	 */
	int8_t getLinkDelayInterval() {
		return linkDelayInterval;
	}

	/**
	 * @brief  Sets the link delay interval.
	 * @param 8 bit signed value of the link delay interval.
	 * @return void
	 */
	void setLinkDelayInterval(int8_t linkDelayInterval) {
		this->linkDelayInterval = linkDelayInterval;
	}

	/**
	 * @brief  Gets the time sync interval.
	 * @return 8 bit signed value of the time sync interval.
	 */
	int8_t getTimeSyncInterval() {
		return timeSyncInterval;
	}

	/**
	 * @brief  Sets the time sync interval.
	 * #param  8 bit signed value of the time sync interval.
	 * @return void
	 */
	void setTimeSyncInterval(int8_t timeSyncInterval) {
		this->timeSyncInterval = timeSyncInterval;
	}

	/**
	 * @brief  Gets the announce interval.
	 * @return 8 bit signed value of the announce interval.
	 */
	int8_t getAnnounceInterval() {
		return announceInterval;
	}

	/**
	 * @brief  Sets the announce interval.
	 * @param  8 bit signed value of the announce interval.
	 * @return void
	 */
	void setAnnounceInterval(int8_t announceInterval) {
		this->announceInterval = announceInterval;
	}
};

/* back to whatever the previous packing mode was */
#pragma pack(pop)

/**
 * @brief Provides a class for building a PTP signalling message
 */
class PTPMessageSignalling:public PTPMessageCommon {
private:
	int8_t targetPortIdentify;
	SignallingTLV tlv;

public:
	PTPMessageSignalling(void);

	static const int8_t sigMsgInterval_Initial =  126;
	static const int8_t sigMsgInterval_NoSend =  127;
	static const int8_t sigMsgInterval_NoChange =  -128;

	/**
	 * @brief Builds the PTPMessageSignalling object
	 */
	PTPMessageSignalling(EtherPort *port);

	/**
	 * @brief Destroys the PTPMessageSignalling object
	 */
	~PTPMessageSignalling();

	/**
	 * @brief Sets the signalling intervals
	 * @param  linkDelayInterval link delay interval
	 * @param  timeSyncInterval Sync interval
	 * @param  announceInterval Announce interval
	 * @return void
	 */
	void setintervals(int8_t linkDelayInterval, int8_t timeSyncInterval, int8_t announceInterval);

	/**
	 * @brief  Assembles PTPMessageSignalling message on the
	 *         EtherPort payload
	 * @param  port EtherPort where the message will be
	 * assembled
	 * @param  destIdentity [in] Destination PortIdentity
	 * @return true on success
	 */
	bool sendPort(EtherPort *port, std::shared_ptr<PortIdentity> destIdentity);

	/**
	 * @brief  Processes PTP messages
	 * @param  port [in] EtherPort
	 * @return void
	 */
	void processMessage(EtherPort *port);

	friend std::shared_ptr<PTPMessageCommon> buildPTPMessage(char *buf, size_t size,
	 LinkLayerAddress *remote, EtherPort *port, const Timestamp& ingressTime);
};

#endif
