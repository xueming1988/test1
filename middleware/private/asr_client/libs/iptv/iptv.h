#ifndef __IPTV_H__
#define __IPTV_H__

typedef enum {
    LINKPLAY_PLATFORM, // Linkplay platform will create shared memory
    CUSTOMER_PLATFORM  // Pineone platform will map shared memory
} IPTV_ROLE;

typedef enum {
    OPERATION_SUCCEED, // Operation succeed
    OPERATION_FAILED   // Operation failed
} IPTV_OPERATION_RESULT;

/********************************************************************
*   Function:
*       iptv_init
*   Description:
*       initialize iptv feature
*   Arguments:
*       for linkplay, it is LINKPLAY_PLATFORM(0)
*       for pipeone, it is CUSTOMER_PLATFORM(1)
*   Return:
*       OPERATION_SUCCEED(0) or OPERATION_FAILED(1)
*********************************************************************/
IPTV_OPERATION_RESULT
iptv_init(int);

/********************************************************************
*   Function:
*       iptv_uninit
*   Description:
*       quit iptv feature
*   Arguments:
*       void
*   Return:
*       OPERATION_SUCCEED(0) or OPERATION_FAILED(1)
*********************************************************************/
IPTV_OPERATION_RESULT
iptv_uninit(void);

/********************************************************************
*   Function:
*       iptv_shm_reset
*   Description:
*       initialize shared memory
*   Arguments:
*       void
*   Return:
*       no return
*********************************************************************/
void iptv_shm_reset(void);

/********************************************************************
*   Function:
*       iptv_pcm_write
*   Description:
*       write pcm data to shared memory
*   Arguments:
*       Argument 1: pcm data buffer which will be written to shared memory
*       Argument 2: pcm data buffer length
*   Return:
*       actual size written to shared memory
*********************************************************************/
size_t iptv_pcm_write(char *, size_t);

/********************************************************************
*   Function:
*       iptv_pcm_read
*   Description:
*       read pcm data from shared memory
*   Arguments:
*       Argument 1: pcm data buffer which will be read from shared memory
*       Argument 2: pcm data buffer length
*   Return:
*       actual size read from shared memory(>=0)
*       no data to read, but will have soon(-1)
*       should quit reading loop(-2)
*********************************************************************/
int iptv_pcm_read(char *, size_t);

/********************************************************************
*   Function:
*       iptv_is_alive
*   Description:
*       check iptvagent is alive or not
*   Arguments:
*       void
*   Return:
*       alive(1)
*       not alive(0)
*********************************************************************/
int iptv_is_alive(void);

/********************************************************************
*   Function:
*       iptv_cancel_recording
*   Description:
*       cancel iptvagent reading loop
*   Arguments:
*       void
*   Return:
*       no return
*********************************************************************/
void iptv_cancel_recording(void);

/********************************************************************
*   Function:
*       monitor_is_alive
*   Description:
*       check memory_monitor.sh is alive or not
*   Arguments:
*       void
*   Return:
*       alive(1)
*       not alive(0)
*********************************************************************/
int monitor_is_alive(void);

#endif