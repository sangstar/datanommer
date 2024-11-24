//
// Created by Sanger Steel on 11/23/24.
//

#include <stdio.h>

void change_char(void *data) {
    char *str = (char *) data;
    printf("Received: %s", str);
}
