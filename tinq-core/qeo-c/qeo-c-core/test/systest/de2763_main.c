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
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <qeocore/api.h>
#include "TypeWithStructs.h"
#include "common.h"
#include "verbose.h"

static sem_t _sync;
static sem_t _sync_nodata;

/*
 * \brief To get random value of integer in predefined range.
 *
 * \param[in] min_num   The minimum value of the range
 * \param[in] max_num   The maximum value of the range
 *
 * \return  The random integer value inside (minimum , maximum) range
 */
static int _random_number_in_range(int min_num, int max_num)
{
    int result  = 0;
    int low_num = 0;
    int hi_num  = 0;

    if(min_num<max_num)
    {
             low_num=min_num;
             hi_num=max_num+1;
    }else{
             low_num=max_num+1;
             hi_num=min_num;
    }
    result = (rand()%(hi_num-low_num))+low_num;
    return result;
}

/*
 * \brief To get random string based on given maximum length.
 *
 * \param[in,out] input_char    The preallocated string that should be populated with random characters
 * \param[in] max_length        The maximum length of the string
 *
 */
static char* str_random(char *input_char, int max_length)
{
    int i       = 0;
    int rnd     = 0;

    if (!input_char)
    {
           return NULL;
    }

    for(i=0; i < max_length-1; i++)
    {
        if(i%2 == 0)
        {
            rnd = _random_number_in_range(48,90);
        }
        else
        {
            rnd = _random_number_in_range(97,126);
        }
        input_char[i] = (char)rnd;
    }
    input_char[i] = '\0';

    return input_char;
}

static void on_data_available(const qeocore_reader_t *reader,
                              const qeocore_data_t *data,
                              uintptr_t userdata)
{
    switch (qeocore_data_get_status(data)) {
        case QEOCORE_DATA: {
            org_qeo_dynamic_qdm_test_TypeWithStructs_t *t = (org_qeo_dynamic_qdm_test_TypeWithStructs_t *) qeocore_data_get_data(data);

            /* verify struct TODO */
            assert(NULL != t);
            /* release main thread */
            sem_post(&_sync);
            break;
        }
        case QEOCORE_NO_MORE_DATA:
            /* release main thread */
            sem_post(&_sync_nodata);
            break;
        default:
            abort();
            break;
    }
}

static qeo_retcode_t write_generated_data(qeocore_writer_t *writer, int arraysize){
    org_qeo_dynamic_qdm_test_TypeWithStructs_t t = {};
    size_t i, j;
    qeo_retcode_t rc = QEO_EFAIL;
    int stringsize = 10;

    t.mfloat32 = 32;
    t.mstring = "asdfdfasdfqwreqwrqwdfadfadsfadfd";

    DDS_SEQ_INIT(t.msubstruct3);
    org_qeo_dynamic_qdm_test_Substruct3_t ss3[arraysize];
    char strings[arraysize][arraysize+2][stringsize];
    for (i = 0; i < arraysize; ++i) {
        ss3[i].msubfloat = i;
        ss3[i].msubstring = str_random(strings[i][0], stringsize);
        DDS_SEQ_INIT(ss3[i].msubstruct2);
        org_qeo_dynamic_qdm_test_Substruct2_t ss2[arraysize];
        for (j = 0; j < arraysize; ++j) {
            ss2[j].msubshort = j*i;
            ss2[j].msubstring = str_random(strings[i][j+2], stringsize);
        }
        dds_seq_from_array(&ss3[i].msubstruct2, ss2, arraysize);
    }
    dds_seq_from_array(&t.msubstruct3, ss3, arraysize);

    DDS_SEQ_INIT(t.msubstruct1);
    org_qeo_dynamic_qdm_test_Substruct1_t ss1[arraysize];
    for (i = 0; i < arraysize; ++i) {
        ss1[i].msubint32 = i;
        ss1[i].msubstring = str_random(strings[i][1], stringsize);
    }
    dds_seq_from_array(&t.msubstruct1, ss1, arraysize);

    rc = qeocore_writer_write(writer, &t);
    for(i = 0; i < DDS_SEQ_LENGTH(t.msubstruct3); ++i) {
        dds_seq_cleanup(&(DDS_SEQ_ITEM(t.msubstruct3, i).msubstruct2));
    }
    dds_seq_cleanup(&t.msubstruct3);
    dds_seq_cleanup(&t.msubstruct1);

    return rc;
}

int main(int argc, const char **argv)
{
    qeo_factory_t *factory;
    qeocore_type_t *type;
    qeocore_reader_t *reader;
    qeocore_reader_listener_t listener = { .on_data = on_data_available };
    qeocore_writer_t *writer;
    int i = 0;

    /* initialize */
    sem_init(&_sync, 0, 0);
    sem_init(&_sync_nodata, 0, 0);
    assert(NULL != (factory = qeocore_factory_new(QEO_IDENTITY_DEFAULT)));
    init_factory(factory);
    assert(NULL != (type = qeocore_type_register_tsm(factory, org_qeo_dynamic_qdm_test_TypeWithStructs_type, org_qeo_dynamic_qdm_test_TypeWithStructs_type[0].name)));
    assert(NULL != (reader = qeocore_reader_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA, &listener, NULL)));
    assert(NULL != (writer = qeocore_writer_open(factory, type, NULL, QEOCORE_EFLAG_EVENT_DATA, NULL, NULL)));
    /* test late enabling of readers/writers */
    assert(QEO_OK == qeocore_reader_enable(reader));
    assert(QEO_OK == qeocore_writer_enable(writer));
    /* send structure */
    for(i = 0; i < 20; i++){
        log_verbose("testing with %d", i);
        assert(QEO_OK == write_generated_data(writer, i));
        log_verbose("waiting for data");
        /* wait for reception */
        sem_wait(&_sync);
        sem_wait(&_sync_nodata);
    }

    /* clean up */
    qeocore_writer_close(writer);
    qeocore_reader_close(reader);
    qeocore_type_free(type);
    qeocore_factory_close(factory);
    sem_destroy(&_sync);
}
