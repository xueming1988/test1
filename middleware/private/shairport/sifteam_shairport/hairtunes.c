/*
 * HairTunes - RAOP packet handler and slave-clocked replay engine
 * Copyright (c) James Laird 2011
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <openssl/aes.h>
#include <math.h>
#include <sys/stat.h>
#include <signal.h>

#include "hairtunes.h"
#include <sys/signal.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdarg.h>
#include <errno.h>

#include "mv_message.h"
#include "cm1_msg_comm.h"
#if	defined(PLATFORM_MTK)
#include "nvram.h"
#endif

#ifdef FANCY_RESAMPLING
    #include <samplerate.h>
#endif

#include <assert.h>
//FILE* debug = 0;
static int debuglev = 1;
//int debug = 0;
//#define debug stderr

#include "alac.h"
#include "audio.h"
#include "common.h"
#include "wm_util.h"

//fix bk version build error
#ifdef	S11_EVB_BK
    #undef AF_INET6
#endif


#define MAX_PACKET      2048
#define MV_ALSA_PERIOD_SIZE 8192
#define MV_ALSA_OUTPUT_BUFFER_SIZE  32768

typedef unsigned short seq_t;

// global options (constant after init)
static unsigned char aeskey[16], aesiv[16];
static AES_KEY aes;
static char *rtphost = 0;
static int dataport = 0, controlport = 0, timingport = 0;
static int fmtp[32];
static int sampling_rate;
static int frame_size;


// FIFO name and file handle
static char *pipename = NULL;
static device_type devices[NDEVICES] = {
	{ "Airplay",      0,  1, 0},			// iPad, iPhone
	{ "forked-daapd", 0,  1, 1},			// forked daapd
	{ "iTunes",       12, 1, 0},			// iTunes
	{ "iTunes",       10, 1, 1},			// Air Audio
	{ "iTunes",       0,  0, 1},			// Airfoil
	{ "Unident",      0,  1, 1}
};

static device_type *user_agent=&devices[0];

#define FRAME_BYTES (4*frame_size)
// maximal resampling shift - conservative
#define OUTFRAME_BYTES(x) (4*((x)+3))
shairport_cfg config;


/*	Audio out 	RTSP 		*/
int  ao_rtsp = AO_RTSP_DISABLE;
#define RTSP_AUDIO_BUFFER_SIZE (262144 +4)   //128k *2


typedef struct ao_rtsp_data {
	unsigned int size;
	unsigned int wptr;
	unsigned int rptr;
	pthread_mutex_t mutex;
} ao_rtsp_data;


char * rtsp_audio_buffer = NULL;
unsigned int* buffer_size;
unsigned int* write_ptr;
unsigned int* read_ptr;
pthread_mutex_t* ao_mutex;

/*	Audio out 	RTSP 	END	*/


static alac_file *decoder_info;

#ifdef FANCY_RESAMPLING
static int fancy_resampling = 1;
static SRC_STATE *src;
#endif

static int  init_rtp(int tport);
static void init_buffer(void);
static int  init_output(void);
static void deinit_output(void);
static void rtp_request_resend(seq_t first, seq_t last);
static void ab_resync(void);
static inline long long get_sync_time(long long ntp_tsp);
void rtp_record(int rtp_mode);
unsigned long player_flush(int seqno, unsigned long rtp_tsp);


#define FIX_VOLUME 0x10000
static pthread_mutex_t alsa_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static char* alsa_buffer = NULL;
static int alsa_prt = 0;
static int sane_buffer_size;

typedef struct audio_buffer_entry   // decoded audio packets
{
    int ready;
    sync_cfg sync;
    signed short *data;
} abuf_t;
static abuf_t audio_buffer[BUFFER_FRAMES];
#define BUFIDX(seqno) ((seq_t)(seqno) % BUFFER_FRAMES)
#define RESEND_FOCUS   (BUFFER_FRAMES/4)

// mutex-protected variables
static volatile seq_t ab_read = 0, ab_write = 0;
static volatile int ab_buffering = 1, ab_synced = 0;
static pthread_mutex_t ab_mutex = PTHREAD_MUTEX_INITIALIZER;
static int ab_need_reset = 1;
//static pthread_cond_t ab_buffer_ready = PTHREAD_COND_INITIALIZER;

#define NET_TIMEOUT_COUNT 60
//static pthread_mutex_t check_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int check_count = NET_TIMEOUT_COUNT;
static int ntp_sync_rate;
static long long ltsp;
static pthread_mutex_t vol_mutex = PTHREAD_MUTEX_INITIALIZER;
extern WIIMU_CONTEXT* g_wiimu_shm;

void debug(int level, char *format, ...) {
    time_t rawtime;
    char newtime[30];
    if (level > debuglev)
        return;

    rawtime = time(NULL);
    ctime_r(&rawtime, newtime);
    newtime[24] = '\0';
    fprintf(stderr,  "%s DEBUG: ", newtime);
    //vprintf("%s DEBUG: ", newtime);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    //vprintf( format, args);
    va_end(args);
}

// wrapped number between two seq_t.
static inline uint16_t seq_diff(seq_t a, seq_t b) {
    int16_t diff = b - a;
    return diff;
}

#ifdef HAIRTUNES_STANDALONE
static int hex2bin(unsigned char *buf, char *hex) {
    int i, j;
    if (strlen(hex) != 0x20)
        return 1;
    for (i=0; i<0x10; i++)
    {
        if (!sscanf(hex, "%2X", &j))
            return 1;
        hex += 2;
        *buf++ = j;
    }
    return 0;
}
#endif

static int init_decoder(void) {
    alac_file *alac;

    frame_size = fmtp[1]; // stereo samples
    sampling_rate = fmtp[11];

    int sample_size = fmtp[3];
    if (sample_size != 16)
        die("only 16-bit samples supported!");

    alac = create_alac(sample_size, 2);
    if (!alac)
        return 1;
    decoder_info = alac;

    alac->setinfo_max_samples_per_frame = frame_size;
    //alac->setinfo_7a =      fmtp[2];
    alac->setinfo_sample_size = sample_size;
    alac->setinfo_rice_historymult = fmtp[4];
    alac->setinfo_rice_initialhistory = fmtp[5];
    alac->setinfo_rice_kmodifier = fmtp[6];
    //alac->setinfo_7f =      fmtp[7];
    //alac->setinfo_80 =      fmtp[8];
    //alac->setinfo_82 =      fmtp[9];
    //alac->setinfo_86 =      fmtp[10];
   	//alac->setinfo_8a_rate = fmtp[11];
    allocate_buffers(alac);
    return 0;
}

static void exit_sighandler(int x)
{
 	switch (x)
    {
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
    case SIGKILL:
    case SIGHUP:
		WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_AIRPLAY, 1);
        debug(1, "*****************%d******************* airplay exit!\n", x);
        debug(1, "==airplay child process(hairtunes)%d exit by signal %d==\n", getpid(), x);
        break;
    default:
        debug(1, "*****************%d******************* airplay ignore signal!\n", x);
        return;
    }

    exit(1);
}


int hairtunes_init(char *pAeskey, char *pAesiv, char *fmtpstr, int pCtrlPort, int pTimingPort,
                   int pDataPort, char *pRtpHost, char*pPipeName, char *pLibaoDriver, char *pLibaoDeviceName, char *pLibaoDeviceId,
                   int bufStartFill)
{
    signal(SIGTERM, exit_sighandler); // kill
    signal(SIGHUP, exit_sighandler);  // kill -HUP  /  xterm closed
    signal(SIGINT, exit_sighandler);  // Interrupt from keyboard
    signal(SIGQUIT, exit_sighandler); // Quit from keyboard
    signal(SIGKILL, exit_sighandler); // Quit from keyboard

    if (pAeskey != NULL)
        memcpy(aeskey, pAeskey, sizeof(aeskey));
    if (pAesiv != NULL)
        memcpy(aesiv, pAesiv, sizeof(aesiv));
    if (pRtpHost != NULL)
        rtphost = pRtpHost;
    if (pPipeName != NULL)
        pipename = pPipeName;
    if (pLibaoDriver != NULL)
        audio_set_driver(pLibaoDriver);
    if (pLibaoDeviceName != NULL)
        audio_set_device_name(pLibaoDeviceName);
    if (pLibaoDeviceId != NULL)
        audio_set_device_id(pLibaoDeviceId);

    controlport = pCtrlPort;
    timingport = pTimingPort;
    dataport = pDataPort;
    debug(1, "====%s controlport=%d, timingport=%d dataport=%d\n",
        __FUNCTION__, controlport, timingport, dataport);

    AES_set_decrypt_key(aeskey, 128, &aes);

    memset(fmtp, 0, sizeof(fmtp));
    int i = 0;
    char *arg;
    while ((arg = strsep(&fmtpstr, " \t")))
        fmtp[i++] = atoi(arg);

	WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_AIRPLAY, 1);

    init_decoder();
    init_buffer();
    init_rtp(timingport);      // open a UDP listen port and start a listener; decode into ring buffer
    fflush(stdout);
    init_output();              // resample and output from ring buffer

    char line[128];
    int in_line = 0;
    int n;
//    double f;

	//system("amixer cset numid=1 90");

    while (fgets(line + in_line, sizeof(line) - in_line, stdin))
    {
        n = strlen(line);
        if (line[n-1] != '\n')
        {
            in_line = strlen(line) - 1;
            if (n == sizeof(line)-1)
                in_line = 0;
            continue;
        }
        if (!strcmp(line, "exit\n"))
        {
            debug(1, "recv exit\n");
            exit(0);
        }
        if (!strcmp(line, "flush\n"))
        {
            pthread_mutex_lock(&ab_mutex);
            ab_resync();
            pthread_mutex_unlock(&ab_mutex);
        }
        else if (!strncmp(line, "flush_rtp:", 10))
        {
			seq_t seqno = (seq_t)atoi(line+10);
			int rtpmode;
			unsigned long rtptime;
			sscanf(line+10, "%d:%hu:%lu", &rtpmode, &seqno, &rtptime);
			if( rtpmode != -1 )
				rtp_record(rtpmode);
			unsigned long result = player_flush(seqno, rtptime);
			debug(1, "recv FLUSH RTP rtpmode=%d, seqno=%d, time=%lu result=%lu\n",
				rtpmode, (int)seqno, rtptime, result);
			if( seqno > 0 )
			{
				pthread_mutex_lock(&ab_mutex);
				ab_resync();
				ab_read = seqno;
				ab_write = seqno;
				ab_synced = 1;
				pthread_mutex_unlock(&ab_mutex);
			}
        }
    }

	WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_AIRPLAY, 1);
	debug(3, "deinit_output!\n");
    usleep(50000);
    deinit_output();
    usleep(50000);
    debug(1, "bye!\n");
    fflush(stderr);

    return EXIT_SUCCESS;
}

#ifdef HAIRTUNES_STANDALONE
int gPid;

int main(int argc, char **argv) {
    char *hexaeskey = 0, *hexaesiv = 0;
    char *fmtpstr = 0;
    char *arg;
    assert(RAND_MAX >= 0x10000);    // XXX move this to compile time
    while ((arg = *++argv))
    {
        if (!strcasecmp(arg, "iv"))
        {
            hexaesiv = *++argv;
            argc--;
        }
        else
            if (!strcasecmp(arg, "key"))
        {
            hexaeskey = *++argv;
            argc--;
        }
        else
            if (!strcasecmp(arg, "fmtp"))
        {
            fmtpstr = *++argv;
        }
        else
            if (!strcasecmp(arg, "cport"))
        {
            controlport = atoi(*++argv);
        }
        else
            if (!strcasecmp(arg, "tport"))
        {
            timingport = atoi(*++argv);
        }
        else
            if (!strcasecmp(arg, "dport"))
        {
            dataport = atoi(*++argv);
        }
        else
            if (!strcasecmp(arg, "host"))
        {
            rtphost = *++argv;
        }
        else
            if (!strcasecmp(arg, "pipe"))
        {
            if (audio_get_driver() || audio_get_device_name() || audio_get_device_id())
            {
                die("Option 'pipe' may not be combined with 'ao_driver', 'ao_devicename' or 'ao_deviceid'");
            }
            pipename = *++argv;
        }
        else
            if (!strcasecmp(arg, "ao_driver"))
        {
            if (pipename)
            {
                die("Option 'ao_driver' may not be combined with 'pipe'");
            }

            audio_set_driver(*++argv);
        }
        else
            if (!strcasecmp(arg, "ao_devicename"))
        {
            if (pipename || audio_get_device_id())
            {
                die("Option 'ao_devicename' may not be combined with 'pipe' or 'ao_deviceid'");
            }

            audio_set_device_name(*++argv);
        }
        else
            if (!strcasecmp(arg, "ao_deviceid"))
        {
            if (pipename || audio_get_device_name())
            {
                die("Option 'ao_deviceid' may not be combined with 'pipe' or 'ao_devicename'");
            }

            audio_set_device_id(*++argv);
        }
#ifdef FANCY_RESAMPLING
        else
            if (!strcasecmp(arg, "resamp"))
        {
            fancy_resampling = atoi(*++argv);
        }
#endif
    }

    if (!hexaeskey || !hexaesiv)
        die("Must supply AES key and IV!");

    if (hex2bin(aesiv, hexaesiv))
        die("can't understand IV");
    if (hex2bin(aeskey, hexaeskey))
        die("can't understand key");
    return hairtunes_init(NULL, NULL, fmtpstr, controlport, timingport, dataport,
                          NULL, NULL, NULL, NULL, NULL, 0);
}
#endif

static void init_buffer(void) {
    int i;
    for (i=0; i<BUFFER_FRAMES; i++)
        audio_buffer[i].data = malloc(OUTFRAME_BYTES(frame_size));
    ab_resync();
}

static void ab_resync(void) {
    int i;
    for (i=0; i<BUFFER_FRAMES; i++)
    {
        audio_buffer[i].ready = 0;
        audio_buffer[i].sync.sync_mode = NOSYNC;
    }
    ab_synced = SIGNALLOSS;
    ab_buffering = 1;
    state = BUFFERING;
}


// reset the audio frames in the range to NOT ready
static void ab_reset(seq_t from, seq_t to) {
    abuf_t *abuf = 0;

    while (seq_diff(from, to)) {
        if (seq_diff(from, to) > BUFFER_FRAMES) {
	   from = to - BUFFER_FRAMES;
        } else {
           abuf = audio_buffer + BUFIDX(from);
           abuf->ready = 0;
           abuf->sync.sync_mode = NOSYNC;
           from++;
        }
    }
}

extern long long GetDelay(void);


// the sequence numbers will wrap pretty often.
// this returns true if the second arg is after the first
static inline int seq_order(seq_t a, seq_t b) {
    signed short d = b - a;
    return d > 0;
}

static long us_to_frames(long long us) {
	return us * sampling_rate / 1000000;
}

static long long frames_to_us(long frames) {
	return (long long) frames * 1000000LL / (long long) sampling_rate;
}



static void alac_decode(short *dest, char *buf, int len) {
    unsigned char packet[MAX_PACKET];
    assert(len<=MAX_PACKET);

    unsigned char iv[16];
    int aeslen = len & ~0xf;
    memcpy(iv, aesiv, sizeof(iv));
    AES_cbc_encrypt((unsigned char*)buf, packet, aeslen, &aes, iv, AES_DECRYPT);
    memcpy(packet+aeslen, buf+aeslen, len-aeslen);

    int outsize;

    decode_frame(decoder_info, packet, dest, &outsize);

  //  assert(outsize == FRAME_BYTES);
}

static void buffer_put_packet(seq_t seqno, sync_cfg sync_tag, char *data, int len) {
    abuf_t *abuf = 0;
    unsigned short buf_fill;
    pthread_mutex_lock(&ab_mutex);
    if (ab_synced == SIGNALLOSS) {
        ab_write = seqno;
        ab_read = seqno;
        ab_synced = UNSYNC;
    }

    if ((seq_diff(ab_read,seqno) >= BUFFER_FRAMES) &&           // this packet will cause overrun
        (seq_order(ab_read,seqno))) {                           // must handle here to tidy buffer frames skipped
        debug(1, "%s %d Overrun %04X (%04X:%04X)\n", __FUNCTION__, __LINE__, seqno, ab_read, ab_write);
        ab_read = seqno - (BUFFER_FRAMES/2);                    // restart at a sane distance
        ab_reset((ab_write - BUFFER_FRAMES), ab_read);          // reset any ready frames in those we've skipped (avoiding wrap around)
        ab_write = seqno;                                       // ensure
    }

    if (seqno == ab_write)                  // expected packet
    {
        abuf = audio_buffer + BUFIDX(seqno);
        ab_write++;
    }
    else if (seq_order(ab_write, seqno))    // newer than expected
    {
        // Be careful with new packets that are in advance
        // Too far in advance will lead to multiple resends and probable overrun
        // Better just to force a resync on the new packet
        if (seq_diff(ab_write, seqno) >= (BUFFER_FRAMES/4)) {   // packet too far in advance - better to resync
           debug(1, "out of range re-sync %04X (%04X:%04X)\n", seqno, ab_read, ab_write);
           ab_resync();
           ab_synced = UNSYNC;
           ab_read = seqno;
        } else {
           debug(3, "advance packet %04X (%04X:%04X) ab_buffering=%d\n", seqno, ab_read, ab_write, ab_buffering);
           if (!ab_buffering) {rtp_request_resend(ab_write, seqno-1);}
        }
        abuf = audio_buffer + BUFIDX(seqno);
        ab_write = seqno+1;
    }
    else if (seq_order(ab_read-1, seqno))     // late but not yet played
    {
        abuf = audio_buffer + BUFIDX(seqno);
        if (abuf->ready) {                      // discard this frame if frame previously received
           abuf =0;
           debug(1, "duplicate packet %04X (%04X:%04X)\n", seqno, ab_read, ab_write);
        }
    }
    else    // too late.
    {
    	debug(3, "too late packet %u (read=%u write=%u)\n", seqno, ab_read, ab_write);
    }
    buf_fill = ab_write - ab_read;
    pthread_mutex_unlock(&ab_mutex);

    if (abuf)
    {
        alac_decode(abuf->data, data, len);
        abuf->sync.rtp_tsp = sync_tag.rtp_tsp;
        // sync packets with extension bit seem to be one audio packet off:
        // if the extension bit was set, pull back the ntp time by one packet's time
        // Some devices have addressed this and are listed as fast_start
        if ((sync_tag.sync_mode == E_NTPSYNC) && (!user_agent->fast_start)) {
            abuf->sync.ntp_tsp = sync_tag.ntp_tsp - (long long)frame_size * 1000000LL / (long long)sampling_rate;
            abuf->sync.sync_mode = NTPSYNC;
        } else {
            abuf->sync.ntp_tsp = sync_tag.ntp_tsp;
            abuf->sync.sync_mode = sync_tag.sync_mode;
        }
        abuf->ready = 1;
        if( abuf->sync.sync_mode )
            debug(5, "== seqno %04X is ready, ntp_tsp=%lld, sync_mode=%d\n",
				seqno, abuf->sync.ntp_tsp, abuf->sync.sync_mode);
    }

    pthread_mutex_lock(&ab_mutex);
	{
		if ((abuf) && (ab_synced == UNSYNC) && ((sync_tag.sync_mode == NTPSYNC) || (sync_tag.sync_mode == E_NTPSYNC))) {
			// only stop buffering when the new frame is a timestamp with good sync
			long long sync_time = get_sync_time(sync_tag.ntp_tsp);
			if( !sync_time )
			{
				pthread_mutex_unlock(&ab_mutex);
				debug(1, "ntp not synced yet at %lld ab_synced=%d\n", tickSincePowerOn(), ab_synced);
				if ( rtp_strictmode() )
					return;
				pthread_mutex_lock(&ab_mutex);
				ab_reset(ab_read, seqno);
				ab_read = seqno;
				ab_need_reset = 1;
				ab_synced = INSYNC;
			}
			else if ( sync_time > (config.latency/8)) {
				debug(1, "found good sync %lld  seqno=%hu buf_fill=%hu reset=%d at %lld\n",
					sync_time, seqno, buf_fill, ab_need_reset, tickSincePowerOn());
				ab_synced = INSYNC;
				if( ab_need_reset )
				{
					ab_reset(ab_read, seqno);
					ab_read = seqno;
					ab_need_reset = 0;
				}
			}
			else
			{
				debug(1, "found bad sync %lld  seqno=%hu buf_fill=%hu ab_synced=%d  at %lld\n",
					sync_time, seqno, buf_fill, ab_synced, tickSincePowerOn());
				ab_reset(ab_read, seqno);
				ab_read = seqno;
				ab_need_reset = 1;
				ab_synced = INSYNC;
			}
		}
	}

	buf_fill = ab_write - ab_read;
	if ((abuf) && (ab_synced == INSYNC) && (ab_buffering) && (buf_fill >= sane_buffer_size)) {
		debug(1, "buffering over. starting play buf_fill=%d\n", buf_fill);
        ab_buffering = 0;
			//pthread_cond_signal(&ab_buffer_ready);
	}
    pthread_mutex_unlock(&ab_mutex);
}

static int rtp_sockets[3];  // data, control
#ifdef AF_INET6
static struct sockaddr_in6 ntp_client;
#else
static struct sockaddr_in ntp_client;
#endif
static int ntp_client_set = 0;

static void *check_thread_func(void *arg) {
    while (1)
    {
        sleep(1);
       // pthread_mutex_lock(&check_mutex);
        check_count --;
        if (check_count <= 0)
        {
            debug(1, "check_thread_func  net disconnect  \n");
			FILE* fp=fopen("/tmp/airplay_stop","w");
			if(fp)
				fclose(fp);
            break;
        }
       // pthread_mutex_unlock(&check_mutex);
    }

    return NULL;
}

void player_set_latency(long latency_samples) {

    config.latency = (int) (((long long) latency_samples * 1000000LL) / ((long long) sampling_rate));
    config.latency = config.latency + (config.delay - LATENCY_BASELINE_US);

    sane_buffer_size = (us_to_frames(config.latency)/frame_size) * 3 / 10;
    sane_buffer_size = (sane_buffer_size >= 10 ? sane_buffer_size : 10);
    if (sane_buffer_size > BUFFER_FRAMES)
        debug(3, "buffer starting fill %d > buffer size %d", sane_buffer_size, BUFFER_FRAMES);
    debug(3, "Set latency: delay/latency/samples/sane %d %d %ld %d\n", config.delay, config.latency, latency_samples, sane_buffer_size);
}

static int strict_rtp = 0;
long long ntp_cache[NTPCACHESIZE + 1];
static int ntp_cache_skip_count;
static int please_shutdown = 0;

void rtp_record(int rtp_mode){
    debug(3, "Setting strict_rtp to %d\n", rtp_mode);
    strict_rtp = rtp_mode;
}

int rtp_strictmode(void){
    return strict_rtp;
}

static long long tspk_to_us(struct timespec tspk) {
    long long usecs;

    usecs = tspk.tv_sec * 1000000LL;

    return usecs + (tspk.tv_nsec / 1000);
}

static void get_current_time(struct timespec *tsp) {
    clock_gettime(CLOCK_MONOTONIC, tsp);
}


static void reset_ntp_cache(void) {
    int i;
    for (i = 0; i < NTPCACHESIZE; i++) {
        ntp_cache[i] = LLONG_MIN;
    }
    ntp_cache[NTPCACHESIZE] = 0;
    ntp_cache_skip_count = 0;
}

long long get_ntp_offset(void) {
    return ntp_cache[NTPCACHESIZE];
}

static void update_ntp_cache(long long offset, long long arrival_time) {
    // average the offsets, filter out outliers

    int i, d, minindex, maxindex;
    long long total;
    long long old_avg = ntp_cache [NTPCACHESIZE];
    long long offset_range = offset - old_avg;;

    // NTP offset derived using the timing protocol, is subject to
    // error if the out & return delays are not even.
    // The averaging/max/min process will smooth but not remove the error
    // We must therefore ignore outlying variants using the defined range (NTPRANGE)

    if (ntp_cache_skip_count == NTPSKIPLIMIT) {				// if we have skipped too many
	debug(3, "ntp: time sync lost, being reset...\n");			// reset the cache & start again
	reset_ntp_cache();
    }

    if ((ntp_cache[0] == LLONG_MIN) ||					// if initial filling of cache
       ((offset_range < NTPRANGE) && (offset_range > -(NTPRANGE)))) {	// or validated within range
    ntp_cache_skip_count = 0;						// process & reset skip count


    for (i = 0; i < (NTPCACHESIZE - 1);  i++) {
        ntp_cache[i] = ntp_cache[i+1];
    }
    ntp_cache[NTPCACHESIZE - 1] = offset;

    d = 0;
    minindex = 0;
    maxindex = 0;
    for (i = 0; i < NTPCACHESIZE; i++) {
        if (ntp_cache[i] != LLONG_MIN) {
            d++;
            minindex = (ntp_cache[i] < ntp_cache[minindex] ? i : minindex);
            maxindex = (ntp_cache[i] > ntp_cache[maxindex] ? i : maxindex);
        }
    }
    //debug(2, "ntp: valid entries: %d\n", d);
    if (d < 5)
        minindex = maxindex = -1;
    d = 0;
    total = 0;
    for (i = 0; i < NTPCACHESIZE; i++) {
        //debug(3, "ntp[%d]: %lld, d: %d\n", i, ntp_cache[i] , d);
        if ((ntp_cache[i] != LLONG_MIN) && (i != minindex) && (i != maxindex)) {
            d++;
            total += ntp_cache[i];
        }
    }
    ntp_cache[NTPCACHESIZE] = total / d;
    debug(5, "ntp: offset: %lld +-(%5lld), d: %d\n", ntp_cache[NTPCACHESIZE], ntp_cache[NTPCACHESIZE]-old_avg, d);

    } else {								// NTP offset is not within acceptable range
	ntp_cache_skip_count++;
	debug(3, "ntp: clock offset out of range: %6lld skipped (%i)\n", offset_range, ntp_cache_skip_count);
    }
}

long long tstp_us(void) {
    struct timespec tv;
    get_current_time(&tv);
    return tspk_to_us(tv);
}

static long long ntp_tsp_to_us(uint32_t timestamp_hi, uint32_t timestamp_lo) {
    long long timetemp;

    timetemp = (long long)timestamp_hi * 1000000LL;
    timetemp += ((long long)timestamp_lo * 1000000LL) >> 32;

    return timetemp;
}

static inline long long get_sync_time(long long ntp_tsp) {
    long long sync_time_est = ntp_tsp;
    long long ntp_offset = get_ntp_offset();
    sync_time_est = (ntp_tsp + config.latency) - (tstp_us() + ntp_offset + 1000*GetDelay());
    if( !ntp_offset && rtp_strictmode() )
        return 0;
    return sync_time_est;
}

void player_set_device_type(char *name, int version) {
    int i;
    for (i=0; i<NDEVICES; i++) {
        if ((!strcasecmp(name, devices[i].name)) &&     // check for name matcch
            (version >= devices[i].version)) {          // and version same or higher
            user_agent = &devices[i];                   // set matching User Agent
	    break;
        }
    }
	if(i < NDEVICES)
	{
	    user_agent = &devices[i];                   // or default to Unident (last in list)
	    debug(3, "set device type %s v%i from %s v%i\n", user_agent->name, user_agent->version, name, version);
	}
    return;
}

int platform_get_audio_latency_us(void)
{
	return config.latency;
}

static void *rtp_thread_func(void *arg) {
#ifdef AF_INET6
    struct sockaddr_in6 rtp_client;
#else
    struct sockaddr_in rtp_client;
#endif
    socklen_t si_len = sizeof(rtp_client);
    char packet[MAX_PACKET];
    char *pktp;
    seq_t seqno;
    ssize_t plen;
    int sock = rtp_sockets[0], csock = rtp_sockets[1], tsock = rtp_sockets[2];
    int readsock;
    char type;
    unsigned long tsp_less_latency = 0;
    sync_cfg sync_tag, no_tag;
    sync_cfg * pkt_tag;
    int sync_fresh = 0;
    int select_fd = tsock;
    //ssize_t nread;

    no_tag.rtp_tsp = 0;
    no_tag.ntp_tsp = 0;
    no_tag.sync_mode = NOSYNC;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    FD_SET(csock, &fds);
    FD_SET(tsock, &fds);

    if( csock > select_fd )
        select_fd = csock;

    if( sock > select_fd )
        select_fd = sock;

    while (select(select_fd+1, &fds, 0, 0, 0)!=-1)
    {
        if (FD_ISSET(sock, &fds))
        {
            readsock = sock;
        }
        else if (FD_ISSET(csock, &fds))
        {
            readsock = csock;
        }
        else if (FD_ISSET(tsock, &fds))
        {
            readsock = tsock;
        }
        FD_SET(sock, &fds);
        FD_SET(csock, &fds);
        FD_SET(tsock, &fds);

        plen = recvfrom(readsock, packet, sizeof(packet), 0, (struct sockaddr*)&rtp_client, &si_len);

        check_count = NET_TIMEOUT_COUNT;
        if (plen <= 0)
        {
            debug(1, "%s airplay recvfrom error\n", __FUNCTION__);
			usleep(4000);
            continue;
        }

        if( !ntp_client_set )
        {
            debug(1, "%s set ntp client, pack type 0x%x at %lld\n", __FUNCTION__, type, tickSincePowerOn());
            memcpy((void *)&ntp_client, (void *)&rtp_client, sizeof(ntp_client));
            ntp_client_set = 1;
        }
		assert(plen<=MAX_PACKET);

        type = packet[1] & ~0x80;
		if (type == 0x54) // sync
		{
			if (plen != 20) {
				debug(1,"Sync packet with wrong length %d received\n", plen);
				continue;
			}

			tsp_less_latency = ntohl(*(uint32_t *)(packet+4));
			sync_tag.rtp_tsp = ntohl(*(uint32_t *)(packet+16));
			//debug(3, "Sync packet rtp_tsp %lu\n", sync_tag.rtp_tsp);
			sync_tag.ntp_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+8)), ntohl(*(uint32_t *)(packet+12)));
			//debug(3, "Sync packet ntp_tsp %lld\n", sync_tag.ntp_tsp);
			// check if extension bit is set; this will be the case for the first sync
			sync_tag.sync_mode = ((packet[0] & 0x10) ? E_NTPSYNC : NTPSYNC);
			sync_fresh = 1;
			if (sync_tag.sync_mode == E_NTPSYNC) {
				player_set_latency(sync_tag.rtp_tsp - tsp_less_latency); 	   // Set audio latency according to details in packet
			}
			continue;
		}

        if (type == 0x53) {
            if (plen != 32) {
                debug(1, "Timing packet with wrong length %d received\n", plen);
                continue;
            }
			struct timespec tv;
			get_current_time(&tv);
            long long ntp_ref_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+8)), ntohl(*(uint32_t *)(packet+12)));
            debug(5, "Timing packet ntp_ref_tsp %lld\n", ntp_ref_tsp);
            long long ntp_rec_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+16)), ntohl(*(uint32_t *)(packet+20)));
            debug(5, "Timing packet ntp_rec_tsp %lld\n", ntp_rec_tsp);
            long long ntp_sen_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+24)), ntohl(*(uint32_t *)(packet+28)));
            debug(5, "Timing packet ntp_sen_tsp %lld\n", ntp_sen_tsp);
            long long ntp_loc_tsp = tspk_to_us(tv);
            debug(5, "Timing packet ntp_loc_tsp %lld\n", ntp_loc_tsp);

            // from the ntp spec:
            //    d = (t4 - t1) - (t3 - t2)  and  c = (t2 - t1 + t3 - t4)/2
            long long d = (ntp_loc_tsp - ntp_ref_tsp) - (ntp_sen_tsp - ntp_rec_tsp);
            long long c = ((ntp_rec_tsp - ntp_ref_tsp) + (ntp_sen_tsp - ntp_loc_tsp)) / 2;

            debug(5, "Round-trip delay %lld us, Clock offset %lld us\n", d, c);
            update_ntp_cache(c, ntp_loc_tsp);

            continue;
        }

        if (type == 0x60 || type == 0x56)   // audio data / resend
        {
            pktp = packet;
            if (type==0x56)
            {
                pktp += 4;
                plen -= 4;
            }
            seqno = ntohs(*(unsigned short *)(pktp+2));
			unsigned long rtp_tsp = ntohl(*(uint32_t *)(pktp+4));

            // adjust pointer and length
            pktp += 12;
            plen -= 12;

            // check if packet contains enough content to be reasonable
            if (plen >= 16)
            {
                // strict -> find a rtp match, this might happen on resend packets, or,
                // in weird network circumstances, even more than once.
                // non-strickt -> just stick it to the first audio packet, _once_
                if ((strict_rtp && (rtp_tsp == sync_tag.rtp_tsp))
                        || (!strict_rtp && sync_fresh && (type == 0x60))) {
                    pkt_tag = &sync_tag;
                    sync_fresh = 0;
                } else
                    pkt_tag = &no_tag;
                buffer_put_packet(seqno, *pkt_tag, pktp, plen);
                continue;
            }
	       	if (type == 0x56 && seqno == 0) {
                debug(3, "seqno=0 packet received, plen=%d  (read=%u, write=%u)\n", plen, ab_read, ab_write);
                continue;
            }
        }
		else
		{
			debug(1, "!!! %s unknow pack type 0x%x\n", __FUNCTION__, type);
		}
    }
     close(sock);
     close(csock);
     close(tsock);

	debug(1, "rtp thread func quit\n");
    return 0;
}

static void rtp_request_resend(seq_t first, seq_t last) {
#ifdef AF_INET6
    struct sockaddr_in6 rtp_client;
#else
    struct sockaddr_in rtp_client;
#endif
    if (seq_order(last, first))
        return;

    char req[8]={0};    // *not* a standard RTCP NACK
    req[0] = 0x80;
    req[1] = 0x55|0x80;  // Apple 'resend'
    *(unsigned short *)(req+2) = htons(1);  // our seqnum
    *(unsigned short *)(req+4) = htons(first);  // missed seqnum
    *(unsigned short *)(req+6) = htons(last-first+1);  // count

    memcpy((void *)&rtp_client, (void *)&ntp_client, sizeof(rtp_client));
#ifdef AF_INET6
    rtp_client.sin6_port = htons(controlport);
#else
    rtp_client.sin_port = htons(controlport);
#endif
    sendto(rtp_sockets[1], req, sizeof(req), 0, (struct sockaddr *)&rtp_client, sizeof(rtp_client));
}

#if 0
static void *ntp_receiver(void *arg) {
    // we inherit the signal mask (SIGUSR1)
    uint8_t packet[2048];
    struct timespec tv;
	int *port = (int *)arg;
	int timing_sock = port[2];
    ssize_t nread;
#ifdef AF_INET6
    ntp_client.sin6_port = htons(timingport);
#else
    ntp_client.sin_port = htons(timingport);
#endif
    while (1) {
        if (please_shutdown)
        	break;

        nread = recv(timing_sock, packet, sizeof(packet), 0);
        if (nread < 0)
        {
			debug(1, "ntp_receiver return %d, thread break\n", errno);
        	break;
        }
        get_current_time(&tv);

        ssize_t plen = nread;
        uint8_t type = packet[1] & ~0x80;
        if (type == 0x53) {
            if (plen != 32) {
                debug(3, "Timing packet with wrong length %d received\n", plen);
                continue;
            }
            long long ntp_ref_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+8)), ntohl(*(uint32_t *)(packet+12)));
            debug(5, "Timing packet ntp_ref_tsp %lld\n", ntp_ref_tsp);
            long long ntp_rec_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+16)), ntohl(*(uint32_t *)(packet+20)));
            debug(5, "Timing packet ntp_rec_tsp %lld\n", ntp_rec_tsp);
            long long ntp_sen_tsp = ntp_tsp_to_us(ntohl(*(uint32_t *)(packet+24)), ntohl(*(uint32_t *)(packet+28)));
            debug(5, "Timing packet ntp_sen_tsp %lld\n", ntp_sen_tsp);
            long long ntp_loc_tsp = tspk_to_us(tv);
            debug(5, "Timing packet ntp_loc_tsp %lld\n", ntp_loc_tsp);

            // from the ntp spec:
            //    d = (t4 - t1) - (t3 - t2)  and  c = (t2 - t1 + t3 - t4)/2
            long long d = (ntp_loc_tsp - ntp_ref_tsp) - (ntp_sen_tsp - ntp_rec_tsp);
            long long c = ((ntp_rec_tsp - ntp_ref_tsp) + (ntp_sen_tsp - ntp_loc_tsp)) / 2;

            debug(5, "Round-trip delay %lld us, Clock offset %lld us\n", d, c);
            update_ntp_cache(c, ntp_loc_tsp);

            continue;
        }
        debug(3, "Unknown Timing packet of type 0x%02X length %d", type, nread);
    }

    debug(1, "Time receive thread interrupted. terminating.\n");

    return NULL;
}
#endif

static void *ntp_sender(void *arg) {
    // we inherit the signal mask (SIGUSR1)
    int i = 0;
    int cc;
    struct timespec tv;
    char req[32];
	int *port = (int *)arg;
	int timing_sock = port[2];
	while(1)
	{
		i++;
		if(i%20==0)
			debug(1, "%s ntp_client %d at %lld ntp_client_set=%d\n", __FUNCTION__, i, tickSincePowerOn(), ntp_client_set);
		if( ntp_client_set )
			break;
		usleep(100000);
	}
	debug(1, "=========%s %d timing_sock=%d timingport=%d==========\n", __FUNCTION__, __LINE__, timing_sock, timingport);
#ifdef AF_INET6
	ntp_client.sin6_port = htons(timingport);
#else
	ntp_client.sin_port = htons(timingport);
#endif

	i=0;
    memset(req, 0, sizeof(req));
    while (1) {
        // at startup, we send more timing request to fill up the cache
        if (please_shutdown)
            break;

        req[0] = 0x80;
        req[1] = 0x52|0x80;  // Apple 'ntp request'
        *(uint16_t *)(req+2) = htons(7);  // seq no, needs to be 7 or iTunes won't respond

        get_current_time(&tv);
        *(uint32_t *)(req+24) = htonl((uint32_t)tv.tv_sec);
        *(uint32_t *)(req+28) = htonl((uint32_t)tv.tv_nsec * 0x100000000ULL / (1000 * 1000 * 1000));

        cc = sendto(timing_sock, req, sizeof(req), 0, (struct sockaddr*)&ntp_client, sizeof(ntp_client));
        if (cc < 0){
            // Warn if network error - then exit loop and wait for host to reset
			i++;
			if(i%20==0)
	            debug(1, "%s send packet failed ? timingport=%d (%d) ntp_client_set=%d\n", __FUNCTION__, timingport, errno, ntp_client_set);
			ntp_client_set = 0;
#ifdef AF_INET6
			ntp_client.sin6_port = htons(timingport);
#else
			ntp_client.sin_port = htons(timingport);
#endif
			usleep(100000);
			continue;
        }
        debug(5, "Current time s:%lu us:%lu\n", (unsigned int) tv.tv_sec, (unsigned int) tv.tv_nsec / 1000);
        // todo: randomize time at which to send timing packets to avoid timing floods at the client
        if (i<2){
            i++;
            usleep(50000);
        } else
            sleep(3);
    }

    debug(3, "Time send thread interrupted. terminating.\n");

    return NULL;
}

extern int gPid;
static int init_rtp(int tport) {
    struct sockaddr_in si;
    int type = AF_INET;
    struct sockaddr* si_p = (struct sockaddr*)&si;
    socklen_t si_len = sizeof(si);
    unsigned short *sin_port = &si.sin_port;
    memset(&si, 0, sizeof(si));
#ifdef AF_INET6
    struct sockaddr_in6 si6;
    type = AF_INET6;
    si_p = (struct sockaddr*)&si6;
    si_len = sizeof(si6);
    sin_port = &si6.sin6_port;
    memset(&si6, 0, sizeof(si6));
#endif

    si.sin_family = AF_INET;
#ifdef SIN_LEN
    si.sin_len = sizeof(si);
#endif
    si.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef AF_INET6
    si6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
    si6.sin6_len = sizeof(si);
#endif
    si6.sin6_addr = in6addr_any;
    si6.sin6_flowinfo = 0;
#endif
    int sock = -1, csock = -1, tsock = -1;    // data and control (we treat the streams the same here)
    int bind1 = -1, bind2 = -1, bind3 = -1;
    // server side(ios/itunes) port: dataport, controlport, timingport
    // our ports: my_dataport, my_controlport, my_timingport
    int my_dataport = dataport;
    int my_controlport = controlport;
    int my_timingport = timingport;
    while (1)
    {
        if (sock < 0)
            sock = socket(type, SOCK_DGRAM, IPPROTO_UDP);
#ifdef AF_INET6
        if (sock==-1 && type == AF_INET6)
        {
            // try fallback to IPv4
            type = AF_INET;
            si_p = (struct sockaddr*)&si;
            si_len = sizeof(si);
            sin_port = &si.sin_port;
            continue;
        }
#endif
        if (sock==-1)
            die("Can't create data socket!");

        if (csock < 0)
            csock = socket(type, SOCK_DGRAM, IPPROTO_UDP);
        if (csock==-1)
            die("Can't create control socket!");

		if(tsock < 0)
			tsock = socket(type, SOCK_DGRAM, IPPROTO_UDP);
        if (tsock==-1)
            die("Can't create ntp socket!");

        if( bind1 == -1 )
        {
            *sin_port = htons(my_dataport);
            bind1 = bind(sock, si_p, si_len);
        }

        if( bind2 == -1 )
        {
            *sin_port = htons(my_controlport);
            bind2 = bind(csock, si_p, si_len);
        }

        if( bind3 == -1 )
        {
            *sin_port = htons(my_timingport);
            bind3 = bind(tsock, si_p, si_len);
        }
        debug(1, "bind1=%d, bind2=%d, bind3=%d\n", bind1, bind2, bind3);

        if (bind1 != -1 && bind2 != -1 && bind3 != -1) break;
        if (bind1 == -1)
        {
            close(sock); sock = -1;
            my_dataport += 3;
        }
        if (bind2 == -1)
        {
            close(csock); csock = -1;
            my_controlport += 3;
        }
        if (bind3 == -1)
        {
            close(tsock); tsock = -1;
            my_timingport += 3;
        }
    }

    debug(1, "itunes port: %d cport: %d tport: %d sock:%d csock:%d tsock:%d\n",
        dataport, controlport, timingport, sock, csock, tsock); // let our handler know where we end up listening
    printf("dataport: %d %d %d\n", my_dataport, my_controlport, my_timingport);
    fflush(stdout);

    pthread_t rtp_thread;
	//99
	pthread_attr_t attr;
	struct sched_param param;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_RR);
     param.sched_priority = 90;
	pthread_attr_setschedparam(&attr, &param);
		//pthread_create(&threadid, &attr, &threadfunc, NULL);

    rtp_sockets[0] = sock;
    rtp_sockets[1] = csock;
    rtp_sockets[2] = tsock;
    reset_ntp_cache();
    pthread_create(&rtp_thread,&attr, rtp_thread_func, (void *)rtp_sockets);

//	pthread_t ntp_receive_thread;
//	pthread_create(&ntp_receive_thread, NULL, &ntp_receiver, (void *)rtp_sockets);

	pthread_t ntp_send_thread;
	pthread_create(&ntp_send_thread, NULL, &ntp_sender, (void *)rtp_sockets);

    pthread_t check_thread;
    pthread_create(&check_thread, NULL, check_thread_func, (void *)NULL);
    return 0;
}

static short lcg_rand(void) {
    static unsigned long lcg_prev = 12345;
    lcg_prev = lcg_prev * 69069 + 3;
    return lcg_prev & 0xffff;
}

static inline short dithered_vol(short sample) {
    static short rand_a, rand_b;
    long out;

    out = (long)sample * FIX_VOLUME;
    if (FIX_VOLUME < 0x10000)
    {
        rand_b = rand_a;
        rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }
    return out>>16;
}


static double bf_playback_rate = 1.0;

// get the next frame, when available. return 0 if underrun/stream reset.
static short *buffer_get_frame(sync_cfg *sync_tag) {
    int16_t buf_fill;
    seq_t next;
    abuf_t *abuf = 0;
    int i;
    sync_tag->sync_mode = NOSYNC;

	pthread_mutex_lock(&ab_mutex);
	if (ab_buffering) {
		pthread_mutex_unlock(&ab_mutex);
		return NULL;
	}

    buf_fill = seq_diff(ab_read, ab_write);
    if (buf_fill < 1) {
		debug(1, "underrun %i (%04X:%04X)\n", buf_fill, ab_read, ab_write);
		ab_resync();
		pthread_mutex_unlock(&ab_mutex);
		return NULL;
    }
    if (buf_fill > BUFFER_FRAMES) {   // overrunning! uh-oh. restart at a sane distance
		debug(1,"overrun %i (%04X:%04X)\n", buf_fill, ab_read, ab_write);
        ab_read = ab_write - (BUFFER_FRAMES/2);
		ab_reset((ab_write - BUFFER_FRAMES), ab_read);	// reset any ready frames in those we've skipped (avoiding wrap around)
    }

    // check if t+16, t+32, t+64, t+128, ... (buffer_start_fill / 2)
    // packets have arrived... last-chance resend
 	if (!ab_buffering) {
        for (i = 16; i < RESEND_FOCUS; i = (i * 2)) {
            next = ab_read + i;
            abuf = audio_buffer + BUFIDX(next);
            if (!abuf->ready && (seq_order(next,(ab_write-1)))) {
				debug(3, "last chance resend T+%i, %04X (%04X:%04X)\n", i, next, ab_read, ab_write);
                rtp_request_resend(next, next);
            }
        }
    }


    abuf_t *curframe = audio_buffer + BUFIDX(ab_read);
    if (!curframe->ready)
    {
		debug(3, "missing frame %04X\n", ab_read);
		memset(curframe->data, 0, FRAME_BYTES);
    }
    ab_read++;
    curframe->ready = 0;
	sync_tag->rtp_tsp = curframe->sync.rtp_tsp;
	sync_tag->ntp_tsp = curframe->sync.ntp_tsp;
	sync_tag->sync_mode = curframe->sync.sync_mode;
	curframe->sync.sync_mode = NOSYNC;
    pthread_mutex_unlock(&ab_mutex);

    return curframe->data;
}// get the next frame, when available. return 0 if underrun/stream reset.


#define TUNE_RATIO 10L
#define TUNE_THRESHOLD 5L
#define ADJUST_SAMPLE 220 // +-5ms

static long tuning_samples = 0L;
static long tuning_stuffs = 0L;
static long max_sync_delay = 0L;
static long min_sync_delay = 0L;

static int stuff_buffer(short *inptr, short *outptr, long *sync_info, long *sync_diff, int *stuff_count) {
    int i;
    int stuff = 0;
    long p_stuff;

    // use the sync info which contains the total number of samples adrift
    // to adjust the behaviour of stuff buffer

    if ((*sync_info <= -ADJUST_SAMPLE) && (rtp_strictmode())) {			// if we are in Strict mode and more than a couple of full frame behind drop that frame
        *sync_info = *sync_info + frame_size;
        return 0;
    }

    // maintain tuning statistics for the rate matching algorithm
    if (tuning_samples >= 1000000L) {
       debug(3, "fill: %3i, stuffs: %3ld ppm, sample err max/min:%3ld:%4ld\n", seq_diff(ab_read, ab_write), tuning_stuffs, max_sync_delay, min_sync_delay);

       tuning_samples = 0L;
       tuning_stuffs = 0L;
       max_sync_delay = *sync_info;
       min_sync_delay = *sync_info;
    }
    tuning_samples = tuning_samples + frame_size;
    max_sync_delay = (*sync_info > max_sync_delay ? *sync_info : max_sync_delay);
    min_sync_delay = (*sync_info < min_sync_delay ? *sync_info : min_sync_delay);

    p_stuff = (labs(*sync_diff) / TUNE_RATIO);	  		 // Set to add or delete the desired samples one per frame

	if ((p_stuff > TUNE_THRESHOLD) && ((rand()/p_stuff) < (RAND_MAX/(long) ntp_sync_rate)) 	 // Apply over the next ntp sync period to frames chosen pseudo randomly
        ) {	                         // Do not stuff on minor delta
        long abs_sync_diff = labs(*sync_diff);
        stuff = *sync_diff/(abs_sync_diff ? abs_sync_diff : 1);                     // this should mean we are adjusting fewer samples - better for bit perfect playback (!)
    }

    pthread_mutex_lock(&vol_mutex);
    memset(outptr, 0, OUTFRAME_BYTES(frame_size));
    for (i=0; i<frame_size; i++) {   // the whole frame, if no stuffing
        *outptr++ = dithered_vol(*inptr++);
        *outptr++ = dithered_vol(*inptr++);
    }
	static long old_adj_sample = 0;
	long new_adj_sample = *sync_info;
	long adj_delta = old_adj_sample - new_adj_sample;
	if( rtp_strictmode() && new_adj_sample && (adj_delta > 88 || adj_delta < -88) )
	{
		debug(3, "=== need to adjust %ld samples last %ld p_stuff=%ld stuff=%d ntp_sync_rate=%d==\n",
			new_adj_sample, old_adj_sample, p_stuff, stuff, ntp_sync_rate);
		old_adj_sample = *sync_info;
	}
	/* TODO: need to resample ? */
	stuff = 0;

    pthread_mutex_unlock(&vol_mutex);
    return frame_size + stuff;
}

//constant first-order filter
#define ALPHA 5

static void *audio_thread_func(void *arg) {
    int play_samples = frame_size;
    int ntp_sync_count = 0;
    sync_cfg sync_tag;
    long long sync_time;
    long sync_frames = 0;
    long sync_frames_diff;
    int stuff_count = 0;
//	int buf_count = -1;
    state = BUFFERING;

    signed short *inbuf, *outbuf, *resbuf, *silence;
    outbuf = resbuf = malloc(OUTFRAME_BYTES(frame_size));
    inbuf = silence = malloc(OUTFRAME_BYTES(frame_size));
    memset(silence, 0, OUTFRAME_BYTES(frame_size));
    wm_set_thread_priority(80);

    while (1)
    {
		switch (state) {
			case BUFFERING: {
				inbuf = buffer_get_frame(&sync_tag);
				if(inbuf)
				debug(3, "buffer_get_frame return %p sync_mode = %d ntp_tsp=%lld rtp_tsp=%lu\n", inbuf, sync_tag.sync_mode,
					sync_tag.ntp_tsp, sync_tag.rtp_tsp);
				// as long as the buffer keeps returning NULL, we assume it is still filling up
				if (inbuf) {
					if (sync_tag.sync_mode != NOSYNC) {
						// figure out how much silence to insert before playback starts
						sync_frames = us_to_frames(get_sync_time(sync_tag.ntp_tsp));
					} else {
						// what if first packet(s) is lost?
						debug(1, "Ouch! first packet has no sync r=%04X w=%04X sync_tag.sync_mode=%d...\n", ab_read, ab_write, sync_tag.sync_mode);
						if( rtp_strictmode())
						{
							pthread_mutex_lock(&ab_mutex);
							ab_resync();
							ab_need_reset = 1;
							pthread_mutex_unlock(&ab_mutex);
							outbuf = silence;
							play_samples = frame_size;
							//sync_frames = us_to_frames(config.latency) - GetDelay();
							debug(1, "!!!force resync ab_read %hu to ab_write %hu at %lld!!!\n\n\n", ab_read, ab_write, tickSincePowerOn());
							break;
						}
						else
							sync_frames = us_to_frames(config.latency) - GetDelay();
					}
					if (sync_frames < 0)
						sync_frames = 0;
					debug(3,  "Fill with %ld frames and %ld samples\n", sync_frames / frame_size , sync_frames % frame_size);
					state = SYNCING;
					debug(3,  "Changing player STATE: %d at %lld\n", state, tickSincePowerOn());
					if ( sync_frames > 44100*3 )
					{
						debug(3,  "\n\n!!!!!! Fill with %ld frames and %ld samples!!!!\n\n", sync_frames / frame_size , sync_frames % frame_size);
						sync_frames = 0; // by liaods, fix ios issue
					}

				}
				outbuf = silence;
				play_samples = frame_size;
				break;
			}
			case SYNCING: {
				debug(5,  "%s %d Player STATE SYNCING: sync_frames=%d\n", __FUNCTION__, __LINE__, sync_frames);
	            if (sync_frames > 0) {
	                if (((sync_frames < frame_size * 10) && (sync_frames >= frame_size * 9)) && \
	                        (sync_tag.sync_mode != NOSYNC)) {
	                    debug(3,  "sync_frames adjusting: %d\n", sync_frames);
	                    // figure out how much silence to insert before playback starts
	                    sync_frames = us_to_frames(get_sync_time(sync_tag.ntp_tsp));
	                    if (sync_frames < 0)
	                        sync_frames = 0;
	                    debug(3,  "%d\n", sync_frames);
	                }
	                play_samples = (sync_frames >= frame_size ? frame_size : sync_frames);
	                outbuf = silence;
	                sync_frames -= play_samples;

	                debug(5,  "Samples to go before playback start: %d\n", sync_frames);
	            } else {
	                outbuf = resbuf;
	                sync_frames_diff = 0L;
	                play_samples = stuff_buffer(inbuf, outbuf, &sync_frames, &sync_frames_diff, &stuff_count);
	                stuff_count = 0;
	                state = PLAYING;
					ntp_sync_count = 0;
					ltsp = sync_tag.ntp_tsp;
	                debug(3,  "Changing player STATE: sync_frames=%ld %d at %lld\n", sync_frames, state, tickSincePowerOn());
	            }
	            break;
        	}
			case PLAYING: {
            	inbuf = buffer_get_frame(&sync_tag);
	            if (!inbuf)
	                inbuf = silence;
            	ntp_sync_count++;
	            if (sync_tag.sync_mode == NTPSYNC) {
	                ntp_sync_rate = ntp_sync_count;				// Establish the number of frames between NTP Syncs
	                ntp_sync_count = 0;
					// Need to validate ntp timestamp, as weak sources
					// this is not stable and impacts time sync
					long long tsp_adjust = frames_to_us(ntp_sync_rate * frame_size);		// Expected tsp gap based on packets process					ed
					long long sync_tsp = ltsp + tsp_adjust; 								// Expected new tsp
					long long tsp_sync_diff = sync_tsp - sync_tag.ntp_tsp;					// Gap between expected and received

					debug(3, "NTP sync rate error %lld\n", tsp_sync_diff);

					if ((llabs(tsp_sync_diff) >   800LL ) &&			// If received tsp looks unstable
						(llabs(tsp_sync_diff) < 20000LL )) {			// but allow small variations and cap protection for full resync
						sync_tag.ntp_tsp = sync_tsp;					// Use expected tsp to maintain stability in playback
						debug(3, "Override ntp timestamp, error:%lldus\n", tsp_sync_diff);
					}
					ltsp = sync_tag.ntp_tsp;

	               //check if we're still in sync.
	                sync_time = get_sync_time(sync_tag.ntp_tsp);
	                sync_frames = us_to_frames(sync_time);
	                sync_frames_diff = ((ALPHA * sync_frames_diff) + ((10 - ALPHA) * sync_frames))/10;
	                debug(5,  "fill: %i, sync:%5lld (samples):%4d:%4d, stuffs %d\n", seq_diff(ab_read, ab_write), sync_time, sync_frames, sync_frames_diff, stuff_count);
	                stuff_count = 0;
	                if (((sync_frames) >= ADJUST_SAMPLE) && (rtp_strictmode())) { // If we are in Strict mode and find ourselves playing ahead of sync (often caused by underrun on source)
						// we need to inject some silence to realign before playing this frame - warn if serious
		                debug(1, "Injecting %ld frames of silence ntp_off %lld ltsp=%lld tsp_sync_diff=%d\n",
		                	sync_frames, get_ntp_offset(), ltsp, tsp_sync_diff);
		                play_samples = (sync_frames >= frame_size ? frame_size : sync_frames);
		                outbuf = silence;
		                sync_frames -= play_samples;
		                state = SYNCING;
		                debug(3,  "Changing player STATE: %d, play_samples=%d sync_frames=%ld\n", state, play_samples, sync_frames);
				    	break;						// move directly to play silence
					}
	            }
	            play_samples = stuff_buffer(inbuf, outbuf, &sync_frames, &sync_frames_diff, &stuff_count);
            	break;
        	}
			default:
				break;
		}

            pthread_mutex_lock(&alsa_buffer_mutex);
	        if (NULL != alsa_buffer)
	        {
	            memcpy(alsa_buffer+(alsa_prt*4),(char *)outbuf,play_samples *4);
	            alsa_prt += play_samples;

				int write_size = 64;
				while(alsa_prt > 0 )
				{
					int write_len = ((alsa_prt > write_size)? write_size:alsa_prt);
	                audio_play((char *)alsa_buffer, write_len, arg);
	                memcpy(alsa_buffer,(char *)(alsa_buffer+(write_len*4)), ((alsa_prt-write_len)*4));
					alsa_prt = (alsa_prt-write_len);
				}
	        }

            pthread_mutex_unlock(&alsa_buffer_mutex);
        }

	free(resbuf);
	free(silence);
    return 0;
}

unsigned long player_flush(int seqno, unsigned long rtp_tsp) {
    unsigned long result = 0;
    pthread_mutex_lock(&ab_mutex);
    abuf_t *curframe = audio_buffer + BUFIDX(ab_read);
    if (curframe->ready) {
        result = curframe->sync.rtp_tsp;
    }

    ab_resync();
    ab_write = seqno;
    ab_read = seqno;
    // a negative seqno mean the client did not supply one, so we will
    // treat the first audio packet that comes along, as the first in the audio stream
    // Also use this when User-Agent dies not support clean start.
    ab_synced = ((seqno < 0) || !(user_agent->clean_start) ? SIGNALLOSS : UNSYNC);
    pthread_mutex_unlock(&ab_mutex);
    return result;
}


static int init_output(void) {
    void* arg = 0;

    arg = audio_init(sampling_rate);

	pthread_mutex_lock(&alsa_buffer_mutex);
	alsa_prt = 0;
	alsa_buffer = malloc(MV_ALSA_OUTPUT_BUFFER_SIZE+8192); //32768 is period size , add 8192 in case overwrite.
	if (NULL != alsa_buffer)
	{
	   memset(alsa_buffer,0,(MV_ALSA_OUTPUT_BUFFER_SIZE+8192));
	}
	else
	{
	    //debug(3," init_output  alsa_buffer malloc failed \n");
	}
    pthread_mutex_unlock(&alsa_buffer_mutex);

    pthread_t audio_thread;
	//99
//	pthread_attr_t attr;
//	struct sched_param param;
//	pthread_attr_init(&attr);
//	pthread_attr_setschedpolicy(&attr, SCHED_RR);
//    param.sched_priority = 60;
//	pthread_attr_setschedparam(&attr, &param);
	player_set_latency(2 * sampling_rate);
    pthread_create(&audio_thread, NULL, audio_thread_func, arg);

    return 0;
}

static void deinit_output(void)
{
	    audio_deinit();

	    pthread_mutex_lock(&alsa_buffer_mutex);
	    if (alsa_buffer)
	    {
	        free(alsa_buffer);
	        alsa_buffer = NULL;
	    }
	    alsa_prt = 0;
	    pthread_mutex_unlock(&alsa_buffer_mutex);
}

