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

#ifndef AVBTS_OSNET_HPP
#define AVBTS_OSNET_HPP

#include <algorithm>
#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <map>
#include <ieee1588.hpp>
#include <ptptypes.hpp>
#include <arpa/inet.h>
#include <sstream>

#include "common_tstamper.hpp"
#include "macaddress.hpp"

/**@file*/


#define FACTORY_NAME_LENGTH 48		/*!< Factory name maximum length */
#define DEFAULT_TIMEOUT 1			/*!< Default timeout in milliseconds*/

/**
 * @brief LinkLayerAddress Class
 * Provides methods for initializing and comparing ethernet addresses.
 */
class LinkLayerAddress:public InterfaceLabel
{
 public:
	/**
	 * @brief Default constructor
	 */
	LinkLayerAddress()
	{
	};

	LinkLayerAddress(const std::string& ip, uint16_t portNumber);

	/**
	 * @brief Receives a 64bit scalar address and initializes its internal octet
	 * array with the first 48 bits.
	 * @param address_scalar 64 bit address
	 */
	LinkLayerAddress(uint64_t address_scalar)
	{
		uint8_t *ptr;
		address_scalar <<= 16;
		
		for (ptr = fIpv6Addr; ptr < fIpv6Addr + ETHER_ADDR_OCTETS; ++ptr) {
			*ptr = (address_scalar & 0xFF00000000000000ULL) >> 56;
			address_scalar <<= 8;
		}
	}

	LinkLayerAddress(const sockaddr &other)
	{
		if (AF_INET == other.sa_family)
		{
			fIpVersion = 4;
			const sockaddr_in *dest = reinterpret_cast<const sockaddr_in*>(&other);
			//ok = inet_pton(AF_INET, other.sa_data, &(dest->sin_addr);
			fIpv4Addr = dest->sin_addr.s_addr;

			char buf[INET_ADDRSTRLEN];
			in_addr source;
			source.s_addr = fIpv4Addr;
			inet_ntop(AF_INET, &source, buf, sizeof(buf));

			GPTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  sockaddr  ipv4  rawAddress:%s", buf);
		}
		else if (AF_INET6 == other.sa_family)
		{
			fIpVersion = 6;
	      const sockaddr_in6 *dest = reinterpret_cast<const sockaddr_in6*>(&other);
	      //ok = inet_pton(AF_INET6, other.sa_data, &(dest->sin6_addr));
	      memcpy(fIpv6Addr, dest->sin6_addr.s6_addr, sizeof(dest->sin6_addr.s6_addr));

			char buf[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &(dest->sin6_addr.s6_addr), buf, sizeof(buf));
			GPTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  sockaddr  ipv6  rawAddress:%s", buf);
		}
		else
		{
			GPTP_LOG_ERROR("Error constructing LinkLayAddress from sockaddr, "
			 "invalid family %d", other.sa_family);	
		}

		GPTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  sockaddr  address:%s", AddressString().c_str());
	}

	LinkLayerAddress(const sockaddr_in &other)
	{
	   memset(fIpv6Addr, 0, sizeof(fIpv6Addr));
	   fIpv4Addr = 0;
		fIpVersion = 4;

		char buf[INET_ADDRSTRLEN];
		const char* resultBuf = inet_ntop(other.sin_family, &other.sin_addr, buf, sizeof(buf));
		if (nullptr == resultBuf)
		{
			GPTP_LOG_ERROR("Error constructing LinkLayAddress from sockaddr_in6.");
		}
		else
		{
			GPTP_LOG_VERBOSE("LinkLayerAddress ctor  ipv4  addrStr:%s  sin_port:%d  "
				"sin_family:%d", buf, other.sin_port, other.sin_family);

			fIpv4Addr = other.sin_addr.s_addr;
		}
		//memset(&fIpv4Addr, 0, sizeof(fIpv4Addr));
		//ParseIpAddress(buf, fIpv4Addr, ETHER_ADDR_OCTETS, ".");
	}

	LinkLayerAddress(const sockaddr_in6 &other)
	{
	   memset(fIpv6Addr, 0, sizeof(fIpv6Addr));
	   fIpv4Addr = 0;
		fIpVersion = 6;

		char buf[INET6_ADDRSTRLEN];
		const char* resultBuf = inet_ntop(other.sin6_family, &other.sin6_addr, buf, sizeof(buf));
		if (nullptr == resultBuf)
		{
			GPTP_LOG_ERROR("Error constructing LinkLayAddress from sockaddr_in6.");
		}
		else
		{
			GPTP_LOG_VERBOSE("LinkLayerAddress ctor  ipv6  addrStr:%s  sin6_port:%d  "
				"sin6_family:%d", buf, other.sin6_port, other.sin6_family);

			FixBSDLinkLocal(other);
		}

	}
	/**
	 * @brief Receives an address as an array of octets
	 * and copies the first 6 over the internal ethernet address.
	 * @param address_octet_array Array of octets containing the address
	 */
	LinkLayerAddress(uint8_t * address_octet_array)
	{
	   memset(fIpv6Addr, 0, sizeof(fIpv6Addr));
	   fIpv4Addr = 0;

		uint8_t *ptr = fIpv6Addr;
		uint8_t *dest = ptr;
		if (nullptr == address_octet_array)
		{
			return;
		}

		for (; ptr < dest + ETHER_ADDR_OCTETS; ++ptr, ++address_octet_array)
		{
			*ptr = *address_octet_array;
		}
	}

	/**
	 * @brief  Operator '==' overloading method.
	 * It provides a comparison between cmp and the class ethernet address defined
	 * at its constructor.
	 * @param  cmp Value to be compared against.
	 * @return TRUE if they are equal; FALSE otherwise.
	 */
	bool operator==(const LinkLayerAddress & cmp) const 
	{
		return (4 == fIpVersion 
		 ? fIpv4Addr == cmp.fIpv4Addr
		 : 0 == memcmp(fIpv6Addr, cmp.fIpv6Addr, IPV6_ADDR_OCTETS)) &&
		 cmp.fPort == fPort;
	}

	/**
	 * @brief  Operator '<' overloading method.
	 ** It provides a comparison between cmp and the class ethernet address defined
	 * at its constructor.
	 * @param  cmp Value to be compared against.
	 * @return TRUE if cmp is lower than addr, FALSE otherwise.
	 */
	bool operator <(const LinkLayerAddress & cmp) const
	{
		return 4 == fIpVersion 
		 ? fIpv4Addr < cmp.fIpv4Addr
		 : memcmp(fIpv6Addr, cmp.fIpv6Addr, IPV6_ADDR_OCTETS) < 0;
	}

	/**
	 * @brief  Operator '>' overloading method.
	 ** It provides a comparison between cmp and the class ethernet address defined
	 * at its constructor.
	 * @param  cmp Value to be compared against.
	 * @return TRUE if cmp is bigger than addr, FALSE otherwise.
	 */
	bool operator>(const LinkLayerAddress & cmp) const
	{
		return 4 == fIpVersion
		 ? fIpv4Addr > cmp.fIpv4Addr
		 : memcmp(fIpv6Addr, cmp.fIpv6Addr, IPV6_ADDR_OCTETS) > 0;
	}

	/**
	 * @brief  Gets bytes from ethernet address of
	 * object LinkLayerAddress.
	 * @param  address_octet_array [out] Pointer to store the
	 * ethernet address information.
	 * @return void
	 */
	void toOctetArray(uint8_t * address_octet_array, size_t octetSize)
	{
		if (nullptr == address_octet_array)
		{
			return;
		}

		if (4 == fIpVersion)
		{
			char buf[ETHER_ADDR_OCTETS];
			in_addr source;
			source.s_addr = fIpv4Addr;
			inet_ntop(AF_INET, &source, buf, sizeof(buf));
			ParseIpAddress(buf, address_octet_array, octetSize, ".");
		}
		else
		{
			sockaddr_in6 other;
			memcpy(other.sin6_addr.s6_addr, fIpv6Addr, sizeof(fIpv6Addr));
			std::string address = FixBSDLinkLocal(other);			
			ParseIpAddress(address, address_octet_array, octetSize, ":");
		}
	}

	void getAddress(in_addr *dest)
	{
		if (fIpVersion != 4)
		{
			GPTP_LOG_ERROR("Invalid coversion...This address is version %d but "
			 "converting to 4", fIpVersion);
			return;
		}

		// Convert the raw values to a string for use with inet_pton
		std::string address = AddressString();

		GPTP_LOG_VERBOSE("address:%s", address.c_str());

		// Convert the address eg "192.168.0.189"
		inet_pton(AF_INET, address.c_str(), dest);
	}

	void getAddress(in6_addr *dest)
	{
		std::string address = AddressString();
		GPTP_LOG_VERBOSE("fIpVersion:%d   address:%s", fIpVersion, address.c_str());

		inet_pton(AF_INET6, address.c_str(), dest);
	}

	uint16_t Port() const
	{
		return fPort;
	}

	void Port(uint16_t number)
	{
		fPort = number;
	}

	const std::string AddressString()
	{
		std::string address;

		if (4 == fIpVersion)
		{
			char buf[INET_ADDRSTRLEN];
			in_addr source;
			source.s_addr = fIpv4Addr;
			inet_ntop(AF_INET, &source, buf, sizeof(buf));
			address = buf;
		}
		else
		{
			sockaddr_in6 other;
			memcpy(other.sin6_addr.s6_addr, fIpv6Addr, sizeof(fIpv6Addr));
			address = FixBSDLinkLocal(other);
		}

		GPTP_LOG_VERBOSE("AddressString  fIpVersion:%d address:%s", fIpVersion, address.c_str());

		return address;
	}

	int IpVersion() const
	{
		return fIpVersion;
	}

private:
	const std::string FixBSDLinkLocal(const sockaddr_in6& other)
	{
		char buf[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &other.sin6_addr, buf, sizeof(buf));
		std::string address = buf;

		// On BSD based platforms, the scope ID is embedded into the address
		// itself for link local addresses as the second 16 bit word so we need
		// to remove it - it can't be used on POSIX conformant platforms.
		// !!!...
		// This assumes link local addresses, all bets are off for global
		// addresses.
		// ...!!!
		size_t pos = address.find(":");
		std::string first = address.substr(0, pos);
		if (pos != std::string::npos && ("fe80" == first || "FE80" == first) &&
		 address.substr(pos + 1, 1) != ":")
		{
			std::string rest = address.substr(address.find(":", pos + 1));
			address = first + rest;
			in6_addr dest;
			inet_pton(AF_INET6, address.c_str(), &dest);
			memcpy(fIpv6Addr, dest.s6_addr, sizeof(dest.s6_addr));
		}
		else
		{
			memset(fIpv6Addr, 0, sizeof(fIpv6Addr));
			memcpy(fIpv6Addr, other.sin6_addr.s6_addr, sizeof(other.sin6_addr.s6_addr));
			//ParseIpAddress(buf, fIpv6Addr, IPV6_ADDR_OCTETS, ":");
		}

		GPTP_LOG_VERBOSE("FixBSDLinkLocal    address:%s", address.c_str());

		return address;
	}

	void ParseIpAddress(const std::string& ip, uint8_t * destination,
 	 size_t maxSize, const std::string& token)
	{
		size_t start = 0;
		size_t end = 0;
		size_t idx = 0;
		uint8_t *p = destination;
		int base = ":" == token ? 16 : 10;
		size_t length;

		while (idx < maxSize)
		{
			end = ip.find(token, start);
			if (std::string::npos == end)
			{
				*p = std::stoi(ip.substr(start), 0, base);
				break;
			}
			else
			{
			   length = end - start;
			   if (length > 0)
			   {
			      *p = std::stoi(ip.substr(start, length), 0, base);
			   }
			}
			++idx;
			++p;
			start = end + 1;
		}
	}

 private:
	uint32_t fIpv4Addr;
	uint8_t fIpv6Addr[IPV6_ADDR_OCTETS];
	uint16_t fPort;
	int fIpVersion;
};

/**
 * @brief Provides methods for dealing with the network interface name
 * @todo: Destructor doesnt delete this->name.
 */
class InterfaceName: public InterfaceLabel {
 private:
	//!< Interface name
	char *name;
 public:
	/**
	 * @brief Default constructor
	 */
	InterfaceName() : name(nullptr) { }
	/**
	 * @brief Initializes Interface name with name and size lenght+1
	 * @param name [in] String with the interface name
	 * @param length Size of name
	 */
	InterfaceName(char *name, int length) {
		this->name = new char[length + 1];
		PLAT_strncpy(this->name, name, length);
	}
	~InterfaceName() {
		delete[](this->name);
	}

	/**
	 * @brief  Operator '==' overloading method.
	 * Compares parameter cmp to the interface name
	 * @param  cmp String to be compared
	 * @return TRUE if they are equal, FALSE otherwise
	 */
	bool operator==(const InterfaceName & cmp) const {
		return strcmp(name, cmp.name) == 0 ? true : false;
	}

	/**
	 * @brief  Operator '<' overloading method.
	 * Compares cmp to the interface name
	 * @param  cmp String to be compared
	 * @return TRUE if interface name is found to be less than cmd. FALSE otherwise
	 */
	bool operator<(const InterfaceName & cmp)const {
		return strcmp(name, cmp.name) < 0 ? true : false;
	}

	/**
	 * @brief  Operator '>' overloading method.
	 * Compares cmp to the interface name
	 * @param  cmp String to be compared
	 * @return TRUE if the interface name is found to be greater than cmd. FALSE otherwise
	 */
	bool operator>(const InterfaceName & cmp)const {
		return strcmp(name, cmp.name) > 0 ? true : false;
	}

	/**
	 * @brief  Gets interface name from the class' internal variable
	 * @param  string [out] String to store interface's name
	 * @param  length Length of string
	 * @return TRUE if length is greater than size of interface name plus one. FALSE otherwise.
	 */
	bool toString(char *string, size_t length) {
		if(string == NULL)
			return false;

		if (length >= strlen(name) + 1) {
			PLAT_strncpy(string, name, length);
			return true;
		}
		return false;
	}

	const std::string Name() const
	{
		return name;
	}
};

/**
 * @brief Provides a generic class to be used as a key to create factory maps.
 */
class factory_name_t {
 private:
	/*<! Factory name*/
	char name[FACTORY_NAME_LENGTH];
	factory_name_t();
 public:
	/**
	 * @brief Assign a name to the factory_name
	 * @param name_a [in] Name to be assigned to the object
	 */
	factory_name_t(const char *name_a) {
		PLAT_strncpy(name, name_a, FACTORY_NAME_LENGTH - 1);
	}

	/**
	 * @brief  Operator '==' overloading method
	 * Compares cmp to the factory name
	 * @param  cmp String to be compared
	 * @return TRUE if they are equal, FALSE otherwise
	 */
	bool operator==(const factory_name_t & cmp) {
		return strcmp(cmp.name, this->name) == 0 ? true : false;
	}

	/**
	 * @brief  Operator '<' overloading method
	 * Compares cmp to the factory name
	 * @param  cmp String to be compared
	 * @return TRUE if the factory_name is to be found less than cmp, FALSE otherwise
	 */
	bool operator<(const factory_name_t & cmp)const {
		return strcmp(cmp.name, this->name) < 0 ? true : false;
	}

	/**
	 * @brief  Operator '>' overloading method
	 * Compares cmp to the factory name
	 * @param  cmp String to be compared
	 * @return TRUE if the factory_name is to be found greater than cmp, FALSE otherwise
	 */
	bool operator>(const factory_name_t & cmp)const {
		return strcmp(cmp.name, this->name) > 0 ? true : false;
	}
};

/**
 * @brief Enumeration net_result:
 * 	- net_trfail
 * 	- net_fatal
 * 	- net_succeed
 */
typedef enum { net_trfail, net_fatal, net_succeed } net_result;


/**
 * @brief Enumeration net_link_event:
 * 	- net_linkup
 * 	- net_linkdown
 */
typedef enum { NET_LINK_EVENT_DOWN, NET_LINK_EVENT_UP, NET_LINK_EVENT_FAIL } net_link_event;

/**
 * @brief Provides a generic network interface
 */
class OSNetworkInterface {
 public:
	 /**
	  * @brief Sends a packet to a remote address
	  * @param addr [in] Remote link layer address
	  * @param etherType [in] The EtherType of the message
	  * @param payload [in] Data buffer
	  * @param length Size of data buffer
	  * @param timestamp TRUE if to use the event socket with the PTP multicast address. FALSE if to use
	  * a general socket.
	  */
	 virtual net_result send
		(LinkLayerAddress * addr, uint16_t etherType, uint8_t * payload, size_t length,
		  bool timestamp) = 0;

	 /**
	  * @brief  Receives data
	  * @param  addr [out] Destination Mac Address
	  * @param  payload [out] Payload received
	  * @param  length [out] Received length
	  * @return net_result enumeration
	  */
	 virtual net_result nrecv
		(LinkLayerAddress *addr, uint8_t *payload, size_t &length) = 0;

	 /**
	  * @brief  Receives data from port 319
	  * @param  addr [out] Destination Mac Address
	  * @param  payload [out] Payload received
	  * @param  length [out] Received length
	  * @return net_result enumeration
	  */
	 virtual net_result nrecvEvent
		(LinkLayerAddress *addr, uint8_t *payload, size_t &length, Timestamp &ingressTime, EtherPort *port) = 0;

	 /**
	  * @brief  Receives data from port 320
	  * @param  addr [out] Destination Mac Address
	  * @param  payload [out] Payload received
	  * @param  length [out] Received length
	  * @return net_result enumeration
	  */
	 virtual net_result nrecvGeneral
		(LinkLayerAddress *addr, uint8_t *payload, size_t &length, Timestamp &ingressTime, EtherPort *port) = 0;

	 /**
	  * @brief Get Link Layer address (mac address)
	  * @param addr [out] Link Layer address
	  * @return void
	  */
	 virtual void getMacAddress(AMacAddress * addr) = 0;

	 /**
	  * @brief Watch for netlink changes.
	  */
	 virtual void watchNetLink(CommonPort *pPort) = 0;

	 /**
	  * @brief  Provides generic method for getting the payload offset
	  */
	 virtual unsigned getPayloadOffset() = 0;

	 /**
	  * @brief Native support for polimorphic destruction
	  */
	 virtual ~OSNetworkInterface() = 0;

	 virtual void LogLockState() {}

	 virtual bool IsWireless(const std::string& netInterfaceName) const
	 {
	 	return false;
	 }
};

inline OSNetworkInterface::~OSNetworkInterface() {}

class OSNetworkInterfaceFactory;

/**
 * @brief Provides a map for the OSNetworkInterfaceFactory::registerFactory method
 */
typedef std::map <factory_name_t, std::shared_ptr<OSNetworkInterfaceFactory> > FactoryMap_t;

/**
 * @brief Builds and registers a network interface
 */
class OSNetworkInterfaceFactory {
 public:
	 /**
	  * @brief  Registers network factory
	  * @param id
	  * @param factory Factory name
	  * @return TRUE success, FALSE when could not register it.
	  */
	static bool registerFactory
	(factory_name_t id, std::shared_ptr<OSNetworkInterfaceFactory> factory) {
		FactoryMap_t::iterator iter = factoryMap.find(id);
		if (iter != factoryMap.end())
			return false;
		factoryMap[id] = factory;
		return true;
	}

	/**
	 * @brief Builds the network interface
	 * @param iface [out] Pointer to interface name
	 * @param id Factory name index
	 * @param iflabel Interface label
	 * @param timestamper CommonTimestamper class pointer
	 * @return TRUE ok, FALSE error.
	 */
	static bool buildInterface(OSNetworkInterface ** iface, factory_name_t id,
	 InterfaceLabel * iflabel, std::shared_ptr<CommonTimestamper> timestamper)
	{
		return factoryMap[id]->createInterface(iface, iflabel, timestamper);
	}

	virtual ~OSNetworkInterfaceFactory() = 0;
private:
	virtual bool createInterface(OSNetworkInterface ** iface,
	 InterfaceLabel * iflabel, std::shared_ptr<CommonTimestamper> timestamper) = 0;

	static FactoryMap_t factoryMap;
};

inline OSNetworkInterfaceFactory::~OSNetworkInterfaceFactory() { }

#endif
