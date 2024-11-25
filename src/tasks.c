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
void op_change_char(char *input_channel_data, char *output_channel_data) {
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    strcat(str2, str);
    strcat(str2, "And then..");
}

void op_write_zeros(char *input_channel_data, char *output_channel_data) {
    char *str = (char *) input_channel_data;
    char *str2 = (char *) output_channel_data;
    strcpy(str2, "Hewo");
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

            printf("Ready to do work on task %i\n", idx);

            // Perform the job and then go back to the outer while
            // loop to pick up the next task from the queue
            worker->job->func
                    (worker->job->input_channel->data[idx],
                     worker->job->output_channel->data[idx]);

            // Increment the end idx. This is basically a way of
            // keeping a record for other workers how many jobs
            // have been complete, so if the last worker picked up
            // job queued where queued = end_idx + 1, they know
            // the previous worker completed the final task
            // and can break.
            int output_end_idx = atomic_load
            (&worker->job->output_channel->end_idx);
            atomic_store(&worker->job->output_channel->end_idx,
                         output_end_idx + 1);
        }
    }
    pthread_mutex_unlock(&worker->job->context->mutex);
    printf("Thread %i finished. \n", worker->idx);

    // I don't actually check for this, but nice anyway
    worker->finished = 1;
    return NULL;
}
