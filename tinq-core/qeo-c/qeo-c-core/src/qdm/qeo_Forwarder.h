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

#ifndef QDM_QEO_FORWARDER_H_
#define QDM_QEO_FORWARDER_H_

#include <qeo/types.h>


/**
 * Information for a single locator of a Qeo forwarder.
 */
typedef struct {
    /**
     * The type of the locator. Possible values are: 	 0 = unknown, 	 1 = TCPv4, 	 2 = TCPv6, 	 3 = UDPv4, 	 4 = UDPv6.
     */
    int32_t type;
    /**
     * Address of the locator. This can be an IP address or a DNS name.
     */
    char * address;
    /**
     * Port of the locator.
     */
    int32_t port;
} org_qeo_system_ForwarderLocator_t;
extern const DDS_TypeSupport_meta org_qeo_system_ForwarderLocator_type[];

DDS_SEQUENCE(org_qeo_system_ForwarderLocator_t, org_qeo_system_Forwarder_locator_seq);
/**
 * Representation of a single Qeo forwarder.
 */
typedef struct {
    /**
     * [Key] The device ID as known by the location service.
     */
    int64_t deviceId;
    /**
     * The list of locators present on this Qeo forwarder.
     */
    org_qeo_system_Forwarder_locator_seq locator;
} org_qeo_system_Forwarder_t;
extern const DDS_TypeSupport_meta org_qeo_system_Forwarder_type[];


#endif /* QDM_QEO_FORWARDER_H_ */

