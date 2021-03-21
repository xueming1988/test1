#ifndef _COMMON_H
#define _COMMON_H

#define die(x) { \
    fprintf(stderr, "FATAL: %s\n", x); \
    exit(1); \
}

typedef struct {
	int delay;
	int latency;
} shairport_cfg;

// Define Latency baseline time
#define LATENCY_BASELINE_S	2	// Default latency figure normally provided by iTunes (in seconds)
#define LATENCY_BASELINE_US	2000000 // and in U seconds

// Table of compaitble device types and associated charecteristics

typedef struct {
	char *name;			// User-Agent name in RECORD statement
	int  version;		// Min version supported, higher versions must appear first in list
	int  clean_start;		// First audio packet sent is as identified in RTP-Info statement
	int  fast_start;		// First audio timestamp does NOT require adjustment
} device_type;

#define NDEVICES        6

#define NOSYNC 0
#define NTPSYNC 1
#define RTPSYNC 2
#define E_NTPSYNC 3

typedef struct {
    long long ntp_tsp;
    unsigned long rtp_tsp;
    int sync_mode;
} sync_cfg;


//player states
#define BUFFERING 0
#define SYNCING   1
#define PLAYING   2
int state;

//buffer states
#define SIGNALLOSS 0
#define UNSYNC 1
#define INSYNC 2

#ifndef LLONG_MIN
#define LLONG_MIN  (-__LONG_LONG_MAX__ - 1)
#endif

#define NTPCACHESIZE 12
#define NTPRANGE     5000LL
#define NTPSKIPLIMIT 20

#endif

