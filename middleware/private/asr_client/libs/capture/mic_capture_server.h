#ifndef MIC_CAPTURE_SERVER_H
#define MIC_CAPTURE_SERVER_H

#define MIC_CAPTURE_SERVER_FIFO "/tmp/mic_capture_fifo"

int mic_capture_disable(void);
int mic_capture_enable(void);

int mic_capture_server_register(void);

#endif
