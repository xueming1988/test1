//*****************************************************************************
// circ_buffer.c
//
// APIs for Implementing circular buffer
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
//
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//*****************************************************************************
//
//! \addtogroup wifi_audio_app
//! @{
//
//*****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// App include
#include "circ_buff.h"

static pthread_mutex_t bufmutex = PTHREAD_MUTEX_INITIALIZER;

//*****************************************************************************
//
//! Creating and Initializing the Circular Buffer
//!
//! \param ulBufferSize is the size allocated to the circular buffer.
//!
//! This function
//!        1. Allocates memory for the circular buffer.
//!     2. Initializes the control structure for circular buffer.
//!
//! \return pointer to the Circular buffer Control Structure.
//
//*****************************************************************************
tCircularBuffer *CreateCircularBuffer(long ulBufferSize)
{
    tCircularBuffer *pTempBuff;
    pTempBuff = (tCircularBuffer *)malloc(sizeof(tCircularBuffer));
    if (pTempBuff == NULL) {
        return NULL;
    }

    pTempBuff->pucBufferStartPtr = (char *)malloc(ulBufferSize);
    pTempBuff->pucReadPtr = pTempBuff->pucBufferStartPtr;
    pTempBuff->pucWritePtr = pTempBuff->pucBufferStartPtr;
    pTempBuff->ulBufferSize = ulBufferSize;
    pTempBuff->ulFreeSize = ulBufferSize;
    pTempBuff->pucBufferEndPtr = (pTempBuff->pucBufferStartPtr + ulBufferSize);

    return (pTempBuff);
}

//*****************************************************************************
//
//! Creating and Initializing the Circular Buffer
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//!
//! This function
//!    1. free memory allocated to the the circular buffer and its control
//!        structure.
//!
//! \return none
//
//*****************************************************************************
void DestroyCircularBuffer(tCircularBuffer *pCircularBuffer)
{
    if (pCircularBuffer->pucBufferStartPtr) {
        free(pCircularBuffer->pucBufferStartPtr);
    }

    free(pCircularBuffer);
}

//*****************************************************************************
//
//! return the current value of the read pointer.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//!
//! \return current value of the read pointer.
//
//*****************************************************************************
char *GetReadPtr(tCircularBuffer *pCircularBuffer)
{

    pthread_mutex_lock(&bufmutex);
    char *retPtr = pCircularBuffer->pucReadPtr;
    pthread_mutex_unlock(&bufmutex);

    return retPtr;
}

//*****************************************************************************
//
//! return the current value of the write pointer.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//!
//! \return current value of the write pointer.
//
//*****************************************************************************
char *GetWritePtr(tCircularBuffer *pCircularBuffer)
{
    pthread_mutex_lock(&bufmutex);
    char *retPtr = pCircularBuffer->pucWritePtr;
    pthread_mutex_unlock(&bufmutex);

    return retPtr;
}

//*****************************************************************************
//
//! fills the one buffer by the content of other buffer.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//! \param pucBuffer is the pointer to the buffer from where the buffer is
//!        filled.
//! \param uiPacketSize is the amount of data need to be copied from pucBuffer
//!        to the Circular Buffer.
//!
//! This function
//!        1. Copies the uiPacketSize of data from pucBuffer to the
//!        pCircularBuffer.
//!
//! \return amount of data copied.
//
//*****************************************************************************
long FillBuffer(tCircularBuffer *pCircularBuffer, char *pucBuffer, int uiPacketSize)
{

    int iSizeWirte = uiPacketSize;

    pthread_mutex_lock(&bufmutex);
    if (iSizeWirte > pCircularBuffer->ulFreeSize) {
        pthread_mutex_unlock(&bufmutex);
        return -1;
    }

    if (pCircularBuffer->pucWritePtr >= pCircularBuffer->pucReadPtr) {
        int iChunkSize = pCircularBuffer->pucBufferEndPtr - pCircularBuffer->pucWritePtr;

        if (iChunkSize > iSizeWirte) {
            iChunkSize = iSizeWirte;
        }

        memcpy(pCircularBuffer->pucWritePtr, pucBuffer, iChunkSize);

        iSizeWirte -= iChunkSize;
        pCircularBuffer->pucWritePtr += iChunkSize;
        pCircularBuffer->ulFreeSize -= iChunkSize;
        if (pCircularBuffer->pucWritePtr >= pCircularBuffer->pucBufferEndPtr) {
            pCircularBuffer->pucWritePtr = pCircularBuffer->pucBufferStartPtr;
        }
    }

    if (iSizeWirte) {
        memcpy(pCircularBuffer->pucWritePtr, (pucBuffer + uiPacketSize - iSizeWirte), iSizeWirte);
        pCircularBuffer->pucWritePtr += iSizeWirte;
        pCircularBuffer->ulFreeSize -= iSizeWirte;
    }

    pthread_mutex_unlock(&bufmutex);

    return (uiPacketSize);
}

//*****************************************************************************
//
//! reads the content of one buffer into the other.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//! \param pucBuffer is the pointer to the buffer to which the content is
//!        copied.
//! \param uiPacketSize is the amount of data need to be copied from Circular
//!        Buffer to the pucBuffer.
//!
//! This function
//!        1. Copies the uiPacketSize of data from pCircularBuffer to the
//!        pucBuffer.
//!
//! \return amount of data copied.
//
//*****************************************************************************
long ReadBuffer(tCircularBuffer *pCircularBuffer, char *pucBuffer, int uiDataSize)
{

    int iSizeRead = uiDataSize;

    pthread_mutex_lock(&bufmutex);
    if (iSizeRead > (pCircularBuffer->ulBufferSize - pCircularBuffer->ulFreeSize)) {
        pthread_mutex_unlock(&bufmutex);
        return -1;
    }

    if (pCircularBuffer->pucReadPtr > pCircularBuffer->pucWritePtr) {
        int iChunkSize = pCircularBuffer->pucBufferEndPtr - pCircularBuffer->pucReadPtr;
        if (iChunkSize > iSizeRead) {
            iChunkSize = iSizeRead;
        }

        memcpy(pucBuffer, pCircularBuffer->pucReadPtr, iChunkSize);

        iSizeRead -= iChunkSize;
        pCircularBuffer->pucReadPtr += iChunkSize;
        pCircularBuffer->ulFreeSize += iChunkSize;

        if (pCircularBuffer->pucReadPtr >= pCircularBuffer->pucBufferEndPtr) {
            pCircularBuffer->pucReadPtr = pCircularBuffer->pucBufferStartPtr;
        }
    }

    if (iSizeRead) {
        memcpy((pucBuffer + uiDataSize - iSizeRead), pCircularBuffer->pucReadPtr, iSizeRead);
        pCircularBuffer->pucReadPtr += iSizeRead;
        pCircularBuffer->ulFreeSize += iSizeRead;
    }

    pthread_mutex_unlock(&bufmutex);

    return (uiDataSize);
}

//*****************************************************************************
//
//! fills the buffer with zeores.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//! \param uiPacketSize is the amount of space needed to be filled with zeroes.
//!
//! This function
//!        1. fill uiPacketSize amount of memory with zeroes.
//!
//! \return amount of zeroes filled.
//
//*****************************************************************************
long FillZeroes(tCircularBuffer *pCircularBuffer)
{

    pthread_mutex_lock(&bufmutex);
    memset(pCircularBuffer->pucBufferStartPtr, 0, pCircularBuffer->ulBufferSize);

    pCircularBuffer->pucReadPtr = pCircularBuffer->pucBufferStartPtr;
    pCircularBuffer->pucWritePtr = pCircularBuffer->pucBufferStartPtr;
    pCircularBuffer->ulFreeSize = pCircularBuffer->ulBufferSize;

    pthread_mutex_unlock(&bufmutex);
    return (pCircularBuffer->ulBufferSize);
}

//*****************************************************************************
//
//! check if the buffer is empty or not.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//!
//! This function
//!        1. check if the buffer is empty or not.
//!
//! \return true if buffer is empty othr wise returns false.
//
//*****************************************************************************
tboolean IsBufferEmpty(tCircularBuffer *pCircularbuffer)
{
    pthread_mutex_lock(&bufmutex);
    tboolean ret = (pCircularbuffer->ulBufferSize == pCircularbuffer->ulFreeSize);
    pthread_mutex_unlock(&bufmutex);
    return ret;
}

//*****************************************************************************
//
//! check if the specified amount of buffer is filled or not.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//! \param ulsize is the amount of data which is to checked.
//!
//! This function
//!  1. check if the buffer is filled with specified amount of data or not.
//!
//! \return true if buffer is filled with specifed amount of data or more,
//!         otherwise false.
//
//*****************************************************************************
long BufferSizeFilled(tCircularBuffer *pCircularBuffer, long ulSize)
{
    pthread_mutex_lock(&bufmutex);
    long ret = pCircularBuffer->ulBufferSize - pCircularBuffer->ulFreeSize;
    pthread_mutex_unlock(&bufmutex);
    return ret;
}

//*****************************************************************************
//
//! check if the buffer is filled less than the specified amount or not.
//!
//! \param pCircularBuffer is the pointer to the control structure for circular
//!        buffer.
//! \param ulsize is the amount of data which is to checked.
//!
//! This function
//!  1. checks if the buffer is filled less than the specified amount or not.
//!
//! \return true if buffer is filled less than the specifed amount, otherwise
//!         false.
//
//*****************************************************************************
tboolean IsBufferVacant(tCircularBuffer *pCircularBuffer, long ulSize)
{
    pthread_mutex_lock(&bufmutex);
    tboolean ret = (pCircularBuffer->ulFreeSize != 0);
    pthread_mutex_unlock(&bufmutex);
    return ret;
}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
