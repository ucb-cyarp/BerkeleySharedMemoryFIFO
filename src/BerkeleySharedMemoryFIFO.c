//
// Project: BerkeleySharedMemoryFIFO
// Created by Christopher Yarp on 10/29/19.
// Availible at https://github.com/ucb-cyarp/BerkeleySharedMemoryFIFO
//

// BSD 3-Clause License
//
// Copyright (c) 2019, Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "BerkeleySharedMemoryFIFO.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

void initSharedMemoryFIFO(sharedMemoryFIFO_t *fifo){
    fifo->sharedName = NULL;
    fifo->sharedFD = -1;
    fifo->txSemaphoreName = NULL;
    fifo->rxSemaphoreName = NULL;
    fifo->txSem = NULL;
    fifo->rxSem = NULL;
    fifo->fifoCount = NULL;
    fifo->fifoBlock = NULL;
    fifo->fifoBuffer = NULL;
    fifo->fifoSizeBytes = 0;
    fifo->currentOffset = 0;
    fifo->fifoSharedBlockSizeBytes = 0;
    fifo->rxReady = false;
}

int producerOpenInitFIFO(char *sharedName, size_t fifoSizeBytes, sharedMemoryFIFO_t *fifo){
    fifo->sharedName = sharedName;
    fifo->fifoSizeBytes = fifoSizeBytes;
    size_t sharedBlockSize = fifoSizeBytes + sizeof(atomic_int_fast32_t);
    fifo->fifoSharedBlockSizeBytes = sharedBlockSize;

    //The producer is responsible for initializing the FIFO and releasing the Tx semaphore
    //Note: Both Tx and Rx use the O_CREAT mode to create the semaphore if it does not already exist
    //The Rx semaphore is used to block the producer from continuing after FIFO init until a consumer is started and is ready
    //---- Get access to the semaphore ----
    int sharedNameLen = strlen(sharedName);
    fifo->txSemaphoreName = malloc(sharedNameLen+5);
    strcpy(fifo->txSemaphoreName, "/");
    strcat(fifo->txSemaphoreName, sharedName);
    strcat(fifo->txSemaphoreName, "_TX");
    fifo->txSem = sem_open(fifo->txSemaphoreName, O_CREAT, S_IRWXU, 0); //Initialize to 0, the consumer will wait
    if (fifo->txSem == SEM_FAILED){
        printf("Unable to open tx semaphore\n");
        perror(NULL);
        exit(1);
    }

    fifo->rxSemaphoreName = malloc(sharedNameLen+5);
    strcpy(fifo->rxSemaphoreName, "/");
    strcat(fifo->rxSemaphoreName, sharedName);
    strcat(fifo->rxSemaphoreName, "_RX");
    fifo->rxSem = sem_open(fifo->rxSemaphoreName, O_CREAT, S_IRWXU, 0); //Initialize to 0, the consumer will wait
    if (fifo->rxSem == SEM_FAILED){
        printf("Unable to open rx semaphore\n");
        perror(NULL);
        exit(1);
    }

    //---- Init shared mem ----
    fifo->sharedFD = shm_open(sharedName, O_CREAT | O_RDWR, S_IRWXU);
    if (fifo->sharedFD == -1){
        printf("Unable to open tx shm\n");
        perror(NULL);
        exit(1);
    }

    //Resize the shared memory
    int status = ftruncate(fifo->sharedFD, sharedBlockSize);
    if(status == -1){
        printf("Unable to resize tx fifo\n");
        perror(NULL);
        exit(1);
    }

    fifo->fifoBlock = mmap(NULL, sharedBlockSize, PROT_READ | PROT_WRITE, MAP_SHARED, fifo->sharedFD, 0);
    if (fifo->fifoBlock == MAP_FAILED){
        printf("Rx mmap failed\n");
        perror(NULL);
        exit(1);
    }

    //---- Init the fifoCount ----
    fifo->fifoCount = (atomic_int_fast32_t*) fifo->fifoBlock;
    atomic_init(fifo->fifoCount, 0);

    char* fifoBlockBytes = (char*) fifo->fifoBlock;
    fifo->fifoBuffer = (void*) (fifoBlockBytes + sizeof(atomic_int_fast32_t));

    //FIFO init done
    //---- Release the semaphore ----
    sem_post(fifo->txSem);

    return sharedBlockSize;
}

int consumerOpenFIFOBlock(char *sharedName, size_t fifoSizeBytes, sharedMemoryFIFO_t *fifo){
    fifo->sharedName = sharedName;
    fifo->fifoSizeBytes = fifoSizeBytes;
    size_t sharedBlockSize = fifoSizeBytes + sizeof(atomic_int_fast32_t);
    fifo->fifoSharedBlockSizeBytes = sharedBlockSize;

    //---- Get access to the semaphore ----
    int sharedNameLen = strlen(sharedName);
    fifo->txSemaphoreName = malloc(sharedNameLen+5);
    strcpy(fifo->txSemaphoreName, "/");
    strcat(fifo->txSemaphoreName, sharedName);
    strcat(fifo->txSemaphoreName, "_TX");
    fifo->txSem = sem_open(fifo->txSemaphoreName, O_CREAT, S_IRWXU, 0); //Initialize to 0, the consumer will wait
    if (fifo->txSem == SEM_FAILED){
        printf("Unable to open tx semaphore\n");
        perror(NULL);
        exit(1);
    }

    fifo->rxSemaphoreName = malloc(sharedNameLen+5);
    strcpy(fifo->rxSemaphoreName, "/");
    strcat(fifo->rxSemaphoreName, sharedName);
    strcat(fifo->rxSemaphoreName, "_RX");
    fifo->rxSem = sem_open(fifo->rxSemaphoreName, O_CREAT, S_IRWXU, 0); //Initialize to 0, the consumer will wait
    if (fifo->rxSem == SEM_FAILED){
        printf("Unable to open rx semaphore\n");
        perror(NULL);
        exit(1);
    }

    //Block on the semaphore while the producer is initializing
    int status = sem_wait(fifo->txSem);
    if(status == -1){
        printf("Unable to wait on tx semaphore\n");
        perror(NULL);
        exit(1);
    }

    //---- Open shared mem ----
    fifo->sharedFD = shm_open(sharedName, O_RDWR, S_IRWXU);
    if(fifo->sharedFD == -1){
        printf("Unable to open rx shm\n");
        perror(NULL);
        exit(1);
    }

    //No need to resize shared memory, the producer has already done that

    fifo->fifoBlock = mmap(NULL, sharedBlockSize, PROT_READ | PROT_WRITE, MAP_SHARED, fifo->sharedFD, 0);
    if(fifo->fifoBlock == MAP_FAILED){
        printf("Rx mmap failed\n");
        perror(NULL);
        exit(1);
    }

    //---- Get appropriate pointers from the shared memory block ----
    fifo->fifoCount = (atomic_int_fast32_t*) fifo->fifoBlock;

    char* fifoBlockBytes = (char*) fifo->fifoBlock;
    fifo->fifoBuffer = (void*) (fifoBlockBytes + sizeof(atomic_int_fast32_t));

    //inform producer that consumer is ready
    sem_post(fifo->rxSem);

    return sharedBlockSize;
}

//currentOffset is updated by the call
//currentOffset is in bytes
//fifosize is in bytes
//fifoCount is in bytes

//returns number of elements written
int writeFifo(void* src_uncast, size_t elementSize, int numElements, sharedMemoryFIFO_t *fifo){
    char* dst = (char*) fifo->fifoBuffer;
    char* src = (char*) src_uncast;

    if(!fifo->rxReady) {
        //---- Wait for consumer to join ---
        sem_wait(fifo->rxSem);
        fifo->rxReady = true;
    }

    bool hasRoom = false;

    size_t bytesToWrite = elementSize*numElements;

    while(!hasRoom){
        int currentCount = atomic_load(fifo->fifoCount);
        int spaceInFIFO = fifo->fifoSizeBytes - currentCount;
        //TODO: REMOVE
        if(spaceInFIFO<0){
            printf("FIFO had a negative count");
            exit(1);
        }

        if(bytesToWrite <= spaceInFIFO){
            hasRoom = true;
        }
    }

    //There is room in the FIFO, write into it
    //Write up to the end of the buffer, wrap around if nessisary
    size_t currentOffsetLocal = fifo->currentOffset;
    size_t bytesToEnd = fifo->fifoSizeBytes - currentOffsetLocal;
    size_t bytesToTransferFirst = bytesToEnd < bytesToWrite ? bytesToEnd : bytesToWrite;
    memcpy(dst+currentOffsetLocal, src, bytesToTransferFirst);
    currentOffsetLocal += bytesToTransferFirst;
    if(currentOffsetLocal >= fifo->fifoSizeBytes){
        //Wrap around
        currentOffsetLocal = 0;

        //Write remaining (if any)
        size_t remainingBytesToTransfer = bytesToWrite - bytesToTransferFirst;
        if(remainingBytesToTransfer>0){
            //Know currentOffsetLocal is 0 so does not need to be added
            //However, need to offset source by the number of bytes transfered before
            memcpy(dst, src+bytesToTransferFirst, remainingBytesToTransfer);
            currentOffsetLocal+=remainingBytesToTransfer;
        }
    }

    //Update the current offset
    fifo->currentOffset = currentOffsetLocal;

    //Update the fifoCount, do not need the new value
    atomic_fetch_add(fifo->fifoCount, bytesToWrite);

    return numElements;
}

int readFifo(void* dst_uncast, size_t elementSize, int numElements, sharedMemoryFIFO_t *fifo){
    char* dst = (char*) dst_uncast;
    char* src = (char*) fifo->fifoBuffer;

    bool hasData = false;

    size_t bytesToRead = elementSize*numElements;

    while(!hasData){
        int currentCount = atomic_load(fifo->fifoCount);
        //TODO: REMOVE
        if(currentCount<0){
            printf("FIFO had a negative count");
            exit(1);
        }

        if(currentCount >= bytesToRead){
            hasData = true;
        }
    }

    //There is enough data in the fifo to complete a read operation
    //Read from the FIFO
    //Read up to the end of the buffer and wrap if nessisary
    size_t currentOffsetLocal = fifo->currentOffset;
    size_t bytesToEnd = fifo->fifoSizeBytes - currentOffsetLocal;
    size_t bytesToTransferFirst = bytesToEnd < bytesToRead ? bytesToEnd : bytesToRead;
    memcpy(dst, src+currentOffsetLocal, bytesToTransferFirst);
    currentOffsetLocal += bytesToTransferFirst;
    if(currentOffsetLocal >= fifo->fifoSizeBytes){
        //Wrap around
        currentOffsetLocal = 0;

        //Read remaining (if any)
        size_t remainingBytesToTransfer = bytesToRead - bytesToTransferFirst;
        if(remainingBytesToTransfer>0){
            //Know currentOffsetLocal is 0 so does not need to be added to src
            //However, need to offset dest by the number of bytes transfered before
            memcpy(dst+bytesToTransferFirst, src, remainingBytesToTransfer);
            currentOffsetLocal+=remainingBytesToTransfer;
        }
    }

    //Update the current offset
    fifo->currentOffset = currentOffsetLocal;

    //Update the fifoCount, do not need the new value
    atomic_fetch_sub(fifo->fifoCount, bytesToRead);

    return numElements;
}

void cleanupHelper(sharedMemoryFIFO_t *fifo){
    void* fifoBlockCast = (void *) fifo->fifoBlock;
    if(fifo->fifoBlock != NULL) {
        int status = munmap(fifoBlockCast, fifo->fifoSharedBlockSizeBytes);
        if (status == -1) {
            printf("Error in tx munmap\n");
            perror(NULL);
        }
    }

    if(fifo->txSem != NULL) {
        int status = sem_close(fifo->txSem);
        if (status == -1) {
            printf("Error in tx semaphore close\n");
            perror(NULL);
        }
    }

    if(fifo->rxSem != NULL) {
        int status = sem_close(fifo->rxSem);
        if (status == -1) {
            printf("Error in rx semaphore close\n");
            perror(NULL);
        }
    }
}

void cleanupProducer(sharedMemoryFIFO_t *fifo){
    bool unlinkSharedBlock = fifo->fifoBlock != NULL;
    bool unlinkTxSem = fifo->txSem != NULL;
    bool unlinkRxSem = fifo->rxSem != NULL;

    cleanupHelper(fifo);

    if(unlinkSharedBlock) {
        int status = shm_unlink(fifo->sharedName);
        if (status == -1) {
            printf("Error in tx fifo unlink\n");
            perror(NULL);
        }
    }

    if(unlinkTxSem) {
        int status = sem_unlink(fifo->txSemaphoreName);
        if (status == -1) {
            printf("Error in tx semaphore unlink\n");
            perror(NULL);
        }
    }

    if(unlinkRxSem) {
        int status = sem_unlink(fifo->rxSemaphoreName);
        if (status == -1) {
            printf("Error in rx semaphore unlink\n");
            perror(NULL);
        }
    }

    if(fifo->txSemaphoreName != NULL){
        free(fifo->txSemaphoreName);
    }

    if(fifo->rxSemaphoreName != NULL){
        free(fifo->rxSemaphoreName);
    }
}

void cleanupConsumer(sharedMemoryFIFO_t *fifo) {
    cleanupHelper(fifo);

    if (fifo->txSemaphoreName != NULL) {
        free(fifo->txSemaphoreName);
    }

    if (fifo->rxSemaphoreName != NULL) {
        free(fifo->rxSemaphoreName);
    }
}

bool isReadyForReading(sharedMemoryFIFO_t *fifo){
    int32_t currentCount = atomic_load(fifo->fifoCount);
    return currentCount != 0;
}

bool isReadyForWriting(sharedMemoryFIFO_t *fifo){
    if(!fifo->rxReady){
        return false;
    }
    int32_t currentCount = atomic_load(fifo->fifoCount);
    return currentCount < fifo->fifoSizeBytes;
}