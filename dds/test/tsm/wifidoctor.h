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

#ifndef QDM_WIFIDOCTOR_H_
#define QDM_WIFIDOCTOR_H_

#include "qeo_types.h"

typedef struct {
    /**
     * MAC address associated with station
     */
    char * MACAddress;
    /**
     * expressed in dBm
     */
    int32_t RSSI;
    /**
     * 			fixed point 3 decimals. Average of # spatial streams used in TX direction. 		
     */
    int16_t avgSpatialStreamsTX;
    /**
     * fixed point 3 decimals. Average of # spatial streams used in RX direction.
     */
    int16_t avgSpatialStreamsRX;
    /**
     * fixed point 3 decimals. Average signal bandwidth used in TX direction.
     */
    int16_t avgUsedBandwidthTX;
    /**
     * fixed point 3 decimals. Average signal bandwidth used in TX direction.
     */
    int16_t avgUsedBandwidthRX;
    /**
     * kbps. PhyRate used under traffic in TX direction.
     */
    int32_t trainedPhyRateTX;
    /**
     * kbps. PhyRate used under traffic in RX direction.
     */
    int32_t trainedPhyRateRX;
    /**
     * kbps. Datarate in TX direction.
     */
    int32_t dataRateTX;
    /**
     * kbps. Datarate in RX direction.
     */
    int32_t dataRateRX;
    /**
     * Integer percentage. Fraction of time that the Station spends in Power Save mode.
     */
    int8_t powerSaveTimeFraction;
} com_technicolor_wifidoctor_AssociatedStationStats_t;
extern const DDS_TypeSupport_meta com_technicolor_wifidoctor_AssociatedStationStats_type[];

DDS_SEQUENCE(com_technicolor_wifidoctor_AssociatedStationStats_t, com_technicolor_wifidoctor_wifidoctor_associatedStationStats_seq);
typedef struct {
    /**
     * MAC address associated with BSSID
     */
    char * MACAddress;
    /**
     * Integer percentage. Fraction of time spent receiving packets from inside the BSSID.
     */
    int8_t RXTimeFractionIBSS;
    /**
     * Integer percentage. Fraction of time spent receiving packets from outside the BSSID.
     */
    int8_t RXTimeFractionOBSS;
    /**
     * statistics per associated station
     */
    com_technicolor_wifidoctor_wifidoctor_associatedStationStats_seq associatedStationStats;
} com_technicolor_wifidoctor_APStats_t;
extern const DDS_TypeSupport_meta com_technicolor_wifidoctor_APStats_type[];

typedef struct {
    char * MACAddress;
    /**
     * 		 expressed in dBm. Note that this value is a filtered average 		 - only taking into account RSSI samples taken when traffic is flowing. 		
     */
    int32_t RSSI;
} com_technicolor_wifidoctor_STAStats_t;
extern const DDS_TypeSupport_meta com_technicolor_wifidoctor_STAStats_type[];

DDS_SEQUENCE(com_technicolor_wifidoctor_APStats_t, com_technicolor_wifidoctor_wifidoctor_APStats_seq);
DDS_SEQUENCE(com_technicolor_wifidoctor_STAStats_t, com_technicolor_wifidoctor_wifidoctor_STAStats_seq);
typedef struct {
    /**
     * [Key] ID of the TestRequest for which these stats are published. A value of 0 indicates these are passive monitoring stats, not associated with a specific test request
     */
    int32_t testId;
    /**
     * [Key] 			ID of the wifi radio. Basically a random number, assumed to be unique over the whole Qeo realm. 			In the future, we'd probably use a UUID here but for the POC that's a bit overkill. 	
     */
    org_qeo_UUID_t radio;
    /**
     * Seconds since Jan 1, 1970
     */
    int64_t timestamp;
    /**
     * 			Integer percentage. Fraction of time that the medium is assessed available by the CCA of the wifi radio. 			For radios for which we cannot derive this statistic, use -1 as a sentinel value. 		
     */
    int8_t mediumAvailable;
    com_technicolor_wifidoctor_wifidoctor_APStats_seq APStats;
    com_technicolor_wifidoctor_wifidoctor_STAStats_seq STAStats;
} com_technicolor_wifidoctor_RadioStats_t;
extern const DDS_TypeSupport_meta com_technicolor_wifidoctor_RadioStats_type[];

/**
 * A coordinator (typically the WifiDr Android app on the STA, but not necessarily) publishes a TestRequest to trigger a test between an AP and a STA. As long as the TestRequest instance lives, the test is 	 relevant and will be (eventually) carried out, or the results will 	 remain available. When the TestRequest instance is removed, all other traces 	 of the test (test states, results) will be removed as well.
 */
typedef struct {
    /**
     * [Key]
     */
    int32_t id;
    /**
     * MAC address of the transmitting node for this test
     */
    char * sourceMAC;
    /**
     * MAC address of the receiving node for this test
     */
    char * destinationMAC;
    /**
     * The test type. This is a poor man's substitute for an enumeration. Possible values are: 0: PING test 1: TX test
     */
    int32_t type;
    /**
     * test duration in seconds (0 < = x < = 86400)
     */
    int32_t duration;
    /**
     * TX test parameter (64 < = x < = 2346) 	 ping test: 0 < = x < = 20000 	
     */
    int16_t packetSize;
    /**
     * 			TX test parameter (0 < = x < = 4) 			0 = Background 			1 = Best Effort 			2 = Video 			3 = Voice 		
     */
    int8_t WMMClass;
} com_technicolor_wifidoctor_TestRequest_t;
extern const DDS_TypeSupport_meta com_technicolor_wifidoctor_TestRequest_type[];

typedef struct {
    /**
     * [Key] id of the corresponding TestRequest
     */
    int32_t id;
    /**
     * [Key] MAC address of the test participant publishing this test state
     */
    char * participantMAC;
    /**
     * This should be an enum really. Possible values: 0 = QUEUED: acknowledge we've seen the test request, but it is not yet ready for execution 1 = WILLING: RX node indicates it is ready to participate in the test, waits for a COMMIT from the TX node before starting 2 = COMMIT: TX node indicates it is committed to starting the test, waits for RX node to go to TESTING before actually starting 3 = TESTING: test ongoing (for both RX and TX node) 4 = DONE: test is finished, results will be published 5 = REJECTED: node is unwilling to perform this test or aborting this test for some reason. For tests where both TX and RX node are WifiDr-capable, we assume the following sequence of states: Coordinator TX node RX node --------------------------------------------------------- publish TestRequest QUEUED QUEUED v v WILLING COMMIT v v TESTING TESTING v v DONE v read TX node results DONE read RX node results remove TestRequest v v remove TestState remove TestState For "blind" tests (where the RX node is not WifiDr-capable), we assume the following sequence of states: Coordinator TX node ----------------------------------------- publish TestRequest QUEUED v TESTING v v DONE read TX node results remove TestRequest v remove TestState 									 		 		 Note that transition to REJECTED state can happen from all other states. 		 In reaction to this, the related Testrequest and TestState instances shall be removed.
     */
    int32_t state;
} com_technicolor_wifidoctor_TestState_t;
extern const DDS_TypeSupport_meta com_technicolor_wifidoctor_TestState_type[];


#endif /* QDM_WIFIDOCTOR_H_ */

