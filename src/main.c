//
// Created by Sanger Steel on 11/16/24.
//

#include "../include/concurrent.h"
#include "../include/channels.h"
#include <sys/time.h>

#include <stdio.h>

// TODO:
//  > Change print statements to some macro that only
//   shows these print statements for a specific macro value at compilation
//  > Continue extending this. At the processing function in `perform_queued_tasks`
//  > Clean up the error handling

int main(void) {
    struct timeval start, end;

    gettimeofday(&start, NULL);

    FILE *infile = fopen("../data/bigfile.txt", "r");
    FILE *outfile = fopen("../data/output.jsonl", "w");

    if (infile == NULL) {
        perror("Error opening input file");
        return 1;
    }
    if (outfile == NULL) {
        perror("Error opening output_file");
        return 1;
    }

    context_t *ctx = new_context(infile, outfile, 2);


    write_messages_to_channel(ctx);
    job_t *job = create_job(1, ctx,
                            ctx->writing_channel,
                            ctx->additional_channels[0],
                            op_escape_string);
    queue_job(job);

    job_t *second_job = create_job(2, ctx,
                                   ctx->additional_channels[0],
                                   ctx->file_writing_channel,
                                   op_make_json);
    queue_job(second_job);


    job_t *third_job = create_job(3, ctx,
                                  ctx->file_writing_channel,
                                  NULL,
                                  op_write_to_file);
    queue_job(third_job);

    wait_on_writing_thread(ctx);
    wait_job(job);
    wait_job(second_job);
    wait_job(third_job);


    gettimeofday(&end, NULL);

    double elapsed_time = (end.tv_sec - start.tv_sec) +
                          ((end.tv_usec - start.tv_usec) / 1000000.0);

    printf("Execution time: %f seconds\n", elapsed_time);

    destroy_context(ctx);
    return 0;
}