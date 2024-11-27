#include "../include/concurrent.h"
#include "../include/tasks.h"
#include "../include/channels.h"
#include <string.h>



job_t *
create_job(int idx, context_t *ctx, channel_t *input_channel,
           channel_t *output_channel, void (*func)(context_t *, char *, char
*)) {
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
    if (job->output_channel) {
        job->output_channel->closed = 1;
    }
    LOG("Job complete\n");
    return 0;
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
        fclose(ctx->input_file);
        fclose(ctx->output_file);
        free(ctx);

    }
}

// TODO: I honestly don't need num_channels for a parameter
//  but it doesn't hurt to make it extensible like this
//  in case my parser needs more factories
context_t *new_context(FILE *input_file, FILE *output_file, int num_channels) {


    context_t *ctx = malloc(sizeof(context_t));
    CHECK_MALLOC(ctx, "Failed to allocate context.")
    ctx->writing_channel = new_channel();
    ctx->file_writing_channel = new_channel();
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

    ctx->input_file = input_file;
    ctx->output_file = output_file;

    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        perror("Failed to initialize mutex.");
        destroy_context(ctx);
    }

    return ctx;

}


int wait_on_writing_thread(context_t *ctx) {
    int err = 0;
    err = pthread_join(ctx->message_writing_thread, NULL);
    if (err != 0) {
        printf("Failed waiting on message writing thread.");
        exit(1);
    }
    ctx->writing_channel->closed = 1;
    LOG("Writing thread finished.");
    return 0;
}

int wait_on_file_writing_thread(context_t *ctx) {
    int err = 0;
    err = pthread_join(ctx->file_writing_thread, NULL);
    if (err != 0) {
        printf("Failed waiting on input_file writing thread.");
        exit(1);
    }
    LOG("File writing thread finished.");
    return 0;
}


