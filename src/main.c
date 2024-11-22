//
// Created by Sanger Steel on 11/16/24.
//

#include "../include/concurrent.h"
#include "../include/tasks.h"

#include <stdio.h>

// TODO:
//  > Change print statements to some macro that only
//   shows these print statements for a specific macro value at compilation
//  > Continue extending this. At the processing function in `perform_queued_tasks`
//  > Clean up the error handling

int main(void) {
    FILE *file = fopen("../data/test.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    context_t *ctx = new_context(file, 3);


    write_messages_to_channel(ctx);
    job_t *job = create_job(1, ctx,
                            ctx->writing_channel,
                            ctx->additional_channels[0],
                            op_change_char);
    queue_job(job);
    wait_on_writing_thread(ctx),
            wait_job(job);
    job_t *second_job = create_job(2, ctx,
                                   ctx->additional_channels[0],
                                   ctx->additional_channels[1],
                                   op_write_zeros);
    queue_job(second_job);
    wait_job(second_job);

    destroy_context(ctx);
    return 0;
}