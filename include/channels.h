//
// Created by Sanger Steel on 11/24/24.
//

#ifndef DATANOMMER_CHANNELS_H
#define DATANOMMER_CHANNELS_H

#include "concurrent.h"

typedef struct context_t context_t;

typedef struct {
    char **data;
    int capacity;
    _Atomic int end_idx;
    _Atomic int queued;
} channel_t;

void *file_to_writing_channel(void *arg);

channel_t *new_channel();

void write_messages_to_channel(context_t *ctx);

void destroy_channel(channel_t *channel);

void atomic_write_to_channel(channel_t *channel, int idx, char *string);

#endif //DATANOMMER_CHANNELS_H
