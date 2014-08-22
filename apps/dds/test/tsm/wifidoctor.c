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

/**************************************************************
 ********          THIS IS A GENERATED FILE         ***********
 **************************************************************/

#include "wifidoctor.h"

const DDS_TypeSupport_meta com_technicolor_wifidoctor_AssociatedStationStats_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.wifidoctor.AssociatedStationStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 11, .size = sizeof(com_technicolor_wifidoctor_AssociatedStationStats_t) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "MACAddress", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, MACAddress), .size = 0 },  
    { .tc = CDR_TYPECODE_LONG, .name = "RSSI", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, RSSI) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "avgSpatialStreamsTX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, avgSpatialStreamsTX) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "avgSpatialStreamsRX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, avgSpatialStreamsRX) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "avgUsedBandwidthTX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, avgUsedBandwidthTX) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "avgUsedBandwidthRX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, avgUsedBandwidthRX) },  
    { .tc = CDR_TYPECODE_LONG, .name = "trainedPhyRateTX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, trainedPhyRateTX) },  
    { .tc = CDR_TYPECODE_LONG, .name = "trainedPhyRateRX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, trainedPhyRateRX) },  
    { .tc = CDR_TYPECODE_LONG, .name = "dataRateTX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, dataRateTX) },  
    { .tc = CDR_TYPECODE_LONG, .name = "dataRateRX", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, dataRateRX) },  
    { .tc = CDR_TYPECODE_OCTET, .name = "powerSaveTimeFraction", .offset = offsetof(com_technicolor_wifidoctor_AssociatedStationStats_t, powerSaveTimeFraction) },  
};

const DDS_TypeSupport_meta com_technicolor_wifidoctor_APStats_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.wifidoctor.APStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 4, .size = sizeof(com_technicolor_wifidoctor_APStats_t) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "MACAddress", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(com_technicolor_wifidoctor_APStats_t, MACAddress), .size = 0 },  
    { .tc = CDR_TYPECODE_OCTET, .name = "RXTimeFractionIBSS", .offset = offsetof(com_technicolor_wifidoctor_APStats_t, RXTimeFractionIBSS) },  
    { .tc = CDR_TYPECODE_OCTET, .name = "RXTimeFractionOBSS", .offset = offsetof(com_technicolor_wifidoctor_APStats_t, RXTimeFractionOBSS) },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "associatedStationStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(com_technicolor_wifidoctor_APStats_t, associatedStationStats) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = com_technicolor_wifidoctor_AssociatedStationStats_type },  
};

const DDS_TypeSupport_meta com_technicolor_wifidoctor_STAStats_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.wifidoctor.STAStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .nelem = 2, .size = sizeof(com_technicolor_wifidoctor_STAStats_t) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "MACAddress", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(com_technicolor_wifidoctor_STAStats_t, MACAddress), .size = 0 },  
    { .tc = CDR_TYPECODE_LONG, .name = "RSSI", .offset = offsetof(com_technicolor_wifidoctor_STAStats_t, RSSI) },  
};

const DDS_TypeSupport_meta com_technicolor_wifidoctor_RadioStats_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.wifidoctor.RadioStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 6, .size = sizeof(com_technicolor_wifidoctor_RadioStats_t) },  
    { .tc = CDR_TYPECODE_LONG, .name = "testId", .flags = TSMFLAG_KEY, .offset = offsetof(com_technicolor_wifidoctor_RadioStats_t, testId) },  
    { .tc = CDR_TYPECODE_TYPEREF, .name = "radio", .flags = TSMFLAG_KEY, .offset = offsetof(com_technicolor_wifidoctor_RadioStats_t, radio), .tsm = org_qeo_UUID_type },  
    { .tc = CDR_TYPECODE_LONGLONG, .name = "timestamp", .offset = offsetof(com_technicolor_wifidoctor_RadioStats_t, timestamp) },  
    { .tc = CDR_TYPECODE_OCTET, .name = "mediumAvailable", .offset = offsetof(com_technicolor_wifidoctor_RadioStats_t, mediumAvailable) },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "APStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(com_technicolor_wifidoctor_RadioStats_t, APStats) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = com_technicolor_wifidoctor_APStats_type },  
    { .tc = CDR_TYPECODE_SEQUENCE, .name = "STAStats", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_MUTABLE, .offset = offsetof(com_technicolor_wifidoctor_RadioStats_t, STAStats) },  
    { .tc = CDR_TYPECODE_TYPEREF, .tsm = com_technicolor_wifidoctor_STAStats_type },  
};

const DDS_TypeSupport_meta com_technicolor_wifidoctor_TestRequest_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.wifidoctor.TestRequest", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 7, .size = sizeof(com_technicolor_wifidoctor_TestRequest_t) },  
    { .tc = CDR_TYPECODE_LONG, .name = "id", .flags = TSMFLAG_KEY, .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, id) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "sourceMAC", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, sourceMAC), .size = 0 },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "destinationMAC", .flags = TSMFLAG_DYNAMIC, .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, destinationMAC), .size = 0 },  
    { .tc = CDR_TYPECODE_LONG, .name = "type", .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, type) },  
    { .tc = CDR_TYPECODE_LONG, .name = "duration", .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, duration) },  
    { .tc = CDR_TYPECODE_SHORT, .name = "packetSize", .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, packetSize) },  
    { .tc = CDR_TYPECODE_OCTET, .name = "WMMClass", .offset = offsetof(com_technicolor_wifidoctor_TestRequest_t, WMMClass) },  
};

const DDS_TypeSupport_meta com_technicolor_wifidoctor_TestState_type[] = {
    { .tc = CDR_TYPECODE_STRUCT, .name = "com.technicolor.wifidoctor.TestState", .flags = TSMFLAG_DYNAMIC|TSMFLAG_GENID|TSMFLAG_KEY|TSMFLAG_MUTABLE, .nelem = 3, .size = sizeof(com_technicolor_wifidoctor_TestState_t) },  
    { .tc = CDR_TYPECODE_LONG, .name = "id", .flags = TSMFLAG_KEY, .offset = offsetof(com_technicolor_wifidoctor_TestState_t, id) },  
    { .tc = CDR_TYPECODE_CSTRING, .name = "participantMAC", .flags = TSMFLAG_DYNAMIC|TSMFLAG_KEY, .offset = offsetof(com_technicolor_wifidoctor_TestState_t, participantMAC), .size = 0 },  
    { .tc = CDR_TYPECODE_LONG, .name = "state", .offset = offsetof(com_technicolor_wifidoctor_TestState_t, state) },  
};

