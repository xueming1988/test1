#ifndef __OPUS_ENCODER__
#define __OPUS_ENCODER__

int opus_start(void);
int opus_process_samples(char *samples, int size, int (*func)(char *, size_t));
void opus_close(void);

#endif
