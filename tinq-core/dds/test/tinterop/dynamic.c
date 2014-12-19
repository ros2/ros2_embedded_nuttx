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

/* dynamic.c -- Dynamic shapes type handler. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/dds_dreader.h"
#include "dds/dds_dwriter.h"
#include "dds/dds_xtypes.h"
#include "dynamic.h"

/*#define USE_TAKE		** Use take() i.o. read() to get items. */
#define	RELIABLE		/* Use Reliable mode i.o. Best-Effort. */
#define TRANSIENT_LOCAL 	/* Define to use Transient-Local Durability QoS.*/
/*#define KEEP_ALL		** Use KEEP_ALL history. */
#define HISTORY 	1	/* History depth */
#define	EXCLUSIVE	0	/* Exclusive mode? */
#define	STRENGTH	2	/* Exclusive strength. */

typedef struct shape_type_st {
	char		color [128];
	int		x;
	int		y;
	int		shapesize;
} ShapeType_t;

#define	MAX_TOPICS	4
#define	MAX_READERS	8
#define	MAX_WRITERS	8

typedef struct {
	char			*name;
	DDS_Topic		t;
	DDS_TopicDescription	desc;
	unsigned		nreaders;
	unsigned		nwriters;
} Topic_t;

static Topic_t		topics [MAX_TOPICS];
static unsigned		ntopics;

typedef struct {
	Topic_t			*t;
	DDS_DynamicDataReader	dr;
} Reader_t;

static Reader_t		readers [MAX_READERS];
static unsigned		nreaders;

typedef struct {
	Topic_t			*t;
	DDS_DynamicDataWriter	dw;
} Writer_t;

static Writer_t		writers [MAX_READERS];
static unsigned		nwriters;

static DDS_DomainParticipant	part;
static DDS_DynamicType		dtype;
static DDS_DynamicTypeSupport	shape_ts;
static DDS_Subscriber		sub;
static DDS_Publisher		pub;

extern void fatal (const char *s);

/* Fatal error function. */

/* register_dynamic_type -- Register a dynamic shapes type. */

void register_dynamic_type (DDS_DomainParticipant dpart)
{
	DDS_ReturnCode_t	error;
	DDS_TypeDescriptor 	*desc;
	DDS_MemberDescriptor 	*md;
	DDS_DynamicTypeMember	dtm;
	DDS_AnnotationDescriptor *key_ad;
	DDS_DynamicTypeBuilder	sb, ssb;
	DDS_DynamicType 	s, ss;

	desc = DDS_TypeDescriptor__alloc ();
	if (!desc)
		goto done;

	md = DDS_MemberDescriptor__alloc ();
	if (!md)
		goto done;

	key_ad = DDS_AnnotationDescriptor__alloc ();
	if (!key_ad)
		goto done;

	key_ad->type = DDS_DynamicTypeBuilderFactory_get_builtin_annotation ("Key");
	if (!key_ad->type)
		goto done;

	desc->kind = DDS_STRUCTURE_TYPE;
	desc->name = "ShapeType";
	sb = DDS_DynamicTypeBuilderFactory_create_type (desc);
	if (!sb)
		goto done;

	ssb = DDS_DynamicTypeBuilderFactory_create_string_type (128);
	if (!ssb)
		goto done;

	ss = DDS_DynamicTypeBuilder_build (ssb);
	if (!ss)
		goto done;

	DDS_DynamicTypeBuilderFactory_delete_type (ssb);

	md->name = "color";
	md->index = md->id = 0;
	md->type = ss;
	md->default_value = NULL;
	if (!md->type)
		goto done;

	error = DDS_DynamicTypeBuilder_add_member (sb, md);
	if (error)
		goto done;

	dtm = DDS_DynamicTypeMember__alloc ();
	if (!dtm)
		goto done;

	error = DDS_DynamicTypeBuilder_get_member (sb, dtm, 0);
	if (error)
		goto done;

	error = DDS_DynamicTypeMember_apply_annotation (dtm, key_ad);
	if (error)
		goto done;

	md->name = "x";
	md->index = md->id = 1;
	md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
	if (!md->type)
		goto done;

	error = DDS_DynamicTypeBuilder_add_member (sb, md);
	if (error)
		goto done;

	md->name = "y";
	md->index = md->id = 2;
	md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
	if (!md->type)
		goto done;

	error = DDS_DynamicTypeBuilder_add_member (sb, md);
	if (error)
		goto done;

	md->name = "shapesize";
	md->index = md->id = 3;
	md->type = DDS_DynamicTypeBuilderFactory_get_primitive_type (DDS_INT_32_TYPE);
	if (!md->type)
		goto done;

	error = DDS_DynamicTypeBuilder_add_member (sb, md);
	if (error)
		goto done;

	s = DDS_DynamicTypeBuilder_build (sb);
	if (!s)
		goto done;

	DDS_DynamicTypeBuilderFactory_delete_type (sb);

	dtype = s;
	shape_ts = (DDS_TypeSupport) DDS_DynamicTypeSupport_create_type_support (s);

	DDS_DynamicTypeBuilderFactory_delete_type (ss);

	DDS_TypeDescriptor__free (desc);
	DDS_MemberDescriptor__free (md);
	DDS_AnnotationDescriptor__free (key_ad);
	DDS_DynamicTypeMember__free (dtm);

       	if (!shape_ts)
		goto done;

	error = DDS_DynamicTypeSupport_register_type (shape_ts, dpart, "ShapeType");
	if (error != DDS_RETCODE_OK)
               	fatal ("Can't register dynamic type in participant!");

	part = dpart;
	return;

    done:
	fatal ("Can't create dynamic type!");
}

/* unregister_dynamic_type -- Unregister a dynamic shapes type. */

void unregister_dynamic_type (void)
{
	/* ... TBD ... */
}

/* dynamic_writer_create -- Create a dynamic shapes writer. */

unsigned dynamic_writer_create (const char *topic)
{
	/* ... TBD ... */
}

/* dynamic_writer_delete -- Delete a dynamic shapes writer. */

void dynamic_writer_delete (unsigned h)
{
	/* ... TBD ... */
}

/* dynamic_writer_write -- Write sample data on a dynamic writer. */

void dynamic_writer_write (unsigned h, const char *color, unsigned x, unsigned y)
{
	/* ... TBD ... */
}

/* dynamic_reader_create -- Create a dynamic shapes reader. */

unsigned dynamic_reader_create (const char *topic)
{
	/* ... TBD ... */
}

/* dynamic_reader_delete -- Delete a dynamic shapes reader. */

void dynamic_reader_delete (unsigned h)
{
	/* ... TBD ... */
}

