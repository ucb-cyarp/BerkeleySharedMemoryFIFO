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

#ifndef BERKELEYSHAREDMEMORYFIFO_H
#define BERKELEYSHAREDMEMORYFIFO_H

#include <stdatomic.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct{
    char *sharedName;
    int sharedFD;
    char* txSemaphoreName;
    char* rxSemaphoreName;
    sem_t *txSem;
    sem_t *rxSem;
    atomic_int_fast32_t* fifoCount;
    volatile void* fifoBlock;
    volatile void* fifoBuffer;
    size_t fifoSizeBytes;
    size_t fifoSharedBlockSizeBytes;
    size_t currentOffset;
    bool rxReady;
} sharedMemoryFIFO_t;

void initSharedMemoryFIFO(sharedMemoryFIFO_t *fifo);

int producerOpenInitFIFO(char *sharedName, size_t fifoSizeBytes, sharedMemoryFIFO_t *fifo);

int consumerOpenFIFOBlock(char *sharedName, size_t fifoSizeBytes, sharedMemoryFIFO_t *fifo);

//NOTE: this function blocks until numElements can be written into the FIFO
int writeFifo(void* src, size_t elementSize, int numElements, sharedMemoryFIFO_t *fifo);

int readFifo(void* dst, size_t elementSize, int numElements, sharedMemoryFIFO_t *fifo);

void cleanupProducer(sharedMemoryFIFO_t *fifo);

void cleanupConsumer(sharedMemoryFIFO_t *fifo);

bool isReadyForReading(sharedMemoryFIFO_t *fifo);

bool isReadyForWriting(sharedMemoryFIFO_t *fifo);

#endif //BERKELEYSHAREDMEMORYFIFO_H
