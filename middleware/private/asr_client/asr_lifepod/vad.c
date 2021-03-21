// TODO: some times the vad checked error
#define VAD_SKIP_START_MS 800
#define VAD_SKIP_START_MS_FAR_FIELD 0

//#define VAD_SKIP_START_MS 600

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#ifdef X86
#define wiimu_log(x, arg...) printf(#arg)
#define PRINT_DEBUG 0
#endif
uint32_t record_bytes = 0;
#define SPEECH_TIMEOUT_MS 6000

#define SAMPLE_RATE 16000
#define SILENCE_THRESOLD 120                /*30*/
#define SILENCE_THRESOLD_FAR 360 /* 1000 */ // TODO: maybe should be changed
#define MS_TO_BYTES(x) (SAMPLE_RATE * 2 * (x) / 1000)
#define BYTES_TO_MS(x) (1000 * (x) / 2 / SAMPLE_RATE)
#define SAMPLE_TO_MS(x) (1000 * (x) / SAMPLE_RATE)
#define SAMPLES_TO_MS(x) ((x)*1000 / SAMPLE_RATE)
#define VOICE_COUNT_MIN 3

int16_t *period_samples = 0;

#define CHUNK_TIME_MS 64
#define PEROID_MS 640
#define PEROID_MS_FAR 832

uint32_t samples_per_chunk = (CHUNK_TIME_MS << 4);
uint32_t threshold = SILENCE_THRESOLD;
uint32_t default_threshold = SILENCE_THRESOLD;

uint32_t num_of_chunks = 0;
uint32_t period_initialized = 0;
uint32_t chunk_initialized = 0;
uint32_t voice_detected = 0;
uint32_t period_sample_idx = 0;
uint32_t chunk_sample_idx = 0;
uint32_t period_sample_total = 0;
uint32_t chunk_sample_total = 0;
uint32_t chunk_sample_total_min = 0;
uint32_t chunk_sample_total_max = 0;
uint32_t total_sampless_parsed = 0;
uint32_t total_sampless = 0;
uint32_t first_valid_sample_got = 0;
uint32_t first_valid_sample_index = 0;

static int g_self_vad_init = 0;

extern int get_period_ms(void);
static uint32_t get_samples_per_period(void)
{
    static uint32_t _samples_per_period = 0;
    if (!_samples_per_period)
        _samples_per_period = ((float)get_period_ms() * SAMPLE_RATE / 1000);
    return _samples_per_period;
}
#define samples_per_period get_samples_per_period()

int get_voice_timeout_ms(void)
{
    if (voice_detected >= 30)
        return SPEECH_TIMEOUT_MS + 2000;
    return SPEECH_TIMEOUT_MS;
}

int get_voice_count(void) { return voice_detected; }

void self_vad_reset(void)
{
    period_initialized = 0;
    chunk_initialized = 0;
    // voice_detected = 0;
    period_sample_idx = 0;
    chunk_sample_idx = 0;
    period_sample_total = 0;
    chunk_sample_total = 0;
    // chunk_sample_total_min = 0;
    chunk_sample_total_max = 0;
    total_sampless_parsed = 0;
}

int32_t self_vad_process(int16_t *data, int32_t *len, int ignore_invalid_start_samples,
                         int vad_min_samples, int voice_min_ms)
{
    int i;
    int vad_detected = 0;
    int negative_default_threshold = -1 * default_threshold;

    // ignore the  invalid  data at the begining
    if (ignore_invalid_start_samples && !first_valid_sample_got) {
        for (i = 0; i < *len; i++) {
            if (data[i] > default_threshold || data[i] < negative_default_threshold) {
                first_valid_sample_got = 1;
                break;
            }
        }
        first_valid_sample_index += i;
        printf("===%s first_valid_sample_got=%d first_valid_sample=%d i=%d\n", __FUNCTION__,
               first_valid_sample_got, first_valid_sample_index, i);
        if (!first_valid_sample_got) {
            *len = 0;
            return 0;
        }
        data += i;
        *len = *len - i;
    }

    if (period_samples == 0) {
        printf("Fatal error, %s %d\n", __func__, __LINE__);
        return -1;
    }

    for (i = 0; i < *len; i++) {
        int chunk_1st_sample_idx =
            (period_sample_idx + samples_per_period - samples_per_chunk) % samples_per_period;
        uint16_t period_first_sample = period_samples[period_sample_idx];
        uint16_t chunk_first_sample = period_samples[chunk_1st_sample_idx];
        uint16_t last_sample;
        int16_t sample = data[i];
        if (sample < 0)
            last_sample = -sample;
        else
            last_sample = sample;

        if (!chunk_initialized) {
            chunk_sample_total += last_sample;
            if (period_sample_idx >= samples_per_chunk) {
                chunk_initialized = 1;
                if (chunk_sample_total_min == 0 || chunk_sample_total_min > chunk_sample_total)
                    chunk_sample_total_min = chunk_sample_total;
                chunk_sample_total_max = chunk_sample_total;
#ifdef VAD_DEBUG
                printf("self_vad_process chunk inited %u min=%d\n", chunk_sample_total,
                       chunk_sample_total_min / samples_per_chunk);
#endif
            }
        } else {
            if (chunk_sample_total < chunk_first_sample) {
                printf("Fatal error at line %d, %u %u\n", __LINE__, chunk_sample_total,
                       chunk_first_sample);
                return -1;
            }
            chunk_sample_total -= chunk_first_sample;
            chunk_sample_total += last_sample;

            if (chunk_sample_total_min > chunk_sample_total || chunk_sample_total_min == 0) {
                chunk_sample_total_min = chunk_sample_total;
                // if (chunk_sample_total_min/samples_per_chunk > default_threshold) {
                //  threshold = (chunk_sample_total_min/samples_per_chunk) * 1.4;
                //}

                if ((chunk_sample_total_min >> 10) > default_threshold) {
                    threshold = (chunk_sample_total_min + (chunk_sample_total_min >> 1)) >> 10;
                }
            }
            if (chunk_sample_total_max < chunk_sample_total)
                chunk_sample_total_max = chunk_sample_total;
            int period_sample = period_sample_idx;
            if (period_initialized)
                period_sample = samples_per_period;
            // if( chunk_sample_total_min/samples_per_chunk < threshold14)
            //{
            //  chunk_sample_total_min = samples_per_chunk* threshold14;
            //  if( threshold < chunk_sample_total_min*1.4 )
            //      threshold = (chunk_sample_total_min/samples_per_chunk) * 1.4;
            //}
            if ((chunk_sample_total_min >> 10) <= default_threshold) {
                chunk_sample_total_min = (default_threshold << 10);
                threshold = (chunk_sample_total_min + (chunk_sample_total_min >> 1)) >> 10;
            }

            // if (chunk_sample_total_max/2 >= chunk_sample_total_min
            //      &&(chunk_sample_total/2 > chunk_sample_total_min
            //      ||(period_sample_total/period_sample) >
            //      (chunk_sample_total_min/samples_per_chunk)*2)
            //      &&(chunk_sample_total/samples_per_chunk > threshold
            //      ||period_sample_total/period_sample > threshold)
            if ((chunk_sample_total_max >> 1) >= chunk_sample_total_min &&
                ((chunk_sample_total >> 1) > chunk_sample_total_min ||
                 (period_sample_total / period_sample) > (chunk_sample_total_min >> (10 - 1))) &&
                ((chunk_sample_total >> 10) > threshold ||
                 period_sample_total / period_sample > threshold)) {

                if (SAMPLES_TO_MS(total_sampless) > voice_min_ms)
                    voice_detected += 1;
                else
                    printf("ignore voice %d record_bytes=%d(%d ms)(avg %d, max %d, min %d thres "
                           "%d) min_ms/total= %d/%d...\n",
                           voice_detected, record_bytes, BYTES_TO_MS(record_bytes),
                           chunk_sample_total / samples_per_chunk,
                           chunk_sample_total_max / samples_per_chunk,
                           chunk_sample_total_min / samples_per_chunk, threshold, voice_min_ms,
                           SAMPLES_TO_MS(total_sampless));
#ifdef VAD_DEBUG
                printf("self_vad_process Voice detected %d record_bytes=%d(%d ms)(avg %d, max %d, "
                       "min %d thres %d)...\n",
                       voice_detected, record_bytes, BYTES_TO_MS(record_bytes),
                       chunk_sample_total / samples_per_chunk,
                       chunk_sample_total_max / samples_per_chunk,
                       chunk_sample_total_min / samples_per_chunk, threshold);
#endif
                self_vad_reset();
                ++total_sampless_parsed;
                ++total_sampless;
                continue;
            }
        }

        if (!period_initialized) {
            period_sample_total += last_sample;
            if (period_sample_idx + 1 == samples_per_period) {
                period_initialized = 1;
#ifdef VAD_DEBUG
                printf("self_vad_process Period initialized record_bytes=%d(%d ms) min=%d "
                       "threhold=%d\n",
                       record_bytes, BYTES_TO_MS(record_bytes), threshold,
                       chunk_sample_total_min >> 10);
#endif
            }
        } else {
            if (period_sample_total < period_first_sample) {
                printf("Fatal error at line %d, %u %u\n", __LINE__, period_sample_total,
                       last_sample);
                return -1;
            }
            period_sample_total -= period_first_sample;
            period_sample_total += last_sample;

            // if (voice_detected >= VOICE_COUNT_MIN && !vad_detected
            //  && total_sampless_parsed > vad_min_samples
            //  && period_sample_total/samples_per_period < threshold
            //  && chunk_sample_total/samples_per_chunk < threshold
            //  && period_sample_total/samples_per_period < threshold*2
            //  ) {
            if (voice_detected >= VOICE_COUNT_MIN && !vad_detected &&
                total_sampless_parsed > vad_min_samples &&
                period_sample_total / samples_per_period < threshold &&
                (chunk_sample_total >> 10) < threshold &&
                period_sample_total / samples_per_period < (threshold << 1)) {
                printf("self_vad_process VAD done, exiting voice_detected=%d record_bytes=%d(%d "
                       "ms)...\n",
                       voice_detected, record_bytes, BYTES_TO_MS(record_bytes));
                printf("period_avg %d chunk_avg %d threshold %d voice_detected=%d\n",
                       period_sample_total / samples_per_period,
                       chunk_sample_total / samples_per_chunk, threshold, voice_detected);
                vad_detected = 1;
            }
        }

        period_samples[period_sample_idx] = last_sample;
        period_sample_idx = (period_sample_idx + 1) % samples_per_period;
        if (vad_detected)
            break;
        ++total_sampless_parsed;
        ++total_sampless;
    }
    // total_sampless_parsed += i;
    // total_sampless += i;
    *len = i;
    return vad_detected;
}

#ifdef X86
void write_to_excel(char *file_name, int size, int check_pos, int result)
{
    if (file_name) {
        FILE *fp = fopen("./result/result.xls", "a+");
        if (fp) {
            fprintf(fp, "%s\t%d\t%d\t%s\n", file_name, size, check_pos,
                    result ? "Check OK" : "Check Failed");
            fclose(fp);
        }
    }
}

int get_period_ms(void)
{
#ifdef ASR_FAR_FIELD
    return PEROID_MS_FAR;
#else
    return PEROID_MS;
#endif
}

int main(int argc, char **argv)
{
    int i;
    int file_size = 0;
    int read_size = 0;
    int vad = 1;

    int32_t ret = 0;
    int32_t write_size = 0;
    unsigned char *buf = 0;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s input_file output_file skip_ms\n", argv[0]);
        return -1;
    }
    FILE *in_file = fopen(argv[1], "rb");
    FILE *out_file = fopen(argv[2], "wb");
    int skip_ms = atoi(argv[3]);

    if (in_file == 0 || out_file == 0)
        goto exit;
#ifdef ASR_FAR_FIELD
    default_threshold = SILENCE_THRESOLD_FAR;
#else
    default_threshold = SILENCE_THRESOLD;
#endif

    printf("Sampling reate %d\n", SAMPLE_RATE);
    printf("Chunk time in minisecond %d\n", CHUNK_TIME_MS);
    printf("samples_per_chunk %d\n", samples_per_chunk);
    printf("samples_per_period %d\n", samples_per_period);
    printf("skip_ms %d\n", skip_ms);

    if (samples_per_period != (samples_per_period / samples_per_chunk) * samples_per_chunk) {
        printf("PEROID_MS must be multiple of CHUNK_TIME_MS, %f\n",
               (float)samples_per_period / samples_per_chunk);
        goto exit;
    }
    num_of_chunks = samples_per_period / samples_per_chunk;
    period_samples = malloc(sizeof(*period_samples) * samples_per_period);
    if (period_samples)
        goto exit;

    fseek(in_file, 0L, SEEK_END);
    file_size = (int)ftell(in_file);
    if (file_size <= 0)
        goto exit;

    buf = malloc(file_size);
    if (buf == 0)
        goto exit;

    fseek(in_file, 0, SEEK_SET);
    read_size = fread(buf, 1, 44 + MS_TO_BYTES(skip_ms), in_file);
    fwrite(buf, 1, read_size, out_file);
    printf("skip read_size=%d\n", read_size);
    record_bytes = read_size;

    // fwrite(buf, 1, read_size, out_file);
    int expect_read_size = 0;

    while (1) {
        read_size = fread(buf, 1, 2972, in_file);
        if (read_size <= 0)
            break;
        record_bytes += read_size;
        read_size = read_size / 2;
        expect_read_size = read_size;
        ret = self_vad_process((int16_t *)buf, &read_size, 1,
                               VOICE_COUNT_MIN * CHUNK_TIME_MS + get_period_ms(),
                               VOICE_COUNT_MIN * CHUNK_TIME_MS + get_period_ms());
        if (ret > 0) {
            printf("VAD done\n");
            break;
        }
        if (read_size > 0) // && read_size == expect_read_size)
            fwrite(buf, 1, read_size * 2, out_file);
    }
#ifdef X86
    system("mkdir -p ./result/ok/");
    system("mkdir -p ./result/failed/");
    char out_result[256] = {0};
    if (ret > 0) {
        memset(out_result, 0x0, sizeof(out_result));
        snprintf(
            out_result, sizeof(out_result),
            "echo 'file(%s) len_ms(%d) check_vad_ms(%d) vad checked OK' >> ./result/result.log",
            argv[2], read_size / 32, record_bytes);
        system(out_result);
        write_to_excel(argv[1], read_size / 32, record_bytes, 1);
        memset(out_result, 0x0, sizeof(out_result));
        snprintf(out_result, sizeof(out_result), "cp %s ./result/ok/", argv[2]);
        system(out_result);
    } else {
        memset(out_result, 0x0, sizeof(out_result));
        snprintf(
            out_result, sizeof(out_result),
            "echo 'file(%s) len(%d) check_vad_pos(%d) vad checked Failed' >> ./result/result.log",
            argv[2], read_size / 32, record_bytes);
        system(out_result);
        write_to_excel(argv[1], read_size / 32, record_bytes, 0);
        memset(out_result, 0x0, sizeof(out_result));
        snprintf(out_result, sizeof(out_result), "cp %s ./result/failed/", argv[2]);
        system(out_result);
    }
#endif

exit:
    if (period_samples)
        free(period_samples);
    if (buf)
        free(buf);
    if (in_file)
        fclose(in_file);
    if (out_file)
        fclose(out_file);
    return 0;
}
#endif

void self_vad_init(int silence_val)
{
    if (g_self_vad_init == 0) {
        int size = 0;

        default_threshold = silence_val;
        threshold = default_threshold;
        period_initialized = 0;
        chunk_initialized = 0;
        voice_detected = 0;
        period_sample_idx = 0;
        chunk_sample_idx = 0;
        period_sample_total = 0;
        chunk_sample_total = 0;
        chunk_sample_total_min = 0;
        chunk_sample_total_max = 0;
        total_sampless_parsed = 0;
        total_sampless = 0;
        first_valid_sample_got = 0;
        first_valid_sample_index = 0;
#ifdef VAD_DEBUG
        printf("Sampling reate %d\n", SAMPLE_RATE);
        printf("Chunk time in minisecond %d\n", CHUNK_TIME_MS);
        printf("samples_per_chunk %d\n", samples_per_chunk);
        printf("samples_per_period %d\n", samples_per_period);
#endif
        if (samples_per_period != (samples_per_period / samples_per_chunk) * samples_per_chunk) {
            printf("PEROID_MS must be multiple of CHUNK_TIME_MS, %f\n",
                   (float)samples_per_period / samples_per_chunk);
            return;
        }

        g_self_vad_init = 1;

        num_of_chunks = samples_per_period / samples_per_chunk;
        size = sizeof(*period_samples) * samples_per_period;
        period_samples = malloc(size);
        memset(period_samples, 0, size);

#ifndef X86
        g_start_time = tickSincePowerOn();
#endif
    }
}

void self_vad_uninit(void)
{
    if (g_self_vad_init == 1) {
        if (period_samples) {
            free(period_samples);
            period_samples = NULL;
        }
        g_self_vad_init = 0;
    }
}
