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
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>

#include <qeocore/api.h>
#include <qeocore/config.h>
#include <qeo/factory.h>
#include <qeo/log.h>
#include "forwarder.h"

/* delay in milliseconds */
#define UPNP_DELAY 2000

/* DDS forwarding port */
#define FWD_BASE_PORT 7400
#define FWD_BASE_PORT_SPAN 10

/* periods in seconds */
#define UPNP_POLL_PERIOD 300
#define PORTMAP_LEASE_PERIOD 0

#define PORTMAP_DESCRIPTION QeoForwarder

#define xstr(s) str(s)
#define str(s) #s

typedef enum {
    STATE_QUIT,
    STATE_IDLE,
    STATE_PUBLIC_IP_REQUESTED,
    STATE_WAIT_FOR_UPNP_POLL,
    STATE_RUNNING,
} state_t;

typedef struct {
    char *control_url;
    char *service_type;
    char *lan_ip;
    char *wan_ip;
    char *wan_port;
    bool wan_updated;
} igd_t;

typedef enum {
    TIMER_UPNP_POLL,
    TIMER_NUM
} timer_type_t;

static state_t _state = STATE_IDLE;
static pthread_mutex_t _state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _state_cv = PTHREAD_COND_INITIALIZER;

static unsigned int _upnp_disc_timeout = UPNP_DELAY;
static unsigned int _upnp_poll_period = UPNP_POLL_PERIOD;
static unsigned int _upnp_portmap_lease_period = PORTMAP_LEASE_PERIOD;

static char *_public_ip = NULL;
static int _public_port = -1;
static char *_local_port = NULL;

static qeo_factory_t *_factory = NULL;

static void state_change(state_t new_state)
{
    pthread_mutex_lock(&_state_mutex);
    _state = new_state;
    pthread_cond_signal(&_state_cv);
    pthread_mutex_unlock(&_state_mutex);
}

static int get_free_tcp_port()
{
    struct sockaddr_in sin;
    int port = -1;
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sock) {
        qeo_log_e("failed to open socket");
    }
    else {
        int i;

        for (i = FWD_BASE_PORT; i < FWD_BASE_PORT + FWD_BASE_PORT_SPAN; i++) {
            sin.sin_port = htons(i);
            sin.sin_addr.s_addr = 0;
            sin.sin_addr.s_addr = INADDR_ANY;
            sin.sin_family = AF_INET;
            if (-1 == bind(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in))) {
                qeo_log_d("local TCP port %d unavailable", i);
            }
            else {
                qeo_log_d("local TCP port %d available", i);
                port = i;
                break;
            }
        }
        close(sock);
        if (-1 == port) {
            qeo_log_e("failed to find free TCP port");
        }
    }
    return port;
}

/* ===[ UPnP handling ]===================================================== */

/**
 * Check if a specific portmap is already available
 */
static bool has_portmap(igd_t * igd) {
    if (igd == NULL || igd->wan_port == NULL) {
        return false;
    }
    char intClientIP[40];
    char intClientPort[6];
    char duration[16];
    char description[256];
    char enabled[16];

    if (0 == UPNP_GetSpecificPortMappingEntry(igd->control_url, igd->service_type, igd->wan_port, "TCP",
                                                  intClientIP, intClientPort, description, enabled, duration)) {
        //portmap available
        if (strcmp(intClientIP, igd->lan_ip) == 0 && strcmp(intClientPort, _local_port) == 0) {
            return true;
        }
    }
    return false;
}

// DE3155 driven :
static int get_startport() {
    int extPortStart = FWD_BASE_PORT;
    char *extPortStartString = getenv("QEO_FWD_EXTERNAL_PORT_START");
    if (extPortStartString != NULL) {
        extPortStart = atoi(extPortStartString);
        qeo_log_i("External TCP port start set to %d", extPortStart);
    }
    return extPortStart;
}

/**
 * \return 0 on success, something else on failure
 */
static int get_free_igd_port(igd_t *igd)
{
    int rc = 1;
    char intClientIP[40];
    char intClientPort[6];
    char duration[16];
    char description[256];
    char enabled[16];

    int extPortInt;
    char extPortString[12];
    int startport;
    int extPortStart=get_startport();

    // DE3155
    if (NULL == igd->wan_port) {startport = extPortStart;}
        else  {startport = atoi(igd->wan_port)+1;}
    // DE3155 end
    // DE3155 for (extPortInt = extPortStart; became for (extPortInt=startport ;
    for (extPortInt=startport ; extPortInt < (extPortStart + FWD_BASE_PORT_SPAN); extPortInt++) {
        sprintf(extPortString, "%d", extPortInt);
        if (0 == UPNP_GetSpecificPortMappingEntry(igd->control_url, igd->service_type, extPortString, "TCP",
                                                  intClientIP, intClientPort, description, enabled, duration)) {
            /* occupied portmap */
            qeo_log_i("%s is an occupied external port", extPortString);
        }
        else {
            /* found a free portmap according UPNP */
            qeo_log_i("%s is a free external port (i.e. not used by UPnP)", extPortString);
            igd->wan_port = strdup(extPortString);
            rc = 0;
            break;}
    }
    return rc;
}

/**
 * \return 0 on success, something else on failure
 *
 * A portmap is made where local port equals the public port
 */
static int add_portmap(igd_t *igd)
{
    int rc = 0;

    if (!has_portmap(igd)) {

        qeo_log_d("checking for existing portmaps");
        int i = 0;
        do {
            //loop over all existing portmaps to check if there is already a portmap to our local ip:port
            //If so, re-use it as you can't create a 2nd one.
            char extPort[6];
            char intClient[16];
            char intPort[6];
            char protocol[4];
            char desc[80];
            char enabled[4];
            char rHost[64];
            char duration[16];
            char nr[12];
            sprintf(nr, "%d", i);
            rc = UPNP_GetGenericPortMappingEntry(igd->control_url, igd->service_type, nr,
                      extPort, intClient, intPort, protocol, desc, enabled, rHost, duration);
            if (rc == UPNPCOMMAND_SUCCESS) {
                qeo_log_d("found %s -> %s:%s (%s, %s)", extPort, intClient, intPort, protocol, desc);
                if (strcmp(intClient, igd->lan_ip) == 0 && strcmp(intPort, _local_port) == 0) {
                    qeo_log_i("Portmap to our local port is already created, re-using");
                    igd->wan_port = strdup(extPort);
                    break;
                }
            }
            else {
                //end of list or failure. does not matter, stop anyway
                break;
            }
            ++i;
        } while(1);
    
        if (NULL == igd->wan_port) {
            /* get an available port */
            rc = get_free_igd_port(igd);
            if (0 != rc) {
                qeo_log_e("failed to find an available UPNP external port");
                return rc;
            }
        }
    }
    else {
        qeo_log_d("only need to refresh portmap");
    }

    qeo_log_i("adding/refreshing portmap for external IP %s:%s to local IP %s:%s duration=%s",
              igd->wan_ip, igd->wan_port,igd->lan_ip, _local_port, xstr(PORTMAP_LEASE_PERIOD));
    rc = UPNP_AddPortMapping(igd->control_url, igd->service_type, igd->wan_port, _local_port,
                             igd->lan_ip, xstr(PORTMAP_DESCRIPTION), "TCP", 0, xstr(PORTMAP_LEASE_PERIOD));
 
    if (0 != rc) {
        qeo_log_e("Failed to add portmap: %d (%s)\n", rc, strupnperror(rc));
        /* It is possible a portmap was made on the IGD by other means than UPnP
         * then a UPNP_AddPortMapping can fail; we need to try the next external
         * port starting with current extPort+1 */
        int extPortStart=get_startport();
        while ( atoi(igd->wan_port) < (extPortStart + FWD_BASE_PORT_SPAN+1) )  {
            /* continue trying to get an available port */
            rc = get_free_igd_port(igd);
            if (0 != rc) {
                qeo_log_e("failed to find an available UPNP external port");
                return rc;
            }
            else {
                /* try to add portmap */
                qeo_log_i("adding portmap for external IP %s:%s to local IP %s:%s duration=%s",
                          igd->wan_ip, igd->wan_port,igd->lan_ip, _local_port, xstr(PORTMAP_LEASE_PERIOD));
                rc = UPNP_AddPortMapping(igd->control_url, igd->service_type, igd->wan_port, _local_port,
                                         igd->lan_ip, xstr(PORTMAP_DESCRIPTION), "TCP", 0, xstr(PORTMAP_LEASE_PERIOD));
                if (0 != rc) {
                    qeo_log_e("Failed to add portmap: %d (%s)\n", rc, strupnperror(rc));
                }
                else {
                    break;
                }
            }
        }
    }

    if (0 == rc) {
        printf("- Public address : %s:%s\n", igd->wan_ip, igd->wan_port);
    }


    return rc;
}


static void igd_clean(igd_t *igd)
{
    if (NULL != igd) {
        free(igd->lan_ip);
        free(igd->wan_ip);
        free(igd->wan_port);
        free(igd->control_url);
        free(igd->service_type);
    }
}

/**
 * \return 0 on success, something else on failure
 */
static int get_igd_details(igd_t *igd)
{
    int rc = 1;

    igd->wan_updated = false;
    if (NULL == igd->control_url) {
        char *intf = getenv("QEO_FWD_UPNP_INTF");
        struct UPNPDev *devlist = NULL;

        /* no IGD device selected yet */
        qeo_log_i("Doing upnp discovery");
        devlist = upnpDiscover(_upnp_disc_timeout, intf, NULL, 0, 0, NULL);
        if (NULL != devlist) {
            struct UPNPUrls urls;
            struct IGDdatas data;
            char lan_ip[16];

            rc = UPNP_GetValidIGD(devlist, &urls, &data, lan_ip, sizeof(lan_ip));
            if (0 != rc) {
                //found device
                if (1 == rc) {
                    //valid and connected device
                    igd->lan_ip = strdup(lan_ip);
                    igd->control_url = strdup(urls.controlURL);
                    qeo_log_i("Found IGD device: %s", igd->control_url);
                    igd->service_type = strdup(data.first.servicetype);
                    if ((NULL == igd->lan_ip) || (NULL == igd->control_url) || (NULL == igd->service_type)) {
                        qeo_log_e("out of memory");
                    }
                    else {
                        rc = 0;
                    }
                }
                FreeUPNPUrls(&urls);
            }
            else {
                qeo_log_w("No IGD devices found");
                rc = 1;
            }
            freeUPNPDevlist(devlist);
        }
        else {
            qeo_log_w("No UPNP devices found");
        }
    }
    else {
        rc = 0; /* IGD data already available */
    }
    /* get external IP address */
    if (0 == rc) {
        char wan_ip[16];

        rc = UPNP_GetExternalIPAddress(igd->control_url, igd->service_type, wan_ip);
        if (0 != rc) {
            qeo_log_e("failed to get external IP address: %d (%s)", rc, strupnperror(rc));
        }
        else {
            if ((NULL == igd->wan_ip) || (0 != strcmp(igd->wan_ip, wan_ip))) {
                if (NULL != igd->wan_ip) {
                    qeo_log_i("WAN ip update from %s to %s", igd->wan_ip, wan_ip);
                    qeo_log_i("deleting portmap for external port %s", igd->wan_port);
                    UPNP_DeletePortMapping(igd->control_url, igd->service_type, igd->wan_port, "TCP", 0);
                    free(igd->wan_ip);
                }
                igd->wan_ip = strdup(wan_ip);
                igd->wan_updated = true;
                if (NULL == igd->wan_ip) {
                    qeo_log_e("out of memory");
                    rc = 1;
                }
            }
        }
    }
    return rc;
}

/* ===[ Public IP address handling ]======================================== */

/**
 * No local forwarder was discovered within the specified time-out.  So we can
 * start up as a forwarder.  This means:
 * - determining the public IP address using UPnP IGD
 * - configuring a port map
 * - announcing ourselves on the forwarder topic
 */
static bool update_locator(igd_t *igd)
{
    bool success = false;

    /* get IGD details */
    if (0 != get_igd_details(igd)) {
        qeo_log_e("failed to get public address using UPnP IGD");
    }
    else if (igd->wan_updated) {
        /* create the port map */
        if (0 != add_portmap(igd)) {
            qeo_log_e("failed to configure port map using UPnP IGD");
        }
        /* configure factory with public address */
        else {
            int port = atoi(igd->wan_port);

            if (QEO_OK != qeocore_fwdfactory_set_public_locator(_factory, igd->wan_ip, port)) {
                qeo_log_e("failed to configure public address");
            }
            else {
                success = true;
            }
        }
    }
    else {
        /* wan ip did not change, nothing to be done */
        success = true;
    }
    return success;
}

/* ===[ Main ]============================================================== */

static void get_public_locator_callback(qeo_factory_t *factory)
{
    /* use preconfigured locator (if any) */
    if (NULL != _public_ip) {
        printf("- Public locator : %s:%d\n", _public_ip, _public_port);
        if (QEO_OK != qeocore_fwdfactory_set_public_locator(_factory, _public_ip, _public_port)) {
            qeo_log_e("failed to configure public address");
        }
        state_change(STATE_RUNNING);
    }
    else {
        state_change(STATE_PUBLIC_IP_REQUESTED);
    }
}

static void update_timer(struct timespec *timer,
                         timer_type_t type,
                         int sec_period)
{
    clock_gettime(CLOCK_REALTIME, &timer[type]);
    timer[type].tv_sec += sec_period;
}

/**
 * For finding the timer that will expire the earliest only the seconds of the
 * timespec are taken into account.
 */
static timer_type_t find_earliest_timer(struct timespec *timer)
{
    struct timespec *ts = NULL;
    timer_type_t earliest = TIMER_NUM;
    int i;

    for (i = 0; i < TIMER_NUM; i++) {
        if (0 == timer[i].tv_sec) {
            continue; /* disabled */
        }
        if ((NULL == ts) || (timer[i].tv_sec < ts->tv_sec)) {
            earliest = i;
            ts = &timer[i];
        }
    }
    return earliest;
}

static state_t state_for_timer(timer_type_t type)
{
    state_t state = STATE_IDLE;

    switch (type) {
        case TIMER_UPNP_POLL:
            state = STATE_WAIT_FOR_UPNP_POLL;
            break;
        case TIMER_NUM:
            /* should never happen */
            assert(false);
            break;
    }
    return state;
}

/* clean up */
static void handle_quit(igd_t *igd)
{
    qeo_factory_close(_factory);
    if (NULL != igd) {
        UPNP_DeletePortMapping(igd->control_url, igd->service_type, igd->wan_port, "TCP", 0);
    }
}


static void state_machine_run(void)
{
    igd_t igd = {0};
    struct timespec *ts = NULL;
    struct timespec timer[TIMER_NUM] = { {0} };
    timer_type_t earliest;
    bool quit = false;
    bool timeout = false;
    int rc;

    qeo_log_i("state machine started");
    pthread_mutex_lock(&_state_mutex);
    while (!quit) {
        /* timer handling */
        timeout = false;
        ts = NULL;
        earliest = find_earliest_timer(timer);
        if (TIMER_NUM != earliest) {
            ts = &timer[earliest];
            _state = state_for_timer(earliest);
        }
        /* wait */
        if (NULL == ts) {
            rc = pthread_cond_wait(&_state_cv, &_state_mutex);
        }
        else {
            rc = pthread_cond_timedwait(&_state_cv, &_state_mutex, ts);
        }
        if (ETIMEDOUT == rc) {
            timeout = true;
        } else if (0 != rc) {
            qeo_log_e("state machine failure");
            break;
        }
        switch (_state) {
            case STATE_IDLE:
                /* nop */
                break;
            case STATE_QUIT:
                quit = true;
                handle_quit(&igd);
                break;
            case STATE_PUBLIC_IP_REQUESTED:
                if (update_locator(&igd)) {
                    update_timer(timer, TIMER_UPNP_POLL, _upnp_poll_period);
                }
                else {
                    /* abort program */
                    quit = true;
                    handle_quit(&igd);
                }
                break;
            case STATE_WAIT_FOR_UPNP_POLL:
                if (timeout) {
                    // TODO LAN IP addresses changes not (yet) taken into account
                    if (!update_locator(&igd)) {
                        qeo_log_e("failed to refresh public locator");
                    }
                    update_timer(timer, TIMER_UPNP_POLL, _upnp_poll_period);
                }
                break;
            case STATE_RUNNING:
                /* nop */
                break;
        }
    }
    pthread_mutex_unlock(&_state_mutex);
    igd_clean(&igd);
    qeo_log_i("state machine stopped");
}

/* ===[ Public API ]======================================================== */

void forwarder_config_public_locator(const char *ip,
                                     int port)
{
    if (NULL != _public_ip) {
        free(_public_ip);
    }
    _public_ip = strdup(ip);
    _public_port = port;
    qeo_log_i("using public IP and port : %s:%d", _public_ip, _public_port);
}

void forwarder_config_local_port(const char *port)
{
    if (NULL != _local_port) {
        free(_local_port);
    }
    _local_port = strdup(port);
    qeo_log_i("using local port : %s", _local_port);
}

void forwarder_config_local_discovery(unsigned int discover_timeout)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "%d", discover_timeout);
    qeocore_parameter_set("FWD_WAIT_LOCAL_FWD", buf);
    qeo_log_i("using local discovery delay : %dms", discover_timeout);
}

void forwarder_config_upnp(unsigned int discover_timeout,
                           unsigned int poll_period,
                           unsigned int lease_period)
{
    _upnp_disc_timeout = discover_timeout;
    _upnp_poll_period = poll_period;
    _upnp_portmap_lease_period = lease_period;
    qeo_log_i("using UPnP timers : discovery delay = %dms, poll period = %ds, lease period = %ds",
              _upnp_disc_timeout, _upnp_poll_period, _upnp_portmap_lease_period);
}

int forwarder_start(void)
{
    int rc = 1;

    /* if no local port was configured, find a free one */
    if (NULL == _local_port) {
        int port;

        port = get_free_tcp_port();
        if (-1 != port) {
            char buf[8];

            snprintf(buf, sizeof(buf), "%d", port);
            _local_port = strdup(buf);
        }
    }
    if (NULL != _local_port) {
        _factory = qeocore_fwdfactory_new(get_public_locator_callback, _local_port);
        if (NULL == _factory) {
            qeo_log_e("failed to create forwarder factory");
        }
        else {
            printf("Starting forwarder\n");
            printf("- Realm ID       : %0"PRIx64"\n", qeocore_factory_get_realm_id(_factory));
            printf("- Local port     : %s\n", _local_port);
            /* start */
            state_machine_run();
            rc = 0;
            printf("Stopping forwarder\n");
        }
    }
    return rc;
}

int forwarder_stop(void)
{
    if (NULL != _public_ip) {
        free(_public_ip);
        _public_ip = NULL;
    }
    if (NULL != _local_port) {
        free(_local_port);
        _local_port = NULL;
    }
    state_change(STATE_QUIT);
    return 0;
}
