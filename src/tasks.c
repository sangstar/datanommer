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

char *op_change_char(char *input_channel_data, char *output_channel_data) {
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    strcat(str2, str);
    strcat(str2, "And then..");
    return str2;
}

char *op_write_zeros(char *input_channel_data, char *output_channel_data) {
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    strcpy(str2, "Hewo");
    return str2;
}


void *perform_queued_tasks(void *arg) {
    worker_t *worker = (worker_t *) arg;

    printf("Thread %i entering loop..\n", worker->idx);

    while (1) {

        // Snag the current queue idx and pass
        // off before others can. There may not be any data there yet.

        // Lock the mutex to and ensure only worker accessing an idx
        // at once, preventing race conditions (so no two workers try to
        // snag the same idx)
        pthread_mutex_lock(&worker->job->context->mutex);
        int idx = atomic_load(&worker->job->input_channel->queued);
        int end_idx = atomic_load(&worker->job->input_channel->end_idx);

        printf("Thread %i trying to pick up job %i with end job at %i\n",
               worker->idx, idx, end_idx);

        // If the queue has been completely exhausted, end.
        if (idx >= end_idx | idx >= worker->job->input_channel->capacity) {
            break;
        } else {
            printf("Thread %i doing work on job %i\n", worker->idx, idx);

            // Peek to see if there's data to work on. If none, go to
            // beginning.
            if (worker->job->input_channel->data[idx][0] == '\0') {
                pthread_mutex_unlock(&worker->job->context->mutex);
                continue;
            }

            // Snagged the idx at the top of the queue, so increment it by
            // 1 so others can snag their own unique idx
            atomic_store(&worker->job->input_channel->queued, idx + 1);

            // Unlock the mutex as we're not needing to access anything
            // and longer
            pthread_mutex_unlock(&worker->job->context->mutex);

            // Claimed job id. Wait for data at that idx and perform task.
            while (1) {
                if (worker->job->input_channel->data[idx][0] != '\0') {
                    printf("Ready to do work on task %i\n", idx);

                    // Perform the job and then go back to the outer while
                    // loop to pick up the next task from the queue
                    worker->job->func
                            (worker->job->input_channel->data[idx],
                             worker->job->output_channel->data[idx]);

                    // Increment the end idx
                    int output_end_idx = atomic_load
                    (&worker->job->output_channel->end_idx);
                    atomic_store(&worker->job->output_channel->end_idx,
                                 output_end_idx + 1);
                    break;
                } else {
                    printf("No data for job %i: %s\n", idx,
                           worker->job->input_channel->data[idx]);
                }
            }
        }
    }
    pthread_mutex_unlock(&worker->job->context->mutex);
    printf("Thread %i finished. \n", worker->idx);

    // I don't actually check for this, but nice anyway
    worker->finished = 1;
    return NULL;
}
