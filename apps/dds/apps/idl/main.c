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

#include <stdio.h>
#include <stdlib.h>
#include "idldump.h"
#include "tsmdump.h"
#include "cdump.h"
#include "defsampledump.h"

extern FILE *idlin;

static char *output_file_name;
static int gen_test;

static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s {options} <filename>\n", name);
	fprintf(stderr, "\nSwitches:\n");
	fprintf(stderr, "   -o <outfile>  Send output to the given output file (default = out.c).\n");
	fprintf(stderr, "   -t            Generate TSM test code.\n");
	fprintf(stderr, "   -h            Display help info.\n");
}

static void handle_args(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			printf("Input file name: %s\n", argv[i]);
			if (idlin) {
				fprintf(stderr,
					"Multiple input files not allowed\n",
					argv[i]);
				exit(EXIT_FAILURE);
			}
			idlin = fopen(argv[i], "r");
			if (!idlin) {
				fprintf(stderr, "Could not open %s\n", argv[i]);
				exit(EXIT_FAILURE);
			}
		} else if (argv[i][1] == '\0') {
			printf("Reading input from stdin\n");
		} else if (argv[i][2] != '\0') {
			fprintf(stderr, "Unknown commandline switch: %s\n",
				argv[i]);
			exit(EXIT_FAILURE);
		} else {
			switch (argv[i][1]) {
			case 'o':
				if (i + 1 >= argc) {
					fprintf(stderr,
						"-o expects an argument\n");
					exit(EXIT_FAILURE);
				}
				if (output_file_name) {
					fprintf(stderr,
						"Multiple output files not allowed\n",
						argv[i]);
					exit(EXIT_FAILURE);
				}

				i++;
				printf("Output name: %s\n", argv[i]);

				output_file_name = strdup(argv[i]);
				break;
			case 't':
				gen_test = 1;
				break;
			case 'a':
				gen_test = 1;
				break;
			case 'h':
				usage(argv[0]);
				exit(0);
				break;
			default:
				fprintf(stderr,
					"Unknown commandline switch: %s\n",
					argv[i]);
				usage(argv[0]);
				exit(EXIT_FAILURE);
				break;
			}
		}

	}

	if (!output_file_name) {
		output_file_name = strdup("out.c");
	}
}

int main(int argc, char **argv)
{
	FILE *fp;
	TypeList *l;

	handle_args(argc, argv);

	parser_data_init();

	fp = fopen(output_file_name, "w");

	fprintf(fp, "#include <stdlib.h>\n");
	fprintf(fp, "#include <stdio.h>\n");
	fprintf(fp, "#include <dds/dds_dcps.h>\n\n\n");

	if (gen_test)
		fprintf(fp, "#include <cdr.h>\n\n\n");

	idlparse();

	c_dump_set_output(fp);
	if (gen_test)
		defsample_dump_set_output(fp);
	tsm_dump_set_output(fp);

	for (l = get_type_list(); l != NULL; l = l->next) {
		idl_dump_toplevel(l->type);
		c_dump_toplevel(l->type);
		if (gen_test)
			defsample_dump_toplevel(l->type);

		if (l->type->kind == STRUCTURE_TYPE) {
			tsm_dump_struct_type((StructureType *) l->type);
		} else if (l->type->kind == ENUMERATION_TYPE) {
		} else if (l->type->kind == ALIAS_TYPE) {

		}
	}

	if (gen_test) {
		fprintf(fp, "int main() {\n");
		fprintf(fp, "DDS_DomainParticipant participant;\n");
		fprintf(fp, "DDS_Publisher         publisher;\n");
		fprintf(fp, "DDS_Subscriber        subscriber;\n");
		fprintf(fp, "DDS_InstanceHandle_t  instance;\n");
		fprintf(fp, "DDS_DataWriterQos     wr_qos;\n");
		fprintf(fp, "DDS_DataReaderQos     rd_qos;\n");
		fprintf(fp, "DDS_ReturnCode_t      error;\n");
		fprintf(fp, "unsigned char * buf;\n");
		fprintf(fp, "size_t size;\n");
		fprintf(fp, "DBW dbw;\n");
		fprintf(fp, "DBW dbw2;\n");
		fprintf(fp,
			"static DDS_DataSeq      rx_sample = DDS_SEQ_INITIALIZER (void *);\n");
		fprintf(fp,
			"static DDS_SampleInfoSeq rx_info = DDS_SEQ_INITIALIZER (DDS_SampleInfo *);\n");
		fprintf(fp, "static DDS_DataSeq      rx_sample_own;\n");
		fprintf(fp, "static DDS_SampleInfoSeq rx_info_own;\n");

		for (l = get_type_list(); l != NULL; l = l->next)
			if (l->type->kind == STRUCTURE_TYPE) {
				fprintf(fp, "DDS_TypeSupport * %s_ts;\n",
					l->type->name);
				fprintf(fp,
					"DDS_Topic *%s_topic, *%s_topic2;\n",
					l->type->name, l->type->name);
				fprintf(fp, "DDS_DataWriter * %s_writer;\n",
					l->type->name);
				fprintf(fp, "DDS_DataReader * %s_reader;\n",
					l->type->name);
				fprintf(fp, "%s * %s_sample;\n", l->type->name,
					l->type->name);

			}

		fprintf(fp, "DDS_RTPS_control (0);\n");

		fprintf(fp,
			"participant = DDS_DomainParticipantFactory_create_participant (0, NULL, NULL, 0);\n");
		fprintf(fp,
			"if (!participant) { fprintf (stderr, \"Could not create participant\\n\"); exit (EXIT_FAILURE); }\n");
		fprintf(fp,
			"publisher = DDS_DomainParticipant_create_publisher (participant, NULL, NULL, 0);\n");
		fprintf(fp,
			"if (!publisher) { fprintf (stderr, \"Could not create publisher\\n\"); exit (EXIT_FAILURE); }\n");
		fprintf(fp,
			"subscriber = DDS_DomainParticipant_create_subscriber (participant, NULL, NULL, 0);\n");
		fprintf(fp,
			"if (!subscriber) { fprintf (stderr, \"Could not create subscriber\\n\"); exit (EXIT_FAILURE); }\n");

		fprintf(fp,
			"DDS_Publisher_get_default_datawriter_qos (publisher, &wr_qos);\n");
		fprintf(fp,
			"wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;\n");
		fprintf(fp,
			"DDS_Subscriber_get_default_datareader_qos (subscriber, &rd_qos);\n");
		fprintf(fp,
			"rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;\n");

		for (l = get_type_list(); l != NULL; l = l->next) {
			if (l->type->kind == STRUCTURE_TYPE) {
				fprintf(fp, "printf(\"Working on %s\\n\");\n",
					l->type->name);
				/* Register dynamic type */
				fprintf(fp,
					"%s_ts = DDS_DynamicType_register (%s_tsm);\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"if (!%s_ts) { printf (\"DDS_DynamicType_register () failed for %s!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name, l->type->name);

				/* Register dynamic type with the domain participant */
				fprintf(fp,
					"if (DDS_DomainParticipant_register_type (participant, %s_ts, \"%s\")) {\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"printf (\"DDS_DomainParticipant_register_type () failed for %s!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name);

				/* Register dynamic type again with the domain participant, but use a different name */
				fprintf(fp,
					"if (DDS_DomainParticipant_register_type (participant, %s_ts, \"%s_alias\")) {\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"printf (\"DDS_DomainParticipant_register_type () failed for %s_alias!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name);

				/* Register dynamic type again with the domain participant, with the same name */
				fprintf(fp,
					"if (DDS_DomainParticipant_register_type (participant, %s_ts, \"%s\")) {\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"printf (\"DDS_DomainParticipant_register_type () failed for %s (second time)!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name);

				/* Create a topic from the registered type */
				fprintf(fp,
					"%s_topic = DDS_DomainParticipant_create_topic (participant, \"%s\", \"%s\", NULL, NULL, 0);\n",
					l->type->name, l->type->name,
					l->type->name);
				fprintf(fp,
					"if (!%s_topic) { printf (\"DDS_DomainParticipant_create_topic () failed for %s!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name, l->type->name);

				/* Create a topic from the same registered type, give it a different name. */
				fprintf(fp,
					"%s_topic2 = DDS_DomainParticipant_create_topic (participant, \"%s_alias\", \"%s\", NULL, NULL, 0);\n",
					l->type->name, l->type->name,
					l->type->name);
				fprintf(fp,
					"if (!%s_topic2) { printf (\"DDS_DomainParticipant_create_topic () failed for %s_alias!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name, l->type->name);

				/* Create a writer for the topic */
				fprintf(fp,
					"%s_writer = DDS_Publisher_create_datawriter (publisher, %s_topic, &wr_qos, NULL, 0);\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"if (!%s_writer) { fprintf (stderr, \"Could not create data writer\\n\"); exit (EXIT_FAILURE); }\n",
					l->type->name);

				/* Create a reader for the topic */
				fprintf(fp,
					"%s_reader = DDS_Subscriber_create_datareader (subscriber, %s_topic, &rd_qos, NULL, 0);\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"if (!%s_reader) { fprintf (stderr, \"Could not create data reader\\n\"); exit (EXIT_FAILURE); }\n",
					l->type->name);

				/* Allocate native sample */
				fprintf(fp, "%s_sample = %s_create ();\n",
					l->type->name, l->type->name);

				fprintf(fp,
					"size = DDS_MarshalledDataSize((void *) %s_sample, %s_ts, &error);\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"if (error != DDS_OK) { fprintf (stderr, \"Could not get datasize for %s\\n\"); exit(EXIT_FAILURE); }\n",
					l->type->name);

				/* Initialize dbw, to marshall the data manually */
				fprintf(fp,
					"dbw.dbp = db_alloc_data (size, 1);\n");
				fprintf(fp,
					"dbw.data = dbw.dbp->data; dbw.left = size; dbw.length = size;\n");

				/* Marshall data */
				fprintf(fp,
					"error = DDS_MarshallData(dbw.data, %s_sample, %s_ts);\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"if (error != DDS_OK) { fprintf (stderr, \"Failed to marshall data for %s\\n\"); exit(EXIT_FAILURE); }\n",
					l->type->name);

				/* From the marshalled data, find out how large the
				 * unmarshalled data would be, It will be >= the size
				 * of the struct (depending on dynamic data) */
				fprintf(fp,
					"size = DDS_UnmarshalledDataSize(dbw, %s_ts, &error);\n",
					l->type->name);
				fprintf(fp,
					"printf(\"%s %%d <> %%d\\n\", size, sizeof(%s));\n",
					l->type->name, l->type->name);
				fprintf(fp, "buf = malloc(size);\n");
				fprintf(fp,
					"DDS_UnmarshallData(buf, &dbw, %s_ts);\n",
					l->type->name);
				fprintf(fp, "free(buf);\n");

				if (structure_contains_keys
				    ((StructureType *) l->type)) {
					fprintf(fp,
						"dbw.data = dbw.dbp->data; dbw.left = size; dbw.length = size;\n");
					fprintf(fp,
						"size = DDS_KeySizeFromMarshalled(dbw, %s_ts, 0, &error);\n",
						l->type->name);
					fprintf(fp,
						"dbw2.dbp = db_alloc_data (size, 1);\n");
					fprintf(fp,
						"dbw2.data = dbw2.dbp->data; dbw2.left = size; dbw2.length = size;\n");
					fprintf(fp,
						"fprintf(stderr, \"KEY SIZE=%%d\\n\", size);\n");
					fprintf(fp,
						"DDS_KeyFromMarshalled(dbw2.data, dbw, %s_ts, 0);\n",
						l->type->name);
					fprintf(fp,
						"DDS_KeyToNativeData((void *) %s_sample, dbw2.data, %s_ts);\n",
						l->type->name, l->type->name);
					fprintf(fp,
						"DDS_KeySizeFromMarshalled(dbw2, %s_ts, 1, &error);\n",
						l->type->name);

					fprintf(fp,
						"instance = DDS_DataWriter_register_instance(%s_writer, (void *) %s_sample);\n",
						l->type->name, l->type->name);
					fprintf(fp,
						"DDS_DataWriter_write(%s_writer, (void *) %s_sample, instance);\n",
						l->type->name, l->type->name);
					//fprintf(fp, "buf = malloc(sizeof(%s));\n",l->type->name);
					//fprintf(fp, "DDS_DataWriter_get_key_value(%s_writer, (void *) buf, instance);\n",  l->type->name); 
				}
				fprintf(fp,
					"DDS_DataWriter_write(%s_writer, (void *) %s_sample, DDS_HANDLE_NIL);\n",
					l->type->name, l->type->name);

				/*fprintf(fp, "DDS_SEQ_INIT(rx_sample_own);\n");
				   fprintf(fp, "DDS_SEQ_INIT(rx_info_own);\n");
				   fprintf(fp, "DDS_SEQ_OWNED_SET(rx_sample_own, 1);\n");
				   fprintf(fp, "DDS_SEQ_OWNED_SET(rx_info_own, 1);\n");
				   fprintf(fp, "dds_seq_require(&rx_sample_own, 1);\n");
				   fprintf(fp, "dds_seq_require(&rx_info_own, 1);\n");
				   fprintf(fp, "DDS_SEQ_ITEM_SET(rx_sample_own,0,malloc(sizeof(%s)));\n", l->type->name);
				   fprintf(fp, "DDS_SEQ_ITEM_SET(rx_info_own,0,malloc(sizeof(DDS_SampleInfo)));\n"); */
				/*fprintf(fp, "DDS_DataReader_read (%s_reader, &rx_sample_own, &rx_info_own, 1, DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);\n", l->type->name); */
				fprintf(fp,
					"DDS_DataReader_take (%s_reader, &rx_sample, &rx_info, 1, DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);\n",
					l->type->name);

				fprintf(fp,
					"DDS_DataReader_return_loan (%s_reader, &rx_sample, &rx_info);\n",
					l->type->name);
				fprintf(fp,
					"if (DDS_Subscriber_delete_datareader (subscriber, %s_reader)) { fprintf(stderr, \"Could not delete datareader\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name);
				fprintf(fp,
					"DDS_Publisher_delete_datawriter (publisher, %s_writer);\n",
					l->type->name);
				fprintf(fp,
					"DDS_DomainParticipant_delete_topic (participant, %s_topic);\n",
					l->type->name);
				fprintf(fp,
					"DDS_DomainParticipant_delete_topic (participant, %s_topic2);\n",
					l->type->name);
			}
		}

		for (l = get_type_list(); l != NULL; l = l->next) {
			if (l->type->kind == STRUCTURE_TYPE) {
				/* Register it a second time */
				fprintf(fp,
					"%s_ts = DDS_DynamicType_register (%s_tsm);\n",
					l->type->name, l->type->name);
				fprintf(fp,
					"if (!%s_ts) { printf (\"DDS_DynamicType_register () failed for %s!\\n\"); return (EXIT_FAILURE); }\n",
					l->type->name, l->type->name);
				fprintf(fp, "DDS_DynamicType_free (%s_ts);\n",
					l->type->name);
				fprintf(fp, "DDS_DynamicType_free (%s_ts);\n",
					l->type->name);
				fprintf(fp, "DDS_DynamicType_free (%s_ts);\n",
					l->type->name);
			}
		}

		fprintf(fp,
			"DDS_DomainParticipant_delete_contained_entities (participant);\n"
			"if (DDS_DomainParticipantFactory_delete_participant (participant)) {\n"
			"fprintf (stderr, \"Could not delete domain participant.\\n\");\n"
			"exit (EXIT_FAILURE);\n"
			"}\n" "return (EXIT_SUCCESS);\n" "\n" "\n" "}");
	}

	fclose(fp);

	return 0;
}
