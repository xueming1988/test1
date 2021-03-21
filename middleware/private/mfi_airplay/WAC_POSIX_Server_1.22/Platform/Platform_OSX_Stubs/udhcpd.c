#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statfs.h>   	
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/un.h>
#include <stddef.h>
#include <arpa/inet.h> 
#include <net/route.h>
#include <sys/ioctl.h> 
#include <net/if_arp.h>
#include <fcntl.h> 
#include <sys/wait.h> 
#include <netdb.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include <pthread.h>
//#include "udhcpd.h"
//#include "stringutil.h"



//typedef unsigned char uint8_t;
//typedef unsigned char u_int8_t;
//typedef unsigned short  uint16_t;
//typedef unsigned short  u_int16_t;
//typedef unsigned int uint32_t;
//typedef unsigned int u_int32_t;
#ifndef bool
typedef int bool;
#endif

//#define true  (1)
//#define false (0)

#define DHCP_OPTIONS_BUFSIZE  308
#define CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS 80

#define ENABLE_FEATURE_UDHCP_RFC3397 1
#pragma pack(1)
struct dhcp_packet {
	uint8_t op;      /* 1 = BOOTREQUEST, 2 = BOOTREPLY */
	uint8_t htype;   /* hardware address type. 1 = 10mb ethernet */
	uint8_t hlen;    /* hardware address length */
	uint8_t hops;    /* used by relay agents only */
	uint32_t xid;    /* unique id */
	uint16_t secs;   /* elapsed since client began acquisition/renewal */
	uint16_t flags;  /* only one flag so far: */
#define BROADCAST_FLAG 0x8000 /* "I need broadcast replies" */
	uint32_t ciaddr; /* client IP (if client is in BOUND, RENEW or REBINDING state) */
	uint32_t yiaddr; /* 'your' (client) IP address */
	/* IP address of next server to use in bootstrap, returned in DHCPOFFER, DHCPACK by server */
	uint32_t siaddr_nip;
	uint32_t gateway_nip; /* relay agent IP address */
	uint8_t chaddr[16];   /* link-layer client hardware address (MAC) */
	uint8_t sname[64];    /* server host name (ASCIZ) */
	uint8_t file[128];    /* boot file name (ASCIZ) */
	uint32_t cookie;      /* fixed first four option bytes (99,130,83,99 dec) */
	uint8_t options[DHCP_OPTIONS_BUFSIZE + CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS];
} ;

struct dhcp_option {
	uint8_t flags;
	uint8_t code;
};
struct iphdr
  {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ihl:4;
    unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int version:4;
    unsigned int ihl:4;
#else
# error	"Please fix <bits/endian.h>"
#endif
    u_int8_t tos;
    u_int16_t tot_len;
    u_int16_t id;
    u_int16_t frag_off;
    u_int8_t ttl;
    u_int8_t protocol;
    u_int16_t check;
    u_int32_t saddr;
    u_int32_t daddr;
    /*The options start here. */
  };
struct udphdr
{
  u_int16_t source;
  u_int16_t dest;
  u_int16_t len;
  u_int16_t check;
};
struct ip_udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcp_packet data;
};
#define BB_BIG_ENDIAN 0

#pragma pack()
/* DHCP protocol -- see RFC 2131 */
#define DHCP_MAGIC		0x63825363

/* DHCP option codes (partial list) */
#define DHCP_PADDING            0x00
#define DHCP_SUBNET             0x01
#define DHCP_TIME_OFFSET        0x02
#define DHCP_ROUTER             0x03
#define DHCP_TIME_SERVER        0x04
#define DHCP_NAME_SERVER        0x05
#define DHCP_DNS_SERVER         0x06
#define DHCP_LOG_SERVER         0x07
#define DHCP_COOKIE_SERVER      0x08
#define DHCP_LPR_SERVER         0x09
#define DHCP_HOST_NAME          0x0c
#define DHCP_BOOT_SIZE          0x0d
#define DHCP_DOMAIN_NAME        0x0f
#define DHCP_SWAP_SERVER        0x10
#define DHCP_ROOT_PATH          0x11
#define DHCP_IP_TTL             0x17
#define DHCP_MTU                0x1a
#define DHCP_BROADCAST          0x1c
#define DHCP_NTP_SERVER         0x2a
#define DHCP_WINS_SERVER        0x2c
#define DHCP_REQUESTED_IP       0x32
#define DHCP_LEASE_TIME         0x33
#define DHCP_OPTION_OVERLOAD    0x34
#define DHCP_MESSAGE_TYPE       0x35
#define DHCP_SERVER_ID          0x36
#define DHCP_PARAM_REQ          0x37
#define DHCP_MESSAGE            0x38
#define DHCP_MAX_SIZE           0x39
#define DHCP_T1                 0x3a
#define DHCP_T2                 0x3b
#define DHCP_VENDOR             0x3c
#define DHCP_CLIENT_ID          0x3d
#define DHCP_FQDN               0x51
#define DHCP_STATIC_ROUTES      0x79
#define DHCP_END                0xFF
/* Offsets in option byte sequence */
#define OPT_CODE                0
#define OPT_LEN                 1
#define OPT_DATA                2
/* Bits in "overload" option */
#define OPTION_FIELD            0
#define FILE_FIELD              1
#define SNAME_FIELD             2


#define BOOTREQUEST             1
#define BOOTREPLY               2

#define ETH_10MB                1
#define ETH_10MB_LEN            6


#define DHCPDISCOVER            1 /* client -> server */
#define DHCPOFFER               2 /* client <- server */
#define DHCPREQUEST             3 /* client -> server */
#define DHCPDECLINE             4 /* client -> server */
#define DHCPACK                 5 /* client <- server */
#define DHCPNAK                 6 /* client <- server */
#define DHCPRELEASE             7 /* client -> server */
#define DHCPINFORM              8 /* client -> server */


typedef unsigned int  leasetime_t;
typedef int signed_leasetime_t;

struct dyn_lease {
	/* "nip": IP in network order */
	/* Unix time when lease expires. Kept in memory in host order.
	 * When written to file, converted to network order
	 * and adjusted (current time subtracted) */
	leasetime_t expires;
	uint32_t lease_nip;
	/* We use lease_mac[6], since e.g. ARP probing uses
	 * only 6 first bytes anyway. We check received dhcp packets
	 * that their hlen == 6 and thus chaddr has only 6 significant bytes
	 * (dhcp packet has chaddr[16], not [6])
	 */
	uint8_t lease_mac[6];
	char hostname[20];
	uint8_t pad[2];
	/* total size is a multiply of 4 */
} ;

#define TYPE_MASK       0x0F
#pragma pack(1)
const uint8_t dhcp_option_lengths[]  = {
	0,
	4,
	8,
	1,
	1,
	1,
	1,
	2,
	2,
	4,
	4,
	/* Just like OPTION_STRING, we use minimum length here */
	5,
};
#pragma pack()
enum {
	OPTION_IP = 1,
	OPTION_IP_PAIR,
	OPTION_STRING,
#if ENABLE_FEATURE_UDHCP_RFC3397
	OPTION_STR1035,	/* RFC1035 compressed domain name list */
#endif
	OPTION_BOOLEAN,
	OPTION_U8,
	OPTION_U16,
	OPTION_S16,
	OPTION_U32,
	OPTION_S32,
	OPTION_STATIC_ROUTES,
};
#define move_to_unaligned32(u32p, v) do { \
	uint32_t __t = (v); \
	memcpy((u32p), &__t, 4); \
} while (0)
#define move_from_unaligned32(v, u32p) (memcpy(&(v), (u32p), 4))
/* Client requests this option by default */
#define OPTION_REQ      0x10
/* There can be a list of 1 or more of these */
#define OPTION_LIST     0x20
/* Supported options are easily added here */
const struct dhcp_option dhcp_options[] = {
	/* flags                                    code */
	{ OPTION_IP                   | OPTION_REQ, 0x01 }, /* DHCP_SUBNET        */
	{ OPTION_S32                              , 0x02 }, /* DHCP_TIME_OFFSET   */
	{ OPTION_IP | OPTION_LIST     | OPTION_REQ, 0x03 }, /* DHCP_ROUTER        */
	{ OPTION_IP | OPTION_LIST                 , 0x04 }, /* DHCP_TIME_SERVER   */
	{ OPTION_IP | OPTION_LIST                 , 0x05 }, /* DHCP_NAME_SERVER   */
	{ OPTION_IP | OPTION_LIST     | OPTION_REQ, 0x06 }, /* DHCP_DNS_SERVER    */
	{ OPTION_IP | OPTION_LIST                 , 0x07 }, /* DHCP_LOG_SERVER    */
	{ OPTION_IP | OPTION_LIST                 , 0x08 }, /* DHCP_COOKIE_SERVER */
	{ OPTION_IP | OPTION_LIST                 , 0x09 }, /* DHCP_LPR_SERVER    */
	{ OPTION_STRING               | OPTION_REQ, 0x0c }, /* DHCP_HOST_NAME     */
	{ OPTION_U16                              , 0x0d }, /* DHCP_BOOT_SIZE     */
	{ OPTION_STRING | OPTION_LIST | OPTION_REQ, 0x0f }, /* DHCP_DOMAIN_NAME   */
	{ OPTION_IP                               , 0x10 }, /* DHCP_SWAP_SERVER   */
	{ OPTION_STRING                           , 0x11 }, /* DHCP_ROOT_PATH     */
	{ OPTION_U8                               , 0x17 }, /* DHCP_IP_TTL        */
	{ OPTION_U16                              , 0x1a }, /* DHCP_MTU           */
	{ OPTION_IP                   | OPTION_REQ, 0x1c }, /* DHCP_BROADCAST     */
	{ OPTION_STRING                           , 0x28 }, /* nisdomain          */
	{ OPTION_IP | OPTION_LIST                 , 0x29 }, /* nissrv             */
	{ OPTION_IP | OPTION_LIST     | OPTION_REQ, 0x2a }, /* DHCP_NTP_SERVER    */
	{ OPTION_IP | OPTION_LIST                 , 0x2c }, /* DHCP_WINS_SERVER   */
	{ OPTION_IP                               , 0x32 }, /* DHCP_REQUESTED_IP  */
	{ OPTION_U32                              , 0x33 }, /* DHCP_LEASE_TIME    */
	{ OPTION_U8                               , 0x35 }, /* dhcptype           */
	{ OPTION_IP                               , 0x36 }, /* DHCP_SERVER_ID     */
	{ OPTION_STRING                           , 0x38 }, /* DHCP_MESSAGE       */
	{ OPTION_STRING                           , 0x3C }, /* DHCP_VENDOR        */
	{ OPTION_STRING                           , 0x3D }, /* DHCP_CLIENT_ID     */
	{ OPTION_STRING                           , 0x42 }, /* tftp               */
	{ OPTION_STRING                           , 0x43 }, /* bootfile           */
	{ OPTION_STRING                           , 0x4D }, /* userclass          */
#if ENABLE_FEATURE_UDHCP_RFC3397
	{ OPTION_STR1035 | OPTION_LIST            , 0x77 }, /* search             */
#endif
	{ OPTION_STATIC_ROUTES                    , 0x79 }, /* DHCP_STATIC_ROUTES */
	/* MSIE's "Web Proxy Autodiscovery Protocol" support */
	{ OPTION_STRING                           , 0xfc }, /* wpad               */

	/* Options below have no match in dhcp_option_strings[],
	 * are not passed to dhcpc scripts, and cannot be specified
	 * with "option XXX YYY" syntax in dhcpd config file. */

	{ OPTION_U16                              , 0x39 }, /* DHCP_MAX_SIZE      */
	//{ } /* zeroed terminating entry */
};

struct static_lease {
	struct static_lease *next;
	uint32_t nip;
	uint8_t mac[6];
};



#define IPVERSION	4
#define MAXTTL		255
#define IPDEFTTL	64

#define SOCKET_ERROR  -1
#define INVALID_SOCKET (-1)

struct static_lease *g_st_lease=NULL;
const int SERVER_PORT = 67;
const int CLIENT_PORT = 68;

#define STOP_UDHCP  0x55

int ip_alloc_start=128; 	//x.x.x.128
int ip_alloc_end=254; 	//x.x.x.254

char server_ip_address[64]="0.0.0.0";
//int server_ip_address;
char eth_ip_address[64]="0.0.0.0";
char sub_net[]="255.255.255.0";
unsigned char server_mac[6] = {0};

#define MAX_LEASE_TIME  24*3600 // one day

char WIFI_ADAPTER_INTERFACE[8];//="p2p0"

static int udhcp_state=0;

int g_SwitchDebugInf=1;

#define DNS_INIT  "/usr/local/etc/dns/dns.sh"

#include <sys/wait.h> 


static bool find_dns_server(uint32_t *pdns1,uint32_t *pdns2)
{
	*pdns1=inet_addr(server_ip_address);
	*pdns2=0;
	return true;
}

static void free_static_lease(struct static_lease *st_lease_pp,
	struct static_lease *st_lease_node)
{
	struct static_lease *st_lease;

	while ((st_lease = st_lease_pp) != NULL) {
		if(st_lease->next == st_lease_node)
		{
			st_lease->next=st_lease_node->next;
			free(st_lease_node);
			break;
		}
		st_lease_pp =st_lease->next;
	}

}
static void  add_static_lease(struct static_lease *st_lease_pp,
		uint8_t *mac,
		uint32_t nip)
{
	struct static_lease *st_lease;

	/* Find the tail of the list */
	while (st_lease_pp->next != NULL) {
		st_lease_pp = st_lease_pp->next;
	}

	/* Add new node */
	st_lease = (struct static_lease *)malloc(sizeof(*st_lease));
	memcpy(st_lease->mac, mac, 6);
	st_lease->nip = nip;
	st_lease->next = NULL;
	st_lease_pp->next=st_lease;

}
/* Find static lease IP by mac */
static uint32_t  get_static_nip_by_mac(struct static_lease *st_lease, void *mac)
{
	//char* cMac = (char*)mac;
	if (g_SwitchDebugInf)
	{
		//;
	}
	while (st_lease) {
		if (memcmp(st_lease->mac, mac, 6) == 0)
			return st_lease->nip;
		st_lease = st_lease->next;
	}

	return 0;

}
static struct static_lease*  get_static_lease_by_mac(struct static_lease *st_lease, void *mac)
{
	while (st_lease) {
		if (memcmp(st_lease->mac, mac, 6) == 0)
			return st_lease;
		st_lease = st_lease->next;
	}

	return NULL;

}
static int  is_nip_reserved(struct static_lease *st_lease, uint32_t nip)
{
	while (st_lease) {
		if (st_lease->nip == nip)
			return 1;
		st_lease = st_lease->next;
	}

	return 0;
}
static char *myitoa(int value,char *string,int radix)
{
   int rt=0;
   if(string==NULL)
      return NULL;
   if(radix<=0||radix>30)
      return NULL;
   rt=snprintf(string,radix,"%d",value);
   if(rt>radix)
     return NULL;
   string[rt]='\0';
   return string;
}
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <sys/poll.h>
#include <errno.h>

struct arpMsg {
	/* Ethernet header */
	uint8_t  h_dest[6];     /* 00 destination ether addr */
	uint8_t  h_source[6];   /* 06 source ether addr */
	uint16_t h_proto;       /* 0c packet type ID field */

	/* ARP packet */
	uint16_t htype;         /* 0e hardware type (must be ARPHRD_ETHER) */
	uint16_t ptype;         /* 10 protocol type (must be ETH_P_IP) */
	uint8_t  hlen;          /* 12 hardware address length (must be 6) */
	uint8_t  plen;          /* 13 protocol address length (must be 4) */
	uint16_t operation;     /* 14 ARP opcode */
	uint8_t  sHaddr[6];     /* 16 sender's hardware address */
	uint8_t  sInaddr[4];    /* 1c sender's IP address */
	uint8_t  tHaddr[6];     /* 20 target's hardware address */
	uint8_t  tInaddr[4];    /* 26 target's IP address */
	uint8_t  pad[18];       /* 2a pad for min. ethernet payload (60 bytes) */
} PACKED;

enum {
	ARP_MSG_SIZE = 0x2a
};
#if ENABLE_MONOTONIC_SYSCALL
#include <sys/syscall.h>
/* Old glibc (< 2.3.4) does not provide this constant. We use syscall
 * directly so this definition is safe. */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* libc has incredibly messy way of doing this,
 * typically requiring -lrt. We just skip all this mess */
static void get_mono(struct timespec *ts)
{
	if (syscall(__NR_clock_gettime, CLOCK_MONOTONIC, ts))
		printf("clock_gettime(MONOTONIC) failed");
}
unsigned long long  monotonic_us(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000000ULL + ts.tv_nsec/1000;
}
#else
static unsigned long long monotonic_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}
#endif

static char*  safe_strncpy(char *dst, const char *src, size_t size)
{
	if (!size) return dst;
	dst[--size] = '\0';
	return strncpy(dst, src, size);
}
static int  safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout)
{
	while (1) {
		int n = poll(ufds, nfds, timeout);
		if (n >= 0)
			return n;
		/* Make sure we inch towards completion */
		if (timeout > 0)
			timeout--;
		/* E.g. strace causes poll to return this */
		if (errno == EINTR)
			continue;
		/* Kernel is very low on memory. Retry. */
		/* I doubt many callers would handle this correctly! */
		if (errno == ENOMEM)
			continue;
		printf("poll");
		return n;
	}
}

static int  arpping(uint32_t test_nip,
		const uint8_t *safe_mac,
		uint32_t from_ip,
		uint8_t *from_mac,
		const char *interface)
{
	int timeout_ms;
	struct pollfd pfd[1];
#define s (pfd[0].fd)           /* socket */
	int rv = 1;             /* "no reply received" yet */
	struct sockaddr addr;   /* for interface name */
	struct arpMsg arp;
	int const_int_1= 1;


	s = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP));
	if (s == -1) {
		printf("bb_msg_can_not_create_raw_socket \r\n");
		return -1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&const_int_1, sizeof(const_int_1))== -1) {
		printf("cannot enable bcast on raw socket \r\n");
		goto ret;
	}

	/* send arp request */
	memset(&arp, 0, sizeof(arp));
	memset(arp.h_dest, 0xff, 6);                    /* MAC DA */
	memcpy(arp.h_source, from_mac, 6);              /* MAC SA */
	arp.h_proto = htons(ETH_P_ARP);                 /* protocol type (Ethernet) */
	arp.htype = htons(ARPHRD_ETHER);                /* hardware type */
	arp.ptype = htons(ETH_P_IP);                    /* protocol type (ARP message) */
	arp.hlen = 6;                                   /* hardware address length */
	arp.plen = 4;                                   /* protocol address length */
	arp.operation = htons(ARPOP_REQUEST);           /* ARP op code */
	memcpy(arp.sHaddr, from_mac, 6);                /* source hardware address */
	memcpy(arp.sInaddr, &from_ip, sizeof(from_ip)); /* source IP address */
	/* tHaddr is zero-filled */                     /* target hardware address */
	memcpy(arp.tInaddr, &test_nip, sizeof(test_nip));/* target IP address */

	memset(&addr, 0, sizeof(addr));
	safe_strncpy(addr.sa_data, interface, sizeof(addr.sa_data));
	if (sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0) {
		// TODO: error message? caller didn't expect us to fail,
		// just returning 1 "no reply received" misleads it.
		goto ret;
	}

	/* wait for arp reply, and check it */
	timeout_ms = 2000;
	do {
		int r;
		unsigned prevTime = monotonic_us();

		pfd[0].events = POLLIN;
		r = safe_poll(pfd, 1, timeout_ms);
		if (r < 0)
			break;
		if (r) {
			r = read(s, &arp, sizeof(arp));
			if (r < 0)
				break;

			//log3("sHaddr %02x:%02x:%02x:%02x:%02x:%02x",
			//	arp.sHaddr[0], arp.sHaddr[1], arp.sHaddr[2],
			//	arp.sHaddr[3], arp.sHaddr[4], arp.sHaddr[5]);

			if (r >= ARP_MSG_SIZE
			 && arp.operation == htons(ARPOP_REPLY)
			 /* don't check it: Linux doesn't return proper tHaddr (fixed in 2.6.24?) */
			 /* && memcmp(arp.tHaddr, from_mac, 6) == 0 */
			 && *((uint32_t *) arp.sInaddr) == test_nip
			) {
				/* if ARP source MAC matches safe_mac
				 * (which is client's MAC), then it's not a conflict
				 * (client simply already has this IP and replies to ARPs!)
				 */
				if (!safe_mac || memcmp(safe_mac, arp.sHaddr, 6) != 0)
					rv = 0;
				//else log2("sHaddr == safe_mac");
				break;
			}
		}
		timeout_ms -= ((unsigned)monotonic_us() - prevTime) / 1000;
	} while (timeout_ms > 0);

 ret:
	close(s);
	printf("%srp reply received for this address", rv ? "No a" : "A");
	return rv;
}

static int nobody_responds_to_arp(uint32_t nip, const uint8_t *safe_mac)
{
	/* 16 zero bytes */
	//static const uint8_t blank_chaddr[16] = { 0 };
	/* = { 0 } helps gcc to put it in rodata, not bss */

	struct in_addr temp;
	int r;

	r = arpping(nip, safe_mac,
			inet_addr(server_ip_address),
			server_mac,
			WIFI_ADAPTER_INTERFACE);
	if (r)
		return r;

	temp.s_addr = nip;
	//printf("%s belongs to someone, reserving it for %u seconds",
	//	inet_ntoa(temp), (unsigned)server_config.conflict_time);
	//add_lease(blank_chaddr, nip, server_config.conflict_time, NULL, 0);
	return 0;
}
static uint32_t  find_free_or_expired_nip(const uint8_t *safe_mac)
{
	uint32_t addr;
	//struct dyn_lease *oldest_lease = NULL;
	//char *temp_ip[64];
	char ipaddress_base[64];
	char ip_last[8];
	int current;
	int count=0;

	//printf("find_free_or_expired_nip ++  \r\n");

	strcpy(ipaddress_base,server_ip_address);

	char *temp_address=ipaddress_base;
	while((temp_address = strchr(temp_address+1,'.')) != NULL && count++<2);
	temp_address++;
	current=atoi(temp_address);
	*temp_address='\0';

	//printf("current = %d \r\n",current);
	char ipaddress[64];


	addr = ip_alloc_start; /* addr is in host order here */
	for (; addr <= (uint32_t)ip_alloc_end; addr++) {
		uint32_t nip;
		myitoa(addr,ip_last,8);
		strcpy(ipaddress,ipaddress_base);
		strcat(ipaddress,ip_last);
		//printf("ipaddress = %s  \r\n",ipaddress);

		//struct dyn_lease *lease;
		nip=inet_addr(ipaddress);
		/* ie, 192.168.10.0 */
		if ((addr & 0xff) == 0)
			continue;
		/* ie, 192.168.10.255 */
		if ((addr & 0xff) == 0xff)
			continue;

		if(nip ==inet_addr(server_ip_address))
			continue;

		//nip = htonl(addr_ip);
		//printf("nip 0x%x \r\n",nip);
		/* is this a static lease addr? */
		if (is_nip_reserved(g_st_lease, nip))
			continue;

		if(!nobody_responds_to_arp(nip,safe_mac))
			continue;

		return nip;
	}


	return 0;
}

static uint8_t*  get_option(struct dhcp_packet *packet, int code)
{
	uint8_t *optionptr;
	int len;
	int rem;
	int overload = 0;
	enum {
		FILE_FIELD101  = FILE_FIELD  * 0x101,
		SNAME_FIELD101 = SNAME_FIELD * 0x101,
	};

	/* option bytes: [code][len][data1][data2]..[dataLEN] */
	optionptr = packet->options;
	rem = sizeof(packet->options);
	while (1) {
		if (rem <= 0) {
			if (g_SwitchDebugInf)
			{
				printf("bogus packet, malformed option field");
			}
			return NULL;
		}
		if (optionptr[OPT_CODE] == DHCP_PADDING) {
			rem--;
			optionptr++;
			continue;
		}
		if (optionptr[OPT_CODE] == DHCP_END) {
			if ((overload & FILE_FIELD101) == FILE_FIELD) {
				/* can use packet->file, and didn't look at it yet */
				overload |= FILE_FIELD101; /* "we looked at it" */
				optionptr = packet->file;
				rem = sizeof(packet->file);
				continue;
			}
			if ((overload & SNAME_FIELD101) == SNAME_FIELD) {
				/* can use packet->sname, and didn't look at it yet */
				overload |= SNAME_FIELD101; /* "we looked at it" */
				optionptr = packet->sname;
				rem = sizeof(packet->sname);
				continue;
			}
			break;
		}
		len = 2 + optionptr[OPT_LEN];
		rem -= len;
		if (rem < 0)
			continue; /* complain and return NULL */

		if (optionptr[OPT_CODE] == code) {
		//	printf("Option found", optionptr);
			return optionptr + OPT_DATA;
		}

		if (optionptr[OPT_CODE] == DHCP_OPTION_OVERLOAD) {
			overload |= optionptr[OPT_DATA];
			/* fall through */
		}
		optionptr += len;
	}

	/* log3 because udhcpc uses it a lot - very noisy */
	//printf("Option 0x%02x not found", code);
	return NULL;
}
/* return the position of the 'end' option (no bounds checking) */
static int  end_option(uint8_t *optionptr)
{
	int i = 0;

	while (optionptr[i] != DHCP_END) {
		if (optionptr[i] != DHCP_PADDING)
			i += optionptr[i + OPT_LEN] + 1;
		i++;
	}
	return i;
}
static int  add_option_string2(uint8_t *optionptr, uint8_t *string)
{
	int end = end_option(optionptr);

	/* end position + string length + option code/length + end option */
	if (end + string[OPT_LEN] + 2 + 1 >= DHCP_OPTIONS_BUFSIZE) {
		if (g_SwitchDebugInf)
		{
			printf("option 0x%02x did not fit into the packet",
				string[OPT_CODE]);
		}
		return 0;
	}
	//printf("Adding option", string);
	memcpy(optionptr + end, string, string[OPT_LEN] + 2);
	optionptr[end + string[OPT_LEN] + 6] = DHCP_END;

	return string[OPT_LEN] + 6;
}
static int  add_simple_option2(uint8_t *optionptr, uint8_t code, uint32_t data1,uint32_t data2)
{
	const struct dhcp_option *dh;

	for (dh = dhcp_options; dh->code; dh++) {
		if (dh->code == code) {
			uint8_t option[10], len;

			option[OPT_CODE] = code;
			len = 8;
			option[OPT_LEN] = len;
			//if (BB_BIG_ENDIAN)
			//	data1 <<= 8 * (4 - len);

			/* Assignment is unaligned! */
			move_to_unaligned32(&option[OPT_DATA], data1);
			move_to_unaligned32(&option[OPT_DATA]+4, data2);
			return add_option_string2(optionptr, option);
		}
	}
	if (g_SwitchDebugInf)
	{
		printf("cannot add option 0x%02x", code);
	}
	return 0;
}
/* add an option string to the options */
/* option bytes: [code][len][data1][data2]..[dataLEN] */
static int  add_option_string(uint8_t *optionptr, uint8_t *string)
{
	int end = end_option(optionptr);

	/* end position + string length + option code/length + end option */
	if (end + string[OPT_LEN] + 2 + 1 >= DHCP_OPTIONS_BUFSIZE) {
		if (g_SwitchDebugInf)
		{
			printf("option 0x%02x did not fit into the packet",
				string[OPT_CODE]);
		}
		return 0;
	}
	//printf("Adding option", string);
	memcpy(optionptr + end, string, string[OPT_LEN] + 2);

	optionptr[end + string[OPT_LEN] + 2] = DHCP_END;

	return string[OPT_LEN] + 2;
}

static int  add_simple_option(uint8_t *optionptr, uint8_t code, uint32_t data)
{
	const struct dhcp_option *dh;

	for (dh = dhcp_options; dh->code; dh++) {
		if (dh->code == code) {
			uint8_t option[6], len;

			option[OPT_CODE] = code;
			len = dhcp_option_lengths[dh->flags & TYPE_MASK];
			option[OPT_LEN] = len;
			if (BB_BIG_ENDIAN)
				data <<= 8 * (4 - len);
			/* Assignment is unaligned! */
			move_to_unaligned32(&option[OPT_DATA], data);
			return add_option_string(optionptr, option);
		}
	}
	if (g_SwitchDebugInf)
	{
		printf("cannot add option 0x%02x", code);
	}
	return 0;
}

static void  udhcp_init_header(struct dhcp_packet *packet, char type)
{
	memset(packet, 0, sizeof(struct dhcp_packet));
	packet->op = BOOTREQUEST; /* if client to a server */
	switch (type) {
	case DHCPOFFER:
	case DHCPACK:
	case DHCPNAK:
		packet->op = BOOTREPLY; /* if server to client */
	}
	packet->htype = ETH_10MB;
	packet->hlen = ETH_10MB_LEN;
	packet->cookie = htonl(DHCP_MAGIC);
	packet->options[0] = DHCP_END;
	add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
}
static void add_bootp_options(struct dhcp_packet *packet)
{
	packet->siaddr_nip = inet_addr(server_ip_address);
}

static void init_packet(struct dhcp_packet *packet, struct dhcp_packet *oldpacket, char type)
{
	udhcp_init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, sizeof(oldpacket->chaddr));
	packet->flags = oldpacket->flags;
	packet->gateway_nip = oldpacket->gateway_nip;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID,inet_addr(server_ip_address));
}



static int  udhcp_send_raw_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port, const uint8_t *dest_arp,
		int ifindex)
{
	struct sockaddr_in dest;
	//struct ip_udp_dhcp_packet packet;
	int fd;
	int result = -1;
	//const char *msg;
	int const_int_1= 1;
	(void)source_ip;
	(void)source_port;
	(void)dest_ip;
	(void)dest_port;
	(void)ifindex;
	(void)dest_arp;

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (fd < 0) {
		if (g_SwitchDebugInf)
		{
			printf("socket error\r\n");
		}
		return -1;
	}
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&const_int_1, sizeof(const_int_1));

	if(SOCKET_ERROR == setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (char *)&const_int_1, sizeof(int)))
	{
		if (g_SwitchDebugInf)
		{
			printf("SO_BROADCAST error\r\n");
		}
		return -2;
	}

	//const_int_1 = 1234;
    //setsockopt(fd, SOL_SOCKET, 0x8000, (char *)&const_int_1, sizeof(const_int_1));

	memset(&dest, 0, sizeof(dest));

	dest.sin_family = AF_INET;
	dest.sin_port = htons (SERVER_PORT);
	dest.sin_addr.s_addr = inet_addr(server_ip_address);

	if (bind(fd, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
		return -1;
	}

	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons (CLIENT_PORT);
	dest.sin_addr.s_addr = INADDR_BROADCAST;

	int retryCount=5;
//	DWORD uByteSent=0;
retry_send:

	result = sendto(fd, (char *)dhcp_pkt, 590-42, MSG_DONTROUTE ,
				(struct sockaddr *) &dest, sizeof(dest));

	if (retryCount-- > 0) {

		//printf(" sendto PACKET \r\n");
		goto retry_send;
	}

	close(fd);
	return result;
}
static int send_packet_to_client(struct dhcp_packet *dhcp_pkt, int force_broadcast)
{
	const uint8_t *chaddr;
	uint32_t ciaddr;
	(void)force_broadcast;
	ciaddr = dhcp_pkt->ciaddr;
	chaddr = dhcp_pkt->chaddr;

	return udhcp_send_raw_packet(dhcp_pkt,
		/*src*/ inet_addr(server_ip_address), SERVER_PORT,
		/*dst*/ ciaddr, CLIENT_PORT, chaddr,
		1);
}
static int send_packet(struct dhcp_packet *dhcp_pkt, int force_broadcast)
{

	return send_packet_to_client(dhcp_pkt, force_broadcast);
}
/*
static uint32_t select_lease_time(struct dhcp_packet *packet)
{
	uint32_t lease_time_sec = MAX_LEASE_TIME;
	uint8_t *lease_time_opt = get_option(packet, DHCP_LEASE_TIME);
	if (lease_time_opt) {
		move_from_unaligned32(lease_time_sec, lease_time_opt);
		lease_time_sec = ntohl(lease_time_sec);
		if (lease_time_sec >MAX_LEASE_TIME)
			lease_time_sec = MAX_LEASE_TIME;
		if (lease_time_sec < MAX_LEASE_TIME)
			lease_time_sec = MAX_LEASE_TIME;
	}
	return lease_time_sec;
}
*/
static int  send_offer(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;
//	uint32_t req_nip;
	uint32_t lease_time_sec = MAX_LEASE_TIME;//server_config.max_lease_sec;
	uint32_t static_lease_ip;
//	uint8_t *req_ip_opt;
	//const char *p_host_name;
	//struct option_set *curr;
	struct in_addr addr;
	uint32_t dns_server_1,dns_server_2;
	//printf("send_offer ++ \r\n");
	init_packet(&packet, oldpacket, DHCPOFFER);

	static_lease_ip = get_static_nip_by_mac(g_st_lease, oldpacket->chaddr);

	/* ADDME: if static, short circuit */

	if (!static_lease_ip) {
		static_lease_ip=find_free_or_expired_nip(oldpacket->chaddr);
		add_static_lease(g_st_lease,oldpacket->chaddr,static_lease_ip);
		packet.yiaddr = static_lease_ip;
	} else if(static_lease_ip == inet_addr(server_ip_address))
	{
		static_lease_ip=find_free_or_expired_nip(oldpacket->chaddr);
		free_static_lease(g_st_lease,get_static_lease_by_mac(g_st_lease,oldpacket->chaddr));
		add_static_lease(g_st_lease,oldpacket->chaddr,static_lease_ip);
		packet.yiaddr = static_lease_ip;
	}
	else {
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}

	//lease_time_sec=select_lease_time(oldpacket);

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_sec));
	add_simple_option(packet.options, DHCP_SUBNET, inet_addr(sub_net));
	add_simple_option(packet.options, DHCP_ROUTER,inet_addr(server_ip_address));
	if(find_dns_server(&dns_server_1,&dns_server_2))
	{
		if(dns_server_1 && dns_server_2)
		{
			add_simple_option2(packet.options, DHCP_DNS_SERVER,dns_server_1,dns_server_2);
		}else if(dns_server_1)
		{
			add_simple_option(packet.options, DHCP_DNS_SERVER,dns_server_1);
		}else if(dns_server_2)
		{
			add_simple_option(packet.options, DHCP_DNS_SERVER,dns_server_2);
		}
	}else
	{
		add_simple_option(packet.options, DHCP_DNS_SERVER,inet_addr(server_ip_address));
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
	if (g_SwitchDebugInf)
	{
		printf("Sending OFFER of %s\r\n", inet_ntoa(addr));
	}
		
	return send_packet(&packet, 0);
}


static int  send_ACK(struct dhcp_packet *oldpacket, uint32_t yiaddr)
{
	struct dhcp_packet packet;
	//struct option_set *curr;
	uint32_t lease_time_sec=MAX_LEASE_TIME;
	struct in_addr addr;
	//const char *p_host_name;
	uint32_t dns_server_1,dns_server_2;

	init_packet(&packet, oldpacket, DHCPACK);
	packet.yiaddr = yiaddr;
	//lease_time_sec=select_lease_time(oldpacket);
	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_sec));
	add_simple_option(packet.options, DHCP_SUBNET, inet_addr(sub_net));
	add_simple_option(packet.options, DHCP_ROUTER, inet_addr(server_ip_address));
	if(find_dns_server(&dns_server_1,&dns_server_2))
	{
		if(dns_server_1 && dns_server_2)
		{
			add_simple_option2(packet.options, DHCP_DNS_SERVER,dns_server_1,dns_server_2);
		}else if(dns_server_1)
		{
			add_simple_option(packet.options, DHCP_DNS_SERVER,dns_server_1);
		}else if(dns_server_2)
		{
			add_simple_option(packet.options, DHCP_DNS_SERVER,dns_server_2);
		}
	}else
	{
		add_simple_option(packet.options, DHCP_DNS_SERVER,inet_addr(server_ip_address));
	}
	
	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
	if (g_SwitchDebugInf)
	{
		printf("Sending ACK to %s\r\n", inet_ntoa(addr));
	}

	char chaddr[32]={0};
     sprintf(chaddr,"%02x:%02x:%02x:%02x:%02x:%02x",
       	packet.chaddr[0],packet.chaddr[1],
       	packet.chaddr[2],packet.chaddr[3],
        packet.chaddr[4],packet.chaddr[5]);
	printf("add %s \r\n",chaddr);
	
	//huaijing 
	//if(strcmp(chaddr,APNetWork::p2p_get_peer_addr()) == 0)
	//{
	//	printf("p2p_ip_leases \r\n");
	//	APNetWork::p2p_setip_addr(addr.s_addr);
	//	
	//}
		
	if (send_packet(&packet, 0) < 0)
		return -1;


	return 0;
}
static int  send_NAK(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;

	init_packet(&packet, oldpacket, DHCPNAK);
	if (g_SwitchDebugInf)
	{
		printf("Sending NAK\r\n");
	}
	return send_packet(&packet, 1);
}
static int GetLocalIP(char *adaptername)
{
	int sk;
	struct ifreq ifr;
	struct sockaddr_in *sin;

	sk=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
	if(sk == -1)
	{
		return -1;
	}

	memset(&ifr,0,sizeof(ifr));

	strcpy(ifr.ifr_name,adaptername);

	if(ioctl(sk,SIOCGIFADDR,&ifr)== -1)
	{
		close(sk);
		return -1;
	}

	sin =(struct sockaddr_in *)&ifr.ifr_addr;

	close(sk);

	return sin->sin_addr.s_addr;
}

static bool GetWiFiIPAddress(char *pIpadress)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = GetLocalIP(WIFI_ADAPTER_INTERFACE);
	strcpy(pIpadress,inet_ntoa(addr.sin_addr));

	return true;
}
static bool GetWiFiIPMAC(unsigned char *mac)
{
	int fd;
	struct ifreq ifr;
	//struct sockaddr_in *our_ip;

	fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if(fd == -1)
	{
		printf("%s %d \r\n",__FUNCTION__,__LINE__);
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name,WIFI_ADAPTER_INTERFACE);

	if (mac) {
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) != 0) {
			close(fd);
			return -1;
		}
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
		printf("MAC %02x:%02x:%02x:%02x:%02x:%02x \r\n",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
	return true;
}


static int  send_inform(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;
//	struct option_set *curr;

	init_packet(&packet, oldpacket, DHCPACK);
	add_bootp_options(&packet);

	return send_packet(&packet, 0);
}


static int udhcpd_thread(void)
{
	const int const_int_1 = 1;
	struct dhcp_packet packet;
	struct static_lease * lease=NULL;
	int sock;
	printf("dhcp thread start +++++++\r\n");
	//GetEthernetMac((char*)eth_mac);
	//GetEthernetIPAddress(eth_ip_address);

	if(g_st_lease == NULL)
	{
		g_st_lease = (struct static_lease *)malloc(sizeof(struct static_lease));
		g_st_lease->next=NULL;
	}

	if(!GetWiFiIPAddress(server_ip_address))
	{
		printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!dhcpd get ip address error \r\n");
		return -15;
	}

	if(!GetWiFiIPMAC(server_mac))
	{
		printf("!!!!!!!!!!!!!!!!!!!!!!!dhcpd get mac address error \r\n");
		return -16;
	}


	sock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == INVALID_SOCKET)
	{
		printf("!!!!!!!!!!!!!!!!!!!!!dhcpd socket error \r\n");
		return -1;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&const_int_1, sizeof(const_int_1));

	if(SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&const_int_1, sizeof(int)))
	{
		printf("!!!!!!!!!!!!!!!!!!!dhcpd SO_BROADCAST error\r\n");
		close(sock);
		return -2;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	/* addr.sin_addr.s_addr = ip; - all-zeros is INADDR_ANY */
	if(SOCKET_ERROR == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
	{
		printf("!!!!!!!!!!!!!!!!!!!dhcpd bind error\r\n");
		close(sock);
		return -3;
	}

	//char buf=0;
	//int count =1;

	while(udhcp_state)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		int ret=select(sock + 1, &rfds, NULL, NULL,&tv);
		if(!udhcp_state)
		{
			break;
		}	

		if(ret <0)
		{
			printf("!!!!!!!!!!!!!!!!! dhcpd select error\r\n");
			usleep(10000);
			continue;
		}
		if(ret == 0)
		{
			usleep(10000);
			continue;
		}

		int bytes=recv(sock, (char *)&packet, sizeof(packet),0);
		if (bytes < 0)
		{
			/* bytes can also be -2 ("bad packet data") */
			if (bytes < 0 )
				printf("!!!!!!!!!!!!!!!!!!!!!dhcpd recv error  \r\n");
			if (g_SwitchDebugInf)
			{
				printf("recv bytes=%d  \r\n",bytes);
			}
			usleep(10000);
			continue;
		}

		if (packet.hlen != 6)
		{
			//if (g_SwitchDebugInf)
				printf("MAC length != 6, ignoring packet \r\n");
			usleep(10000);
			continue;
		}

		lease = get_static_lease_by_mac(g_st_lease, &packet.chaddr);
	    if(lease && (lease->nip ==inet_addr(server_ip_address)))
		{
			lease->nip =find_free_or_expired_nip(packet.chaddr);
			free_static_lease(g_st_lease,get_static_lease_by_mac(g_st_lease,packet.chaddr));
			add_static_lease(g_st_lease,packet.chaddr,lease->nip);
	  	}

		uint8_t *state = get_option(&packet, DHCP_MESSAGE_TYPE);
		if (state == NULL)
		{
			//if (g_SwitchDebugInf)
				printf("no message type option, ignoring packet \r\n");
			usleep(10000);
			continue;
		}

		
		switch (state[0])
		{
		case DHCPDISCOVER:
			if (g_SwitchDebugInf)
			{
				printf("Received DISCOVER \r\n");
			}

			if (send_offer(&packet) < 0)
			{
				if (g_SwitchDebugInf)
				{
					printf("send OFFER failed \r\n");
				}
			}
			break;
		case DHCPREQUEST:
			{
			uint8_t *server_id_opt, *requested_opt;
			uint32_t server_id_net = server_id_net; /* for compiler */
			uint32_t requested_nip = requested_nip; /* for compiler */
			if (g_SwitchDebugInf)
			{
				printf("Received REQUEST \r\n");
			}
			requested_opt = get_option(&packet, DHCP_REQUESTED_IP);
			server_id_opt = get_option(&packet, DHCP_SERVER_ID);
			if (requested_opt)
				move_from_unaligned32(requested_nip, requested_opt);
			if (server_id_opt)
				move_from_unaligned32(server_id_net, server_id_opt);

			if (lease) {
				if (server_id_opt) {
					/* SELECTING State */
					if (requested_opt
					 && requested_nip == lease->nip
					) {
						//printf("server_id_opt \r\n");
						send_ACK(&packet, lease->nip);
					}
				} else if (requested_opt) {
					/* INIT-REBOOT State */
					if (lease->nip == requested_nip)
					{
						//printf("server_id_opt 11 \r\n");
						send_ACK(&packet, lease->nip);
					}
					else
					{
						//printf("server_id_opt 22 \r\n");
						send_NAK(&packet);
					}
				} else if (lease->nip == packet.ciaddr) {
					/* RENEWING or REBINDING State */
					//printf("server_id_opt 33 \r\n");
					send_ACK(&packet, lease->nip);
				} else { /* don't know what to do!!!! */
					//printf("server_id_opt 44 \r\n");
					send_NAK(&packet);
				}

			/* what to do if we have no record of the client */
			}else
			{
				//printf("lease no \r\n");
				//send_ACK(&packet,find_free_or_expired_nip(packet.chaddr));

				if (send_offer(&packet) < 0)
				{
					if (g_SwitchDebugInf)
					{
						printf("send OFFER failed \r\n");
					}
				}
			}

			break;
		}
		case DHCPDECLINE:
			//printf("Received DECLINE  \r\n");
			if (lease) {
				free_static_lease(g_st_lease,lease);
			}
			break;
		case DHCPRELEASE:
			//printf("Received RELEASE \r\n");
			if (lease) {
				free_static_lease(g_st_lease,lease);
			}
			break;
		case DHCPINFORM:
			//printf("Received INFORM");
			send_inform(&packet);
			break;
		default:
			if (g_SwitchDebugInf)
			{
				printf("Unsupported DHCP message (%02x) - ignoring \r\n", state[0]);
			}
			break;
		}
	}

	close(sock);

	printf("************dhcpd thread end ---------------\n");
	udhcp_state = 0;
	return 0;
}

static void * udhcpd_thread_linux(void *args)
{
	(void)args;
	udhcpd_thread();
	
	return NULL;
}

static int udhcpd_start(char *netinterface)
{
	if(udhcp_state)
	{
		return 1;
	}
	
	strcpy(WIFI_ADAPTER_INTERFACE,netinterface);
	
	//printf("cmd %s \r\n",DNS_INIT);
	//system(DNS_INIT);
	
	udhcp_state = 1;
	
    pthread_t   pudhcpthread;
    pthread_attr_t  attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int gudhcpret = pthread_create(&pudhcpthread, &attr, udhcpd_thread_linux, (void *)NULL);
    if (gudhcpret != 0)
        printf("************AbstractAP: Create Gtalk Monitor thread failed!\n");

	
	printf("************dhcpd start\n");

	return 1;
}

static int udhcpd_stop(void)
{
	udhcp_state = 0;
	printf("************dhcpd stop\n");
	usleep(1000000);
	return 1;
}

//int udhcpd_status()
//{
//    return udhcp_state;
//}
