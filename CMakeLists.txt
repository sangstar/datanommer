cmake_minimum_required(VERSION 3.30)
project(datanommer C)

set(CMAKE_C_STANDARD 11)

add_executable(datanommer src/concurrent.c
        src/main.c
        include/concurrent.h
        src/tasks.c
        include/tasks.h)