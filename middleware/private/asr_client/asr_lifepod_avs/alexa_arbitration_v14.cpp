#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "alexa_arbitration_v14.h"
#include "alexa_debug.h"

unsigned int channels = 1;
unsigned int cutoff_freq = 1250;
unsigned int sample_rate = 16000;

#define MY_PI (3.14159)
#define FRAME_LEN 256
#define BYTES_EACH_SAMPLE 2

static int multoint(float data)
{
    return (int)(data * 1024); // data<<10
}

static int highpass_filter(short *input_buff, float cutoff_freq, float sample_rate, int samples_num,
                           short *filtered_buff)
{
    int i = 0;
    float a1, a2, a3, b1, b2;
    int inta1, inta2, inta3, intb1, intb2;

    float r = sqrt(2);
    float c = tan(cutoff_freq * MY_PI / sample_rate);

    a1 = 1.0 / (1.0 + r * c + c * c);
    a2 = -2 * a1;
    a3 = a1;
    b1 = 2.0 * (c * c - 1.0) * a1;
    b2 = (1.0 - r * c + c * c) * a1;

    inta1 = multoint(a1);
    inta2 = multoint(a2);
    inta3 = multoint(a3);
    intb1 = multoint(b1);
    intb2 = multoint(b2);

    filtered_buff[0] = input_buff[0];
    filtered_buff[1] = input_buff[1];
    for (i = 2; i < samples_num; i++) {
        filtered_buff[i] =
            (inta1 * input_buff[i] + inta2 * input_buff[i - 1] + inta3 * input_buff[i - 2] -
             intb1 * filtered_buff[i - 1] - intb2 * filtered_buff[i - 2]) >>
            10;
    }
    return 0;
}

// highpass filter process
static int hpfilter_process(char *pcmin_name, char *pcmout_name)
{
#ifndef ENERGY_TEST
    ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[ENTER hpfilter_process]\n");
#endif
    int ret = 0;
    FILE *fp_pcmin = NULL;
    FILE *fp_pcmout = NULL;
    struct stat statpcm;
    int samples_num = 0;
    short *pcm_src = NULL;
    short *pcm_des = NULL;

    fp_pcmin = fopen(pcmin_name, "rb");
    if (!fp_pcmin) {
        fprintf(stderr, "[%s,%d]open pcmin_name file %s failed\n", __FUNCTION__, __LINE__,
                pcmin_name);
        ret = -1;
        goto EXIT;
    }

    fp_pcmout = fopen(pcmout_name, "wb");
    if (!fp_pcmout) {
        fprintf(stderr, "[%s,%d]open pcmout_name file %s failed\n", __FUNCTION__, __LINE__,
                pcmout_name);
        ret = -1;
        goto EXIT;
    }

    stat(pcmin_name, &statpcm);
    samples_num = statpcm.st_size / 2;

    printf("%s file size = %d, sample num = %d\n", pcmin_name, (int)statpcm.st_size, samples_num);

    pcm_src = (short *)malloc(statpcm.st_size);
    if (!pcm_src) {
        fprintf(stderr, "[%s,%d]malloc pcm_src failed\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto EXIT;
    }

    pcm_des = (short *)malloc(statpcm.st_size);
    if (!pcm_des) {
        fprintf(stderr, "[%s,%d]malloc pcm_des failed\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto EXIT;
    }
    // todo it could be too big, need modify!
    ret = fread(pcm_src, 1, statpcm.st_size, fp_pcmin);
    ret = highpass_filter(pcm_src, cutoff_freq, sample_rate, samples_num, pcm_des);
    fwrite(pcm_des, 1, statpcm.st_size, fp_pcmout);

#ifndef ENERGY_TEST
    ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[EXIT hpfilter_process]\n");
#endif

EXIT:
    if (pcm_src)
        free(pcm_src);
    if (pcm_des)
        free(pcm_des);
    if (fp_pcmin)
        fclose(fp_pcmin);
    if (fp_pcmout)
        fclose(fp_pcmout);
    return ret;
}

static int format_output(char *fp_vadoutfile, char *fp_vadfile, char *fp_hpfile)
{
    int i = 0;
    int ret = 0;
    int frame_count = 0;
    short frame_buff_hpdata[FRAME_LEN * BYTES_EACH_SAMPLE];
    char vad_value[4];
    FILE *fp_vad = NULL;
    FILE *fp_hp = NULL;
    FILE *fp_vadout = NULL;

    fp_vad = fopen(fp_vadfile, "r");
    if (!fp_vad) {
        fprintf(stderr, "[%s,%d]open vad file %s failed\n", __FUNCTION__, __LINE__, fp_vadfile);
        ret = -1;
        goto EXIT;
    }

    fp_hp = fopen(fp_hpfile, "rb");
    if (!fp_hp) {
        fprintf(stderr, "[%s,%d]open highpass file %s failed\n", __FUNCTION__, __LINE__, fp_hpfile);
        ret = -1;
        goto EXIT;
    }

    fp_vadout = fopen(fp_vadoutfile, "w+");
    if (!fp_vadout) {
        fprintf(stderr, "[%s,%d]open fp_vadout file %s failed\n", __FUNCTION__, __LINE__,
                fp_vadoutfile);
        ret = -1;
        goto EXIT;
    }

    // format
    while (!feof(fp_vad)) {
        if (!fgets(vad_value, 4, fp_vad)) {
            break;
        }
        ret = fread(frame_buff_hpdata, BYTES_EACH_SAMPLE, FRAME_LEN, fp_hp);
        fprintf(fp_vadout, "Frame\n");
        fprintf(fp_vadout, "%d\n", ++frame_count);
        for (i = 0; i < FRAME_LEN; i++) {
            fprintf(fp_vadout, "%d ", frame_buff_hpdata[i]);
        }
        fprintf(fp_vadout, "\n%s", vad_value);
    }

EXIT:
    if (fp_vad) {
        fclose(fp_vad);
    }
    if (fp_hp) {
        fclose(fp_hp);
    }
    if (fp_vadout) {
        fclose(fp_vadout);
    }

    return ret;
}

#if 0
//处理buff, 转换成float型数组,返回vad的值
static int vad_process_frame(VadVars *vadstate, short *frame_buff_src)
{
    int i = 0;
    int vad = 0;
    short temp;

    float *frame_buff_float = NULL;
    frame_buff_float = (float *)malloc(FRAME_LEN * sizeof(float) * channels);
    if(!frame_buff_float) {
        fprintf(stderr, "[%s,%d]malloc frame_buff_float failed\n", __FUNCTION__, __LINE__);
        goto EXIT;
    }
    //wb_vad算法输入是float 型,需要转换成float
    for(i = 0; i < FRAME_LEN; i++) {
        frame_buff_float[i] = 0;
        temp = 0;
        memcpy(&temp, frame_buff_src + i, sizeof(short));
        frame_buff_float[i] = temp;
    }

    vad = vad_cal_frame(vadstate, frame_buff_float);

EXIT:
    if(frame_buff_float)
        free(frame_buff_float);
    return vad;
}

//源数据是buffer, 每次取一个frame做vad计算
static int vad_process_buff(char *pcm_buff, char *vadout_name)
{
    int rt = 0;
    int size_per_frame = 0;
    int frame_count = 0;
    int vad_of_frame = 0;
    char *frame_buff_src = NULL;


    FILE *fp_vadout = fopen(vadout_name, "w");
    if(!fp_vadout) {
        fprintf(stderr, "[%s,%d]open vadout file %s failed\n", __FUNCTION__, __LINE__, vadout_name);
        return -1;
    }

    //todo if need
}


//源数据是pcm文件，每次取一个frame做vad计算
static int vad_process_pcmfile(char *vadin_name, char *vadout_name)
{
    ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[ENTER vad_process_pcmfile]\n");
    int ret = 0;
    int size_per_frame = 0;
    int frame_count = 0;
    int vad_of_frame = 0;
    char *frame_buff_src = NULL;
    short *frame_buff_hpdata = NULL;
    float *frame_buff_float = NULL;
    VadVars *vadstate = NULL;

    FILE *fp_vadin = fopen(vadin_name, "rb");
    if(!fp_vadin) {
        fprintf(stderr, "[%s,%d]open record file %s failed\n", __FUNCTION__, __LINE__, vadin_name);
        ret = -1;
        goto EXIT;

    }

    FILE *fp_vadout = fopen(vadout_name, "w");
    if(!fp_vadout) {
        fprintf(stderr, "[%s,%d]open vadout file %s failed\n", __FUNCTION__, __LINE__, vadout_name);
        ret = -1;
        goto EXIT;

    }

    FILE *fp_hpdata = fopen(ALEXA_HPFILTERED_FILE, "rb");

    size_per_frame = FRAME_LEN * bytes_each_sample * channels; /* two bytes each sample*/

    frame_buff_src = (short *)malloc(size_per_frame);
    if(!frame_buff_src) {
        fprintf(stderr, "[%s,%d]malloc frame_buff_src failed\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto EXIT;

    }

    frame_buff_hpdata = (short *)malloc(size_per_frame);
    if(!frame_buff_hpdata) {
        fprintf(stderr, "[%s,%d]malloc frame_buff_hpdata failed\n", __FUNCTION__, __LINE__);
        ret = -1;
        goto EXIT;

    }

    /*vad init*/
    ret = vad_setup(&vadstate);

    while(fread(frame_buff_src, 1, size_per_frame, fp_vadin)) {
        fread(frame_buff_hpdata, 1, size_per_frame, fp_hpdata);
        format_output(fp_vadout, ++frame_count, frame_buff_hpdata, vad_of_frame);
    }
    vad_destroy(&vadstate);

EXIT:
    if(frame_buff_src)
        free(frame_buff_src);
    if(frame_buff_hpdata)
        free(frame_buff_hpdata);
    if(fp_vadin)
        fclose(fp_vadin);
    if(fp_vadout)
        fclose(fp_vadout);
    if(fp_hpdata)
        fclose(fp_hpdata);

    ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[EXIT vad_process_pcmfile]\n");

    return ret;
}
static int vad_process_wavfile(char *in_fd_name, char *vadout_name)
{}
#endif

extern int Frame_Energy_Computing(char *fileName, unsigned int *engr_voice,
                                  unsigned int *engr_noise);
extern int vad_process_pcmfile(char *vadin_name, char *vadout_name, const unsigned int frameSize);

int alexa_arbitration(char *vadin_name, unsigned int *engr_voice, unsigned int *engr_noise)
{
    int ret = 0;
    printf("[alexa_arbitration] enter\n");

    /* Step 1: VAD processing */
    ret = vad_process_pcmfile(vadin_name, (char *)ALEXA_VADVAL_FILE, FRAME_LEN);
    if (ret < 0) {
        goto EXIT;
    }

    /* Step 2: highpass filter process */
    ret = hpfilter_process(vadin_name, (char *)ALEXA_HPFILTERED_FILE);
    if (ret < 0) {
        goto EXIT;
    }

    /* Step 3:  Compute energies */
    format_output((char *)ALEXA_VADOUT_FILE, (char *)ALEXA_VADVAL_FILE,
                  (char *)ALEXA_HPFILTERED_FILE);
    ret = Frame_Energy_Computing((char *)ALEXA_VADOUT_FILE, engr_voice, engr_noise);
    printf("[alexa_arbitration] engr_voice = %u,\t negr_noise = %u, SNR = %f\n", *engr_voice,
           *engr_noise, sqrt((*engr_voice * 1.0) / (*engr_noise)));

EXIT:
    return ret;
}

#ifdef ENERGY_TEST
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: energy_test [pcmfile] \n");
        return -1;
    }
    unsigned int engr_voice, engr_noise;

    alexa_arbitration(argv[1], &engr_voice, &engr_noise);
}
#endif
