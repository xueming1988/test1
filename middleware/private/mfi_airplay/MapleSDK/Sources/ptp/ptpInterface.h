/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_INTERFACE_H_
#define _PTP_INTERFACE_H_

#include "ptpConstants.h"

/**
 * @brief Available states of NetworkPort object
 */
typedef enum {
    ePortStateInit, // Port's initial state.
    ePortStateMaster, // Port is PTP Master
    ePortStateSlave // Port is PTP Slave
} PortState;

#define PortStateStr(s) \
    (s) == ePortStateInit ? "initializing" : (s) == ePortStateMaster ? "master" : (s) == ePortStateSlave ? "slave" : "????"

/**
 * @brief This structure describes the current state of PTP context
 */

typedef struct {

    /**
	 * @brief Master clock identity
	 */
    uint64_t _masterClockId;

    /**
	 * @brief The clock identity of the interface
	 */
    uint8_t _localClockId[CLOCK_IDENTITY_LENGTH];

    /**
	 * @brief Local time of last update in nanoseconds
	 */
    NanosecondTs _syncArrivalTime;

    /**
	 * @brief Master to local phase offset in nanoseconds ((T2 - T1) - one_way_delay)
	 */
    NanosecondTs _masterToLocalPhase;

    /**
	 * @brief Master to local frequency ratio
	 */
    double _masterToLocalRatio;

    /**
	 * @brief PTP domain number
	 */
    uint8_t _domainNumber;

    /**
	 * @brief The priority1 field of the grandmaster functionality of the interface, or 0xFF if not supported
	 */
    uint8_t _priority1;

    /**
	 * @brief The clockClass field of the grandmaster functionality of the interface, or 0xFF if not supported
	 */
    uint8_t _clockClass;

    /**
	 * @brief The offsetScaledLogVariance field of the grandmaster functionality of the interface, or 0x0000 if not supported
	 */
    int16_t _offsetScaledLogVariance;

    /**
	 * @brief The clockAccuracy field of the grandmaster functionality of the interface, or 0xFF if not supported
	 */
    uint8_t _clockAccuracy;

    /**
	 * @brief The priority2 field of the grandmaster functionality of the interface, or 0xFF if not supported
	 */
    uint8_t _priority2;

    /**
	 * @brief The domainNumber field of the grandmaster functionality of the interface, or 0 if not supported
	 */
    uint8_t _grandmasterDomainNumber;

    /**
	 * @brief The currentLogSyncInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 */
    int8_t _logSyncInterval;

    /**
	 * @brief The currentLogAnnounceInterval field of the grandmaster functionality of the interface, or 0 if not supported
	 */
    int8_t _logAnnounceInterval;

    /**
	 * @brief The portNumber field of the interface, or 0x0000 if not supported
	 */
    uint16_t _portNumber;

    /**
	 * @brief Sync messages count
	 */
    uint32_t _syncCount;

    /**
	 * @brief PTP port state
	 */
    PortState _portState;

} PtpTimeData;

typedef PtpTimeData* PtpTimeDataRef;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start PTP service in slave mode
 * @param itfName name of the network interface to run the PTP service on
 * @param instanceId PTP instance identifier
 * @param priority1 PTP priority 1 configuration value
 * @param priority2 PTP priority 2 configuration value
 * @return true if succeeded, false if failed
 */
Boolean ptpStart(const char* itfName, uint64_t instanceId, uint8_t priority1, uint8_t priority2);

/**
 * @brief Stop PTP service
 * @param instanceId PTP instance identifier
 * @return
 */
Boolean ptpStop(uint64_t instanceId);

/**
 * @brief Retrieve current state of PTP session. This method is thread safe.
 * @param instanceId PTP instance identifier
 * @param data
 */
void ptpRefresh(uint64_t instanceId, PtpTimeDataRef data);

/**
 * @brief Retrieve ptp version string
 * @return pointer to the version string
 */
const char* ptpVersion(void);

/**
 * @brief Add hostname to the list of unicast PTP target nodes
 * @param instanceId PTP instance identifier
 * @param hostName the url of host
 */
void ptpAddPeer(uint64_t instanceId, const char* hostName);

/**
 * @brief Remove hostname from the list of unicast PTP target nodes
 * @param instanceId PTP instance identifier
 * @param hostName the url of host
 */
void ptpRemovePeer(uint64_t instanceId, const char* hostName);

/**
 * @brief Run ptp test client
 * @param instanceId PTP instance identifier
 * @param interfaceName name of the network interface to run the PTP service on
 * @param hostName url of the PTP server (or NULL if the test shall run in server mode)
 */
void ptpTest(uint64_t instanceId, const char* interfaceName, const char* hostName);

#ifdef __cplusplus
}
#endif

#endif // _PTP_INTERFACE_H_
