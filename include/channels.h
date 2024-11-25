//
// Created by Sanger Steel on 11/24/24.
//

#ifndef DATANOMMER_CHANNELS_H
#define DATANOMMER_CHANNELS_H


void *file_to_writing_channel(void *arg);

void *channel_to_file(void *arg);

int channel_send(channel_t *channel, int idx);

int channel_recv(channel_t *channel);

int try_write(context_t *ctx, int idx);

void write_channel_to_file(context_t *ctx);

channel_t *new_channel();

void write_messages_to_channel(context_t *ctx);

int try_read_from_channel(channel_t *channel);

void destroy_channel(channel_t *channel);

void atomic_write_to_channel(channel_t *channel, int idx, char *string);

#endif //DATANOMMER_CHANNELS_H
