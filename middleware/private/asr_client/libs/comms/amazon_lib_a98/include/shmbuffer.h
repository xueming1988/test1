/*
 * shmbuffer.h
 *
 * Single writer, multiple reader lock-less circular buffer
 * with the ability to wait for new data
 *
 * Copyright (c) 2012-2014 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * Use is subject to license terms.
 *
 */

#ifndef SHMBUFFER_H_
#define SHMBUFFER_H_

#include "AlexaCommsAPI.h"

#include <sys/types.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Return Values
 **/
#define E_SHMBUF_ARG            (-1)        // argument error
#define E_SHMBUF_OVERWRITTEN    (-2)        // overwritten detected
#define E_SHMBUF_OTHERS         (-10)       // other errors


struct ShmbufH_t;
typedef struct ShmbufH_t* ShmbufHandle;

/**
 * Use this to create a new shared memory block.
 *
 * Returns -1 on error
 */
ALEXACOMMSLIB_API int ShmbufCreate(const char* name, size_t msg_size, uint32_t max_msgs);


/**
 * Use this to delete a shared memory block.
 *
 * Returns -1 on error
 */
ALEXACOMMSLIB_API int ShmbufDelete(const char* name);


enum SHMBUF_MODE {
    SHUMBUF_READ, SHUMBUF_WRITE, SHUMBUF_READ_WRITE
};

/**
 * Open an existing buffer.
 *
 * Returns a handle, which should be non zero.
 * Returns NULL on error;
 */
ALEXACOMMSLIB_API ShmbufHandle ShmbufOpen(const char* name, enum SHMBUF_MODE mode);


/**
 * close an existing buffer.

 * Returns 0 on success, or -1 on error.
 */
ALEXACOMMSLIB_API void ShmbufClose(ShmbufHandle pshm);


/**
 * Write data to a  circular buffer.
 *
 * Returns the number of messages written.
 */
ALEXACOMMSLIB_API int ShmbufWrite(ShmbufHandle handle, const unsigned char* data, int num_msgs);

/**
 * Read data from a circular buffer. Readers have to keep up with writers.
 * Data is not preserved for them.
 *
 * Returns the number of messsages ead into the buffer.
 * Will return immediately even if there are no dwbytes to read
 */
ALEXACOMMSLIB_API int ShmbufRead(ShmbufHandle handle, unsigned char* dest, int num_msgs);


/**
 * Same as Read, but will wait for data to become available for at least the timeout
 *
 * Returns the number of messsages read into the buffer.
 *
 * timeout_ms: 0     - wait for ever
 *             other - time in ms to wait
 *
 * Return E_SHMBUF_ARG          if argument error
 *        E_SHMBUF_OVERWRITTEN  if overwritten happens
 *        E_SHMBUF_OTHERS       if other errors
 */
ALEXACOMMSLIB_API int ShmbufReadWait(ShmbufHandle handle, unsigned char* dest, int num_msgs, int timeout_ms);
/*
 * Same as ReadWait, but passes back direct pointers to shmbuffer, no memcpy
 *
 * Returns the number of messsages read into the buffer.
 *
 * poutPtr1, int* msgs1 - return shm ptr and num_msgs available for read
 * poutPtr2, int* msgs2 - return 0 unless read wraps around,
 *           when wrapped around this returns ptr to the beginning of the buffer
 *           and number of messaqges to read from the beginning of the buffer
 * timeout_ms: 0 wait for ever
 * Return E_SHMBUF_ARG if argument error
 *        E_SHMBUF_OVERWRITTEN if overwritten happens
 *        E_SHMBUF_OTHERS if other error
 */
ALEXACOMMSLIB_API int ShmbufReadWaitPtr(ShmbufHandle handle, int max_num_msgs,
                                           unsigned char** poutPtr1, int* msgs1,
                                           unsigned char** poutPtr2, int* msgs2,
                                           int timeout_ms);

/**
  * If the writer starts well ahead of the reader, the reader can use this API
 * to set the right starting point. Also can be used when overwritten happens.
 * If the writer hasn't written enough data, the reader seq_num will be set to 0
 *
 * Recommend to call this before the 1st read.
 *
 * Pre-roll: number of messages before the current write_seqnum
 *
 * Return the new reader's sequence number.
 */
ALEXACOMMSLIB_API uint64_t ShmbufReadStart(ShmbufHandle handle, unsigned int pre_roll);


/**
 * A reader can check to see how much data is available to read using this call.
 *
 * returns the number of messages available.
 * returns -1 if obvious overwritten happens
 */
ALEXACOMMSLIB_API int ShmbufDataAvail(ShmbufHandle handle);


/**
 * returns the message size in unit of bytes.
 */
ALEXACOMMSLIB_API int ShmbufGetMsgSize(ShmbufHandle handle);

/**
 * Gets sequence number for the reader; Only work for a reader's handle.
 *
 * Argument: seqnum is in unit of messages.
 *
 * Return E_SHMBUF_ARG          if argument error
 *        E_SHMBUF_OTHERS       if other errors
 *
 * Note: NO E_SHMBUF_OVERWRITTEN will be returned.
 *
 * There are two use cases for this function.
 *  a) Called immediately before ShmbufReadWait(). Rely on the return value
 *     of ShmbufReadWait() to tell if there is overwritten.
 *
 *  b) If called after ShmbufReadWait(), seqnum will return the seqnum for the
 *  next read.
 *
 */
ALEXACOMMSLIB_API int ShmbufGetReaderSeqNum(ShmbufHandle handle, uint64_t* seqnum);


/**
 * Set sequence number for the reader; Only work for a reader's handle.
 *
 * Argument: seqnum is in unit of messages.
 *
 * Return E_SHMBUF_ARG          if argument error
 *        E_SHMBUF_OVERWRITTEN  if over written / out of range.
 *        E_SHMBUF_OTHERS       if other errors
 *
 *  !!! NOTE !!!
 * This function is NOT multi-thread safe, meaning that you must use it
 * at the same thread as ShmbufReadStart()/ShmbufReadWait()/ShmbufRead(). *
 */
ALEXACOMMSLIB_API int ShmbufSetReaderSeqNum(ShmbufHandle handle, uint64_t seqnum);

/**
 * Gets sequence number for the writer
 *
 * Argument: seqnum is in unit of messages.
 *
 * Return E_SHMBUF_ARG          if argument error
 *        E_SHMBUF_OTHERS       if other errors
 */
ALEXACOMMSLIB_API int ShmbufGetWriterSeqNum(ShmbufHandle handle, uint64_t* seqnum);


struct ShmbufPoll;
typedef struct ShmbufPoll ShmbufPoll_t;
typedef ShmbufPoll_t* ShmbufPollHandle;

/**
 * Create an object that provides a file descriptor to poll the buffer for data
 *
 * This essentially exposes the internal "data available" condition
 * variable as a file descriptor. Obviously, this is only useful for
 * readers.
 *
 * Note that this will create a thread in your process that listens
 * for the condition variable.
 *
 * Any object created by this must be freed by ShmbufPollDestroy().
 *
 * @param qHandle ShmbufHandle of the buffer to operate on
 * @param prio set to 0 to give the thread SCHED_OTHER, set to non-zero
 * to give the thread SCHED_FIFO and set the priority
 * @return A ShmbufPollHandle that you may use to interact with the object.
 */
ALEXACOMMSLIB_API ShmbufPollHandle ShmbufPollCreate(ShmbufHandle qHandle, int prio);

/**
 * Shut down the ShmbufPoll thread and deallocate its resources
 *
 * When this function returns, the thread will have exited and been
 * joined.
 *
 * @param pollHandle the ShmbufPollHandle object
 */
ALEXACOMMSLIB_API void ShmbufPollDestroy(ShmbufPollHandle pollHandle);

/**
 * Access the file descriptor for giving to poll(2)
 *
 * Do not directly read from or write to this file descriptor.
 *
 * When using with poll(2), you must do it the following way:
 *
 *    1. When adding to poll(2), poll for POLLIN
 *
 *    2. If poll(2) returns POLLIN in revents, then you must
 *       call ShmbufPollAckFd().  If you fail to do so, then poll(2)
 *       will continue to wake with POLLIN.
 *
 *    3. The signal is "edge sensitive." This means that you must
 *       always check the status of the buffer between calling
 *       ShmbufPollAckFd() and poll(2). It does not wake when data/space
 *       is available... it wakes when the the other process writes/reads
 *       to/from the buffer.
 *
 *    4. It is safe to call ShmbufPollAckFd() even if poll(2) did not
 *       return POLLIN for the file descriptor. However, it is
 *       still imperative that you check the status of the buffer before
 *       re-entering poll(2).
 *
 * @param pollHandle the ShmbufPollHandle object
 * @param A valid file descriptor
 */
ALEXACOMMSLIB_API int ShmbufPollGetFd(ShmbufPollHandle pollHandle);

/**
 * Acknowledge the POLLIN event on the file descriptor
 *
 * This must be called after poll(2) returns POLLIN or you will
 * continue to receive wake-ups. After this is called, you will not
 * receive another wake-up until the other process manipulates the
 * buffer.
 *
 * @param pollHandle the ShmbufPollHandle object
 */
ALEXACOMMSLIB_API void ShmbufPollAckFd(ShmbufPollHandle pollHandle);

#ifdef __cplusplus
}
#endif

#endif // SHMBUFFER_H_
