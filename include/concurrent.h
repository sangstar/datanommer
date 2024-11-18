//
// Created by Sanger Steel on 11/16/24.
//

#ifndef DATANOM_CONCURRENT_H
#define DATANOM_CONCURRENT_H
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>




#define BUFFER_SIZE 128
#define NUM_THREADS 5

#define MAX_FILE_SIZE 8388608

#define MAX_BUFFERS (MAX_FILE_SIZE / BUFFER_SIZE)

#define WAIT_THREAD(ctx, threads)                           \
    int err = 0;                                            \
    for (int i = 0; i < NUM_THREADS; ++i) {                 \
        err = pthread_join(ctx->threads[i], NULL);          \
    }                                                       \
    if (err != 0) {                                         \
        printf("Failure waiting on thread job completion"); \
    }                                                       \

#define CHECK_ERR(e, msg)                                 \
    if (e != 0) {                                         \
        printf("Exiting with error code %i: %s", e, msg); \
        exit(1);                                          \
    }                                                     \

typedef struct {
    char data[MAX_BUFFERS][BUFFER_SIZE];
    _Atomic int end_idx;
    _Atomic int queued;
} channel;



typedef struct {
    FILE* file;
    channel* chan;
    pthread_mutex_t mutex;
    pthread_t message_writing_thread;
    pthread_t data_reading_thread_pool[NUM_THREADS];
} context;

typedef struct {
    int idx;
    context* ctx;
} thread;


void* write_to_channel(void* arg);

channel* new_channel();

context* new_context(FILE *file);

void destroy_context(context* ctx);

int write_messages_to_channel(context* ctx);

int wait_on_writing_thread(context* ctx);

int read_data_from_channel(void* arg);

void* read_from_channel(void* arg);

#endif //DATANOM_CONCURRENT_H
