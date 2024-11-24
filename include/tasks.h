//
// Created by Sanger Steel on 11/23/24.
//


#ifndef DATANOMMER_TASKS_H
#define DATANOMMER_TASKS_H

typedef struct job_t job_t;

typedef struct {
    int idx;
    int finished;
    job_t *job;
} worker_t;

void change_char(void *data);

#endif //DATANOMMER_TASKS_H
