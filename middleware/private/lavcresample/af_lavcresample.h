#ifndef _AF_LAVCRESAMPLE_
#define _AF_LAVCRESAMPLE_

#ifndef CONFIG_RESAMPLE_HP
#define FILTER_SHIFT 15

#define FELEM int16_t
#define FELEM2 int32_t
#define FELEML int64_t
#define FELEM_MAX INT16_MAX
#define FELEM_MIN INT16_MIN
#define WINDOW_TYPE 9
#elif !defined(CONFIG_RESAMPLE_AUDIOPHILE_KIDDY_MODE)
#define FILTER_SHIFT 30

#define FELEM int32_t
#define FELEM2 int64_t
#define FELEML int64_t
#define FELEM_MAX INT32_MAX
#define FELEM_MIN INT32_MIN
#define WINDOW_TYPE 12
#else
#define FILTER_SHIFT 0

#define FELEM double
#define FELEM2 double
#define FELEML double
#define WINDOW_TYPE 24
#endif


// Audio data chunk
typedef struct af_data_s
{
  void* audio;  // data buffer
  int len;      // buffer length
  int rate;	// sample rate
  int nch;	// number of channels
  int format;	// format
  int bps; 	// bytes per sample
} af_data_t;

#define AF_DETACH   2
#define AF_OK       1
#define AF_TRUE     1
#define AF_FALSE    0
#define AF_UNKNOWN -1
#define AF_ERROR   -2
#define AF_FATAL   -3
// Number of channels
#ifndef AF_NCH
#define AF_NCH 8
#endif
#define AF_FORMAT_BE		(0<<0) // Big Endian
#define AF_FORMAT_LE		(1<<0) // Little Endian
#define AF_FORMAT_END_MASK	(1<<0)

#if HAVE_BIGENDIAN	       	// Native endian of cpu
#define	AF_FORMAT_NE		AF_FORMAT_BE
#else
#define	AF_FORMAT_NE		AF_FORMAT_LE
#endif

// Signed/unsigned
#define AF_FORMAT_SI		(0<<1) // Signed
#define AF_FORMAT_US		(1<<1) // Unsigned
#define AF_FORMAT_SIGN_MASK	(1<<1)

// Fixed or floating point
#define AF_FORMAT_I		(0<<2) // Int
#define AF_FORMAT_F		(1<<2) // Foating point
#define AF_FORMAT_POINT_MASK	(1<<2)

// Bits used
#define AF_FORMAT_8BIT		(0<<3)
#define AF_FORMAT_16BIT		(1<<3)
#define AF_FORMAT_24BIT		(2<<3)
#define AF_FORMAT_32BIT		(3<<3)
#define AF_FORMAT_40BIT		(4<<3)
#define AF_FORMAT_48BIT		(5<<3)
#define AF_FORMAT_BITS_MASK	(7<<3)

// Special flags refering to non pcm data
#define AF_FORMAT_MU_LAW	(1<<6)
#define AF_FORMAT_A_LAW		(2<<6)
#define AF_FORMAT_MPEG2		(3<<6) // MPEG(2) audio
#define AF_FORMAT_AC3		(4<<6) // Dolby Digital AC3
#define AF_FORMAT_IMA_ADPCM	(5<<6)
#define AF_FORMAT_IEC61937      (6<<6)
#define AF_FORMAT_SPECIAL_MASK	(7<<6)

// PREDEFINED formats

#define AF_FORMAT_U8		(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_8BIT|AF_FORMAT_NE)
#define AF_FORMAT_S8		(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_8BIT|AF_FORMAT_NE)
#define AF_FORMAT_U16_LE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_16BIT|AF_FORMAT_LE)
#define AF_FORMAT_U16_BE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_16BIT|AF_FORMAT_BE)
#define AF_FORMAT_S16_LE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_16BIT|AF_FORMAT_LE)
#define AF_FORMAT_S16_BE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_16BIT|AF_FORMAT_BE)
#define AF_FORMAT_U24_LE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_24BIT|AF_FORMAT_LE)
#define AF_FORMAT_U24_BE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_24BIT|AF_FORMAT_BE)
#define AF_FORMAT_S24_LE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_24BIT|AF_FORMAT_LE)
#define AF_FORMAT_S24_BE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_24BIT|AF_FORMAT_BE)
#define AF_FORMAT_U32_LE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_32BIT|AF_FORMAT_LE)
#define AF_FORMAT_U32_BE	(AF_FORMAT_I|AF_FORMAT_US|AF_FORMAT_32BIT|AF_FORMAT_BE)
#define AF_FORMAT_S32_LE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_32BIT|AF_FORMAT_LE)
#define AF_FORMAT_S32_BE	(AF_FORMAT_I|AF_FORMAT_SI|AF_FORMAT_32BIT|AF_FORMAT_BE)

#define AF_FORMAT_U16_NE AF_FORMAT_U16_LE
#define AF_FORMAT_S16_NE AF_FORMAT_S16_LE
#define AF_FORMAT_U24_NE AF_FORMAT_U24_LE
#define AF_FORMAT_S24_NE AF_FORMAT_S24_LE
#define AF_FORMAT_U32_NE AF_FORMAT_U32_LE
#define AF_FORMAT_S32_NE AF_FORMAT_S32_LE
/* Helper function for testing the output format */

#define AF_CONTROL_REINIT  		    0x00000100 
#define AF_CONTROL_RESAMPLE_RATE	0x00000200 
#define AF_CONTROL_COMMAND_LINE		0x00000300

// Linked list of audio filters
typedef struct af_instance_s
{
//  const af_info_t* info;
  int (*control)(struct af_instance_s* af, int cmd, void* arg);
  void (*uninit)(struct af_instance_s* af);
  af_data_t* (*play)(struct af_instance_s* af, af_data_t* data);
  void* setup;	  // setup data for this specific instance and filter
  af_data_t* data; // configuration for outgoing data stream
  struct af_instance_s* next;
  struct af_instance_s* prev;
  double delay; /* Delay caused by the filter, in units of bytes read without
		 * corresponding output */
  double mul; /* length multiplier: how much does this instance change
		 the length of the buffer. */
}af_instance_t;

#ifndef max
#define max(a,b)   (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#endif

typedef struct AVResampleContext{
//    const AVClass *av_class;
    FELEM *filter_bank;
    int filter_length;
    int ideal_dst_incr;
    int dst_incr;
    int index;
    int frac;
    int src_incr;
    int compensation_distance;
    int phase_shift;
    int phase_mask;
    int linear;
}AVResampleContext;

// Data for specific instances of this filter
typedef struct af_resample_s{
    struct AVResampleContext *avrctx;
    int16_t *in[AF_NCH];
    int in_alloc;
    int index;

    int filter_length;
    int linear;
    int phase_shift;
    double cutoff;

    int ctx_out_rate;
    int ctx_in_rate;
    int ctx_filter_size;
    int ctx_phase_shift;
    int ctx_linear;
    double ctx_cutoff;
    int pre_downSample;
    int pre_down_coeff;
    int ctx_pre_down_Out_rate;
}af_resample_t;

void av_resample_close(AVResampleContext *c);
AVResampleContext *av_resample_init(int out_rate, int in_rate, int filter_size, int phase_shift, int linear, double cutoff);
int av_resample(AVResampleContext *c, short *dst, short *src, int *consumed, int src_size, int dst_size, int update_ctx);
int af_open(af_instance_t* af);

#endif
