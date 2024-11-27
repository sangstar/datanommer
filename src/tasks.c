//
// Created by Sanger Steel on 11/23/24.
//

#include <stdio.h>
#include "../include/tasks.h"
#include "../include/concurrent.h"
#include "../include/channels.h"
#include "assert.h"

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
    char *output = malloc(strlen("{\"text\": \"") +
                          strlen("\"}\n\0") +
                          strlen(str2) + 15);
    strcat(output, "{\"text\": \"");
    strcat(output, str);
    strcat(output, "\"}\n\0");
    memcpy(str2, output, strlen(output));
    printf("str2 is now %s", str2);
}

void op_escape_string(context_t *ctx, char *input_channel_data, char
*output_channel_data) {
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    assert(output_channel_data[0] == '\0');
    for (int i = 0; i < strlen(str); ++i) {
        printf("Output str so far is %s\n", str2);
        printf("str[%i]: %c\n", i, str[i]);
        assert(i < strlen(str));
        assert(str[i] == input_channel_data[i]);
        assert(strlen(str2) == i);
        assert(strlen(str2) <= MAX_BUFFERS + 100);
        switch (str[i]) {
            case '"':
                printf("Spotted \" at %i\n", i);
                char *quote = strdup("\"");
                strcat(str2, quote);
                int j;
                if (strlen(quote) > 1) {
                    for (j = 0; j < strlen(quote); ++j) {
                        i++;
                    }
                }
                free(quote);
                printf("str2 after \": %s\n", str2);
                break;
            case '\n':
                printf("Spotted \\n at %i\n", i);
                char *newline = strdup("\\n");
                strcat(str2, newline);
                int k;
                if (strlen(newline) > 1) {
                    for (k = 0; k < strlen(newline); ++k) {
                        i++;
                    }
                }
                printf("str2 after \\n: %s\n", str2);
                free(newline);
                break;
            default:
                printf("Adding to str2[%i]: %c\n", i, str[i]);
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

        assert(worker->job);

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

            assert(idx < worker->job->input_channel->max_capacity);


            printf("Thread %i claimed job (%i, %i)\n", worker->idx,
                   worker->job->idx, idx);


            if (worker->job->output_channel) {
                worker->job->func
                        (worker->job->context,
                         worker->job->input_channel->data[idx],
                         worker->job->output_channel->data[idx]);

                assert(worker->job);

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
