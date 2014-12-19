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

#include <qeo/log.h>

#include <stdarg.h>
#include <stdio.h>
#ifdef ANDROID
#include <android/log.h>
static int _lvl2prio[] = { ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR };
#else
static const char *_lvl2str[] = { "D", "I", "W", "E" };
#endif

static void default_logger(qeo_loglvl_t lvl, const char* fileName, const char* functionName, int lineNumber, const char *format, ...)
{
    va_list args;

    va_start(args, format);
#ifdef ANDROID
    char buf[512];
    int len = 0;
    len = snprintf(buf, sizeof(buf),"%s:%s:%d - ", fileName, functionName, lineNumber);
    vsnprintf(buf + len, sizeof(buf) - len,  format, args);
    __android_log_print(_lvl2prio[lvl], "QeoNative", "%s", buf);
#else
    printf("%s - %s:%s:%d - ", _lvl2str[lvl], fileName, functionName, lineNumber);
    vprintf(format, args);
    printf("\n");
#endif
    va_end(args);
}

qeo_logger_ptr qeo_logger = default_logger;

void qeo_log_set_logger(qeo_logger_ptr logger)
{
    qeo_logger = logger;
}
