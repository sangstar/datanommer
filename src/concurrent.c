#include "../include/concurrent.h"

void* write_to_channel(void* arg) {
    context *ctx = (context *)arg;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        printf("Writing data to %i\n", i);
        if (!fgets(ctx->chan->data[i], sizeof(ctx->chan->data[i]), ctx->file)) {
            printf("end idx at %i\n", i);
            atomic_store(&ctx->chan->end_idx, i);
            return NULL;
        };
    }
}

void* read_from_channel(void* arg){
    thread* th = (thread *)arg;

    printf("Thread %i entering loop..\n", th->idx);
    // Steal the current queue idx and pass
    // off before others can

    while(1) {
        pthread_mutex_lock(&th->ctx->mutex);
        int idx = atomic_load(&th->ctx->chan->queued);
        int end_idx = atomic_load(&th->ctx->chan->end_idx);

        // If the queue has been completely exhausted, end
        // TODO: This might be an endless loop if there are not MAX_BUFFERS
        //  messages

        printf("Thread %i trying to pick up job %i with end job at %i\n", th->idx, idx, end_idx);

        if (idx >= end_idx) {
            break;
        } else {
            printf("Thread %i doing work on job %i\n", th->idx, idx);
            atomic_store(&th->ctx->chan->queued, idx+1);
            pthread_mutex_unlock(&th->ctx->mutex);

            // Do work
            sleep(2);
        }
    }
    pthread_mutex_unlock(&th->ctx->mutex);
    printf("Thread %i finished. Killing..\n", th->idx);
    free(th);
    return NULL;
}

int read_data_from_channel(void* arg) {
    context* ctx = (context *)arg;

    int err = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        thread* th = malloc(sizeof(thread));
        th->ctx = ctx;
        th->idx = i;
        err = pthread_create(&ctx->data_reading_thread_pool[i], NULL, read_from_channel, th);
    }
    return err;
}


channel* new_channel() {
    channel* chan = malloc(sizeof(channel));
    if (chan == NULL) {
        printf("Could not allocate channel");
        exit(1);
    }
    chan->end_idx=MAX_BUFFERS;
    return chan;
}

context* new_context(FILE* file) {
    channel *chan = new_channel();

    context *ctx = malloc(sizeof(context));
    if (ctx == NULL) {
        perror("Failed to allocate memory for context");
        exit(EXIT_FAILURE);
    }
    ctx->chan = chan;
    ctx->file = file;

    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }

    return ctx;

}

int write_messages_to_channel(context* ctx) {
    int err = 0;
    err = pthread_create(&ctx->message_writing_thread, NULL, write_to_channel, ctx);
    return err;
}

int wait_on_writing_thread(context* ctx) {
    int err = 0;
    err = pthread_join(ctx->message_writing_thread, NULL);
    return err;
}

void destroy_context(context* ctx) {
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx->chan);
    free(ctx);
}

