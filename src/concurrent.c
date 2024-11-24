#include "../include/concurrent.h"
#include "../include/tasks.h"

void *file_to_writing_channel(void *arg) {
    context_t *ctx = (context_t *) arg;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        printf("Writing data to %i\n", i);
        if (!fgets(ctx->writing_channel->data[i],
                   sizeof(ctx->writing_channel->data[i]), ctx->file)) {
            printf("end idx at %i\n", i);
            atomic_store(&ctx->writing_channel->end_idx, i);
            return NULL;
        };
    }
    return NULL;
}

void *perform_queued_tasks(void *arg) {
    worker_t *worker = (worker_t *) arg;

    printf("Thread %i entering loop..\n", worker->idx);
    // Steal the current queue idx and pass
    // off before others can

    while (1) {
        pthread_mutex_lock(&worker->job->context->mutex);
        int idx = atomic_load(&worker->job->channel->queued);
        int end_idx = atomic_load(&worker->job->channel->end_idx);

        // If the queue has been completely exhausted, end

        printf("Thread %i trying to pick up job %i with end job at %i\n",
               worker->idx, idx, end_idx);

        if (idx >= end_idx) {
            break;
        } else {
            printf("Thread %i doing work on job %i\n", worker->idx, idx);
            atomic_store(&worker->job->channel->queued, idx + 1);
            pthread_mutex_unlock(&worker->job->context->mutex);

            // Claimed job id. Wait for work and perform
            while (1) {
                if (strlen(worker->job->channel->data[idx]) != 0) {
                    printf("Ready to do work on task %i\n", idx);
                    worker->job->func(&worker->job->channel->data[idx]);
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&worker->job->context->mutex);
    printf("Thread %i finished. \n", worker->idx);
    worker->finished = 1;
    return NULL;
}

job_t *
create_job(int idx, context_t *ctx, channel_t *channel, void (*func)(void *)) {
    job_t *job = malloc(sizeof(job_t));
    job->idx = idx;
    job->context = ctx;
    job->channel = channel;
    job->func = func;
    job->workers = malloc(sizeof(worker_t) * NUM_THREADS);
    return job;
}

int queue_job(job_t *job) {
    int err = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        worker_t *worker = malloc(sizeof(worker));
        worker->job = job;
        worker->finished = 0;
        worker->idx = i;
        job->workers[i] = worker;
        err = pthread_create(&job->thread_pool[i], NULL,
                             perform_queued_tasks, worker);
        if (err != 0) {
            free(worker);
            return err;
        }
    }
    return err;
}

int wait_job(job_t *job) {
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(job->thread_pool[i], NULL) != 0) {
            perror("Failed to join thread");
            return 1;
        }
        free(job->workers[i]);
    }
    printf("Job complete");
    return 0;
}


channel_t *new_channel() {
    channel_t *chan = malloc(sizeof(channel_t));
    if (chan == NULL) {
        printf("Could not allocate channel_t");
        exit(1);
    }
    chan->end_idx = MAX_BUFFERS;
    return chan;
}

// TODO: I honestly don't need num_channels for a parameter
//  but it doesn't hurt to make it extensible like this
//  in case my parser needs more factories
context_t *new_context(FILE *file, int num_channels) {


    context_t *ctx = malloc(sizeof(context_t));
    if (ctx == NULL) {
        perror("Failed to allocate memory for context_t");
        exit(EXIT_FAILURE);
    }
    ctx->writing_channel = new_channel();
    ctx->additional_channels = malloc(sizeof(channel_t) * num_channels);
    for (int i = 0; i < num_channels; ++i) {
        ctx->additional_channels[i] = new_channel();
    }
    ctx->num_additional_channels = num_channels;

    ctx->file = file;

    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }

    return ctx;

}

int write_messages_to_channel(context_t *ctx) {
    int err = 0;
    err = pthread_create(&ctx->message_writing_thread, NULL,
                         file_to_writing_channel,
                         ctx);
    return err;
}

int wait_on_writing_thread(context_t *ctx) {
    int err = 0;
    err = pthread_join(ctx->message_writing_thread, NULL);
    return err;
}

void destroy_context(context_t *ctx) {
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx->writing_channel);
    for (int i = 0; i < ctx->num_additional_channels; ++i) {
        free(ctx->additional_channels[i]);
    }
    free(ctx);
}

