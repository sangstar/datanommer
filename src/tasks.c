//
// Created by Sanger Steel on 11/23/24.
//

#include <stdio.h>
#include "../include/tasks.h"
#include "../include/concurrent.h"
#include "../include/channels.h"
#include "assert.h"


char *add_at_index(char *string, int start_idx, char *to_add) {
    int left_chars = strlen(string) - (strlen(string) - start_idx);
    int right_chars = strlen(string) - left_chars;
    assert(right_chars >= 0);
    int shift_amount = strlen(to_add);
    char *piece_1 = malloc(left_chars);
    int shifted_size = strlen(string) + shift_amount;

    memmove(piece_1, string, left_chars);
    char *shifted = malloc(shifted_size + 1);
    memmove(shifted, piece_1, left_chars);
    strcat(shifted, to_add);
    for (int i = left_chars; i < right_chars; i++) {
        shifted[i + shift_amount] = string[i];
    }
    shifted[shifted_size + 1] = '\0';
    return shifted;
}

// op_-prepended functions will be the factory
// functions that threads perform and pass
// the processed data to the next channel.
// It does the actual writing to the output channel by modifying
// char *output_channel_data in place, and therefore doesn't return
// anything to the caller.
void op_make_json(context_t *ctx, char *input_channel_data, char
*output_channel_data) {
    assert(output_channel_data[0] == '\0');
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    char *jsonl_beginning = strdup("{\"text\": \"");
    char *jsonl_end = strdup("\"}\n\0");
    char *output = malloc(strlen(jsonl_beginning) +
                          strlen(jsonl_end) +
                          strlen(str2));
    strcat(output, jsonl_beginning);
    strcat(output, str);
    strcat(output, jsonl_end);
    memcpy(str2, output, strlen(output));
    printf("str2 is now %s", str2);
}

void op_escape_string(context_t *ctx, char *input_channel_data, char
*output_channel_data) {
    assert(output_channel_data[0] == '\0');
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    for (int i = 0; i < strlen(str); ++i) {
        assert(strlen(str2) == i);
        switch (str[i]) {
            case '"':
                char *quote = strdup("\"");
                str2 = add_at_index(str2, i, quote);
                int j;
                if (strlen(quote) > 1) {
                    for (j = 0; j < strlen(quote); ++j) {
                        i++;
                    }
                }
                free(quote);
                break;
            case '\n':
                char *newline = strdup("\\n");
                str2 = add_at_index(str2, i, newline);
                for (int j = 0; j < strlen(newline); ++j) {
                    i++;
                }
                break;
            default:
                str2[i] = str[i];
        }
    }
}


// Ignore output channel data in this case. No writing to it.
void op_write_to_file(context_t *ctx, char *input_channel_data, char
*output_channel_data) {
    printf("Writing to file: %s\n", input_channel_data);
    fprintf(ctx->output_file, "%s", input_channel_data);
}


/*
 * This is performed per worker and does the following:
 * 1. Locks the mutex
 * 2. Gets the most recent index of the input channel data to do work on
 * using channel_recv
 * 3. Do work on the data
 * 4. Send it off to the output channel with channel_send
 * 5. Holds the mutex because even though I'm using some atomic
 * ops in channel_recv and channel_send, race conditions can still
 * occur
 */
void *perform_queued_tasks(void *arg) {
    worker_t *worker = (worker_t *) arg;

    printf("Thread %i entering loop..\n", worker->idx);

    while (1) {

        // Snag the current queue idx and pass
        // off before others can. There may not be any data there yet.

        // Lock the mutex to and ensure only worker accessing an idx
        // at once, preventing race conditions (so no two workers try to
        // snag the same idx)

        /*
         * Writer fills initial channel
         * Workers look for an index with data in input channel
         * When they find it, they claim that idx and do work
         * They then fill that value for that idx in the output
         * channel
         */

        // TODO: The way of having channels hand off data
        //  to a worker is currently flawed. It doesn't work properly
        pthread_mutex_lock(&worker->job->context->mutex);
        int idx = channel_recv(worker->job->input_channel);
        pthread_mutex_unlock(&worker->job->context->mutex);


        if (idx == -1) {
            // If the queue has been completely exhausted, end.

            // When should an input channel close?
            // When there are no more writers to it
            // When are there no more writers to it?
            if (atomic_load(&worker->job->input_channel->closed) == 1) {
                printf("Thread %i exiting without any work with idx for job %i "
                       "\n",
                       worker->idx, worker->job->idx);
                break;
            } else {
                continue;
            }

        } else {

            printf("Thread %i claimed job (%i, %i)\n", worker->idx,
                   worker->job->idx, idx);

            if (worker->job->output_channel) {
                worker->job->func
                        (worker->job->context,
                         worker->job->input_channel->data[idx],
                         worker->job->output_channel->data[idx]);
                pthread_mutex_lock(&worker->job->context->mutex);
                channel_send(worker->job->output_channel, idx);
                pthread_mutex_unlock(&worker->job->context->mutex);


            } else {
                worker->job->func
                        (worker->job->context,
                         worker->job->input_channel->data[idx],
                         NULL);
            }


        }
    }
    printf("Thread %i finished. \n", worker->idx);
    // I don't actually check for this, but nice anyway
    worker->finished = 1;
    return NULL;
}
