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

#ifndef ETHER_PORT_HPP
#define ETHER_PORT_HPP

#include <stdint.h>
#include <map>
#include <list>

#include "ieee1588.hpp"
#include "avbap_message.hpp"

#include "avbts_ostimer.hpp"
#include "avbts_oslock.hpp"
#include "avbts_osnet.hpp"
#include "avbts_osthread.hpp"
#include "avbts_oscondition.hpp"
#include "ipcdef.hpp"
#include "gptp_log.hpp"


#include "common_port.hpp"

/**@file*/

#define GPTP_MULTICAST 0x0180C200000EULL		/*!< GPTP multicast adddress */
#define PDELAY_MULTICAST GPTP_MULTICAST			/*!< PDELAY Multicast value */
#define OTHER_MULTICAST GPTP_MULTICAST			/*!< OTHER multicast value */
#define TEST_STATUS_MULTICAST 0x011BC50AC000ULL	/*!< AVnu Automotive profile test status msg Multicast value */

#define PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER 3	/*!< PDelay timeout multiplier*/
#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 3			/*!< Sync receipt timeout multiplier*/
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 3		/*!< Announce receipt timeout multiplier*/

#define LOG2_INTERVAL_INVALID -127	/* Simple out of range Log base 2 value used for Sync and PDelay msg internvals */


/**
 * @brief Provides a map for the identityMap member of EtherPort
 * class
 */
typedef std::map <PortIdentity, LinkLayerAddress > IdentityMap_t;


/**
 * @brief Ethernet specific port functions
 */
class EtherPort : public CommonPort
{
	static LinkLayerAddress other_multicast;
	static LinkLayerAddress pdelay_multicast;
	static LinkLayerAddress test_status_multicast;

	/* Port Status */
	// set to 0 when asCapable is false, increment for each pdelay recvd
	bool linkUp;

	/* Port Configuration */
	signed char log_mean_unicast_sync_interval;
	signed char log_min_mean_delay_req_interval;
	signed char log_min_mean_pdelay_req_interval;

	unsigned int duplicate_resp_counter;
	uint16_t last_invalid_seqid;

	/* Automotive Profile : Static variables */
	// port_state : already defined as port_state
	bool isGM;
	// asCapable : already defined as asCapable
	signed char operLogPdelayReqInterval;
	signed char operLogSyncInterval;
	signed char initialLogPdelayReqInterval;
	bool automotive_profile;

	// Test Status variables
	uint32_t linkUpCount;
	uint32_t linkDownCount;
	StationState_t stationState;


	/* Automotive Profile : Persistant variables */
	// neighborPropDelay : defined as one_way_delay ??
	// rateRatio : Optional and didn't fine this variable. Will proceed without writing it.
	// neighborRateRatio : defined as _peer_rate_offset ??

	// Automotive Profile AVB SYNC state indicator. > 0 will inditate valid AVB SYNC state
	uint32_t avbSyncState;

	uint16_t pdelay_sequence_id;
	std::shared_ptr<PTPMessagePathDelayReq> last_pdelay_req;
	std::shared_ptr<PTPMessagePathDelayResp> last_pdelay_resp;
	std::shared_ptr<PTPMessagePathDelayRespFollowUp> last_pdelay_resp_fwup;

	PTPMessageFollowUp last_fwup;
	PTPMessageDelayReq last_delay_req;
	PTPMessageDelayResp last_delay_resp;

	static PTPMessageAnnounce qualified_announce;

	IdentityMap_t identity_map;

	std::shared_ptr<PTPMessageSync> last_sync;

	OSCondition *port_ready_condition;

	OSLock *pdelay_rx_lock;
	OSLock *delay_rx_lock;
	OSLock *port_tx_lock;



	OSLock *pDelayIntervalTimerLock;
	OSLock *delayIntervalTimerLock;

	net_result port_send(uint16_t etherType, uint8_t * buf, int size,
	 MulticastType mcast_type, std::shared_ptr<PortIdentity> destIdentity,
	 bool timestamp, uint16_t port, bool sendToAllUnicast);


	bool pdelay_started;
	bool pdelay_halted;
	bool sync_rate_interval_timer_started;

	FrequencyRatio fMeanPathDelay;
	bool fMeanPathDelayIsSet;

protected:
	static const unsigned int DUPLICATE_RESP_THRESH = 3;

 public:
 	virtual void timestamper_init();
	void becomeMaster( bool annc );
	void becomeSlave( bool restart_syntonization );

	virtual void _watchNetLink()
	{
#ifdef RPI	
		port_ready_condition->signal();
#endif	
	}

	/**
	 * @brief  Starts pDelay event timer.
	 * @return void
	 */
	void startPDelay();

	/**
	 * @brief Stops PDelay event timer
	 * @return void
	 */
	void stopPDelay();

	/**
	 * @brief Enable/Disable PDelay Request messages
	 * @param hlt True to HALT (stop sending), False to resume (start sending).
	 */
	void haltPdelay(bool hlt)
	{
		pdelay_halted = hlt;
	}

	/**
	 * @brief Get the status of pdelayHalted condition.
	 * @return True PdelayRequest halted. False when PDelay Request is running
	 */
	bool pdelayHalted(void)
	{
		return pdelay_halted;
	}

	/**
	 * @brief  Starts Sync Rate Interval event timer. Used for the
	 *         Automotive Profile.
	 * @return void
	 */
	void startSyncRateIntervalTimer();

	/**
	 * @brief  Starts pDelay event timer if not yet started.
	 * @return void
	 */
	void syncDone();

	/**
	 * @brief  Gets the AVnu automotive profile flag
	 * @return automotive_profile flag
	 */
	bool getAutomotiveProfile() { return( automotive_profile ); }

	/**
	 * @brief Destroys a EtherPort
	 */
	~EtherPort();

	/**
	 * @brief  Creates the EtherPort interface.
	 * @param  init PortInit initialization parameters
	 */
	EtherPort( PortInit_t *portInit );

private:
	EtherPort();

public:

	/**
	 * @brief  Adds a new qualified announce the port. IEEE 802.1AS
	 * Clause 10.3.10.2
	 * @param  annc PTP announce message
	 * @return void
	 */
	virtual void setQualifiedAnnounce(const PTPMessageAnnounce &annc)
	{
		qualified_announce = annc;
	}

	virtual void resetQualifiedAnnounce()
	{
		qualified_announce.clear();
	}

	/**
	 * @brief  Gets the "best" announce
	 * @return PTPMessageAnnounce
	 */
	virtual PTPMessageAnnounce& calculateERBest(bool lockIt = true);

	/**
	 * @brief  Initializes the port. Creates network interface, initializes
	 * hardware timestamper and create OS locks conditions
	 * @return FALSE if error during building the interface. TRUE if success
	 */
	bool _init_port( void );

	/**
	 * @brief  Currently doesnt do anything. Just returns.
	 * @return void
	 */
	void recoverPort(void);

	/**
	 * @brief Message processing logic on receipt
	 * @param [in] buf buffer containing message
	 * @param [in] length buffer length
	 * @param [in] remote address of sender
	 * @param [in] link_speed of the receiving device
	 */
	void processMessage
	( char *buf, int length, LinkLayerAddress *remote,
	  uint32_t link_speed );

	bool PortCheck(EtherPort *port, bool isEvent);

	net_result maybeProcessMessage(bool checkEventMessage);

	/**
	 * @brief Receives messages from the network interface
	 * @return Its an infinite loop. Returns NULL in case of error.
	 */
	void *openPort( EtherPort *port );

	/**
	 * @brief Thread worker for receiving event messages from port 319.
	 * @return Its an infinite loop. Returns false in case of error.
	 */
	bool OpenEventPort(EtherPort *port);

	/**
	 * @brief Thread worker for receiving general messages from port 320.
	 * @return Its an infinite loop. Returns false in case of error.
	 */
	bool OpenGeneralPort(EtherPort *port);

	/**
	 * @brief  Sends and event to a IEEE1588 port. It includes timestamp
	 * @param  buf [in] Pointer to the data buffer
	 * @param  len Size of the message
	 * @param  mcast_type Enumeration MulticastType (pdlay, none or other). Depracated.
	 * @param  destIdentity Destination port identity
	 * @param  [out] link_speed indicates link speed
	 * @param  sendToAllUnicast true to send to all unicast nodes, otherwise send
	 *         to just destIdentity
	 * @return void
	 */
	void sendEventPort
	(uint16_t etherType, uint8_t * buf, int len, MulticastType mcast_type,
	 std::shared_ptr<PortIdentity> destIdentity, uint32_t *link_speed = nullptr,
	 bool sendToAllUnicast = false);

	/**
	 * @brief Sends a general message to a port. No timestamps
	 * @param buf [in] Pointer to the data buffer
	 * @param len Size of the message
	 * @param mcast_type Enumeration MulticastType (pdelay, none or other). Depracated.
	 * @param destIdentity Destination port identity
	 * @param  sendToAllUnicast true to send to all unicast nodes, otherwise send
	 *         to just destIdentity
	 * @return void
	 */
	void sendGeneralPort
	(uint16_t etherType, uint8_t * buf, int len, MulticastType mcast_type,
	 std::shared_ptr<PortIdentity> destIdentity, bool sendToAllUnicast = false);

	/**
	 * @brief  Process all events for a EtherPort
	 * @param  e Event to be processed
	 * @return true if event is handled, false otherwise
	 */
	bool _processEvent(Event e);

	/**
	 * @brief  Adds a foreign master.
	 * @param  msg [in] PTP announce message
	 * @return void
	 * @todo Currently not implemented
	 */
	void addForeignMaster(PTPMessageAnnounce * msg);

	/**
	 * @brief  Remove a foreign master.
	 * @param  msg [in] PTP announce message
	 * @return void
	 * @todo Currently not implemented
	 */
	void removeForeignMaster(PTPMessageAnnounce * msg);

	/**
	 * @brief  Remove all foreign masters.
	 * @return void
	 * @todo Currently not implemented
	 */
	void removeForeignMasterAll(void);

	/**
	 * @brief  Gets the pDelay minimum interval
	 * @return PDelay interval
	 */
	signed char getPDelayInterval(void) {
		return log_min_mean_pdelay_req_interval;
	}

	/**
	 * @brief  Sets the pDelay minimum interval
	 * @param  val time interval
	 * @return none
	 */
	void setPDelayInterval(signed char val) {
		log_min_mean_pdelay_req_interval = val;
	}

	/**
	 * @brief  Sets the pDelay minimum interval back to initial
	 *         value
	 * @return none */
	void setInitPDelayInterval(void) {
		log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;
	}

	/**
	 * @brief  Start pDelay interval timer
	 * @param  waitTime time interval
	 * @return none
	 */
	void startPDelayIntervalTimer(long long unsigned int waitTime);

	/**
	 * @brief  Increments PDelay sequence ID and returns.
	 * @return Next PDelay sequence id.
	 */
	uint16_t getNextPDelaySequenceId(void) {
		return pdelay_sequence_id++;
	}

	/**
	 * @brief  Gets last sync sequence number from parent
	 * @return Parent last sync sequence number
	 * @todo Not currently implemented.
	 */
	uint16_t getParentLastSyncSequenceNumber(void);

	/**
	 * @brief  Sets last sync sequence number from parent
	 * @param  num Sequence number
	 * @return void
	 * @todo Currently not implemented.
	 */
	void setParentLastSyncSequenceNumber(uint16_t num);

	/**
	 * @brief  Sets last sync ptp message
	 * @param  msg [in] PTP sync message
	 * @return void
	 */
	void setLastSync(std::shared_ptr<PTPMessageSync> msg, bool lockIt = true);

	void setLastSync(const PTPMessageSync& msg, bool lockIt = true);

	/**
	 * @brief  Gets last sync message
	 * @return PTPMessageSync last sync
	 */
	std::shared_ptr<PTPMessageSync> getLastSync(bool lockIt = true);

	/**
	 * @brief  Locks PDelay RX
	 * @return TRUE if acquired the lock. FALSE otherwise
	 */
	bool getPDelayRxLock() {
		return pdelay_rx_lock->lock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Do a trylock on the PDelay RX
	 * @return TRUE if acquired the lock. FALSE otherwise.
	 */
	bool tryPDelayRxLock() {
		return pdelay_rx_lock->trylock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Unlocks PDelay RX.
	 * @return TRUE if success. FALSE otherwise
	 */
	bool putPDelayRxLock() {
		return pdelay_rx_lock->unlock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Locks Delay RX
	 * @return TRUE if acquired the lock. FALSE otherwise
	 */
	bool getDelayRxLock() {
		return delay_rx_lock->lock() == oslock_ok;
	}

	/**
	 * @brief  Do a trylock on the Delay RX
	 * @return TRUE if acquired the lock. FALSE otherwise.
	 */
	bool tryDelayRxLock() {
		return delay_rx_lock->trylock() == oslock_ok;
	}

	/**
	 * @brief  Unlocks Delay RX.
	 * @return TRUE if success. FALSE otherwise
	 */
	bool putDelayRxLock() {
		return delay_rx_lock->unlock() == oslock_ok;
	}
	/**
	 * @brief  Locks the TX port
	 * @return TRUE if success. FALSE otherwise.
	 */
	bool getTxLock() {
		return port_tx_lock->lock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Unlocks the port TX.
	 * @return TRUE if success. FALSE otherwise.
	 */
	bool putTxLock() {
		return port_tx_lock->unlock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Sets the last_pdelay_req message
	 * @param  msg [in] PTPMessagePathDelayReq message to set
	 * @return void
	 */
	void setLastPDelayReq(std::shared_ptr<PTPMessagePathDelayReq> msg) {
		last_pdelay_req = msg;
	}

	/**
	 * @brief  Gets the last PTPMessagePathDelayReq message
	 * @return Last pdelay request
	 */
	std::shared_ptr<PTPMessagePathDelayReq> getLastPDelayReq() {
		return last_pdelay_req;
	}

	/**
	 * @brief  Sets the last PTPMessagePathDelayResp message
	 * @param  msg [in] Last pdelay response
	 * @return void
	 */
	void setLastPDelayResp(std::shared_ptr<PTPMessagePathDelayResp> msg) {
		last_pdelay_resp = msg;
	}

	void setLastPDelayResp(const PTPMessagePathDelayResp& msg) {
		*last_pdelay_resp = msg;
	}
	/**
	 * @brief  Gets the last PTPMessagePathDelayResp message
	 * @return Last pdelay response
	 */
	std::shared_ptr<PTPMessagePathDelayResp> getLastPDelayResp() {
		return last_pdelay_resp;
	}

	/**
	 * @brief  Sets the last PTPMessagePathDelayRespFollowUp message
	 * @param  msg [in] last pdelay response follow up
	 * @return void
	 */
	void setLastPDelayRespFollowUp(std::shared_ptr<PTPMessagePathDelayRespFollowUp> msg) {
		last_pdelay_resp_fwup = msg;
	}

	void setLastPDelayRespFollowUp(const PTPMessagePathDelayRespFollowUp& msg) {
		*last_pdelay_resp_fwup = msg;
	}
	
	/**
	 * @brief  Gets the last PTPMessagePathDelayRespFollowUp message
	 * @return last pdelay response follow up
	 */
	std::shared_ptr<PTPMessagePathDelayRespFollowUp> getLastPDelayRespFollowUp(void) {
		return last_pdelay_resp_fwup;
	}

	/**
	 * @brief  Sets the last PTPMessageFollowUp message
	 * @param  msg [in] last follow up
	 * @return void
	 */
	void setLastFollowUp(std::shared_ptr<PTPMessageFollowUp> msg);

	/**
	 * @brief  Gets the last PTPMessageFollowUp message
	 * @return last follow up
	 */
	const PTPMessageFollowUp& getLastFollowUp(bool lockIt = true) const;

	/**
	 * @brief  Sets the last_delay_req message
	 * @param  msg [in] PTPMessageDelayReq message to set
	 * @return void
	 */
	void setLastDelayReq(std::shared_ptr<PTPMessageDelayReq> msg);

	void setLastDelayReq(const PTPMessageDelayReq& msg);

	/**
	 * @brief  Gets the last PTPMessageDelayReq message
	 * @return Last delay request
	 */
	const PTPMessageDelayReq& getLastDelayReq(bool lockIt = true) const;

	/**
	 * @brief  Sets the last PTPMessageDelayResp message
	 * @param  msg [in] Last delay response
	 * @return void
	 */
	void setLastDelayResp(std::shared_ptr<PTPMessageDelayResp> msg);

	/**
	 * @brief  Gets the last PTPMessageDelayResp message
	 * @return Last delay response
	 */
	const PTPMessageDelayResp& getLastDelayResp(bool lockIt = true) const;

	/**
	 * @brief  Gets RX timestamp based on port identity
	 * @param  sourcePortIdentity [in] Source port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] RX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the rx lock.
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getRxTimestamp
	(std::shared_ptr<PortIdentity> sourcePortIdentity, const PTPMessageId& messageId,
	 Timestamp & timestamp, unsigned &counter_value, bool last);

	/**
	 * @brief  Gets TX timestamp based on port identity
	 * @param  sourcePortIdentity [in] Source port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] TX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the TX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getTxTimestamp
	(std::shared_ptr<PortIdentity>  sourcePortIdentity, const PTPMessageId& messageId,
	 Timestamp & timestamp, unsigned &counter_value, bool last);

	/**
	 * @brief  Gets TX timestamp based on PTP message
	 * @param  msg PTPMessageCommon message
	 * @param  timestamp [out] TX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the TX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getTxTimestamp
	(std::shared_ptr<PTPMessageCommon> msg, Timestamp & timestamp, unsigned &counter_value,
	 bool last);

	int getTxTimestamp
	(const PTPMessageCommon& msg, Timestamp & timestamp, unsigned &counter_value,
	 bool last);

	/**
	 * @brief  Gets RX timestamp based on PTP message
	 * @param  msg PTPMessageCommon message
	 * @param  timestamp [out] RX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the RX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getRxTimestamp
	(std::shared_ptr<PTPMessageCommon> msg, Timestamp & timestamp, unsigned &counter_value,
	 bool last);

	/**
	 * @brief Sets the value of last duplicated SeqID
	 * @param seqid Value to set
	 * @return void
	 */
	void setLastInvalidSeqID(uint16_t seqid)
	{
		last_invalid_seqid = seqid;
	}

	/**
	 * @brief  Get the value of last invalid seqID
	 * @return Last invalid seq id
	 */
	uint16_t getLastInvalidSeqID(void)
	{
		return last_invalid_seqid;
	}

	/**
	 * @brief  Sets the duplicate pdelay_resp counter.
	 * @param  cnt Value to be set
	 */
	void setDuplicateRespCounter(unsigned int cnt)
	{
		duplicate_resp_counter = cnt;
	}

	/**
	 * @brief  Gets the current value of pdelay_resp duplicate messages counter
	 * @return Counter value
	 */
	unsigned int getDuplicateRespCounter(void)
	{
		return duplicate_resp_counter;
	}

	/**
	 * @brief  Increment the duplicate PDelayResp message counter
	 * @return True if it equals the threshold, False otherwise
	 */
	bool incrementDuplicateRespCounter(void)
	{
		return ++duplicate_resp_counter == DUPLICATE_RESP_THRESH;
	}

	/**
	 * @brief  Maps socket addr to the remote link layer address
	 * @param  destIdentity [in] PortIdentity remote
	 * @param  remote [in] remote link layer address
	 * @return void
	 */
	void mapSocketAddr
	(std::shared_ptr<PortIdentity>  destIdentity, LinkLayerAddress * remote);

	/**
	 * @brief  Adds New sock addr map
	 * @param  destIdentity [in] PortIdentity remote
	 * @param  remote [in] remote link layer address
	 * @return void
	 */
	void addSockAddrMap
	(std::shared_ptr<PortIdentity> destIdentity, LinkLayerAddress * remote);

	/**
	 * @brief  Gets link up count
	 * @return Link up  count
	 */
	uint32_t getLinkUpCount() {
		return linkUpCount;
	}

	/**
	 * @brief  Gets link down count
	 * @return Link down count
	 */
	uint32_t getLinkDownCount() {
		return linkDownCount;
	}

	/**
	 * @brief  Sets the Station State for the Test Status message
	 * @param  StationState_t [in] The station state
	 * @return none
	 */
	void setStationState(StationState_t _stationState) {
		stationState = _stationState;
		if (stationState == STATION_STATE_ETHERNET_READY) {
			GPTP_LOG_STATUS("AVnu AP Status : STATION_STATE_ETHERNET_READY");
		}
		else if (stationState == STATION_STATE_AVB_SYNC) {
			GPTP_LOG_STATUS("AVnu AP Status : STATION_STATE_AVB_SYNC");
		}
		else if (stationState == STATION_STATE_AVB_MEDIA_READY) {
			GPTP_LOG_STATUS("AVnu AP Status : STATION_STATE_AVB_MEDIA_READY");
		}
	}

	/**
	 * @brief  Gets the Station State for the Test Status
	 *         message
	 * @return station state
	 */
	StationState_t getStationState() {
		return stationState;
	}


        /**
	 * @brief  Sets the linkUp status
	 * @param  bool of the linkUp status
	 * @return void
	 */
	void setLinkUpState(bool state) {
		linkUp = state;
	}

	/**
	 * @brief  Gets the linkUp status
	 * @return bool of the linkUp status
	 */
	bool getLinkUpState(void) {
		return linkUp;
	}

public:
	FrequencyRatio meanPathDelay() const;
	void meanPathDelay(FrequencyRatio delay);

	std::mutex* GetLastFwupMutex();
	std::mutex* GetLastDelayReqMutex();
	std::mutex* GetLastDelayRespMutex();
	std::mutex* GetLastSyncMutex();
	std::mutex* GetMeanPathDelayMutex();

	void setClockPriority1(uint8_t priority1);
};

#endif/*ETHER_PORT_HPP*/
