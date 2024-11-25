//
// Created by Sanger Steel on 11/16/24.
//

#ifndef DATANOMMER_CONCURRENT_H
#define DATANOMMER_CONCURRENT_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include "tasks.h"
#include "channels.h"


#define BUFFER_SIZE 128
#define NUM_THREADS 5

#define MAX_FILE_SIZE 8388608

#define MAX_BUFFERS (MAX_FILE_SIZE / BUFFER_SIZE)

#define WAIT_THREAD(ctx, threads)                              \
    int err = 0;                                               \
    for (int i = 0; i < NUM_THREADS; ++i) {                    \
        err = pthread_join(ctx->threads[i], NULL);             \
    }                                                          \
    if (err != 0) {                                            \
        printf("Failure waiting on worker_t job completion");  \
    }                                                          \

#define CHECK_ERR(e, msg)                                      \
    if (e != 0) {                                              \
        printf("Exiting with error code %i: %s", e, msg);      \
        exit(1);                                               \
    }                                                          \

#define CHECK_MALLOC(malloc_data, msg)                         \
    if (!malloc_data) {                                        \
       perror(msg);                                            \
       exit(1);                                                \
       }                                                       \



typedef struct context_t {
    int num_additional_channels;
    FILE *file;
    channel_t *writing_channel;
    channel_t **additional_channels;
    pthread_mutex_t mutex;
    pthread_t message_writing_thread;
} context_t;


typedef struct job_t {
    int idx;
    context_t *context;

    // This is meant to refer to a specific input_channel
    // that either is filled with values or being written to
    // asynchronously
    channel_t *input_channel;
    channel_t *output_channel;
    pthread_t thread_pool[NUM_THREADS];
    worker_t **workers;

    char *(*func)(char *, char *);
} job_t;


context_t *new_context(FILE *file, int num_channels);

void destroy_context(context_t *ctx);

int wait_on_writing_thread(context_t *ctx);

void *perform_queued_tasks(void *arg);

job_t *
create_job(int idx, context_t *ctx, channel_t *input_channel,
           channel_t *output_channel, char *(*func)(char *, char *));

int queue_job(job_t *job);

int wait_job(job_t *job);

#endif //DATANOMMER_CONCURRENT_H
