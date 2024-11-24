//
// Created by Sanger Steel on 11/24/24.
//

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


void destroy_channel(channel_t *channel) {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        free(channel->data[i]);
    }
    free(channel->data);
    free(channel);
}


void *file_to_writing_channel(void *arg) {
    context_t *ctx = (context_t *) arg;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        printf("Writing data to %i\n", i);
        if (!fgets(ctx->writing_channel->data[i],
                   BUFFER_SIZE, ctx->file)) {
            printf("end idx at %i\n", i);
            atomic_store(&ctx->writing_channel->end_idx, i);
            return NULL;
        }
    }
    return NULL;
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

