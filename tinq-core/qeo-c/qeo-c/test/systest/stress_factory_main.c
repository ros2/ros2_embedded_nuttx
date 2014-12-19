/*
 * Copyright (c) 2014 - Qeo LLC
 *
 * The source code form of this Qeo Open Source Project component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the Clear BSD
 * License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
 * for more details.
 *
 * The Qeo Open Source Project also includes third party Open Source Software.
 * See LICENSE file for more details.
 */

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <qeo/api.h>
#define _GNU_SOURCE
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

#include "verbose.h"

#define PNUM 5
#define N 100


static void run_tests()
{
    qeo_factory_t *factory;
    clock_t start = clock();

    pid_t pid = getpid();

    for (unsigned int i = 0; i < N; ++i){
        log_verbose("[PID %d] Creating factory %d", pid, i);
        assert(NULL != (factory = qeo_factory_create_by_id(QEO_IDENTITY_DEFAULT)));
        usleep(100000);
        log_verbose("[PID %d] Closing factory %d", pid, i);
        qeo_factory_close(factory);
    }
    double time_seconds = ((clock() - start) * 1.0) / CLOCKS_PER_SEC;
    printf("[PID %d] Creating/closing %d factories took %2.3fs CPU time\r\n", pid, N, time_seconds);
}

int main(int argc, char **argv)
{
    pid_t pid[PNUM];
    int status;
    unsigned int success = 0;

    setenv("QEO_DOMAIN_ID", "101", 0);

    for (unsigned int i = 0; i < PNUM; i++) {
        pid[i] = fork();
        assert(-1 != pid[i]);
        if (0 == pid[i]) {
            run_tests();
            return EXIT_SUCCESS;
        }
    }

    for (unsigned int i = 0; i < PNUM; i++) {
        log_verbose("Waiting for process with ID %d", pid[i]);
        assert(pid[i] == waitpid(pid[i], &status, 0));
        if (WIFEXITED(status)) {
            ;
        } else if (WIFSIGNALED(status)) {
            printf("killed by signal %s\n", strsignal(WTERMSIG(status)));
        } else if (WIFSTOPPED(status)) {
            printf("stopped by signal %s\n", strsignal(WSTOPSIG(status)));
        } else if (WIFCONTINUED(status)) {
            printf("continued\n");
        }

        if (status == 0){
            ++success;
        }
    }

    if (success == PNUM){
        return EXIT_SUCCESS;
    } else {
        printf("%u/%u childs did not exit cleanly !", PNUM - success, PNUM);
        return EXIT_FAILURE;
    }
}
