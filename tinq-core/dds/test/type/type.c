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

#include <stdlib.h>
#include <stdio.h>
#include <dds/dds_dcps.h>
#include <dds/dds_aux.h>

#include <cdr.h>

typedef struct _CoverageType1_st {
	char key[128];
	char a;
	short aShort;
	unsigned short anuShort;
	int aLong;
	unsigned int anuLong;
	long long aLongLong;
	unsigned long long anuLongLong;
	float aFloat;
	double aDouble;
	unsigned char aBoolean;
	unsigned char anOctet;
	char *dynamic;
	struct _myUnion_st {
		int discriminant;
		union myUnion {
			char aValue[100];
			int bValue;
		} u;
	} aunion;
} CoverageType1;

void CoverageType1_fill(CoverageType1 * ret)
{
	(*ret).key[0] = 0;
	(*ret).a = 1;
	(*ret).aShort = 1;
	(*ret).anuShort = 1;
	(*ret).aLong = 1;
	(*ret).anuLong = 1;
	(*ret).aLongLong = 1;
	(*ret).anuLongLong = 1;
	(*ret).aFloat = 1;
	(*ret).aDouble = 1;
	(*ret).aBoolean = 1;
	(*ret).anOctet = 1;
	(*ret).dynamic = strdup("");
	(*ret).aunion.discriminant = 97;
	(*ret).aunion.u.aValue[0] = 0;
	;
}

CoverageType1 *CoverageType1_create()
{
	CoverageType1 *ret = malloc(sizeof(CoverageType1));
	CoverageType1_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType1_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "CoverageType1",
	 sizeof(struct _CoverageType1_st), 0, 14, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY, "key", 128,
	 offsetof(struct _CoverageType1_st, key), 0, 0, NULL},
	{CDR_TYPECODE_CHAR, 0, "a", 0, offsetof(struct _CoverageType1_st, a), 0,
	 0, NULL},
	{CDR_TYPECODE_SHORT, 0, "aShort", 0,
	 offsetof(struct _CoverageType1_st, aShort), 0, 0, NULL},
	{CDR_TYPECODE_USHORT, 0, "anuShort", 0,
	 offsetof(struct _CoverageType1_st, anuShort), 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "aLong", 0,
	 offsetof(struct _CoverageType1_st, aLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONG, 0, "anuLong", 0,
	 offsetof(struct _CoverageType1_st, anuLong), 0, 0, NULL},
	{CDR_TYPECODE_LONGLONG, 0, "aLongLong", 0,
	 offsetof(struct _CoverageType1_st, aLongLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONGLONG, 0, "anuLongLong", 0,
	 offsetof(struct _CoverageType1_st, anuLongLong), 0, 0, NULL},
	{CDR_TYPECODE_FLOAT, 0, "aFloat", 0,
	 offsetof(struct _CoverageType1_st, aFloat), 0, 0, NULL},
	{CDR_TYPECODE_DOUBLE, 0, "aDouble", 0,
	 offsetof(struct _CoverageType1_st, aDouble), 0, 0, NULL},
	{CDR_TYPECODE_BOOLEAN, 0, "aBoolean", 0,
	 offsetof(struct _CoverageType1_st, aBoolean), 0, 0, NULL},
	{CDR_TYPECODE_OCTET, 0, "anOctet", 0,
	 offsetof(struct _CoverageType1_st, anOctet), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_DYNAMIC, "dynamic", 0,
	 offsetof(struct _CoverageType1_st, dynamic), 0, 0, NULL},
	{CDR_TYPECODE_UNION, 0, "aunion", sizeof(struct _myUnion_st),
	 offsetof(struct _CoverageType1_st, aunion), 2, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "aValue", 100,
	 offsetof(struct _myUnion_st, u), 0, 97, NULL},
	{CDR_TYPECODE_LONG, 0, "bValue", 0, offsetof(struct _myUnion_st, u), 0,
	 98, NULL}
};
typedef struct _CoverageType2_st {
	char key[128];
	char a;
	short aShort;
	unsigned short anuShort;
	int aLong;
	unsigned int anuLong;
	long long aLongLong;
	unsigned long long anuLongLong;
	float aFloat;
	double aDouble;
	unsigned char aBoolean;
	unsigned char anOctet;
	enum myKeyedEnum {
		enumKeyVal1,
		enumKeyVal2,
		enumKeyVal3
	} anEnum;
} CoverageType2;

void CoverageType2_fill(CoverageType2 * ret)
{
	(*ret).key[0] = 0;
	(*ret).a = 1;
	(*ret).aShort = 1;
	(*ret).anuShort = 1;
	(*ret).aLong = 1;
	(*ret).anuLong = 1;
	(*ret).aLongLong = 1;
	(*ret).anuLongLong = 1;
	(*ret).aFloat = 1;
	(*ret).aDouble = 1;
	(*ret).aBoolean = 1;
	(*ret).anOctet = 1;
	(*ret).anEnum = enumKeyVal1;
}

CoverageType2 *CoverageType2_create()
{
	CoverageType2 *ret = malloc(sizeof(CoverageType2));
	CoverageType2_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType2_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType2",
	 sizeof(struct _CoverageType2_st), 0, 13, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "key", 128,
	 offsetof(struct _CoverageType2_st, key), 0, 0, NULL},
	{CDR_TYPECODE_CHAR, TSMFLAG_KEY, "a", 0,
	 offsetof(struct _CoverageType2_st, a), 0, 0, NULL},
	{CDR_TYPECODE_SHORT, TSMFLAG_KEY, "aShort", 0,
	 offsetof(struct _CoverageType2_st, aShort), 0, 0, NULL},
	{CDR_TYPECODE_USHORT, TSMFLAG_KEY, "anuShort", 0,
	 offsetof(struct _CoverageType2_st, anuShort), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "aLong", 0,
	 offsetof(struct _CoverageType2_st, aLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONG, TSMFLAG_KEY, "anuLong", 0,
	 offsetof(struct _CoverageType2_st, anuLong), 0, 0, NULL},
	{CDR_TYPECODE_LONGLONG, TSMFLAG_KEY, "aLongLong", 0,
	 offsetof(struct _CoverageType2_st, aLongLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONGLONG, TSMFLAG_KEY, "anuLongLong", 0,
	 offsetof(struct _CoverageType2_st, anuLongLong), 0, 0, NULL},
	{CDR_TYPECODE_FLOAT, TSMFLAG_KEY, "aFloat", 0,
	 offsetof(struct _CoverageType2_st, aFloat), 0, 0, NULL},
	{CDR_TYPECODE_DOUBLE, TSMFLAG_KEY, "aDouble", 0,
	 offsetof(struct _CoverageType2_st, aDouble), 0, 0, NULL},
	{CDR_TYPECODE_BOOLEAN, TSMFLAG_KEY, "aBoolean", 0,
	 offsetof(struct _CoverageType2_st, aBoolean), 0, 0, NULL},
	{CDR_TYPECODE_OCTET, TSMFLAG_KEY, "anOctet", 0,
	 offsetof(struct _CoverageType2_st, anOctet), 0, 0, NULL},
	{CDR_TYPECODE_ENUM, TSMFLAG_KEY, "anEnum", 0,
	 offsetof(struct _CoverageType2_st, anEnum), 3, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "enumKeyVal1", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "enumKeyVal2", 0, 0, 0, 1, NULL},
	{CDR_TYPECODE_LONG, 0, "enumKeyVal3", 0, 0, 0, 2, NULL}
};

typedef struct _CoverageType2b_st {
	char key[128];
	char a;
	short aShort;
	unsigned short anuShort;
	int aLong;
	unsigned int anuLong;
	long long aLongLong;
	unsigned long long anuLongLong;
	float aFloat;
	double aDouble;
	unsigned char aBoolean;
	unsigned char anOctet;
	char *unbound;
} CoverageType2b;

void CoverageType2b_fill(CoverageType2b * ret)
{
	(*ret).key[0] = 0;
	(*ret).a = 1;
	(*ret).aShort = 1;
	(*ret).anuShort = 1;
	(*ret).aLong = 1;
	(*ret).anuLong = 1;
	(*ret).aLongLong = 1;
	(*ret).anuLongLong = 1;
	(*ret).aFloat = 1;
	(*ret).aDouble = 1;
	(*ret).aBoolean = 1;
	(*ret).anOctet = 1;
	(*ret).unbound = strdup("");
}

CoverageType2b *CoverageType2b_create()
{
	CoverageType2b *ret = malloc(sizeof(CoverageType2b));
	CoverageType2b_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType2b_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "CoverageType2b",
	 sizeof(struct _CoverageType2b_st), 0, 13, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY, "key", 128,
	 offsetof(struct _CoverageType2b_st, key), 0, 0, NULL},
	{CDR_TYPECODE_CHAR, TSMFLAG_KEY, "a", 0,
	 offsetof(struct _CoverageType2b_st, a), 0, 0, NULL},
	{CDR_TYPECODE_SHORT, TSMFLAG_KEY, "aShort", 0,
	 offsetof(struct _CoverageType2b_st, aShort), 0, 0, NULL},
	{CDR_TYPECODE_USHORT, TSMFLAG_KEY, "anuShort", 0,
	 offsetof(struct _CoverageType2b_st, anuShort), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "aLong", 0,
	 offsetof(struct _CoverageType2b_st, aLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONG, TSMFLAG_KEY, "anuLong", 0,
	 offsetof(struct _CoverageType2b_st, anuLong), 0, 0, NULL},
	{CDR_TYPECODE_LONGLONG, TSMFLAG_KEY, "aLongLong", 0,
	 offsetof(struct _CoverageType2b_st, aLongLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONGLONG, TSMFLAG_KEY, "anuLongLong", 0,
	 offsetof(struct _CoverageType2b_st, anuLongLong), 0, 0, NULL},
	{CDR_TYPECODE_FLOAT, TSMFLAG_KEY, "aFloat", 0,
	 offsetof(struct _CoverageType2b_st, aFloat), 0, 0, NULL},
	{CDR_TYPECODE_DOUBLE, TSMFLAG_KEY, "aDouble", 0,
	 offsetof(struct _CoverageType2b_st, aDouble), 0, 0, NULL},
	{CDR_TYPECODE_BOOLEAN, TSMFLAG_KEY, "aBoolean", 0,
	 offsetof(struct _CoverageType2b_st, aBoolean), 0, 0, NULL},
	{CDR_TYPECODE_OCTET, TSMFLAG_KEY, "anOctet", 0,
	 offsetof(struct _CoverageType2b_st, anOctet), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "unbound", 0,
	 offsetof(struct _CoverageType2b_st, unbound), 0, 0, NULL}
};
typedef struct _CoverageType3_st {
	char a[5];
	int nonkey;
	enum myEnum {
		enumVal1,
		enumVal2,
		enumVal3
	} anEnum;
} CoverageType3;

void CoverageType3_fill(CoverageType3 * ret)
{
	do {
		int i0;
		for (i0 = 0; i0 < 5; i0++) {
			(*ret).a[i0] = 1;
		}
	} while (0);
	(*ret).nonkey = 1;
	(*ret).anEnum = enumVal1;
}

CoverageType3 *CoverageType3_create()
{
	CoverageType3 *ret = malloc(sizeof(CoverageType3));
	CoverageType3_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType3_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType3",
	 sizeof(struct _CoverageType3_st), 0, 3, 0, NULL},
	{CDR_TYPECODE_ARRAY, TSMFLAG_KEY, "a", 0,
	 offsetof(struct _CoverageType3_st, a), 5, 0, NULL},
	{CDR_TYPECODE_CHAR, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "nonkey", 0,
	 offsetof(struct _CoverageType3_st, nonkey), 0, 0, NULL},
	{CDR_TYPECODE_ENUM, 0, "anEnum", 0,
	 offsetof(struct _CoverageType3_st, anEnum), 3, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "enumVal1", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "enumVal2", 0, 0, 0, 1, NULL},
	{CDR_TYPECODE_LONG, 0, "enumVal3", 0, 0, 0, 2, NULL}
};

typedef struct _CoverageType4_st {
	char *dynkey;
	int nonkey;
} CoverageType4;

void CoverageType4_fill(CoverageType4 * ret)
{
	(*ret).dynkey = strdup("");
	(*ret).nonkey = 1;
}

CoverageType4 *CoverageType4_create()
{
	CoverageType4 *ret = malloc(sizeof(CoverageType4));
	CoverageType4_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType4_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "CoverageType4",
	 sizeof(struct _CoverageType4_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "dynkey", 0,
	 offsetof(struct _CoverageType4_st, dynkey), 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "nonkey", 0,
	 offsetof(struct _CoverageType4_st, nonkey), 0, 0, NULL}
};
typedef struct _CoverageType5_st {
	int key;
	char *sarr[4];
} CoverageType5;

void CoverageType5_fill(CoverageType5 * ret)
{
	(*ret).key = 1;
	do {
		int i0;
		for (i0 = 0; i0 < 4; i0++) {
			(*ret).sarr[i0] = strdup("");
		}
	} while (0);
}

CoverageType5 *CoverageType5_create()
{
	CoverageType5 *ret = malloc(sizeof(CoverageType5));
	CoverageType5_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType5_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType5",
	 sizeof(struct _CoverageType5_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "key", 0,
	 offsetof(struct _CoverageType5_st, key), 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "sarr", 0,
	 offsetof(struct _CoverageType5_st, sarr), 4, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 0, 0, 0, 0, NULL}
};

typedef struct _CoverageType6_st {
	int key;
	char *sarr[4];
} CoverageType6;

void CoverageType6_fill(CoverageType6 * ret)
{
	(*ret).key = 1;
	do {
		int i0;
		for (i0 = 0; i0 < 4; i0++) {
			(*ret).sarr[i0] = strdup("");
		}
	} while (0);
}

CoverageType6 *CoverageType6_create()
{
	CoverageType6 *ret = malloc(sizeof(CoverageType6));
	CoverageType6_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType6_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType6",
	 sizeof(struct _CoverageType6_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "key", 0,
	 offsetof(struct _CoverageType6_st, key), 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, TSMFLAG_KEY, "sarr", 0,
	 offsetof(struct _CoverageType6_st, sarr), 4, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 0, 0, 0, 0, NULL}
};

typedef struct _CoverageType7_st {
	int key;
	char sarr[4][10];
} CoverageType7;

void CoverageType7_fill(CoverageType7 * ret)
{
	(*ret).key = 1;
	do {
		int i0;
		for (i0 = 0; i0 < 4; i0++) {
			(*ret).sarr[i0][0] = 0;
		}
	} while (0);
}

CoverageType7 *CoverageType7_create()
{
	CoverageType7 *ret = malloc(sizeof(CoverageType7));
	CoverageType7_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType7_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType7",
	 sizeof(struct _CoverageType7_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "key", 0,
	 offsetof(struct _CoverageType7_st, key), 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, TSMFLAG_KEY, "sarr", 0,
	 offsetof(struct _CoverageType7_st, sarr), 4, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 10, 0, 0, 0, NULL}
};

typedef struct _CoverageType8_st {
	int key;
	char sarr[4][10];
} CoverageType8;

void CoverageType8_fill(CoverageType8 * ret)
{
	(*ret).key = 1;
	do {
		int i0;
		for (i0 = 0; i0 < 4; i0++) {
			(*ret).sarr[i0][0] = 0;
		}
	} while (0);
}

CoverageType8 *CoverageType8_create()
{
	CoverageType8 *ret = malloc(sizeof(CoverageType8));
	CoverageType8_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType8_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType8",
	 sizeof(struct _CoverageType8_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "key", 0,
	 offsetof(struct _CoverageType8_st, key), 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "sarr", 0,
	 offsetof(struct _CoverageType8_st, sarr), 4, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 10, 0, 0, 0, NULL}
};

typedef struct _CoverageType9_st {
	int x;
	int y;
} CoverageType9;

void CoverageType9_fill(CoverageType9 * ret)
{
	(*ret).x = 1;
	(*ret).y = 1;
}

CoverageType9 *CoverageType9_create()
{
	CoverageType9 *ret = malloc(sizeof(CoverageType9));
	CoverageType9_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType9_tsm[] = {
	{CDR_TYPECODE_STRUCT, 0, "CoverageType9",
	 sizeof(struct _CoverageType9_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "x", 0, offsetof(struct _CoverageType9_st, x), 0,
	 0, NULL},
	{CDR_TYPECODE_LONG, 0, "y", 0, offsetof(struct _CoverageType9_st, y), 0,
	 0, NULL}
};
typedef struct _CoverageType10_st {
	int test;
	struct _CoverageType9_st cool;
} CoverageType10;

void CoverageType10_fill(CoverageType10 * ret)
{
	(*ret).test = 1;
	(*ret).cool.x = 1;
	(*ret).cool.y = 1;
	;
}

CoverageType10 *CoverageType10_create()
{
	CoverageType10 *ret = malloc(sizeof(CoverageType10));
	CoverageType10_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType10_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType10",
	 sizeof(struct _CoverageType10_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "test", 0,
	 offsetof(struct _CoverageType10_st, test), 0, 0, NULL},
	{CDR_TYPECODE_TYPEREF, TSMFLAG_KEY, "cool", 0,
	 offsetof(struct _CoverageType10_st, cool), 0, 0, CoverageType9_tsm}
};
typedef struct _CoverageType9_st indirectType;

DDS_SEQUENCE(int, int_seq);
typedef struct _CoverageType12_st {
	int test;
	int_seq huh;
} CoverageType12;

void CoverageType12_fill(CoverageType12 * ret)
{
	(*ret).test = 1;
	DDS_SEQ_INIT((*ret).huh);
	dds_seq_require(&(*ret).huh, 1);
}

CoverageType12 *CoverageType12_create()
{
	CoverageType12 *ret = malloc(sizeof(CoverageType12));
	CoverageType12_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType12_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "CoverageType12",
	 sizeof(struct _CoverageType12_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "test", 0,
	 offsetof(struct _CoverageType12_st, test), 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_DYNAMIC, "huh", 0,
	 offsetof(struct _CoverageType12_st, huh), 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "no name", 0, 0, 0, 0, NULL}
};

typedef struct _Keyed_st {
	int key;
	int nokey;
} Keyed;

void Keyed_fill(Keyed * ret)
{
	(*ret).key = 1;
	(*ret).nokey = 1;
}

Keyed *Keyed_create()
{
	Keyed *ret = malloc(sizeof(Keyed));
	Keyed_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta Keyed_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "Keyed", sizeof(struct _Keyed_st), 0,
	 2, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "key", 0,
	 offsetof(struct _Keyed_st, key), 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "nokey", 0, offsetof(struct _Keyed_st, nokey), 0,
	 0, NULL}
};
typedef struct _Keyless_st {
	struct _Keyed_st keyed;
	int extra;
} Keyless;

void Keyless_fill(Keyless * ret)
{
	(*ret).keyed.key = 1;
	(*ret).keyed.nokey = 1;
	;
	(*ret).extra = 1;
}

Keyless *Keyless_create()
{
	Keyless *ret = malloc(sizeof(Keyless));
	Keyless_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta Keyless_tsm[] = {
	{CDR_TYPECODE_STRUCT, 0, "Keyless", sizeof(struct _Keyless_st), 0, 2, 0,
	 NULL},
	{CDR_TYPECODE_TYPEREF, 0, "keyed", 0,
	 offsetof(struct _Keyless_st, keyed), 0, 0, Keyed_tsm},
	{CDR_TYPECODE_LONG, 0, "extra", 0, offsetof(struct _Keyless_st, extra),
	 0, 0, NULL}
};
typedef struct _CoverageType13_st {
	struct _Keyless_st key;
	int extra;
} CoverageType13;

void CoverageType13_fill(CoverageType13 * ret)
{
	Keyed_fill(&(*ret).key.keyed);
	(*ret).key.extra = 1;
	;
	(*ret).extra = 1;
}

CoverageType13 *CoverageType13_create()
{
	CoverageType13 *ret = malloc(sizeof(CoverageType13));
	CoverageType13_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType13_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType13",
	 sizeof(struct _CoverageType13_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_TYPEREF, TSMFLAG_KEY, "key", 0,
	 offsetof(struct _CoverageType13_st, key), 0, 0, Keyless_tsm},
	{CDR_TYPECODE_LONG, 0, "extra", 0,
	 offsetof(struct _CoverageType13_st, extra), 0, 0, NULL}
};
typedef struct _CoverageType14_st {
	char key[300];
	char a;
	short aShort;
} CoverageType14;

void CoverageType14_fill(CoverageType14 * ret)
{
	(*ret).key[0] = 0;
	(*ret).a = 1;
	(*ret).aShort = 1;
}

CoverageType14 *CoverageType14_create()
{
	CoverageType14 *ret = malloc(sizeof(CoverageType14));
	CoverageType14_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType14_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType14",
	 sizeof(struct _CoverageType14_st), 0, 3, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY, "key", 300,
	 offsetof(struct _CoverageType14_st, key), 0, 0, NULL},
	{CDR_TYPECODE_CHAR, 0, "a", 0, offsetof(struct _CoverageType14_st, a),
	 0, 0, NULL},
	{CDR_TYPECODE_SHORT, 0, "aShort", 0,
	 offsetof(struct _CoverageType14_st, aShort), 0, 0, NULL}
};
typedef struct _CoverageType15_st {
	int x00;
	int x01;
	int x02;
	int x03;
	int x04;
	int x05;
	int x06;
	int x07;
	int x08;
	int x09;
	int x10;
	int x11;
	int x12;
	int x13;
	int x14;
	int x15;
	int x16;
	int x17;
	int x18;
	int x19;
	int x20;
	int x21;
	int x22;
	int x23;
	int x24;
	int x25;
	int x26;
	int x27;
	int x28;
	int x29;
	int x30;
	int x31;
	int x32;
	int x33;
	int x34;
	int x35;
	int x36;
	int x37;
	int x38;
	int x39;
	int x40;
	int x41;
	int x42;
	int x43;
	int x44;
	int x45;
	int x46;
	int x47;
	int x48;
	int x49;
	int x50;
	int x51;
	int x52;
	int x53;
	int x54;
	int x55;
	int x56;
	int x57;
	int x58;
	int x59;
	int x60;
	int x61;
	int x62;
	int x63;
	int x64;
	int x65;
	int x66;
	int x67;
	int x68;
	int x69;
	int x70;
	int x71;
	int x72;
	int x73;
	int x74;
	int x75;
	int x76;
	int x77;
	int x78;
	int x79;
	int x80;
	int x81;
	int x82;
	int x83;
	int x84;
	int x85;
	int x86;
	int x87;
	int x88;
	int x89;
} CoverageType15;

void CoverageType15_fill(CoverageType15 * ret)
{
	(*ret).x00 = 1;
	(*ret).x01 = 1;
	(*ret).x02 = 1;
	(*ret).x03 = 1;
	(*ret).x04 = 1;
	(*ret).x05 = 1;
	(*ret).x06 = 1;
	(*ret).x07 = 1;
	(*ret).x08 = 1;
	(*ret).x09 = 1;
	(*ret).x10 = 1;
	(*ret).x11 = 1;
	(*ret).x12 = 1;
	(*ret).x13 = 1;
	(*ret).x14 = 1;
	(*ret).x15 = 1;
	(*ret).x16 = 1;
	(*ret).x17 = 1;
	(*ret).x18 = 1;
	(*ret).x19 = 1;
	(*ret).x20 = 1;
	(*ret).x21 = 1;
	(*ret).x22 = 1;
	(*ret).x23 = 1;
	(*ret).x24 = 1;
	(*ret).x25 = 1;
	(*ret).x26 = 1;
	(*ret).x27 = 1;
	(*ret).x28 = 1;
	(*ret).x29 = 1;
	(*ret).x30 = 1;
	(*ret).x31 = 1;
	(*ret).x32 = 1;
	(*ret).x33 = 1;
	(*ret).x34 = 1;
	(*ret).x35 = 1;
	(*ret).x36 = 1;
	(*ret).x37 = 1;
	(*ret).x38 = 1;
	(*ret).x39 = 1;
	(*ret).x40 = 1;
	(*ret).x41 = 1;
	(*ret).x42 = 1;
	(*ret).x43 = 1;
	(*ret).x44 = 1;
	(*ret).x45 = 1;
	(*ret).x46 = 1;
	(*ret).x47 = 1;
	(*ret).x48 = 1;
	(*ret).x49 = 1;
	(*ret).x50 = 1;
	(*ret).x51 = 1;
	(*ret).x52 = 1;
	(*ret).x53 = 1;
	(*ret).x54 = 1;
	(*ret).x55 = 1;
	(*ret).x56 = 1;
	(*ret).x57 = 1;
	(*ret).x58 = 1;
	(*ret).x59 = 1;
	(*ret).x60 = 1;
	(*ret).x61 = 1;
	(*ret).x62 = 1;
	(*ret).x63 = 1;
	(*ret).x64 = 1;
	(*ret).x65 = 1;
	(*ret).x66 = 1;
	(*ret).x67 = 1;
	(*ret).x68 = 1;
	(*ret).x69 = 1;
	(*ret).x70 = 1;
	(*ret).x71 = 1;
	(*ret).x72 = 1;
	(*ret).x73 = 1;
	(*ret).x74 = 1;
	(*ret).x75 = 1;
	(*ret).x76 = 1;
	(*ret).x77 = 1;
	(*ret).x78 = 1;
	(*ret).x79 = 1;
	(*ret).x80 = 1;
	(*ret).x81 = 1;
	(*ret).x82 = 1;
	(*ret).x83 = 1;
	(*ret).x84 = 1;
	(*ret).x85 = 1;
	(*ret).x86 = 1;
	(*ret).x87 = 1;
	(*ret).x88 = 1;
	(*ret).x89 = 1;
}

CoverageType15 *CoverageType15_create()
{
	CoverageType15 *ret = malloc(sizeof(CoverageType15));
	CoverageType15_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta CoverageType15_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY, "CoverageType15",
	 sizeof(struct _CoverageType15_st), 0, 90, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x00", 0,
	 offsetof(struct _CoverageType15_st, x00), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x01", 0,
	 offsetof(struct _CoverageType15_st, x01), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x02", 0,
	 offsetof(struct _CoverageType15_st, x02), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x03", 0,
	 offsetof(struct _CoverageType15_st, x03), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x04", 0,
	 offsetof(struct _CoverageType15_st, x04), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x05", 0,
	 offsetof(struct _CoverageType15_st, x05), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x06", 0,
	 offsetof(struct _CoverageType15_st, x06), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x07", 0,
	 offsetof(struct _CoverageType15_st, x07), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x08", 0,
	 offsetof(struct _CoverageType15_st, x08), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x09", 0,
	 offsetof(struct _CoverageType15_st, x09), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x10", 0,
	 offsetof(struct _CoverageType15_st, x10), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x11", 0,
	 offsetof(struct _CoverageType15_st, x11), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x12", 0,
	 offsetof(struct _CoverageType15_st, x12), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x13", 0,
	 offsetof(struct _CoverageType15_st, x13), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x14", 0,
	 offsetof(struct _CoverageType15_st, x14), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x15", 0,
	 offsetof(struct _CoverageType15_st, x15), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x16", 0,
	 offsetof(struct _CoverageType15_st, x16), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x17", 0,
	 offsetof(struct _CoverageType15_st, x17), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x18", 0,
	 offsetof(struct _CoverageType15_st, x18), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x19", 0,
	 offsetof(struct _CoverageType15_st, x19), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x20", 0,
	 offsetof(struct _CoverageType15_st, x20), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x21", 0,
	 offsetof(struct _CoverageType15_st, x21), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x22", 0,
	 offsetof(struct _CoverageType15_st, x22), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x23", 0,
	 offsetof(struct _CoverageType15_st, x23), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x24", 0,
	 offsetof(struct _CoverageType15_st, x24), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x25", 0,
	 offsetof(struct _CoverageType15_st, x25), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x26", 0,
	 offsetof(struct _CoverageType15_st, x26), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x27", 0,
	 offsetof(struct _CoverageType15_st, x27), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x28", 0,
	 offsetof(struct _CoverageType15_st, x28), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x29", 0,
	 offsetof(struct _CoverageType15_st, x29), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x30", 0,
	 offsetof(struct _CoverageType15_st, x30), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x31", 0,
	 offsetof(struct _CoverageType15_st, x31), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x32", 0,
	 offsetof(struct _CoverageType15_st, x32), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x33", 0,
	 offsetof(struct _CoverageType15_st, x33), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x34", 0,
	 offsetof(struct _CoverageType15_st, x34), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x35", 0,
	 offsetof(struct _CoverageType15_st, x35), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x36", 0,
	 offsetof(struct _CoverageType15_st, x36), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x37", 0,
	 offsetof(struct _CoverageType15_st, x37), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x38", 0,
	 offsetof(struct _CoverageType15_st, x38), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x39", 0,
	 offsetof(struct _CoverageType15_st, x39), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x40", 0,
	 offsetof(struct _CoverageType15_st, x40), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x41", 0,
	 offsetof(struct _CoverageType15_st, x41), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x42", 0,
	 offsetof(struct _CoverageType15_st, x42), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x43", 0,
	 offsetof(struct _CoverageType15_st, x43), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x44", 0,
	 offsetof(struct _CoverageType15_st, x44), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x45", 0,
	 offsetof(struct _CoverageType15_st, x45), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x46", 0,
	 offsetof(struct _CoverageType15_st, x46), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x47", 0,
	 offsetof(struct _CoverageType15_st, x47), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x48", 0,
	 offsetof(struct _CoverageType15_st, x48), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x49", 0,
	 offsetof(struct _CoverageType15_st, x49), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x50", 0,
	 offsetof(struct _CoverageType15_st, x50), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x51", 0,
	 offsetof(struct _CoverageType15_st, x51), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x52", 0,
	 offsetof(struct _CoverageType15_st, x52), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x53", 0,
	 offsetof(struct _CoverageType15_st, x53), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x54", 0,
	 offsetof(struct _CoverageType15_st, x54), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x55", 0,
	 offsetof(struct _CoverageType15_st, x55), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x56", 0,
	 offsetof(struct _CoverageType15_st, x56), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x57", 0,
	 offsetof(struct _CoverageType15_st, x57), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x58", 0,
	 offsetof(struct _CoverageType15_st, x58), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x59", 0,
	 offsetof(struct _CoverageType15_st, x59), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x60", 0,
	 offsetof(struct _CoverageType15_st, x60), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x61", 0,
	 offsetof(struct _CoverageType15_st, x61), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x62", 0,
	 offsetof(struct _CoverageType15_st, x62), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x63", 0,
	 offsetof(struct _CoverageType15_st, x63), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x64", 0,
	 offsetof(struct _CoverageType15_st, x64), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x65", 0,
	 offsetof(struct _CoverageType15_st, x65), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x66", 0,
	 offsetof(struct _CoverageType15_st, x66), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x67", 0,
	 offsetof(struct _CoverageType15_st, x67), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x68", 0,
	 offsetof(struct _CoverageType15_st, x68), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x69", 0,
	 offsetof(struct _CoverageType15_st, x69), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x70", 0,
	 offsetof(struct _CoverageType15_st, x70), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x71", 0,
	 offsetof(struct _CoverageType15_st, x71), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x72", 0,
	 offsetof(struct _CoverageType15_st, x72), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x73", 0,
	 offsetof(struct _CoverageType15_st, x73), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x74", 0,
	 offsetof(struct _CoverageType15_st, x74), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x75", 0,
	 offsetof(struct _CoverageType15_st, x75), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x76", 0,
	 offsetof(struct _CoverageType15_st, x76), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x77", 0,
	 offsetof(struct _CoverageType15_st, x77), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x78", 0,
	 offsetof(struct _CoverageType15_st, x78), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x79", 0,
	 offsetof(struct _CoverageType15_st, x79), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x80", 0,
	 offsetof(struct _CoverageType15_st, x80), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x81", 0,
	 offsetof(struct _CoverageType15_st, x81), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x82", 0,
	 offsetof(struct _CoverageType15_st, x82), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x83", 0,
	 offsetof(struct _CoverageType15_st, x83), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x84", 0,
	 offsetof(struct _CoverageType15_st, x84), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x85", 0,
	 offsetof(struct _CoverageType15_st, x85), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x86", 0,
	 offsetof(struct _CoverageType15_st, x86), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x87", 0,
	 offsetof(struct _CoverageType15_st, x87), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x88", 0,
	 offsetof(struct _CoverageType15_st, x88), 0, 0, NULL},
	{CDR_TYPECODE_LONG, TSMFLAG_KEY, "x89", 0,
	 offsetof(struct _CoverageType15_st, x89), 0, 0, NULL}
};
typedef struct _AnotherStruct_st {
	int x;
	int y;
} AnotherStruct;

void AnotherStruct_fill(AnotherStruct * ret)
{
	(*ret).x = 1;
	(*ret).y = 1;
}

AnotherStruct *AnotherStruct_create()
{
	AnotherStruct *ret = malloc(sizeof(AnotherStruct));
	AnotherStruct_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta AnotherStruct_tsm[] = {
	{CDR_TYPECODE_STRUCT, 0, "AnotherStruct",
	 sizeof(struct _AnotherStruct_st), 0, 2, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "x", 0, offsetof(struct _AnotherStruct_st, x), 0,
	 0, NULL},
	{CDR_TYPECODE_LONG, 0, "y", 0, offsetof(struct _AnotherStruct_st, y), 0,
	 0, NULL}
};
DDS_SEQUENCE(char, char_seq);
DDS_SEQUENCE(unsigned char, unsigned_char_seq);
DDS_SEQUENCE(char_seq, char_seq_seq);
typedef char char_10[10];
DDS_SEQUENCE(char, char_10_seq);
typedef struct _FilterData_st {
	char aChar;
	char *anubStringmain[3];
	struct _inner_st_st {
		short aShort;
		unsigned short anuShort;
		int aLong;
		unsigned int anuLong;
		long long aLongLong;
		unsigned long long anuLongLong;
		float aFloat;
		double aDouble;
		unsigned char aBoolean;
		unsigned char anOctet;
		char aString[10];
		char *anubString;
		char *anubStringarr[10];
		char *anubStringarrarr[10][20];
		char aStringarr[10][10];
		char_seq charlist;
		unsigned_char_seq keyseq;
		char_seq_seq charlist2;
		char_10_seq weirdkeys;
		struct _AnotherStruct_st withtyperef;
		struct _AnotherStruct_st typerefarr[10];
		enum myEnum2 {
			enum2Val1,
			enum2Val2,
			enum2Val3
		} anEnum;
	} aStruct;
} FilterData;

void FilterData_fill(FilterData * ret)
{
	(*ret).aChar = 1;
	do {
		int i0;
		for (i0 = 0; i0 < 3; i0++) {
			(*ret).anubStringmain[i0] = strdup("");
		}
	} while (0);
	(*ret).aStruct.aShort = 1;
	(*ret).aStruct.anuShort = 1;
	(*ret).aStruct.aLong = 1;
	(*ret).aStruct.anuLong = 1;
	(*ret).aStruct.aLongLong = 1;
	(*ret).aStruct.anuLongLong = 1;
	(*ret).aStruct.aFloat = 1;
	(*ret).aStruct.aDouble = 1;
	(*ret).aStruct.aBoolean = 1;
	(*ret).aStruct.anOctet = 1;
	(*ret).aStruct.aString[0] = 0;
	(*ret).aStruct.anubString = strdup("");
	do {
		int i0;
		for (i0 = 0; i0 < 10; i0++) {
			(*ret).aStruct.anubStringarr[i0] = strdup("");
		}
	} while (0);
	do {
		int i0;
		int i1;
		for (i0 = 0; i0 < 10; i0++) {
			for (i1 = 0; i1 < 20; i1++) {
				(*ret).aStruct.anubStringarrarr[i0][i1] =
				    strdup("");
			}
			;
		}
	} while (0);
	do {
		int i0;
		for (i0 = 0; i0 < 10; i0++) {
			(*ret).aStruct.aStringarr[i0][0] = 0;
		}
	} while (0);
	DDS_SEQ_INIT((*ret).aStruct.charlist);
	dds_seq_require(&(*ret).aStruct.charlist, 1);
	DDS_SEQ_INIT((*ret).aStruct.keyseq);
	dds_seq_require(&(*ret).aStruct.keyseq, 1);
	DDS_SEQ_INIT((*ret).aStruct.charlist2);
	dds_seq_require(&(*ret).aStruct.charlist2, 1);
	DDS_SEQ_INIT((*ret).aStruct.weirdkeys);
	dds_seq_require(&(*ret).aStruct.weirdkeys, 1);
	(*ret).aStruct.withtyperef.x = 1;
	(*ret).aStruct.withtyperef.y = 1;
	;
	do {
		int i0;
		for (i0 = 0; i0 < 10; i0++) {
			AnotherStruct_fill(&(*ret).aStruct.typerefarr[i0]);
		}
	} while (0);
	(*ret).aStruct.anEnum = enum2Val1;
	;
}

FilterData *FilterData_create()
{
	FilterData *ret = malloc(sizeof(FilterData));
	FilterData_fill(ret);
	return ret;
}

static DDS_TypeSupport_meta FilterData_tsm[] = {
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "FilterData",
	 sizeof(struct _FilterData_st), 0, 3, 0, NULL},
	{CDR_TYPECODE_CHAR, 0, "aChar", 0,
	 offsetof(struct _FilterData_st, aChar), 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, TSMFLAG_KEY, "anubStringmain", 0,
	 offsetof(struct _FilterData_st, anubStringmain), 3, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_STRUCT, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "aStruct",
	 sizeof(struct _inner_st_st), offsetof(struct _FilterData_st, aStruct),
	 22, 0, NULL},
	{CDR_TYPECODE_SHORT, 0, "aShort", 0,
	 offsetof(struct _inner_st_st, aShort), 0, 0, NULL},
	{CDR_TYPECODE_USHORT, 0, "anuShort", 0,
	 offsetof(struct _inner_st_st, anuShort), 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "aLong", 0, offsetof(struct _inner_st_st, aLong),
	 0, 0, NULL},
	{CDR_TYPECODE_ULONG, 0, "anuLong", 0,
	 offsetof(struct _inner_st_st, anuLong), 0, 0, NULL},
	{CDR_TYPECODE_LONGLONG, 0, "aLongLong", 0,
	 offsetof(struct _inner_st_st, aLongLong), 0, 0, NULL},
	{CDR_TYPECODE_ULONGLONG, 0, "anuLongLong", 0,
	 offsetof(struct _inner_st_st, anuLongLong), 0, 0, NULL},
	{CDR_TYPECODE_FLOAT, 0, "aFloat", 0,
	 offsetof(struct _inner_st_st, aFloat), 0, 0, NULL},
	{CDR_TYPECODE_DOUBLE, 0, "aDouble", 0,
	 offsetof(struct _inner_st_st, aDouble), 0, 0, NULL},
	{CDR_TYPECODE_BOOLEAN, 0, "aBoolean", 0,
	 offsetof(struct _inner_st_st, aBoolean), 0, 0, NULL},
	{CDR_TYPECODE_OCTET, 0, "anOctet", 0,
	 offsetof(struct _inner_st_st, anOctet), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY, "aString", 10,
	 offsetof(struct _inner_st_st, aString), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_KEY | TSMFLAG_DYNAMIC, "anubString", 0,
	 offsetof(struct _inner_st_st, anubString), 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "anubStringarr", 0,
	 offsetof(struct _inner_st_st, anubStringarr), 10, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "anubStringarrarr", 0,
	 offsetof(struct _inner_st_st, anubStringarrarr), 10, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "noname", 0, 0, 20, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_ARRAY, 0, "aStringarr", 0,
	 offsetof(struct _inner_st_st, aStringarr), 10, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 10, 0, 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "charlist", 0,
	 offsetof(struct _inner_st_st, charlist), 50, 0, NULL},
	{CDR_TYPECODE_CHAR, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "keyseq", 0,
	 offsetof(struct _inner_st_st, keyseq), 50, 0, NULL},
	{CDR_TYPECODE_OCTET, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "charlist2", 0,
	 offsetof(struct _inner_st_st, charlist2), 50, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "no name", 0, 0, 30, 0, NULL},
	{CDR_TYPECODE_CHAR, 0, "no name", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "weirdkeys", 0,
	 offsetof(struct _inner_st_st, weirdkeys), 10, 0, NULL},
	{CDR_TYPECODE_CSTRING, 0, "no name", 10, 0, 0, 0, NULL},
	{CDR_TYPECODE_TYPEREF, 0, "withtyperef", 0,
	 offsetof(struct _inner_st_st, withtyperef), 0, 0, AnotherStruct_tsm},
	{CDR_TYPECODE_ARRAY, 0, "typerefarr", 0,
	 offsetof(struct _inner_st_st, typerefarr), 10, 0, NULL},
	{CDR_TYPECODE_TYPEREF, 0, "no name", 0, 0, 0, 0, AnotherStruct_tsm},
	{CDR_TYPECODE_ENUM, 0, "anEnum", 0,
	 offsetof(struct _inner_st_st, anEnum), 3, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "enum2Val1", 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_LONG, 0, "enum2Val2", 0, 0, 0, 1, NULL},
	{CDR_TYPECODE_LONG, 0, "enum2Val3", 0, 0, 0, 2, NULL}
};

int main()
{
	DDS_DomainParticipant participant;
	DDS_Publisher publisher;
	DDS_Subscriber subscriber;
	DDS_InstanceHandle_t instance;
	DDS_DataWriterQos wr_qos;
	DDS_DataReaderQos rd_qos;
	DDS_ReturnCode_t error;
	unsigned char *buf;
	size_t size;
	DBW dbw;
	DBW dbw2;
	static DDS_DataSeq rx_sample = DDS_SEQ_INITIALIZER(void *);
	static DDS_SampleInfoSeq rx_info =
	    DDS_SEQ_INITIALIZER(DDS_SampleInfo *);
	DDS_TypeSupport *CoverageType1_ts;
	DDS_Topic *CoverageType1_topic, *CoverageType1_topic2;
	DDS_DataWriter *CoverageType1_writer;
	DDS_DataReader *CoverageType1_reader;
	CoverageType1 *CoverageType1_sample;
	DDS_TypeSupport *CoverageType2_ts;
	DDS_Topic *CoverageType2_topic, *CoverageType2_topic2;
	DDS_DataWriter *CoverageType2_writer;
	DDS_DataReader *CoverageType2_reader;
	CoverageType2 *CoverageType2_sample;
	DDS_TypeSupport *CoverageType2b_ts;
	DDS_Topic *CoverageType2b_topic, *CoverageType2b_topic2;
	DDS_DataWriter *CoverageType2b_writer;
	DDS_DataReader *CoverageType2b_reader;
	CoverageType2b *CoverageType2b_sample;
	DDS_TypeSupport *CoverageType3_ts;
	DDS_Topic *CoverageType3_topic, *CoverageType3_topic2;
	DDS_DataWriter *CoverageType3_writer;
	DDS_DataReader *CoverageType3_reader;
	CoverageType3 *CoverageType3_sample;
	DDS_TypeSupport *CoverageType4_ts;
	DDS_Topic *CoverageType4_topic, *CoverageType4_topic2;
	DDS_DataWriter *CoverageType4_writer;
	DDS_DataReader *CoverageType4_reader;
	CoverageType4 *CoverageType4_sample;
	DDS_TypeSupport *CoverageType5_ts;
	DDS_Topic *CoverageType5_topic, *CoverageType5_topic2;
	DDS_DataWriter *CoverageType5_writer;
	DDS_DataReader *CoverageType5_reader;
	CoverageType5 *CoverageType5_sample;
	DDS_TypeSupport *CoverageType6_ts;
	DDS_Topic *CoverageType6_topic, *CoverageType6_topic2;
	DDS_DataWriter *CoverageType6_writer;
	DDS_DataReader *CoverageType6_reader;
	CoverageType6 *CoverageType6_sample;
	DDS_TypeSupport *CoverageType7_ts;
	DDS_Topic *CoverageType7_topic, *CoverageType7_topic2;
	DDS_DataWriter *CoverageType7_writer;
	DDS_DataReader *CoverageType7_reader;
	CoverageType7 *CoverageType7_sample;
	DDS_TypeSupport *CoverageType8_ts;
	DDS_Topic *CoverageType8_topic, *CoverageType8_topic2;
	DDS_DataWriter *CoverageType8_writer;
	DDS_DataReader *CoverageType8_reader;
	CoverageType8 *CoverageType8_sample;
	DDS_TypeSupport *CoverageType9_ts;
	DDS_Topic *CoverageType9_topic, *CoverageType9_topic2;
	DDS_DataWriter *CoverageType9_writer;
	DDS_DataReader *CoverageType9_reader;
	CoverageType9 *CoverageType9_sample;
	DDS_TypeSupport *CoverageType10_ts;
	DDS_Topic *CoverageType10_topic, *CoverageType10_topic2;
	DDS_DataWriter *CoverageType10_writer;
	DDS_DataReader *CoverageType10_reader;
	CoverageType10 *CoverageType10_sample;
	DDS_TypeSupport *CoverageType12_ts;
	DDS_Topic *CoverageType12_topic, *CoverageType12_topic2;
	DDS_DataWriter *CoverageType12_writer;
	DDS_DataReader *CoverageType12_reader;
	CoverageType12 *CoverageType12_sample;
	DDS_TypeSupport *Keyed_ts;
	DDS_Topic *Keyed_topic, *Keyed_topic2;
	DDS_DataWriter *Keyed_writer;
	DDS_DataReader *Keyed_reader;
	Keyed *Keyed_sample;
	DDS_TypeSupport *Keyless_ts;
	DDS_Topic *Keyless_topic, *Keyless_topic2;
	DDS_DataWriter *Keyless_writer;
	DDS_DataReader *Keyless_reader;
	Keyless *Keyless_sample;
	DDS_TypeSupport *CoverageType13_ts;
	DDS_Topic *CoverageType13_topic, *CoverageType13_topic2;
	DDS_DataWriter *CoverageType13_writer;
	DDS_DataReader *CoverageType13_reader;
	CoverageType13 *CoverageType13_sample;
	DDS_TypeSupport *CoverageType14_ts;
	DDS_Topic *CoverageType14_topic, *CoverageType14_topic2;
	DDS_DataWriter *CoverageType14_writer;
	DDS_DataReader *CoverageType14_reader;
	CoverageType14 *CoverageType14_sample;
	DDS_TypeSupport *CoverageType15_ts;
	DDS_Topic *CoverageType15_topic, *CoverageType15_topic2;
	DDS_DataWriter *CoverageType15_writer;
	DDS_DataReader *CoverageType15_reader;
	CoverageType15 *CoverageType15_sample;
	DDS_TypeSupport *AnotherStruct_ts;
	DDS_Topic *AnotherStruct_topic, *AnotherStruct_topic2;
	DDS_DataWriter *AnotherStruct_writer;
	DDS_DataReader *AnotherStruct_reader;
	AnotherStruct *AnotherStruct_sample;
	DDS_TypeSupport *FilterData_ts;
	DDS_Topic *FilterData_topic, *FilterData_topic2;
	DDS_DataWriter *FilterData_writer;
	DDS_DataReader *FilterData_reader;
	FilterData *FilterData_sample;
	DDS_RTPS_control(0);
	participant =
	    DDS_DomainParticipantFactory_create_participant(0, NULL, NULL, 0);
	if (!participant) {
		fprintf(stderr, "Could not create participant\n");
		exit(EXIT_FAILURE);
	}
	publisher =
	    DDS_DomainParticipant_create_publisher(participant, NULL, NULL, 0);
	if (!publisher) {
		fprintf(stderr, "Could not create publisher\n");
		exit(EXIT_FAILURE);
	}
	subscriber =
	    DDS_DomainParticipant_create_subscriber(participant, NULL, NULL, 0);
	if (!subscriber) {
		fprintf(stderr, "Could not create subscriber\n");
		exit(EXIT_FAILURE);
	}
	DDS_Publisher_get_default_datawriter_qos(publisher, &wr_qos);
	wr_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	DDS_Subscriber_get_default_datareader_qos(subscriber, &rd_qos);
	rd_qos.durability.kind = DDS_TRANSIENT_LOCAL_DURABILITY_QOS;
	printf("Working on CoverageType1\n");
	CoverageType1_ts = DDS_DynamicType_register(CoverageType1_tsm);
	if (!CoverageType1_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType1!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType1_ts, "CoverageType1")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType1!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType1_ts, "CoverageType1_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType1_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType1_ts, "CoverageType1")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType1 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType1_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType1",
					       "CoverageType1", NULL, NULL, 0);
	if (!CoverageType1_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType1!\n");
		return (EXIT_FAILURE);
	}
	CoverageType1_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType1_alias",
					       "CoverageType1", NULL, NULL, 0);
	if (!CoverageType1_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType1_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType1_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType1_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType1_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType1_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType1_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType1_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType1_sample = CoverageType1_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType1_sample, 0,
				   CoverageType1_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType1\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType1_sample, 0, CoverageType1_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType1\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType1_ts, &error);
	printf("CoverageType1 %d <> %d\n", size, sizeof(CoverageType1));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType1_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType1_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType1_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType1_sample, 0, dbw2.data,
			    CoverageType1_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType1_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType1_writer,
					     (void *)CoverageType1_sample);
	DDS_DataWriter_write(CoverageType1_writer, (void *)CoverageType1_sample,
			     instance);
	DDS_DataWriter_write(CoverageType1_writer, (void *)CoverageType1_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType1_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType1_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType1_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType1_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType1_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType1_topic2);
	printf("Working on CoverageType2\n");
	CoverageType2_ts = DDS_DynamicType_register(CoverageType2_tsm);
	if (!CoverageType2_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType2!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType2_ts, "CoverageType2")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType2!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType2_ts, "CoverageType2_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType2_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType2_ts, "CoverageType2")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType2 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType2_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType2",
					       "CoverageType2", NULL, NULL, 0);
	if (!CoverageType2_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType2!\n");
		return (EXIT_FAILURE);
	}
	CoverageType2_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType2_alias",
					       "CoverageType2", NULL, NULL, 0);
	if (!CoverageType2_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType2_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType2_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType2_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType2_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType2_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType2_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType2_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType2_sample = CoverageType2_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType2_sample, 0,
				   CoverageType2_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType2\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType2_sample, 0, CoverageType2_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType2\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType2_ts, &error);
	printf("CoverageType2 %d <> %d\n", size, sizeof(CoverageType2));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType2_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType2_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType2_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType2_sample, 0, dbw2.data,
			    CoverageType2_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType2_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType2_writer,
					     (void *)CoverageType2_sample);
	DDS_DataWriter_write(CoverageType2_writer, (void *)CoverageType2_sample,
			     instance);
	DDS_DataWriter_write(CoverageType2_writer, (void *)CoverageType2_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType2_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType2_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType2_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType2_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType2_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType2_topic2);
	printf("Working on CoverageType2b\n");
	CoverageType2b_ts = DDS_DynamicType_register(CoverageType2b_tsm);
	if (!CoverageType2b_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType2b!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType2b_ts, "CoverageType2b")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType2b!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType2b_ts, "CoverageType2b_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType2b_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType2b_ts, "CoverageType2b")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType2b (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType2b_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType2b",
					       "CoverageType2b", NULL, NULL, 0);
	if (!CoverageType2b_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType2b!\n");
		return (EXIT_FAILURE);
	}
	CoverageType2b_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType2b_alias",
					       "CoverageType2b", NULL, NULL, 0);
	if (!CoverageType2b_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType2b_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType2b_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType2b_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType2b_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType2b_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType2b_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType2b_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType2b_sample = CoverageType2b_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType2b_sample, 0,
				   CoverageType2b_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType2b\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType2b_sample, 0,
			     CoverageType2b_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType2b\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType2b_ts, &error);
	printf("CoverageType2b %d <> %d\n", size, sizeof(CoverageType2b));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType2b_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType2b_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType2b_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType2b_sample, 0, dbw2.data,
			    CoverageType2b_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType2b_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType2b_writer,
					     (void *)CoverageType2b_sample);
	DDS_DataWriter_write(CoverageType2b_writer,
			     (void *)CoverageType2b_sample, instance);
	DDS_DataWriter_write(CoverageType2b_writer,
			     (void *)CoverageType2b_sample, DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType2b_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType2b_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType2b_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType2b_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType2b_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType2b_topic2);
	printf("Working on CoverageType3\n");
	CoverageType3_ts = DDS_DynamicType_register(CoverageType3_tsm);
	if (!CoverageType3_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType3!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType3_ts, "CoverageType3")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType3!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType3_ts, "CoverageType3_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType3_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType3_ts, "CoverageType3")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType3 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType3_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType3",
					       "CoverageType3", NULL, NULL, 0);
	if (!CoverageType3_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType3!\n");
		return (EXIT_FAILURE);
	}
	CoverageType3_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType3_alias",
					       "CoverageType3", NULL, NULL, 0);
	if (!CoverageType3_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType3_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType3_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType3_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType3_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType3_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType3_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType3_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType3_sample = CoverageType3_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType3_sample, 0,
				   CoverageType3_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType3\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType3_sample, 0, CoverageType3_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType3\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType3_ts, &error);
	printf("CoverageType3 %d <> %d\n", size, sizeof(CoverageType3));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType3_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType3_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType3_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType3_sample, 0, dbw2.data,
			    CoverageType3_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType3_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType3_writer,
					     (void *)CoverageType3_sample);
	DDS_DataWriter_write(CoverageType3_writer, (void *)CoverageType3_sample,
			     instance);
	DDS_DataWriter_write(CoverageType3_writer, (void *)CoverageType3_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType3_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType3_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType3_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType3_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType3_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType3_topic2);
	printf("Working on CoverageType4\n");
	CoverageType4_ts = DDS_DynamicType_register(CoverageType4_tsm);
	if (!CoverageType4_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType4!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType4_ts, "CoverageType4")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType4!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType4_ts, "CoverageType4_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType4_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType4_ts, "CoverageType4")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType4 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType4_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType4",
					       "CoverageType4", NULL, NULL, 0);
	if (!CoverageType4_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType4!\n");
		return (EXIT_FAILURE);
	}
	CoverageType4_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType4_alias",
					       "CoverageType4", NULL, NULL, 0);
	if (!CoverageType4_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType4_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType4_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType4_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType4_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType4_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType4_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType4_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType4_sample = CoverageType4_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType4_sample, 0,
				   CoverageType4_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType4\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType4_sample, 0, CoverageType4_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType4\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType4_ts, &error);
	printf("CoverageType4 %d <> %d\n", size, sizeof(CoverageType4));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType4_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType4_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType4_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType4_sample, 0, dbw2.data,
			    CoverageType4_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType4_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType4_writer,
					     (void *)CoverageType4_sample);
	DDS_DataWriter_write(CoverageType4_writer, (void *)CoverageType4_sample,
			     instance);
	DDS_DataWriter_write(CoverageType4_writer, (void *)CoverageType4_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType4_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType4_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType4_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType4_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType4_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType4_topic2);
	printf("Working on CoverageType5\n");
	CoverageType5_ts = DDS_DynamicType_register(CoverageType5_tsm);
	if (!CoverageType5_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType5!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType5_ts, "CoverageType5")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType5!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType5_ts, "CoverageType5_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType5_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType5_ts, "CoverageType5")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType5 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType5_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType5",
					       "CoverageType5", NULL, NULL, 0);
	if (!CoverageType5_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType5!\n");
		return (EXIT_FAILURE);
	}
	CoverageType5_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType5_alias",
					       "CoverageType5", NULL, NULL, 0);
	if (!CoverageType5_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType5_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType5_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType5_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType5_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType5_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType5_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType5_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType5_sample = CoverageType5_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType5_sample, 0,
				   CoverageType5_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType5\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType5_sample, 0, CoverageType5_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType5\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType5_ts, &error);
	printf("CoverageType5 %d <> %d\n", size, sizeof(CoverageType5));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType5_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType5_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType5_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType5_sample, 0, dbw2.data,
			    CoverageType5_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType5_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType5_writer,
					     (void *)CoverageType5_sample);
	DDS_DataWriter_write(CoverageType5_writer, (void *)CoverageType5_sample,
			     instance);
	DDS_DataWriter_write(CoverageType5_writer, (void *)CoverageType5_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType5_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType5_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType5_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType5_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType5_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType5_topic2);
	printf("Working on CoverageType6\n");
	CoverageType6_ts = DDS_DynamicType_register(CoverageType6_tsm);
	if (!CoverageType6_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType6!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType6_ts, "CoverageType6")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType6!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType6_ts, "CoverageType6_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType6_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType6_ts, "CoverageType6")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType6 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType6_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType6",
					       "CoverageType6", NULL, NULL, 0);
	if (!CoverageType6_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType6!\n");
		return (EXIT_FAILURE);
	}
	CoverageType6_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType6_alias",
					       "CoverageType6", NULL, NULL, 0);
	if (!CoverageType6_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType6_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType6_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType6_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType6_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType6_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType6_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType6_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType6_sample = CoverageType6_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType6_sample, 0,
				   CoverageType6_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType6\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType6_sample, 0, CoverageType6_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType6\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType6_ts, &error);
	printf("CoverageType6 %d <> %d\n", size, sizeof(CoverageType6));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType6_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType6_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType6_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType6_sample, 0, dbw2.data,
			    CoverageType6_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType6_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType6_writer,
					     (void *)CoverageType6_sample);
	DDS_DataWriter_write(CoverageType6_writer, (void *)CoverageType6_sample,
			     instance);
	DDS_DataWriter_write(CoverageType6_writer, (void *)CoverageType6_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType6_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType6_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType6_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType6_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType6_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType6_topic2);
	printf("Working on CoverageType7\n");
	CoverageType7_ts = DDS_DynamicType_register(CoverageType7_tsm);
	if (!CoverageType7_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType7!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType7_ts, "CoverageType7")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType7!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType7_ts, "CoverageType7_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType7_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType7_ts, "CoverageType7")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType7 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType7_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType7",
					       "CoverageType7", NULL, NULL, 0);
	if (!CoverageType7_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType7!\n");
		return (EXIT_FAILURE);
	}
	CoverageType7_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType7_alias",
					       "CoverageType7", NULL, NULL, 0);
	if (!CoverageType7_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType7_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType7_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType7_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType7_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType7_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType7_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType7_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType7_sample = CoverageType7_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType7_sample, 0,
				   CoverageType7_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType7\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType7_sample, 0, CoverageType7_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType7\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType7_ts, &error);
	printf("CoverageType7 %d <> %d\n", size, sizeof(CoverageType7));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType7_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType7_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType7_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType7_sample, 0, dbw2.data,
			    CoverageType7_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType7_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType7_writer,
					     (void *)CoverageType7_sample);
	DDS_DataWriter_write(CoverageType7_writer, (void *)CoverageType7_sample,
			     instance);
	DDS_DataWriter_write(CoverageType7_writer, (void *)CoverageType7_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType7_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType7_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType7_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType7_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType7_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType7_topic2);
	printf("Working on CoverageType8\n");
	CoverageType8_ts = DDS_DynamicType_register(CoverageType8_tsm);
	if (!CoverageType8_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType8!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType8_ts, "CoverageType8")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType8!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType8_ts, "CoverageType8_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType8_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType8_ts, "CoverageType8")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType8 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType8_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType8",
					       "CoverageType8", NULL, NULL, 0);
	if (!CoverageType8_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType8!\n");
		return (EXIT_FAILURE);
	}
	CoverageType8_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType8_alias",
					       "CoverageType8", NULL, NULL, 0);
	if (!CoverageType8_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType8_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType8_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType8_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType8_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType8_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType8_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType8_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType8_sample = CoverageType8_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType8_sample, 0,
				   CoverageType8_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType8\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType8_sample, 0, CoverageType8_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType8\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType8_ts, &error);
	printf("CoverageType8 %d <> %d\n", size, sizeof(CoverageType8));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType8_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType8_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType8_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType8_sample, 0, dbw2.data,
			    CoverageType8_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType8_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType8_writer,
					     (void *)CoverageType8_sample);
	DDS_DataWriter_write(CoverageType8_writer, (void *)CoverageType8_sample,
			     instance);
	DDS_DataWriter_write(CoverageType8_writer, (void *)CoverageType8_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType8_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType8_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType8_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType8_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType8_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType8_topic2);
	printf("Working on CoverageType9\n");
	CoverageType9_ts = DDS_DynamicType_register(CoverageType9_tsm);
	if (!CoverageType9_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType9!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType9_ts, "CoverageType9")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType9!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType9_ts, "CoverageType9_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType9_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType9_ts, "CoverageType9")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType9 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType9_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType9",
					       "CoverageType9", NULL, NULL, 0);
	if (!CoverageType9_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType9!\n");
		return (EXIT_FAILURE);
	}
	CoverageType9_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType9_alias",
					       "CoverageType9", NULL, NULL, 0);
	if (!CoverageType9_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType9_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType9_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType9_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType9_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType9_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType9_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType9_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType9_sample = CoverageType9_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType9_sample, 0,
				   CoverageType9_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType9\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType9_sample, 0, CoverageType9_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType9\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType9_ts, &error);
	printf("CoverageType9 %d <> %d\n", size, sizeof(CoverageType9));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType9_ts);
	free(buf);
	DDS_DataWriter_write(CoverageType9_writer, (void *)CoverageType9_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType9_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType9_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType9_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType9_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType9_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType9_topic2);
	printf("Working on CoverageType10\n");
	CoverageType10_ts = DDS_DynamicType_register(CoverageType10_tsm);
	if (!CoverageType10_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType10!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType10_ts, "CoverageType10")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType10!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType10_ts, "CoverageType10_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType10_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType10_ts, "CoverageType10")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType10 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType10_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType10",
					       "CoverageType10", NULL, NULL, 0);
	if (!CoverageType10_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType10!\n");
		return (EXIT_FAILURE);
	}
	CoverageType10_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType10_alias",
					       "CoverageType10", NULL, NULL, 0);
	if (!CoverageType10_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType10_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType10_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType10_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType10_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType10_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType10_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType10_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType10_sample = CoverageType10_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType10_sample, 0,
				   CoverageType10_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType10\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType10_sample, 0,
			     CoverageType10_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType10\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType10_ts, &error);
	printf("CoverageType10 %d <> %d\n", size, sizeof(CoverageType10));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType10_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType10_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType10_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType10_sample, 0, dbw2.data,
			    CoverageType10_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType10_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType10_writer,
					     (void *)CoverageType10_sample);
	DDS_DataWriter_write(CoverageType10_writer,
			     (void *)CoverageType10_sample, instance);
	DDS_DataWriter_write(CoverageType10_writer,
			     (void *)CoverageType10_sample, DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType10_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType10_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType10_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType10_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType10_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType10_topic2);
	printf("Working on CoverageType12\n");
	CoverageType12_ts = DDS_DynamicType_register(CoverageType12_tsm);
	if (!CoverageType12_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType12!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType12_ts, "CoverageType12")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType12!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType12_ts, "CoverageType12_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType12_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType12_ts, "CoverageType12")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType12 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType12_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType12",
					       "CoverageType12", NULL, NULL, 0);
	if (!CoverageType12_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType12!\n");
		return (EXIT_FAILURE);
	}
	CoverageType12_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType12_alias",
					       "CoverageType12", NULL, NULL, 0);
	if (!CoverageType12_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType12_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType12_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType12_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType12_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType12_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType12_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType12_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType12_sample = CoverageType12_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType12_sample, 0,
				   CoverageType12_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType12\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType12_sample, 0,
			     CoverageType12_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType12\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType12_ts, &error);
	printf("CoverageType12 %d <> %d\n", size, sizeof(CoverageType12));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType12_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType12_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType12_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType12_sample, 0, dbw2.data,
			    CoverageType12_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType12_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType12_writer,
					     (void *)CoverageType12_sample);
	DDS_DataWriter_write(CoverageType12_writer,
			     (void *)CoverageType12_sample, instance);
	DDS_DataWriter_write(CoverageType12_writer,
			     (void *)CoverageType12_sample, DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType12_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType12_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType12_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType12_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType12_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType12_topic2);
	printf("Working on Keyed\n");
	Keyed_ts = DDS_DynamicType_register(Keyed_tsm);
	if (!Keyed_ts) {
		printf("DDS_DynamicType_register () failed for Keyed!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type(participant, Keyed_ts, "Keyed")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for Keyed!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, Keyed_ts, "Keyed_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for Keyed_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type(participant, Keyed_ts, "Keyed")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for Keyed (second time)!\n");
		return (EXIT_FAILURE);
	}
	Keyed_topic =
	    DDS_DomainParticipant_create_topic(participant, "Keyed", "Keyed",
					       NULL, NULL, 0);
	if (!Keyed_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for Keyed!\n");
		return (EXIT_FAILURE);
	}
	Keyed_topic2 =
	    DDS_DomainParticipant_create_topic(participant, "Keyed_alias",
					       "Keyed", NULL, NULL, 0);
	if (!Keyed_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for Keyed_alias!\n");
		return (EXIT_FAILURE);
	}
	Keyed_writer =
	    DDS_Publisher_create_datawriter(publisher, Keyed_topic, &wr_qos,
					    NULL, 0);
	if (!Keyed_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	Keyed_reader =
	    DDS_Subscriber_create_datareader(subscriber, Keyed_topic, &rd_qos,
					     NULL, 0);
	if (!Keyed_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	Keyed_sample = Keyed_create();
	size = DDS_MarshalledDataSize((void *)Keyed_sample, 0, Keyed_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for Keyed\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error = DDS_MarshallData(dbw.data, Keyed_sample, 0, Keyed_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for Keyed\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, Keyed_ts, &error);
	printf("Keyed %d <> %d\n", size, sizeof(Keyed));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, Keyed_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, Keyed_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, Keyed_ts, 0);
	DDS_KeyToNativeData((void *)Keyed_sample, 0, dbw2.data, Keyed_ts);
	DDS_KeySizeFromMarshalled(dbw2, Keyed_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(Keyed_writer,
					     (void *)Keyed_sample);
	DDS_DataWriter_write(Keyed_writer, (void *)Keyed_sample, instance);
	DDS_DataWriter_write(Keyed_writer, (void *)Keyed_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(Keyed_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(Keyed_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, Keyed_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, Keyed_writer);
	DDS_DomainParticipant_delete_topic(participant, Keyed_topic);
	DDS_DomainParticipant_delete_topic(participant, Keyed_topic2);
	printf("Working on Keyless\n");
	Keyless_ts = DDS_DynamicType_register(Keyless_tsm);
	if (!Keyless_ts) {
		printf("DDS_DynamicType_register () failed for Keyless!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, Keyless_ts, "Keyless")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for Keyless!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, Keyless_ts, "Keyless_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for Keyless_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, Keyless_ts, "Keyless")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for Keyless (second time)!\n");
		return (EXIT_FAILURE);
	}
	Keyless_topic =
	    DDS_DomainParticipant_create_topic(participant, "Keyless",
					       "Keyless", NULL, NULL, 0);
	if (!Keyless_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for Keyless!\n");
		return (EXIT_FAILURE);
	}
	Keyless_topic2 =
	    DDS_DomainParticipant_create_topic(participant, "Keyless_alias",
					       "Keyless", NULL, NULL, 0);
	if (!Keyless_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for Keyless_alias!\n");
		return (EXIT_FAILURE);
	}
	Keyless_writer =
	    DDS_Publisher_create_datawriter(publisher, Keyless_topic, &wr_qos,
					    NULL, 0);
	if (!Keyless_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	Keyless_reader =
	    DDS_Subscriber_create_datareader(subscriber, Keyless_topic, &rd_qos,
					     NULL, 0);
	if (!Keyless_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	Keyless_sample = Keyless_create();
	size =
	    DDS_MarshalledDataSize((void *)Keyless_sample, 0, Keyless_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for Keyless\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error = DDS_MarshallData(dbw.data, Keyless_sample, 0, Keyless_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for Keyless\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, Keyless_ts, &error);
	printf("Keyless %d <> %d\n", size, sizeof(Keyless));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, Keyless_ts);
	free(buf);
	DDS_DataWriter_write(Keyless_writer, (void *)Keyless_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(Keyless_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(Keyless_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, Keyless_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, Keyless_writer);
	DDS_DomainParticipant_delete_topic(participant, Keyless_topic);
	DDS_DomainParticipant_delete_topic(participant, Keyless_topic2);
	printf("Working on CoverageType13\n");
	CoverageType13_ts = DDS_DynamicType_register(CoverageType13_tsm);
	if (!CoverageType13_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType13!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType13_ts, "CoverageType13")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType13!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType13_ts, "CoverageType13_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType13_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType13_ts, "CoverageType13")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType13 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType13_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType13",
					       "CoverageType13", NULL, NULL, 0);
	if (!CoverageType13_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType13!\n");
		return (EXIT_FAILURE);
	}
	CoverageType13_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType13_alias",
					       "CoverageType13", NULL, NULL, 0);
	if (!CoverageType13_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType13_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType13_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType13_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType13_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType13_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType13_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType13_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType13_sample = CoverageType13_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType13_sample, 0,
				   CoverageType13_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType13\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType13_sample, 0,
			     CoverageType13_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType13\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType13_ts, &error);
	printf("CoverageType13 %d <> %d\n", size, sizeof(CoverageType13));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType13_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType13_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType13_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType13_sample, 0, dbw2.data,
			    CoverageType13_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType13_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType13_writer,
					     (void *)CoverageType13_sample);
	DDS_DataWriter_write(CoverageType13_writer,
			     (void *)CoverageType13_sample, instance);
	DDS_DataWriter_write(CoverageType13_writer,
			     (void *)CoverageType13_sample, DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType13_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType13_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType13_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType13_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType13_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType13_topic2);
	printf("Working on CoverageType14\n");
	CoverageType14_ts = DDS_DynamicType_register(CoverageType14_tsm);
	if (!CoverageType14_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType14!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType14_ts, "CoverageType14")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType14!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType14_ts, "CoverageType14_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType14_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType14_ts, "CoverageType14")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType14 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType14_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType14",
					       "CoverageType14", NULL, NULL, 0);
	if (!CoverageType14_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType14!\n");
		return (EXIT_FAILURE);
	}
	CoverageType14_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType14_alias",
					       "CoverageType14", NULL, NULL, 0);
	if (!CoverageType14_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType14_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType14_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType14_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType14_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType14_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType14_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType14_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType14_sample = CoverageType14_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType14_sample, 0,
				   CoverageType14_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType14\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType14_sample, 0,
			     CoverageType14_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType14\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType14_ts, &error);
	printf("CoverageType14 %d <> %d\n", size, sizeof(CoverageType14));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType14_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType14_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType14_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType14_sample, 0, dbw2.data,
			    CoverageType14_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType14_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType14_writer,
					     (void *)CoverageType14_sample);
	DDS_DataWriter_write(CoverageType14_writer,
			     (void *)CoverageType14_sample, instance);
	DDS_DataWriter_write(CoverageType14_writer,
			     (void *)CoverageType14_sample, DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType14_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType14_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType14_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType14_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType14_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType14_topic2);
	printf("Working on CoverageType15\n");
	CoverageType15_ts = DDS_DynamicType_register(CoverageType15_tsm);
	if (!CoverageType15_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType15!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType15_ts, "CoverageType15")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType15!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType15_ts, "CoverageType15_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType15_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, CoverageType15_ts, "CoverageType15")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for CoverageType15 (second time)!\n");
		return (EXIT_FAILURE);
	}
	CoverageType15_topic =
	    DDS_DomainParticipant_create_topic(participant, "CoverageType15",
					       "CoverageType15", NULL, NULL, 0);
	if (!CoverageType15_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType15!\n");
		return (EXIT_FAILURE);
	}
	CoverageType15_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "CoverageType15_alias",
					       "CoverageType15", NULL, NULL, 0);
	if (!CoverageType15_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for CoverageType15_alias!\n");
		return (EXIT_FAILURE);
	}
	CoverageType15_writer =
	    DDS_Publisher_create_datawriter(publisher, CoverageType15_topic,
					    &wr_qos, NULL, 0);
	if (!CoverageType15_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	CoverageType15_reader =
	    DDS_Subscriber_create_datareader(subscriber, CoverageType15_topic,
					     &rd_qos, NULL, 0);
	if (!CoverageType15_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	CoverageType15_sample = CoverageType15_create();
	size =
	    DDS_MarshalledDataSize((void *)CoverageType15_sample, 0,
				   CoverageType15_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for CoverageType15\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, CoverageType15_sample, 0,
			     CoverageType15_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for CoverageType15\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, CoverageType15_ts, &error);
	printf("CoverageType15 %d <> %d\n", size, sizeof(CoverageType15));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, CoverageType15_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, CoverageType15_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, CoverageType15_ts, 0);
	DDS_KeyToNativeData((void *)CoverageType15_sample, 0, dbw2.data,
			    CoverageType15_ts);
	DDS_KeySizeFromMarshalled(dbw2, CoverageType15_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(CoverageType15_writer,
					     (void *)CoverageType15_sample);
	DDS_DataWriter_write(CoverageType15_writer,
			     (void *)CoverageType15_sample, instance);
	DDS_DataWriter_write(CoverageType15_writer,
			     (void *)CoverageType15_sample, DDS_HANDLE_NIL);
	DDS_DataReader_take(CoverageType15_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(CoverageType15_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, CoverageType15_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, CoverageType15_writer);
	DDS_DomainParticipant_delete_topic(participant, CoverageType15_topic);
	DDS_DomainParticipant_delete_topic(participant, CoverageType15_topic2);
	printf("Working on AnotherStruct\n");
	AnotherStruct_ts = DDS_DynamicType_register(AnotherStruct_tsm);
	if (!AnotherStruct_ts) {
		printf
		    ("DDS_DynamicType_register () failed for AnotherStruct!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, AnotherStruct_ts, "AnotherStruct")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for AnotherStruct!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, AnotherStruct_ts, "AnotherStruct_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for AnotherStruct_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, AnotherStruct_ts, "AnotherStruct")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for AnotherStruct (second time)!\n");
		return (EXIT_FAILURE);
	}
	AnotherStruct_topic =
	    DDS_DomainParticipant_create_topic(participant, "AnotherStruct",
					       "AnotherStruct", NULL, NULL, 0);
	if (!AnotherStruct_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for AnotherStruct!\n");
		return (EXIT_FAILURE);
	}
	AnotherStruct_topic2 =
	    DDS_DomainParticipant_create_topic(participant,
					       "AnotherStruct_alias",
					       "AnotherStruct", NULL, NULL, 0);
	if (!AnotherStruct_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for AnotherStruct_alias!\n");
		return (EXIT_FAILURE);
	}
	AnotherStruct_writer =
	    DDS_Publisher_create_datawriter(publisher, AnotherStruct_topic,
					    &wr_qos, NULL, 0);
	if (!AnotherStruct_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	AnotherStruct_reader =
	    DDS_Subscriber_create_datareader(subscriber, AnotherStruct_topic,
					     &rd_qos, NULL, 0);
	if (!AnotherStruct_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	AnotherStruct_sample = AnotherStruct_create();
	size =
	    DDS_MarshalledDataSize((void *)AnotherStruct_sample, 0,
				   AnotherStruct_ts, &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for AnotherStruct\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error =
	    DDS_MarshallData(dbw.data, AnotherStruct_sample, 0, AnotherStruct_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for AnotherStruct\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, AnotherStruct_ts, &error);
	printf("AnotherStruct %d <> %d\n", size, sizeof(AnotherStruct));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, AnotherStruct_ts);
	free(buf);
	DDS_DataWriter_write(AnotherStruct_writer, (void *)AnotherStruct_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(AnotherStruct_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(AnotherStruct_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, AnotherStruct_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, AnotherStruct_writer);
	DDS_DomainParticipant_delete_topic(participant, AnotherStruct_topic);
	DDS_DomainParticipant_delete_topic(participant, AnotherStruct_topic2);
	printf("Working on FilterData\n");
	FilterData_ts = DDS_DynamicType_register(FilterData_tsm);
	if (!FilterData_ts) {
		printf("DDS_DynamicType_register () failed for FilterData!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, FilterData_ts, "FilterData")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for FilterData!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, FilterData_ts, "FilterData_alias")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for FilterData_alias!\n");
		return (EXIT_FAILURE);
	}
	if (DDS_DomainParticipant_register_type
	    (participant, FilterData_ts, "FilterData")) {
		printf
		    ("DDS_DomainParticipant_register_type () failed for FilterData (second time)!\n");
		return (EXIT_FAILURE);
	}
	FilterData_topic =
	    DDS_DomainParticipant_create_topic(participant, "FilterData",
					       "FilterData", NULL, NULL, 0);
	if (!FilterData_topic) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for FilterData!\n");
		return (EXIT_FAILURE);
	}
	FilterData_topic2 =
	    DDS_DomainParticipant_create_topic(participant, "FilterData_alias",
					       "FilterData", NULL, NULL, 0);
	if (!FilterData_topic2) {
		printf
		    ("DDS_DomainParticipant_create_topic () failed for FilterData_alias!\n");
		return (EXIT_FAILURE);
	}
	FilterData_writer =
	    DDS_Publisher_create_datawriter(publisher, FilterData_topic,
					    &wr_qos, NULL, 0);
	if (!FilterData_writer) {
		fprintf(stderr, "Could not create data writer\n");
		exit(EXIT_FAILURE);
	}
	FilterData_reader =
	    DDS_Subscriber_create_datareader(subscriber, FilterData_topic,
					     &rd_qos, NULL, 0);
	if (!FilterData_reader) {
		fprintf(stderr, "Could not create data reader\n");
		exit(EXIT_FAILURE);
	}
	FilterData_sample = FilterData_create();
	size =
	    DDS_MarshalledDataSize((void *)FilterData_sample, 0, FilterData_ts,
				   &error);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Could not get datasize for FilterData\n");
		exit(EXIT_FAILURE);
	}
	dbw.dbp = db_alloc_data(size, 1);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	error = DDS_MarshallData(dbw.data, FilterData_sample, 0, FilterData_ts);
	if (error != DDS_RETCODE_OK) {
		fprintf(stderr, "Failed to marshall data for FilterData\n");
		exit(EXIT_FAILURE);
	}
	size = DDS_UnmarshalledDataSize(dbw, FilterData_ts, &error);
	printf("FilterData %d <> %d\n", size, sizeof(FilterData));
	buf = malloc(size);
	DDS_UnmarshallData(buf, &dbw, FilterData_ts);
	free(buf);
	dbw.data = dbw.dbp->data;
	dbw.left = size;
	dbw.length = size;
	size = DDS_KeySizeFromMarshalled(dbw, FilterData_ts, 0, &error);
	dbw2.dbp = db_alloc_data(size, 1);
	dbw2.data = dbw2.dbp->data;
	dbw2.left = size;
	dbw2.length = size;
	fprintf(stderr, "KEY SIZE=%d\n", size);
	DDS_KeyFromMarshalled(dbw2.data, dbw, FilterData_ts, 0);
	DDS_KeyToNativeData((void *)FilterData_sample, 0, dbw2.data,
			    FilterData_ts);
	DDS_KeySizeFromMarshalled(dbw2, FilterData_ts, 1, &error);
	instance =
	    DDS_DataWriter_register_instance(FilterData_writer,
					     (void *)FilterData_sample);
	DDS_DataWriter_write(FilterData_writer, (void *)FilterData_sample,
			     instance);
	DDS_DataWriter_write(FilterData_writer, (void *)FilterData_sample,
			     DDS_HANDLE_NIL);
	DDS_DataReader_take(FilterData_reader, &rx_sample, &rx_info, 1,
			    DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
			    DDS_ANY_INSTANCE_STATE);
	DDS_DataReader_return_loan(FilterData_reader, &rx_sample, &rx_info);
	if (DDS_Subscriber_delete_datareader(subscriber, FilterData_reader)) {
		fprintf(stderr, "Could not delete datareader\n");
		return (EXIT_FAILURE);
	}
	DDS_Publisher_delete_datawriter(publisher, FilterData_writer);
	DDS_DomainParticipant_delete_topic(participant, FilterData_topic);
	DDS_DomainParticipant_delete_topic(participant, FilterData_topic2);
	CoverageType1_ts = DDS_DynamicType_register(CoverageType1_tsm);
	if (!CoverageType1_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType1!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType1_ts);
	DDS_DynamicType_free(CoverageType1_ts);
	DDS_DynamicType_free(CoverageType1_ts);
	CoverageType2_ts = DDS_DynamicType_register(CoverageType2_tsm);
	if (!CoverageType2_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType2!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType2_ts);
	DDS_DynamicType_free(CoverageType2_ts);
	DDS_DynamicType_free(CoverageType2_ts);
	CoverageType2b_ts = DDS_DynamicType_register(CoverageType2b_tsm);
	if (!CoverageType2b_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType2b!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType2b_ts);
	DDS_DynamicType_free(CoverageType2b_ts);
	DDS_DynamicType_free(CoverageType2b_ts);
	CoverageType3_ts = DDS_DynamicType_register(CoverageType3_tsm);
	if (!CoverageType3_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType3!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType3_ts);
	DDS_DynamicType_free(CoverageType3_ts);
	DDS_DynamicType_free(CoverageType3_ts);
	CoverageType4_ts = DDS_DynamicType_register(CoverageType4_tsm);
	if (!CoverageType4_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType4!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType4_ts);
	DDS_DynamicType_free(CoverageType4_ts);
	DDS_DynamicType_free(CoverageType4_ts);
	CoverageType5_ts = DDS_DynamicType_register(CoverageType5_tsm);
	if (!CoverageType5_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType5!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType5_ts);
	DDS_DynamicType_free(CoverageType5_ts);
	DDS_DynamicType_free(CoverageType5_ts);
	CoverageType6_ts = DDS_DynamicType_register(CoverageType6_tsm);
	if (!CoverageType6_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType6!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType6_ts);
	DDS_DynamicType_free(CoverageType6_ts);
	DDS_DynamicType_free(CoverageType6_ts);
	CoverageType7_ts = DDS_DynamicType_register(CoverageType7_tsm);
	if (!CoverageType7_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType7!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType7_ts);
	DDS_DynamicType_free(CoverageType7_ts);
	DDS_DynamicType_free(CoverageType7_ts);
	CoverageType8_ts = DDS_DynamicType_register(CoverageType8_tsm);
	if (!CoverageType8_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType8!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType8_ts);
	DDS_DynamicType_free(CoverageType8_ts);
	DDS_DynamicType_free(CoverageType8_ts);
	CoverageType9_ts = DDS_DynamicType_register(CoverageType9_tsm);
	if (!CoverageType9_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType9!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType9_ts);
	DDS_DynamicType_free(CoverageType9_ts);
	DDS_DynamicType_free(CoverageType9_ts);
	CoverageType10_ts = DDS_DynamicType_register(CoverageType10_tsm);
	if (!CoverageType10_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType10!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType10_ts);
	DDS_DynamicType_free(CoverageType10_ts);
	DDS_DynamicType_free(CoverageType10_ts);
	CoverageType12_ts = DDS_DynamicType_register(CoverageType12_tsm);
	if (!CoverageType12_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType12!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType12_ts);
	DDS_DynamicType_free(CoverageType12_ts);
	DDS_DynamicType_free(CoverageType12_ts);
	Keyed_ts = DDS_DynamicType_register(Keyed_tsm);
	if (!Keyed_ts) {
		printf("DDS_DynamicType_register () failed for Keyed!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(Keyed_ts);
	DDS_DynamicType_free(Keyed_ts);
	DDS_DynamicType_free(Keyed_ts);
	Keyless_ts = DDS_DynamicType_register(Keyless_tsm);
	if (!Keyless_ts) {
		printf("DDS_DynamicType_register () failed for Keyless!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(Keyless_ts);
	DDS_DynamicType_free(Keyless_ts);
	DDS_DynamicType_free(Keyless_ts);
	CoverageType13_ts = DDS_DynamicType_register(CoverageType13_tsm);
	if (!CoverageType13_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType13!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType13_ts);
	DDS_DynamicType_free(CoverageType13_ts);
	DDS_DynamicType_free(CoverageType13_ts);
	CoverageType14_ts = DDS_DynamicType_register(CoverageType14_tsm);
	if (!CoverageType14_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType14!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType14_ts);
	DDS_DynamicType_free(CoverageType14_ts);
	DDS_DynamicType_free(CoverageType14_ts);
	CoverageType15_ts = DDS_DynamicType_register(CoverageType15_tsm);
	if (!CoverageType15_ts) {
		printf
		    ("DDS_DynamicType_register () failed for CoverageType15!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(CoverageType15_ts);
	DDS_DynamicType_free(CoverageType15_ts);
	DDS_DynamicType_free(CoverageType15_ts);
	AnotherStruct_ts = DDS_DynamicType_register(AnotherStruct_tsm);
	if (!AnotherStruct_ts) {
		printf
		    ("DDS_DynamicType_register () failed for AnotherStruct!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(AnotherStruct_ts);
	DDS_DynamicType_free(AnotherStruct_ts);
	DDS_DynamicType_free(AnotherStruct_ts);
	FilterData_ts = DDS_DynamicType_register(FilterData_tsm);
	if (!FilterData_ts) {
		printf("DDS_DynamicType_register () failed for FilterData!\n");
		return (EXIT_FAILURE);
	}
	DDS_DynamicType_free(FilterData_ts);
	DDS_DynamicType_free(FilterData_ts);
	DDS_DynamicType_free(FilterData_ts);
	DDS_DomainParticipant_delete_contained_entities(participant);
	if (DDS_DomainParticipantFactory_delete_participant(participant)) {
		fprintf(stderr, "Could not delete domain participant.\n");
		exit(EXIT_FAILURE);
	}
	return (EXIT_SUCCESS);

}
