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

/* Filtering test program:

   Known issues: BETWEEN on strings -> does not work
                        "%0 BETWEEN %3 AND %4", 

                        "'hij' BETWEEN 'abc' AND 'zyx'", 
                        "aStruct.aString BETWEEN 'abc' AND 'zyx'",
                        "aStruct.aString BETWEEN %3 AND %4",

                 BETWEEN with 3 different types of arguments might result in wrong results, e.g. PAR BETWEEN PAR AND DOUBLE 
 
                 FLOATING POINT CONSTANTS are upscaled to doubles -> this leads to unexpected results due to limited precision of floating points 

                 OPCODES for String to double conversion, ... are not used
                 OPCODES O_CRFF O_CRFT O_CRTF O_CRTT O_CRNE are not generated 


   Todo: SELECT, '*' operator and JOIN (for multitopic) -> currently not used 
         ARRAY, SEQUENCE, UNION support
         What is the use of the ; operator?

   Improving coverage: use not nested data structures next to nested
                       remove (ifdef) sql_dump_token 

 */




#include <cdr.h>
#include <error.h>
#include <bytecode.h>
#include <parse.h>
#include <dds/dds_types.h>
#include <dds/dds_aux.h>

typedef struct filter_data_st {
        unsigned char aChar;
        struct _inner_st {
                int16_t aShort;
                uint16_t anuShort;
                int32_t aLong;
                uint32_t anuLong;
                int64_t aLongLong;
                uint64_t anuLongLong;
                float aFloat;
                double aDouble;
                uint8_t aBoolean;
                uint8_t anOctet;
                char aString[10];
                enum {
                        enumVal1,
                        enumVal2,
                        enumVal3
                } anEnum;
        } aStruct;
} FilterData_t;


static DDS_TypeSupport_meta filter_data_tsm [] = {
	{ CDR_TYPECODE_STRUCT,     0, "FilterData", sizeof (struct filter_data_st), 0,                                         2,  0, NULL },
	/* Byte to trigger bad alignment */
	{ CDR_TYPECODE_CHAR,       0, "aChar",      0,                              offsetof (struct filter_data_st, aChar),   0,  0, NULL },
	{ CDR_TYPECODE_STRUCT,     0, "aStruct",    sizeof (struct _inner_st),      offsetof (struct filter_data_st, aStruct), 12, 0, NULL },
	{ CDR_TYPECODE_SHORT,      0, "aShort",     0,                              offsetof (struct _inner_st, aShort),       0,  0, NULL },
	{ CDR_TYPECODE_USHORT,     0, "anuShort",   0,                              offsetof (struct _inner_st, anuShort),     0,  0, NULL },
	{ CDR_TYPECODE_LONG,       0, "aLong",      0,                              offsetof (struct _inner_st, aLong),        0,  0, NULL },
	{ CDR_TYPECODE_ULONG,      0, "anuLong",    0,                              offsetof (struct _inner_st, anuLong),      0,  0, NULL },
	{ CDR_TYPECODE_LONGLONG,   0, "aLongLong",  0,                              offsetof (struct _inner_st, aLongLong),    0,  0, NULL },
	{ CDR_TYPECODE_ULONGLONG,  0, "anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong",0,                              offsetof (struct _inner_st, anuLongLong),  0,  0, NULL },
	{ CDR_TYPECODE_FLOAT,      0, "aFloat",     0,                              offsetof (struct _inner_st, aFloat),       0,  0, NULL },
	{ CDR_TYPECODE_DOUBLE,     0, "aDouble",    0,                              offsetof (struct _inner_st, aDouble),      0,  0, NULL },
	{ CDR_TYPECODE_BOOLEAN,    0, "aBoolean",   0,                              offsetof (struct _inner_st, aBoolean),     0,  0, NULL },
	{ CDR_TYPECODE_OCTET,      0, "anOctet",    0,                              offsetof (struct _inner_st, anOctet),      0,  0, NULL },
	{ CDR_TYPECODE_CSTRING,    0, "aString",    10,                             offsetof (struct _inner_st, aString),      0,  0, NULL },
        { CDR_TYPECODE_ENUM,       0, "anEnum",     0,                              offsetof (struct _inner_st, anEnum),       3,  0, NULL },
        { CDR_TYPECODE_LONG,       0, "enumVal1",   0,                              0,                                         0,  0, NULL },
        { CDR_TYPECODE_LONG,       0, "enumVal2",   0,                              0,                                         0,  1, NULL },
        { CDR_TYPECODE_LONG,       0, "enumVal3",   0,                              0,                                         0,  2, NULL }
};

char * illegal_filters[] = {
        /* Non-existing field */
        "noSuchField > 10",
        /* Repetition of operator */
        "aChar >> 10",
        /* Like, not on a string (left and right hand)*/
        "aChar LIKE 10",
        /* Like, not on a string (only right hand)*/
        "aStruct.aString LIKE 3.5",
        /* Illegal character/operator*/
        "aChar @ 10",
        /* Double dots in floating point */
        "aChar > 10..",
        /* Only dots in floating point */
        "aChar > -..",
        "aChar > e123457890123456789012345678901234567890",
        /* Maximum 100 parameters allowed */
        "aChar > %101",
        /* String not terminated */
        "aStruct.aString = 'string",
        /* aStruct has no member aChar */
        "aStruct.aChar = 1",
        "aStruct = 123",
        "aStruct.aString <> aStruct.aLong",
        "aStruct.aString <> aStruct.aLongLong",
        "aStruct.aString <> aStruct.aDouble",
        /* Non-existing enum value */
        "aStruct.anEnum = enumVal4",
        /* Operator at EOL */
        "aChar >",
        "aChar <",
        "aChar <=",
        "aChar >=",
        "aChar <>",
        /* Underscores behave special, but no _ field */
        "_ <> _",
        NULL
};

char * success_filters[] = {
        "'h' BETWEEN 'a' AND 'z'",
        "'5' BETWEEN 1 AND 255",
        "aStruct.aLongLong BETWEEN %2 AND %0",
        "aStruct.aLong = 1",
        "aChar = 'c'",
        "aStruct.aShort BETWEEN 1 AND 3",
        "7 BETWEEN aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong AND aStruct.aDouble",
        "aStruct.aShort NOT BETWEEN -7 AND -20",
        "aStruct.aString = 'string'",
        "aStruct.aString <> 'a'",
        "'a' <> aStruct.aString",
        "aStruct.aString <> aChar",
        "aStruct.aString LIKE 'string'", 
        "aStruct.aString LIKE 'st%'",
        "aStruct.aString LIKE '%tring'",
        "aStruct.aString LIKE '%tri%'",
        "NOT aStruct.aString LIKE '%blah%'",
        "aStruct.anuShort >= 3",
        "NOT aStruct.anuShort <> 3",
        "NOT aStruct.anuShort < 3",
        "NOT aStruct.anuShort > 3",
        "NOT aStruct.anuShort <= 2",
        "NOT aStruct.anuShort >= 4",
        "NOT (NOT (NOT aStruct.anuShort >= 4 AND NOT aStruct.anuShort <= 2))",
        "aStruct.anuLong <= 4",
        "aStruct.anuLong BETWEEN aStruct.aShort AND aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong",
        "aStruct.aFloat BETWEEN aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong AND aStruct.aDouble",
        "aStruct.aLongLong < -1",
        "aStruct.aLongLong = -5",
        "aStruct.aLongLong > -127",
        "aStruct.aLongLong >= -32768",
        "aStruct.aLongLong >= -2147484",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong > 5",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong = 6",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong < 1234567890",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong < 12345",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong < 1234567890123",
        "aStruct.aLongLong > -1234567890123",
        "aStruct.aFloat > 7. AND aStruct.aFloat < 8.", /* Float to double conversion causes comparison issues */ 
        "aStruct.aDouble = 8.2",
        "aStruct.aDouble > -.2",
        "aStruct.aDouble > %5",
        "aStruct.aDouble > 8.",
        "aStruct.aDouble < 856.25e3",
        "aStruct.aDouble < .25e+3",
        "aStruct.aDouble > .25E-3",
        "aStruct.aDouble > aStruct.anuLong",
        "aStruct.aDouble > aStruct.aLong",
        "aStruct.aDouble > aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong",
        "aStruct.aDouble > aStruct.aLongLong",
        "aStruct.aFloat > aStruct.anuLong",
        "aStruct.aFloat > aStruct.aLong",
        "aStruct.aFloat > aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong",
        "aStruct.aFloat > aStruct.aLongLong",
        "aStruct.aBoolean = 1",
        "aStruct.anOctet = 10",
        "aStruct.anOctet = 0xa",
        "aStruct.anOctet > 0x9",
        "aStruct.anOctet < 0xBC",
        "( aStruct.aString = 'string' ) AND ( aStruct.aLong > 0 )",
        "aStruct.aString = 'string' OR aStruct.aLong = 1",
        "aStruct.aString <> 'string' OR aStruct.aLong = 1",
        "aStruct.aString = 'string' OR aStruct.aLong <> 1",
        "aStruct.anEnum = enumVal2",
        /* With params */
        "aChar = %0",
        "aStruct.anuShort <= %0",
        "aStruct.anuLong < %0",
        "aChar = %0 AND aStruct.aString = %1",
        "%0 <> %1",
        NULL
};

char * fail_filters[] = {
        "aStruct.aLong NOT BETWEEN -33 AND 33",
        "aStruct.aLong <> 1",
        "aStruct.aLong = 2",
        "aStruct.anuLong < 3",
        "aStruct.anuLong < 4",
        "aStruct.anuLong <=3 ",
        "aStruct.anuLong > 4",
        "aStruct.anuLong >= 5",
        "aStruct.anuLong > 5",
        "aStruct.aString LIKE 'blablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablablabla%'",
        "NOT aStruct.anuShort NOT BETWEEN 4 AND 100",
        "aStruct.anuShort BETWEEN 4 AND 100",
        "aStruct.anuShort BETWEEN 0 AND 2",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong BETWEEN 10 AND 100",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong BETWEEN 0 AND 3",
        "aStruct.aLongLong BETWEEN -50 AND -20",
        "aStruct.aLongLong BETWEEN 0XFFFFF AND 0xffffffff",
        "aStruct.aLong BETWEEN -50 AND -20",
        "aStruct.aLong BETWEEN 100 AND 200",
        "aStruct.aDouble BETWEEN 8.0 AND 8.1",
        "aStruct.aDouble BETWEEN 8.4 AND 8.5",
        NULL
};

char * error_filters[] = {
        "aChar > %98",
        "aStruct.anuLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLongLong < %98",
        "aStruct.aDouble >= %98",
        "aStruct.aString = %98",
        NULL
};

char **filters[] = {
        success_filters,
        fail_filters,
        error_filters
};

/* Create Plain Sample :{{{ */
void 
plain_sample (unsigned char c, DBW * plain_dbw) 
{
        FilterData_t 		*plain_data;
        plain_dbw->dbp    = db_alloc_data (sizeof(FilterData_t)+4, 1);

        if (plain_dbw->dbp == NULL)
                fatal_printf ("Could not allocate %d bytes\r\n", sizeof(FilterData_t));

        plain_dbw->data   = plain_dbw->dbp->data;
        plain_data       = (FilterData_t *) (plain_dbw->data + 4);
        plain_dbw->left   = sizeof(FilterData_t) + 4;
        plain_dbw->length = sizeof(FilterData_t) + 4;

        plain_data->aChar = c;
        plain_data->aStruct.aLong = 1;
        plain_data->aStruct.aShort = 2;
        strcpy(plain_data->aStruct.aString, "string");
        plain_data->aStruct.anuShort = 3;
        plain_data->aStruct.anuLong = 4;
        plain_data->aStruct.aLongLong = -5LL;
        plain_data->aStruct.anuLongLong = 6ULL;
        plain_data->aStruct.aFloat = 7.1;
        plain_data->aStruct.aDouble = 8.2;
        plain_data->aStruct.aBoolean = 1;
        plain_data->aStruct.anOctet = 10;
        plain_data->aStruct.anEnum = enumVal2;


        plain_dbw->dbp->data [0] = 0;
        plain_dbw->dbp->data [1] = (MODE_RAW << 1);
        plain_dbw->dbp->data [2] = plain_dbw->dbp->data [3] = 0;
}
/* }}} */

/* Create Marshalled Sample: {{{ */
void
marshall_data (void *plain_data, DDS_TypeSupport *ts, int swap, DBW *marshalled_dbw)
{
	size_t                  marshalled_size;
	DDS_ReturnCode_t	ret;
#ifdef DDS_DEBUG
        unsigned int            i;
#endif

        marshalled_size = DDS_MarshalledDataSize ((void *) plain_data, 0, ts, &ret);

        if (ret != DDS_RETCODE_OK)
                fatal_printf ("MarshallDataSize failed\r\n");
#ifdef DDS_DEBUG
        else
                dbg_printf ("MarshallDataSize: %d\r\n", marshalled_size);
#endif

        marshalled_dbw->dbp = db_alloc_data (marshalled_size, 1);

        if (marshalled_dbw->dbp == NULL)
                fatal_printf ("Could not allocate %d bytes\r\n", marshalled_size);

        marshalled_dbw->data = marshalled_dbw->dbp->data;
        marshalled_dbw->left = marshalled_size;
        marshalled_dbw->length = marshalled_size;

        if (cdr_marshall (marshalled_dbw->dbp->data + 4, 4, plain_data, ts->ts_cdr, 0, 0, swap) != DDS_RETCODE_OK)
                fatal_printf ("Failed to marshall data\r\n");

        marshalled_dbw->dbp->data [0] = 0;
        marshalled_dbw->dbp->data [1] = (MODE_CDR << 1) | (ENDIAN_CPU ^ swap);
        marshalled_dbw->dbp->data [2] = marshalled_dbw->dbp->data [3] = 0;

#ifdef DDS_DEBUG
        for (i=0; i < marshalled_size; i++)
                dbg_printf("%02x ", marshalled_dbw->dbp->data [i]);

        dbg_printf("\r\n");
#endif
}
/* }}} */

#define DBW_DEC(w, n) (w).data -= n; (w).left += n; (w).length += n

int 
main (int argc, char ** argv) 
{
        DDS_DomainParticipant	part;
        DDS_TypeSupport		*filter_data_ts;
        unsigned int		i;
        int			res;
        BCProgram		program1;
        BCProgram		program2;
        DBW			plain_dbw1;
        DBW			plain_dbw2;
        DBW			marshalled_dbw1;
        int			swap;
        int			with_cache;
        int                     fail;
        void                    *cache = NULL;
        void                    *cache_test = NULL;
        Strings_t               params;
        char                    **fi;

	(void) argc;
	(void) argv;

        /* Initialize {{{ */
        /* Disable rtps */
        DDS_RTPS_control (0);

        /* Initialize dds */
        part = DDS_DomainParticipantFactory_create_participant (0, NULL, NULL, 0);
        err_actions_add (EL_LOG, ACT_PRINT_STDIO);
        err_actions_add (EL_DEBUG, ACT_PRINT_STDIO);
        if (!part)
                fatal_printf ("DDS_DomainParticipantFactory_create_participant () failed!\r\n");
        else
                dbg_printf ("DDS Domain Participant created.\r\n");


        filter_data_ts = DDS_DynamicType_register (filter_data_tsm);
        if (!filter_data_ts)
                fatal_printf ("DDS_DynamicType_register () failed!\r\n");
        else
                dbg_printf ("DDS DynamicType registered.\r\n");

        if (DDS_DomainParticipant_register_type (part, filter_data_ts, "FilterData") != DDS_RETCODE_OK)
                fatal_printf ("DDS DomainParticipant register type () failed!\r\n"); 

        /* }}} */

        /* Create two plain samples {{{ */
        plain_sample('c', &plain_dbw1);
        plain_sample('e', &plain_dbw2);
        /* }}} */

        /* Trigger some filtering error conditions {{{ */
        res = sql_parse_filter(filter_data_ts, "aChar > 10", NULL);
        if (!res) 
                fatal_printf ("Filters with no program passed should not compile\n");
        res = sql_parse_filter(NULL, "aChar > 10", &program1);
        if (!res) 
                fatal_printf ("Filters with no typecode passed should not compile\n");
        res = sql_parse_filter(filter_data_ts, NULL, &program1);
        if (!res) 
                fatal_printf ("Filters with no filter passed should not compile\n");


        fi = illegal_filters;

        while (*fi)
        {
                res = sql_parse_filter(filter_data_ts, *fi, &program1);
                if (!res) 
                        fatal_printf ("Filters %s should not compile\n", *fi);
                fi++;
        }
        /* }}} */

        /* Parameter and parameter cache testing {{{ */
        res = sql_parse_filter(filter_data_ts, "%0 > 10", &program1);
        if (res) 
                fatal_printf ("Simple parameter test program did not compile\n");

        DDS_SEQ_INIT (params);

        /* %0 */
        strings_append_cstr (&params, "99");

        bc_cache_init((void *) &cache_test);

        i = bc_interpret (&program1, &params, (void *) &cache_test, &plain_dbw1 , NULL, 0, filter_data_ts, &res);

        if (i) 
                fatal_printf("First cache test failed\r\n");

        /* %1 */
        strings_append_cstr (&params, "string");
        /* %2 */
        strings_append_cstr (&params, "-99");
        /* %3 */
        strings_append_cstr (&params, "abc");
        /* %4 */
        strings_append_cstr (&params, "zyx");
        /* %5 */
        strings_append_cstr (&params, "6.1");


        res = sql_parse_filter(filter_data_ts, "%5 > 3", &program1);
        if (res) 
                fatal_printf ("Second simple parameter test program did not compile\n");
        
        i = bc_interpret (&program1, &params, (void *) &cache_test, &plain_dbw1 , NULL, 0, filter_data_ts, &res);

        if (i) 
                fatal_printf("Second cache test failed\r\n");


        /*  }}} */

        /* Filtering real test {{{ */
        for (with_cache =0; with_cache <=1; with_cache ++)
        {
                if (with_cache) 
                        bc_cache_init((void *) &cache);

                for (fail = 0; fail <=2; fail ++)
                {
                        for (swap = 0; swap <=1; swap ++)
                        {
                                fi = filters[fail];

                                marshall_data (plain_dbw1.data + 4, filter_data_ts, swap, &marshalled_dbw1);

                                while (*fi) {
                                        res = sql_parse_filter (filter_data_ts, *fi, &program1);
                                        bc_cache_flush((void *) &cache);
                                        if (res) 
                                                fatal_printf ("Could not parse filter %s\n",*fi);

#ifdef DDS_DEBUG
                                        bc_dump (0, &program1);
                                        dbg_printf("\r\n");
#endif

                                        /* Plain data */

                                        DBW_INC (plain_dbw1, 4);

                                        i = bc_interpret (&program1, &params, (void *) &cache, &plain_dbw1 , NULL, 0, filter_data_ts, &res);
                                        
                                        DBW_DEC (plain_dbw1, 4);

                                        if (i && fail != 2)
                                                fatal_printf ("bc_interpret for program %s on plain data failed %d\r\n", *fi, i); 
                                        if (!i && fail == 2)
                                                fatal_printf ("bc_interpret for program %s on plain data succeeded %d\r\n", *fi, i); 
                                        if (res == fail)
                                                fatal_printf ("Filter %s on plain data had wrong result (%d)\r\n", *fi, res); 


                                        /* Raw prefixed, also cache set to NULL */

                                        i = bc_interpret (&program1, &params, NULL, &plain_dbw1 , NULL, 1, filter_data_ts, &res);
                                        
                                        if (i && fail != 2)
                                                fatal_printf ("bc_interpret for program %s on raw prefixed data failed %d\r\n", *fi, i); 
                                        if (!i && fail == 2)
                                                fatal_printf ("bc_interpret for program %s on raw prefixed data succeeded %d\r\n", *fi, i); 
                                        if (res == fail)
                                                fatal_printf ("Filter %s on plain data had wrong result (%d)\r\n", *fi, res); 


                                        /* Marshalled */

                                        i = bc_interpret (&program1, &params, (void *) &cache, &marshalled_dbw1, NULL, 1, filter_data_ts, &res);
                                        
                                        if (i && fail != 2)
                                                fatal_printf ("bc_interpret for program %s on marshalled data failed %d\r\n", *fi, i); 
                                        if (!i && fail == 2)
                                                fatal_printf ("bc_interpret for program %s on marshalled data succeeded %d\r\n", *fi, i); 

                                        if (res == fail)
                                                fatal_printf ("Filter %s on marshalled %sdata had wrong result\r\n", *fi,swap?"swapped ":""); 

                                        fi++;
                                } 
                        }
                }
        }
        /* }}} */

        /* Trigger some query error conditions {{{ */
        res = sql_parse_query(filter_data_ts, "aChar > 10 ORDER BY aStruct.aLong, aChar", NULL, &program2);
        if (!res) 
                fatal_printf ("Queries with insufficient programs passed should not compile\n");
        res = sql_parse_query(filter_data_ts, "aChar > 10 ORDER BY aStruct.aLong, aChar", &program1, NULL);
        if (!res) 
                fatal_printf ("Queries with insufficient programs passed should not compile\n");
        res = sql_parse_query(filter_data_ts, "aChar > 10 ORDER BY aStruct.aLong, aChar", NULL, NULL);
        if (!res) 
                fatal_printf ("Queries with insufficient programs passed should not compile\n");
        res = sql_parse_query(NULL, "aChar > 10 ORDER BY aStruct.aLong, aChar", &program1, &program2);
        if (!res) 
                fatal_printf ("Queries with insufficient programs passed should not compile\n");
        res = sql_parse_query(filter_data_ts,NULL, &program1, &program2);
        if (!res) 
                fatal_printf ("Queries without program string should not compile\n");
        /* }}} */
       
        /* Querying real test {{{ */ 
        res = sql_parse_query(filter_data_ts, "aChar > 10 ORDER BY aStruct.aLong, aChar", &program1, &program2);
        
        if (res)
                fatal_printf ("Could not parse query\n");

        DBW_INC (plain_dbw1, 4);
        DBW_INC (plain_dbw2, 4);
        if ((i = bc_interpret (&program2, NULL, NULL, &plain_dbw1 , &plain_dbw2, 0, filter_data_ts, &res)))
                fatal_printf ("bc_interpret for program ORDER BY aStruct.aLong, aChar on plain data failed\r\n"); 
        
        if (res != -1)
                fatal_printf ("bc_interpret for program ORDER BY aStruct.aLong, aChar had wrong result\r\n");
        
        if ((i = bc_interpret (&program2, &params, (void *) &cache, &plain_dbw2 , &plain_dbw1, 0, filter_data_ts, &res)))
                fatal_printf ("bc_interpret for program ORDER BY aStruct.aLong, aChar on plain data failed\r\n"); 
        
        if (res != 1)
                fatal_printf ("bc_interpret for program ORDER BY aStruct.aLong, aChar had wrong result\r\n");
        
        if ((i = bc_interpret (&program2, &params, (void *) &cache, &plain_dbw1 , &plain_dbw1, 0, filter_data_ts, &res)))
                fatal_printf ("bc_interpret for program ORDER BY aStruct.aLong, aChar on plain data failed\r\n"); 
        
        if (res != 0)
                fatal_printf ("bc_interpret for program ORDER BY aStruct.aLong, aChar had wrong result\r\n");

        DBW_DEC (plain_dbw1, 4);
        DBW_DEC (plain_dbw2, 4); 
        /* }}} */
        
	if (DDS_DomainParticipantFactory_delete_participant (part) != DDS_RETCODE_OK)
                fatal_printf ("DDS_DomainParticipantFactory_delete_participant () failed!\r\n");

        return (EXIT_SUCCESS);
}

/* vim: set foldmethod=marker foldmarker={{{,}}}: */
