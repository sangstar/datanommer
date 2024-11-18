//
// Created by Sanger Steel on 11/16/24.
//

#include "../include/concurrent.h"
#include <stdio.h>

// TODO: Make channels for each "factory": a channel to put text in to,
//  a channel to put processed text in to, etc
//  Each worker should be in an infinite loop waiting for "tasks" to do from the channels

int main(void) {
    FILE *file = fopen("../data/test.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    context *ctx = new_context(file);

    CHECK_ERR(write_messages_to_channel(ctx), "Writer thread creation failed.");
    CHECK_ERR(read_data_from_channel(ctx), "Reader thread creation failed.");
    CHECK_ERR(wait_on_writing_thread(ctx),
              "Wait for writer threads termination failed.");
    WAIT_THREAD(ctx, data_reading_thread_pool);

    destroy_context(ctx);
    return 0;
}