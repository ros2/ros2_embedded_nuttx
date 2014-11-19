#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include "thread.h"
#include "libx.h"
#include "tty.h"
#ifdef DDS_SECURITY
#include "dds/dds_security.h"
#ifdef DDS_NATIVE_SECURITY
#include "nsecplug/nsecplug.h"
#else
#include "msecplug/msecplug.h"
#include "assert.h"
#include "../../plugins/secplug/xmlparse.h"
#endif
#include "../../plugins/security/engine_fs.h"
#endif
#include "dds/dds_aux.h"
#include "dds/dds_debug.h"
#include "libx.h"
//#include <typecode.h>
//#include <cdr.h>
#include <db.h>


typedef struct _Vector3__st
{
    double x_;
    double y_;
    double z_;
} Vector3_;

void Vector3__fill(Vector3_ *ret)
{
    (*ret).x_ = 1;
    (*ret).y_ = 1;
    (*ret).z_ = 1;
}

Vector3_ *Vector3__create()
{
    Vector3_ *ret = malloc(sizeof(Vector3_));
    Vector3__fill(ret);
    return ret;
}

static DDS_TypeSupport_meta Vector3__tsm [] =
{
    {CDR_TYPECODE_STRUCT   , 0, "Vector3_", sizeof (struct _Vector3__st), 0, 3, 0, NULL},
    {CDR_TYPECODE_DOUBLE   , 0, "x_", 0, offsetof (struct _Vector3__st, x_), 0, 0, NULL},
    {CDR_TYPECODE_DOUBLE   , 0, "y_", 0, offsetof (struct _Vector3__st, y_), 0, 0, NULL},
    {CDR_TYPECODE_DOUBLE   , 0, "z_", 0, offsetof (struct _Vector3__st, z_), 0, 0, NULL}
};

int main()
{
    DDS_DomainParticipant participant;
    DDS_Publisher         publisher;
    DDS_Subscriber        subscriber;
    DDS_InstanceHandle_t  instance;
    DDS_DataWriterQos     wr_qos;
    DDS_DataReaderQos     rd_qos;
    DDS_ReturnCode_t      error;
    unsigned char *buf;
    size_t size;
    DBW dbw;
    DBW dbw2;
    static DDS_DataSeq      rx_sample = DDS_SEQ_INITIALIZER (void *);
    static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);
    static DDS_DataSeq      rx_sample_own;
    static DDS_SampleInfoSeq rx_info_own;
    DDS_TypeSupport *Vector3__ts;
    DDS_Topic *Vector3__topic, *Vector3__topic2;
    DDS_DataWriter *Vector3__writer;
    DDS_DataReader *Vector3__reader;
    Vector3_ * Vector3__sample;
    DDS_RTPS_control (0);
    participant = DDS_DomainParticipantFactory_create_participant (0, NULL, NULL, 0);
    if (!participant)
    {
        fprintf (stderr, "Could not create participant\n");
        exit (EXIT_FAILURE);
    }
    publisher = DDS_DomainParticipant_create_publisher (participant, NULL, NULL, 0);
    if (!publisher)
    {
        fprintf (stderr, "Could not create publisher\n");
        exit (EXIT_FAILURE);
    }
    subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0);
    if (!subscriber)
    {
        fprintf (stderr, "Could not create subscriber\n");
        exit (EXIT_FAILURE);
    }
    DDS_Publisher_get_default_datawriter_qos (publisher, &wr_qos);
    wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
    DDS_Subscriber_get_default_datareader_qos (subscriber, &rd_qos);
    rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
    printf("Working on Vector3_\n");
    Vector3__ts = DDS_DynamicType_register (Vector3__tsm);
    if (!Vector3__ts)
    {
        printf ("DDS_DynamicType_register () failed for Vector3_!\n");
        return (EXIT_FAILURE);
    }
    if (DDS_DomainParticipant_register_type (participant, Vector3__ts, "Vector3_"))
    {
        printf ("DDS_DomainParticipant_register_type () failed for Vector3_!\n"); return (EXIT_FAILURE);
    }
    if (DDS_DomainParticipant_register_type (participant, Vector3__ts, "Vector3__alias"))
    {
        printf ("DDS_DomainParticipant_register_type () failed for Vector3__alias!\n"); return (EXIT_FAILURE);
    }
    if (DDS_DomainParticipant_register_type (participant, Vector3__ts, "Vector3_"))
    {
        printf ("DDS_DomainParticipant_register_type () failed for Vector3_ (second time)!\n"); return (EXIT_FAILURE);
    }
    Vector3__topic = DDS_DomainParticipant_create_topic (participant, "Vector3_", "Vector3_", NULL, NULL, 0);
    if (!Vector3__topic)
    {
        printf ("DDS_DomainParticipant_create_topic () failed for Vector3_!\n");
        return (EXIT_FAILURE);
    }
    Vector3__topic2 = DDS_DomainParticipant_create_topic (participant, "Vector3__alias", "Vector3_", NULL, NULL, 0);
    if (!Vector3__topic2)
    {
        printf ("DDS_DomainParticipant_create_topic () failed for Vector3__alias!\n");
        return (EXIT_FAILURE);
    }
    Vector3__writer = DDS_Publisher_create_datawriter (publisher, Vector3__topic, &wr_qos, NULL, 0);
    if (!Vector3__writer)
    {
        fprintf (stderr, "Could not create data writer\n");
        exit (EXIT_FAILURE);
    }
    Vector3__reader = DDS_Subscriber_create_datareader (subscriber, Vector3__topic, &rd_qos, NULL, 0);
    if (!Vector3__reader)
    {
        fprintf (stderr, "Could not create data reader\n");
        exit (EXIT_FAILURE);
    }
    Vector3__sample = Vector3__create ();
    size = DDS_MarshalledDataSize((void *) Vector3__sample, Vector3__ts, &error);
    if (error != DDS_OK)
    {
        fprintf (stderr, "Could not get datasize for Vector3_\n");
        exit(EXIT_FAILURE);
    }
    dbw.dbp = db_alloc_data (size, 1);
    dbw.data = dbw.dbp->data; dbw.left = size; dbw.length = size;
    error = DDS_MarshallData(dbw.data, Vector3__sample, Vector3__ts);
    if (error != DDS_OK)
    {
        fprintf (stderr, "Failed to marshall data for Vector3_\n");
        exit(EXIT_FAILURE);
    }
    size = DDS_UnmarshalledDataSize(dbw, Vector3__ts, &error);
    printf("Vector3_ %d <> %d\n", size, sizeof(Vector3_));
    buf = malloc(size);
    DDS_UnmarshallData(buf, &dbw, Vector3__ts);
    free(buf);
    DDS_DataWriter_write(Vector3__writer, (void *) Vector3__sample, DDS_HANDLE_NIL);
    DDS_DataReader_take (Vector3__reader, &rx_sample, &rx_info, 1, DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
    DDS_DataReader_return_loan (Vector3__reader, &rx_sample, &rx_info);
    if (DDS_Subscriber_delete_datareader (subscriber, Vector3__reader))
    {
        fprintf(stderr, "Could not delete datareader\n");
        return (EXIT_FAILURE);
    }
    DDS_Publisher_delete_datawriter (publisher, Vector3__writer);
    DDS_DomainParticipant_delete_topic (participant, Vector3__topic);
    DDS_DomainParticipant_delete_topic (participant, Vector3__topic2);
    Vector3__ts = DDS_DynamicType_register (Vector3__tsm);
    if (!Vector3__ts)
    {
        printf ("DDS_DynamicType_register () failed for Vector3_!\n");
        return (EXIT_FAILURE);
    }
    DDS_DynamicType_free (Vector3__ts);
    DDS_DynamicType_free (Vector3__ts);
    DDS_DynamicType_free (Vector3__ts);
    DDS_DomainParticipant_delete_contained_entities (participant);
    if (DDS_DomainParticipantFactory_delete_participant (participant))
    {
        fprintf (stderr, "Could not delete domain participant.\n");
        exit (EXIT_FAILURE);
    }
    return (EXIT_SUCCESS);
}