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

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "forwarder.h"

static bool _handling_signal = false;

static void sig_handler(int signum)
{
    if (_handling_signal) {
        // another signal is already being handled but the user is impatient, fire the default handler
        signal(signum, SIG_DFL);
        raise(signum);
    }
    else {
        _handling_signal = true;
        forwarder_stop();
    }
}

static void syntax(const char *app)
{
    printf("Usage: %s [OPTION]...\n", app);
    printf("  -a IP:PORT              public IP address and port to be used (default = use UPnP-IGD)\n");
    printf("  -l DELAY                delay to wait for local forwarder (in ms, default = 2000)\n");
    printf("  -p PORT                 local TCP port to be used (default = calculated)\n");
    printf("  -u DELAY:PERIOD:PERIOD  delay for initial UPnP-IGD discovery (in ms, default = 2000),\n");
    printf("                          period for requerying the public IP (in sec, default = 300),\n");
    printf("                          period for port map lease (in sec, default = 3600),\n");
    exit(1);
}

static void fail(const char *msg, ...)
{
    va_list args;

    va_start(args, msg);
    fprintf(stderr, "error: ");
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

static bool str2int(const char *str,
                    size_t len,
                    int *value)
{
    bool ok = false;
    char *endp = NULL;
    int i;

    i = strtol(str, &endp, 10);
    if (('\0' != *str) && (('\0' == *endp) || ((0 != len) && (str + len == endp)))) {
        if (NULL != value) {
            *value = i;
        }
        ok = true;
    }
    return ok;
}

static void parse_args(int argc, char **argv)
{
    int c;

    while ((c = getopt (argc, argv, "a:hp:l:u:")) != -1) {
        switch (c) {
            case 'a': {
                /* -a IP : public address */
                const char *colon;
                int port;
                bool ok = false;

                colon = strchr(optarg, ':');
                if (NULL != colon) {
                    if (str2int(colon + 1, 0, &port)) {
                        size_t len = colon - optarg;
                        char *ip = malloc(len + 1);

                        if (NULL != ip) {
                            strncpy(ip, optarg, len);
                            ip[len] = '\0';
                            forwarder_config_public_locator(ip, port);
                            ok = true;
                            free(ip);
                        }
                    }
                }
                if (!ok) {
                    fail("invalid IP:PORT '%s'", optarg);
                }
                break;
            }
            case 'l': {
                /* -l DELAY : local forwarder delay */
                int delay;

                if (str2int(optarg, 0, &delay)) {
                    forwarder_config_local_discovery(delay);
                }
                else {
                    fail("invalid DELAY '%s'", optarg);
                }
                break;
            }
            case 'p': {
                /* -p PORT : local port */
                if (str2int(optarg, 0, NULL)) {
                    forwarder_config_local_port(optarg);
                }
                else {
                    fail("invalid PORT '%s'", optarg);
                }
                break;
            }
            case 'u': {
                /* -u DELAY:PERIOD : UPnP timings */
                const char *colon1, *colon2;
                int delay, period, lease;
                bool ok = false;

                colon1 = strchr(optarg, ':');
                if (NULL != colon1) {
                    colon2 = strchr(colon1 + 1, ':');
                    if (NULL != colon2) {
                        if (str2int(optarg, colon1 - optarg, &delay) && (delay >= 0) &&
                            str2int(colon1 + 1, colon2 - colon1 - 1, &period) && (period > 0) &&
                            str2int(colon2 + 1, 0, &lease) && (lease > 0)) {
                            forwarder_config_upnp(delay, period, lease);
                            ok = true;
                        }
                    }
                }
                if (!ok) {
                    fail("invalid DELAY:PERIOD:PERIOD '%s'", optarg);
                }
                break;
            }
            case 'h':
            default:
                syntax(argv[0]);
                break;
        }
    }
}

int main(int argc, char **argv)
{
    struct sigaction sig_term = { .sa_handler = sig_handler };

    /* disable stdout buffering */
    setvbuf(stdout, NULL, _IONBF, 0);

    parse_args(argc, argv);
    if (sigaction(SIGTERM, &sig_term, NULL)) {
        exit (-1);
    }
    if (sigaction(SIGINT, &sig_term, NULL)) {
        exit (-1);
    }
    return forwarder_start();
}
