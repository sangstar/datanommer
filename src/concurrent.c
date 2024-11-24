#include "../include/concurrent.h"
#include "../include/tasks.h"
#include <string.h>


void atomic_write_to_channel(channel_t *channel, int idx, char *string) {
    strncpy(channel->data[idx], string, BUFFER_SIZE - 1);
    channel->data[idx][BUFFER_SIZE - 1] = '\0';
    int end_idx = atomic_load(&channel->end_idx);
    if (idx < end_idx) {
        atomic_store(&channel->queued, idx);
    }
}




void *file_to_writing_channel(void *arg) {
    context_t *ctx = (context_t *) arg;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        printf("Writing data to %i\n", i);
        if (!fgets(ctx->writing_channel->data[i],
                   sizeof(ctx->writing_channel->data[i]), ctx->file)) {
            printf("end idx at %i\n", i);
            atomic_store(&ctx->writing_channel->end_idx, i);
            return NULL;
        }
    }
    return NULL;
}


void *perform_queued_tasks(void *arg) {
    worker_t *worker = (worker_t *) arg;

    printf("Thread %i entering loop..\n", worker->idx);


    while (1) {

        // Snag the current queue idx and pass
        // off before others can. There may not be any data there yet.

        // Lock the mutex to and ensure only worker accessing an idx
        // at once, preventing race conditions (so no two workers try to
        // snag the same idx)
        pthread_mutex_lock(&worker->job->context->mutex);
        int idx = atomic_load(&worker->job->input_channel->queued);
        int end_idx = atomic_load(&worker->job->input_channel->end_idx);

        printf("Thread %i trying to pick up job %i with end job at %i\n",
               worker->idx, idx, end_idx);

        // If the queue has been completely exhausted, end.
        if (idx >= end_idx) {
            break;
        } else {
            printf("Thread %i doing work on job %i\n", worker->idx, idx);

            // Snagged the idx at the top of the queue, so increment it by
            // 1 so others can snag their own unique idx
            atomic_store(&worker->job->input_channel->queued, idx + 1);

            // Unlock the mutex as we're not needing to access anything
            // and longer
            pthread_mutex_unlock(&worker->job->context->mutex);

            // Claimed job id. Wait for data at that idx and perform task.
            while (1) {
                if (strlen(worker->job->input_channel->data[idx]) != 0) {
                    printf("Ready to do work on task %i\n", idx);

                    // Perform the job and then go back to the outer while
                    // loop to pick up the next task from the queue
                    char *return_val = worker->job->func
                            (worker->job->input_channel->data[idx]);
                    atomic_write_to_channel(worker->job->output_channel, idx,
                                            return_val);
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&worker->job->context->mutex);
    printf("Thread %i finished. \n", worker->idx);

    // I don't actually check for this, but nice anyway
    worker->finished = 1;
    return NULL;
}

job_t *
create_job(int idx, context_t *ctx, channel_t *input_channel,
           channel_t *output_channel, char *(*func)(char *)) {
    job_t *job = malloc(sizeof(job_t));
    CHECK_MALLOC(job, "Failed to allocate job.")
    job->idx = idx;
    job->context = ctx;
    job->input_channel = input_channel;
    job->output_channel = output_channel;
    job->func = func;
    job->workers = malloc(sizeof(worker_t) * NUM_THREADS);
    CHECK_MALLOC(job->workers, "Failed to allocate workers.")
    return job;
}

int queue_job(job_t *job) {
    int err = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        worker_t *worker = malloc(sizeof(worker));
        CHECK_MALLOC(worker, "Failed to allocate worker.")
        worker->job = job;
        worker->finished = 0;
        worker->idx = i;
        job->workers[i] = worker;
        err = pthread_create(&job->thread_pool[i], NULL,
                             perform_queued_tasks, worker);
        if (err != 0) {
            free(worker);
            printf("Failed to create job for task.");
            exit(1);
        }
    }
    return 0;
}

int wait_job(job_t *job) {
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(job->thread_pool[i], NULL) != 0) {
            perror("Failed to join thread");
            return 1;
        }
        free(job->workers[i]);
    }
    printf("Job complete\n");
    return 0;
}


void destroy_channel(channel_t *channel) {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        free(channel->data[i]);
    }
    free(channel->data);
    free(channel);
}

void destroy_context(context_t *ctx) {
    if (ctx) {
        pthread_mutex_destroy(&ctx->mutex);
        for (int i = 0; i < ctx->num_additional_channels; i++) {
            if (ctx->additional_channels[i]) {
                destroy_channel(ctx->additional_channels[i]);
            }
        }
        destroy_channel(ctx->writing_channel);
        free(ctx->additional_channels);
        free(ctx);
    }
}

channel_t *new_channel() {
    channel_t *chan = malloc(sizeof(channel_t));
    CHECK_MALLOC(chan, "Failed to allocate channel.")
    chan->data = malloc(sizeof(char *) * MAX_BUFFERS);
    CHECK_MALLOC(chan->data, "Failed to allocate data for channel.")

    for (int i = 0; i < MAX_BUFFERS; i++) {
        chan->data[i] = malloc(BUFFER_SIZE);
        if (!chan->data[i]) {
            perror("Failed to allocate buffer for channel.");
            // Free already allocated data
            for (int j = 0; j < i; j++) {
                free(chan->data[j]);
            }
            free(chan->data);
            free(chan);
            exit(EXIT_FAILURE);
        }
    }
    chan->end_idx = MAX_BUFFERS;
    return chan;
}

// TODO: I honestly don't need num_channels for a parameter
//  but it doesn't hurt to make it extensible like this
//  in case my parser needs more factories
context_t *new_context(FILE *file, int num_channels) {


    context_t *ctx = malloc(sizeof(context_t));
    CHECK_MALLOC(ctx, "Failed to allocate context.")
    ctx->writing_channel = new_channel();
    ctx->additional_channels = malloc(sizeof(channel_t *) * num_channels);
    CHECK_MALLOC(ctx->additional_channels, "Failed to allocate additional "
                                           "channels")
    for (int i = 0; i < num_channels; ++i) {
        ctx->additional_channels[i] = new_channel();
        if (!ctx->additional_channels[i]) {
            perror("Failed to allocate an additional channel.");
            // Clean up already allocated additional channels
            for (int j = 0; j < i; j++) {
                destroy_channel(ctx->additional_channels[j]);
            }
            destroy_channel(ctx->writing_channel);
            free(ctx->additional_channels);
            free(ctx);
            exit(EXIT_FAILURE);
        }
    }
    ctx->num_additional_channels = num_channels;

    ctx->file = file;

    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        perror("Failed to initialize mutex.");
        destroy_context(ctx);
    }

    return ctx;

}

void write_messages_to_channel(context_t *ctx) {
    int err = pthread_create(&ctx->message_writing_thread, NULL,
                         file_to_writing_channel,
                         ctx);
    if (err != 0) {
        printf("Failed to create message writing thread.");
        exit(1);
    }
}

int wait_on_writing_thread(context_t *ctx) {
    int err = 0;
    err = pthread_join(ctx->message_writing_thread, NULL);
    if (err != 0) {
        printf("Failed waiting on message writing thread.");
        exit(1);
    }
    return 0;
}


