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

/* sec_logging.h -- Implements the security logging functionality. */

#include <stdio.h>
#include "log.h"
#include "error.h"
#include "sec_data.h"
#include "sec_logging.h"

#ifdef DDS_SECURITY

DDS_TypeSupport_meta LogInfo_tsm [] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_DYNAMIC, "DDS_LogInfo", sizeof (struct DDS_LogInfo_st), 0, 4, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "source_guid", 0, offsetof (struct DDS_LogInfo_st, source_guid), 16, 0, NULL},
	{CDR_TYPECODE_OCTET, 0, NULL, 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "log_level", 0, offsetof (struct DDS_LogInfo_st, log_level), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_DYNAMIC, "message", 0, offsetof (struct DDS_LogInfo_st, message), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_DYNAMIC, "category", 0, offsetof (struct DDS_LogInfo_st, category), 0, 0, NULL}
};

static DDS_LogOptions		log_options = { DDS_TRACE_LEVEL, NULL, 0};
static int			log_options_set = 0;
static DDS_DomainParticipant	log_participant;
static DDS_DataWriter		log_writer;
static unsigned			log_activation_count;
static char			logname [32] = ".tdds_sec";

/* sec_activate -- Activate the logging mechanisms. */

void sec_activate_log (DDS_DomainParticipant part)
{
	if (log_activation_count++)
		return;

	log_participant = part;
#if 0
	if (log_options.distribute) {
		/* TODO: create log_publisher and log_writer! */
		DDS_Security_log (DDS_INFO_LEVEL, "SECURITY", "DataWriter started");
	}
#endif
}

/* sec_deactivate -- Deactivate the logging mechanisms. */

void sec_deactivate_log (DDS_DomainParticipant part)
{
	log_activation_count--;
	if (log_participant != part)
		return;
#if 0
	if (log_options.distribute) {
		DDS_Security_log (DDS_INFO_LEVEL, "SECURITY", "DataWriter stopped");
		/* TODO: delete log_publisher and log_writer! */
	}
#endif
	log_participant = NULL;
}

/* DDS_Security_set_log_options -- Set and validate the logging options. */

int DDS_Security_set_log_options (DDS_LogOptions *options)
{
	if (log_options_set) {
		DDS_Security_log (DDS_ERROR_LEVEL, "Unauthorized DDS_Security_set_log_options() request!", "API");
		return (0);
	}
	if (!options)
		return (0);

	if (options->log_level > DDS_FATAL_LEVEL)
		return (0);

	log_options = *options;
	return (1);
}

#ifdef _WIN32
#define snprintf	sprintf_s
#define openf(f,name,m)	fopen_s(&f,name,m) == 0
#define NO_SYSLOG
#else
#define openf(f,name,m)	((f = fopen (name, m)) != NULL)
#endif

/* DDS_Security_log -- Log a security event. */

void DDS_Security_log (unsigned   log_level,
		       const char *message,
		       const char *category)
{
	struct timeval	tv;
	FILE		*f;
#ifdef _WIN32
	struct tm	tm_data, *tm = &tm_data;
#else
	struct tm	*tm;
#endif
	char		tmbuf [40];
	DDS_LogInfo	log_info;

	if (!log_options_set || log_level < log_options.log_level)
		return;

	if (!log_options.log_file && !log_options.distribute) {
		log_printf (SEC_ID, log_level, "%s: %s\r\n", category, message);
		return;
	}
	if (log_options.log_file) {
		if (openf (f, logname, "a")) {
			gettimeofday (&tv, NULL);
#ifdef _WIN32
			_localtime32_s (&tm_data, &tv.tv_sec);
#else
			tm = localtime (&tv.tv_sec);
			if (!tm) {
#ifdef LOG_DATE
				fprintf (f, "\?\?\?\?-\?\?\?-\?\? ");
#endif
				fprintf (f, "\?\?:\?\?:\?\?.\?\?\? ");
			}
			else
#endif
			{
				tmbuf [0] = '\0';
#ifdef LOG_DATE
				strftime (tmbuf, sizeof (tmbuf), "%Y-%m-%d ", tm);
#endif
				strftime (&tmbuf [strlen (tmbuf)],
						sizeof (tmbuf) - strlen (tmbuf),
						"%H:%M:%S.", tm);
				snprintf (&tmbuf [strlen (tmbuf)],
					  sizeof (tmbuf) - strlen (tmbuf),
					  "%03lu ", (unsigned long) tv.tv_usec / 1000);
				fprintf (f, "%s", tmbuf);
			}
			fprintf (f, "%s: %s\r\n", category, message);
			fclose (f);
		}
	}
	if (log_options.distribute && log_writer) {
		log_info.log_level = log_level;
		log_info.message = (char *) message;
		log_info.category = (char *) category;
		DDS_DataWriter_write (log_writer, &log_info, 0);
	}
}

/* DDS_Security_enable_logging -- Enable security logging. */

DDS_EXPORT void DDS_Security_enable_logging (void)
{
	log_options_set = 1;
	DDS_Security_log (DDS_INFO_LEVEL, "SECURITY", "Logging started");
}

#endif

