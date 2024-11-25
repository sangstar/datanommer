//
// Created by Sanger Steel on 11/23/24.
//

#include <stdio.h>
#include "../include/tasks.h"
#include "../include/concurrent.h"
#include "../include/channels.h"


// op_-prepended functions will be the factory
// functions that threads perform and pass
// the processed data to the next channel.
// It does the actual writing to the output channel by modifying
// char *output_channel_data in place, and therefore doesn't return
// anything to the caller.
void op_make_json(context_t *ctx, char *input_channel_data, char
*output_channel_data) {
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
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    for (int i = 0; i < strlen(str); ++i) {
        switch (str[i]) {
            case '"':
                str2[i] = '\\';
                str2[i + 1] = '"';
                break;
            case '\n':
                str2[i] = '\\';
                str2[i + 1] = 'n';
                break;
            default:
                str2[i] = str[i];
                str2[i + 1] = '\0';
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
 * 2. Checks what the queued and end_idx fields are for
 * the input channel (the channel where it receives tasks,
 * does work, and sends the result to the output channel).
 *
 * The queued and end_idx fields are closely related. end_idx tells
 * workers when there's no more tasks to fulfill from the input channel.
 * If queued > end_idx, there are no more tasks to complete and the thread
 * can exit the loop.
 *
 * queued acts as available indices from the input channel for workers
 * to claim. When they do so, while holding the mutex, they see
 * if data on the input channel exists on that index. If there is,
 * it increments queued, telling other workers that the most recent
 * work index to pick up is different since it's now handling the one
 * previously at the top of queued.
 *
 * 3. Once it has its index to do work on and does the work, it
 * sends that output to the output channel at the index it grabbed.
 * 4. It then increments the end_idx of the output channel, basically saying
 * "there is at least one job that was completable for the output channel
 * because I just completed one". All workers doing this will accurately
 * count how many jobs have been sent to this output channel. This way, if
 * there is another processing stage where that output channel becomes the
 * input channel for another factory, workers will know when they've
 * exhausted the input channel because they have a record of how many tasks
 * were completed when it was the output
 * channel.
 *
 * 5. Once it completes that job, it goes to the beginning of the loop and
 * once again sees if there's any work to do at index queued of the input
 * channel. Again, if all work is complete, workers will have incremented
 * queued above end_idx.
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

            // Picked a job idx. Wait until there's data to
            // work on.
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
