//
// Created by Sanger Steel on 11/24/24.
//

#include "../include/concurrent.h"
#include "../include/tasks.h"
#include <string.h>
#include <assert.h>


void destroy_channel(channel_t *channel) {
    for (int i = 0; i < MAX_BUFFERS; i++) {
        free(channel->data[i]);
    }
    free(channel->data);
    free(channel);
}

// 1 Signals a blocked write
int channel_send(channel_t *channel, int idx) {
    if (channel->is_full == 1) {
        return 1;
    }
    size_t max_capacity = atomic_load(&channel->max_capacity);
    size_t capacity = atomic_load(&channel->capacity);
    if (capacity + 1 == max_capacity) {
        printf("AT MAX CAPACITY\n");
        channel->is_full = 1;
        return 1;
    }

    if (channel->is_empty == 1) {
        channel->is_empty = 0;
    }
    // Append idx to queued
    atomic_store(&channel->queued[capacity + 1], idx);
    atomic_store(&channel->capacity, capacity + 1);
    return 0;
}

int channel_recv(channel_t *channel) {
    if (atomic_load(&channel->is_empty)) {
        return -1;
    }
    if (atomic_load(&channel->is_full)) {
        return -1;
    }

    int index_to_remove = atomic_load(&channel->capacity);
    if (index_to_remove < 0 || index_to_remove >= MAX_BUFFERS) {
        printf("Invalid index\n");
        return -1;
    }
    int idx = atomic_load(&channel->queued[index_to_remove]);
    assert(idx < channel->max_capacity);

    // Remove index from queue

    if (index_to_remove == 0) {
        return -1;
    }
    atomic_store(&channel->queued[index_to_remove],
                 channel->queued[index_to_remove - 1]);
    atomic_store(&channel->capacity, channel->capacity - 1);
    return idx;

}

int try_write(context_t *ctx, int idx) {
    if (ctx->writing_channel->is_full == 1) {
        return idx;
    }
    if (!fgets(ctx->writing_channel->data[idx],
               BUFFER_SIZE, ctx->input_file)) {
        if (feof(ctx->input_file)) {
            ctx->writing_channel->closed = 1;
            return -1;
        } else {
            printf("File read error.");
            exit(1);
        }
    } else {
        if (strcmp(ctx->writing_channel->data[idx], "\n") == 0) {
            // No bytes were written. Try this idx again.
            LOG("Retrying a write for %i.. \n", idx);
            if (idx > 0) {
                return idx - 1;
            } else {

                // In this case, i = 0, so we're forced to
                // manually read again instead of decrementing
                // and retrying the loop. Greedily keep
                // trying until we no longer encounter a newline
                while (strcmp(ctx->writing_channel->data[idx], "\n") == 0) {
                    if (!fgets(ctx->writing_channel->data[idx],
                               BUFFER_SIZE, ctx->input_file)) {
                        if (feof(ctx->input_file)) {
                            ctx->writing_channel->closed = 1;
                            return -1;
                        } else {
                            printf("File read error.");
                            exit(1);
                        }
                    }
                }
            }
        }
    }
    LOG("Writing job %i\n", idx);
    pthread_mutex_lock(&ctx->mutex);
    channel_send(ctx->writing_channel, idx);
    pthread_mutex_unlock(&ctx->mutex);

    return 0;
}


void *file_to_writing_channel(void *arg) {
    context_t *ctx = (context_t *) arg;
    for (int i = 0; i < MAX_BUFFERS; i++) {
        int ret = try_write(ctx, i);
        if (ret == -1) {
            break;
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

    _Atomic int max_capacity = sizeof(char *) * MAX_BUFFERS;
    _Atomic int max_str_length = BUFFER_SIZE + 100;

    chan->data = malloc(max_capacity);
    CHECK_MALLOC(chan->data, "Failed to allocate data for channel.")

    for (int i = 0; i < MAX_BUFFERS; i++) {
        // Add a bit of wiggle-room for the json characters to be added
        chan->data[i] = malloc(max_str_length);
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
    chan->is_empty = (_Atomic int) 1;
    chan->closed = (_Atomic int) 0;
    chan->is_full = (_Atomic int) 0;
    chan->capacity = (_Atomic int) 0;
    chan->max_capacity = max_capacity;
    chan->max_str_len = max_str_length;
    return chan;
}

