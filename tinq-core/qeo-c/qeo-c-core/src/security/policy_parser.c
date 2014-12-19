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

/*#######################################################################
   #                       HEADER (INCLUDE) SECTION                        #
 ########################################################################*/
#ifndef DEBUG
#define NDEBUG
#endif
#include "policy_parser.h"
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <qeo/log.h>
/*#######################################################################
   #                           TYPES SECTION                               #
 ########################################################################*/
#define POLICYPARSER_MAGIC    0x34589def
struct policy_parser_s {
#ifndef NDEBUG
    unsigned long       magic;
#endif
    policy_parser_cfg_t cfg;
    uintptr_t           parser_cookie;
};
#define LINE_END              "\r\n"
#define META_TAG              "[meta]"
#define SEQNR                 "seqnr"
#define VERSION               "version"
#define R_ANGLE_BRACKET       "r<"
#define W_ANGLE_BRACKET       "w<"
#define PARTICIPANT_TAG       "[%31s]"

#define SUPPORTED_VERSION     "1.0"
/*#######################################################################
   #                   STATIC FUNCTION DECLARATION                         #
 ########################################################################*/
static bool is_valid_parser_init_cfg(const policy_parser_init_cfg_t *cfg);
static bool is_valid_parser_cfg(const policy_parser_cfg_t *cfg);

/*#######################################################################
   #                       STATIC VARIABLE SECTION                         #
 ########################################################################*/
static policy_parser_init_cfg_t _cfg;
static bool                     _initialized;
/*#######################################################################
   #                   STATIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/
static bool is_valid_parser_init_cfg(const policy_parser_init_cfg_t *cfg)
{
    if (cfg->on_participant_found_cb == NULL) {
        return false;
    }

    if (cfg->on_coarse_grained_rule_found_cb == NULL) {
        return false;
    }

    if (cfg->on_fine_grained_rule_section_found_cb == NULL) {
        return false;
    }

    if (cfg->on_sequence_number_found_cb == NULL) {
        return false;
    }

    return true;
}

static bool is_valid_parser_cfg(const policy_parser_cfg_t *cfg)
{
    if (cfg->buf == NULL) {
        return false;
    }

    return true;
}

#ifndef NDEBUG
static bool is_valid_policy_parser(policy_parser_hndl_t policy_parser)
{
    if (policy_parser->magic != POLICYPARSER_MAGIC) {
        return false;
    }

    if (is_valid_parser_cfg(&policy_parser->cfg) == false) {
        return false;
    }

    return true;
}
#endif

static bool process_sequence_number(policy_parser_hndl_t  parser,
                                    char                  *line)
{
    uint64_t  seqnr;
    char      *endptr;
    char      *value;

    assert(line[sizeof(SEQNR) - 1] == '=');
    value = line + sizeof(SEQNR);
    seqnr = strtoull(value, &endptr, 10);
    if (*endptr != '\0') {
        return false;
    }

    _cfg.on_sequence_number_found_cb(parser, seqnr);
    return true;
}

static bool process_version(policy_parser_hndl_t  parser,
                            char                  *line)
{
    char *value;

    assert(line[sizeof(VERSION) - 1] == '=');
    value = line + sizeof(VERSION);
    if (strcmp(value, SUPPORTED_VERSION) == 0) {
        return true;
    }
    return false;
}

static qeo_retcode_t process_fine_grained_rule(policy_parser_hndl_t parser,
                                               const char           *topic,
                                               char                 *topicvalue)
{
    const struct {
        char                        *needle;
        policy_parser_permission_t  perms;
    } _needles[] =
    {
        { .needle = R_ANGLE_BRACKET, { .read = true,  .write = false } },
        { .needle = W_ANGLE_BRACKET, { .read = false, .write = true  } }
    };


    char *haystack = topicvalue;

    for (int i = 0; i < sizeof(_needles) / sizeof(_needles[0]); ++i) {
        char  *ab_start;
        char  *ab_end;

        if ((ab_start = strstr(haystack, _needles[i].needle)) != NULL) {
            char  *semicolon_saveptr;
            char  *semicolon_ptr;
            char  *semicolon_separated;
            char  *ptr;
            ab_end = strchr(ab_start, '>');
            assert(ab_end != NULL);
            if (ab_end == NULL) {
                return QEO_EINVAL;
            }
            *ab_end = '\0';
            /* as strtok_r() modifies the string and we don't know whether r<
             * or w< will come first, we take a copy here of the section.
             * Consider abandoning the usage of strtok_r() in favor of
             * strspn() or strpbrk() */
            semicolon_ptr = strdup(ab_start + strlen(_needles[i].needle));
            if (semicolon_ptr == NULL) {
                return QEO_ENOMEM;
            }

            ptr = semicolon_ptr;
            while ((semicolon_separated = strtok_r(ptr, ";", &semicolon_saveptr)) != NULL) {
                _cfg.on_fine_grained_rule_section_found_cb(parser, parser->parser_cookie, topic, semicolon_separated, &_needles[i].perms);
                ptr = NULL;
            }

            free(semicolon_ptr);
            *ab_end = '>';
        }
    }

    return QEO_OK;
}

static void remove_ws(char *buf)
{
    char *tmp = buf;

    while (*buf != '\0') {
        if (0 == isspace(*buf)) {
            *tmp++ = *buf;
        }
        ++buf;
    }
    *tmp = '\0';
}

/*#######################################################################
   #                   PUBLIC FUNCTION IMPLEMENTATION                      #
 ########################################################################*/
qeo_retcode_t policy_parser_init(const policy_parser_init_cfg_t *cfg)
{
    if (_initialized == true) {
        return QEO_OK;
    }

    if ((cfg == NULL) || (is_valid_parser_init_cfg(cfg) == false)) {
        return QEO_EINVAL;
    }

    _cfg          = *cfg;
    _initialized  = true;

    return QEO_OK;
}

void policy_parser_destroy(void)
{
    _initialized = false;
}

qeo_retcode_t policy_parser_construct(const policy_parser_cfg_t *cfg,
                                      policy_parser_hndl_t      *parser)
{
    if (_initialized == false) {
        qeo_log_e("Bad state");
        return QEO_EBADSTATE;
    }

    if ((cfg == NULL) || (parser == NULL)) {
        qeo_log_e("Invalid args");
        return QEO_EINVAL;
    }

    if (is_valid_parser_cfg(cfg) == false) {
        qeo_log_e("Invalid cfg");
        return QEO_EINVAL;
    }

    *parser = calloc(1, sizeof(**parser));
    if (*parser == NULL) {
        qeo_log_e("Could not allocate memory");
        return QEO_ENOMEM;
    }

#ifndef NDEBUG
    (*parser)->magic = POLICYPARSER_MAGIC;
#endif
    (*parser)->cfg = *cfg;
    assert(is_valid_policy_parser(*parser));

    return QEO_OK;
}

qeo_retcode_t policy_parser_destruct(policy_parser_hndl_t *parser)
{
    if (_initialized == false) {
        return QEO_EBADSTATE;
    }

    if ((parser == NULL) || (*parser == NULL)) {
        return QEO_EINVAL;
    }

    assert(is_valid_policy_parser(*parser));
    free(*parser);
    *parser = NULL;

    return QEO_OK;
}

uint64_t policy_parser_get_sequence_number(char *content)
{
    char  *saveptr; /* for strtok_r() */
    char  *line         = NULL;
    bool  parsing_meta  = false;
    char  *ptr = NULL;
    char  *line_with_wsp_tabs = NULL;
    char  uidstr[32];
    uint64_t seqnr = 0;

    if (_initialized == false) {
        return QEO_EBADSTATE;
    }
    ptr = content;

    while ((line_with_wsp_tabs = strtok_r(ptr, LINE_END, &saveptr)) != NULL) {
        /* get rid of all \t and spaces */
        line  = line_with_wsp_tabs;
        ptr   = NULL;
        remove_ws(line_with_wsp_tabs);
        if ((line[0] == '\0') || (line[0] == '#')) {
            continue;
        }

        /* real work starts here */
        if (line[0] == '[') { /* start tag */
            if (strncmp(line, META_TAG, sizeof(META_TAG) - 1) == 0) {
                parsing_meta = true;
            }
            else if (sscanf(line, PARTICIPANT_TAG, uidstr) == 1) {
                parsing_meta = false;
            }
            else {
                qeo_log_w("Did not process unrecognized tag: %s", line);
            }
        }
        else {
            if (parsing_meta == true) {
                if (strncmp(line, SEQNR, sizeof(SEQNR) - 1) == 0) {
                    char      *endptr;
                    char      *value;
                    assert(line[sizeof(SEQNR) - 1] == '=');
                    value = line + sizeof(SEQNR);
                    seqnr = strtoull(value, &endptr, 10);
                    if (*endptr != '\0') {
                        seqnr = 0;
                    }
                }
            }
        }
    }

    return seqnr;
}

qeo_retcode_t policy_parser_run(policy_parser_hndl_t parser)
{
    char  *saveptr; /* for strtok_r() */
    char  *line         = NULL;
    bool  parsing_meta  = false;

#ifndef NDEBUG
    bool  metafound     = false;
    bool  versionfound  = false;
#endif
    char  *ptr = NULL;
    char  *line_with_wsp_tabs = NULL;
    char  uidstr[32];
    if (_initialized == false) {
        return QEO_EBADSTATE;
    }
    ptr = (char *)parser->cfg.buf;

    assert(is_valid_policy_parser(parser));
    while ((line_with_wsp_tabs = strtok_r(ptr, LINE_END, &saveptr)) != NULL) {
        /* get rid of all \t and spaces */
        line  = line_with_wsp_tabs;
        ptr   = NULL;
        remove_ws(line_with_wsp_tabs);
        if ((line[0] == '\0') || (line[0] == '#')) {
            continue;
        }

        /* real work starts here */
        if (line[0] == '[') { /* start tag */
            if (strncmp(line, META_TAG, sizeof(META_TAG) - 1) == 0) {
                /* parse meta stuff */
                assert(metafound == false); /* only one meta tag allowed, right ? */
                parsing_meta = true;
#ifndef NDEBUG
                metafound = true;
#endif
            }
            else if (sscanf(line, PARTICIPANT_TAG, uidstr) == 1) {
                char *closing_square_bracket = NULL;
                assert(metafound == true);
                closing_square_bracket = strchr(uidstr, ']');
                assert(closing_square_bracket != NULL);
                if (closing_square_bracket == NULL) {
                    qeo_log_e("Invalid syntax at line %s", line);
                    return QEO_EINVAL;
                }
                *closing_square_bracket = '\0';

                _cfg.on_participant_found_cb(parser, &parser->parser_cookie, uidstr);
                parsing_meta = false;
            }
            else {
                qeo_log_w("Did not process unrecognized tag: %s", line);
            }
        }
        else {
            if (parsing_meta == true) {
                if (strncmp(line, SEQNR, sizeof(SEQNR) - 1) == 0) {
                    assert(versionfound == true);
                    if (process_sequence_number(parser, line) == false) {
                        return QEO_EINVAL;
                    }
                }
                else if (strncmp(line, VERSION, sizeof(VERSION) - 1) == 0) {
                    assert(versionfound == false);
                    if (process_version(parser, line) == false) {
                        qeo_log_e("Version does not match " SUPPORTED_VERSION);
                        return QEO_EFAIL;
                    }
#ifndef NDEBUG
                    versionfound = true;
#endif
                }
            }
            else {
                policy_parser_permission_t  perms     = { .read = false, .write = false };
                char                        *topic    = line;
                char                        *eq_sign  = strchr(line, '=');
                assert(eq_sign != NULL);
                if (eq_sign == NULL) {
                    qeo_log_e("Invalid syntax at line %s", line);
                    return QEO_EINVAL;
                }
                char *value;

                *eq_sign  = '\0';
                value     = eq_sign + 1;
                if (strchr(value, '<') != NULL) {
                    process_fine_grained_rule(parser, line, value);
                }
                else {
                    if ((strchr(value, 'r') != NULL) || (strchr(value, 'R') != NULL)) {
                        perms.read = true;
                    }

                    if ((strchr(value, 'w') != NULL) || (strchr(value, 'W') != NULL)) {
                        perms.write = true;
                    }

                    _cfg.on_coarse_grained_rule_found_cb(parser, parser->parser_cookie, topic, &perms);
                }
            }
        }
    }
    assert(is_valid_policy_parser(parser));

    return QEO_OK;
}

qeo_retcode_t policy_parser_get_user_data(policy_parser_hndl_t  parser,
                                          uintptr_t             *user_data)
{
    if (_initialized == false) {
        return QEO_EBADSTATE;
    }

    if ((parser == NULL) || (user_data == NULL)) {
        return QEO_EINVAL;
    }

    *user_data = parser->cfg.user_data;

    return QEO_OK;
}
