//
// Created by Sanger Steel on 11/23/24.
//


#ifndef DATANOMMER_TASKS_H
#define DATANOMMER_TASKS_H

typedef struct context_t context_t;
typedef struct job_t job_t;


typedef struct {
    int idx;
    int finished;
    job_t *job;
} worker_t;


void op_make_json(context_t *ctx, char *input_channel_data, char
*output_channel_data);

void op_escape_string(context_t *ctx, char *input_channel_data, char
*output_channel_data);

void op_write_to_file(context_t *ctx, char *input_channel_data, char
*output_channel_data);


#endif //DATANOMMER_TASKS_H
