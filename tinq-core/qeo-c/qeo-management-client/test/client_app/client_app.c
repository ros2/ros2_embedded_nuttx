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
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>
#include <openssl/pkcs12.h>
#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <qeo/mgmt_client.h>
#include <qeo/mgmt_client_forwarder.h>
#include <qeo/mgmt_cert_parser.h>
#include <jansson.h>
#include "management-client/qeo_mgmt_json_util.h"
#include "keygen.h"

#define OPERATION_FLAG_DEVICE 1
#define OPERATION_FLAG_POLICY 2
#define OPERATION_FLAG_POLICY_CHECK 3
#define OPERATION_FLAG_FWD_REGISTER 4
#define OPERATION_FLAG_FWD_GETLIST 5
#define OPERATION_FLAG_DO_NOTHING 6

static char* pname = NULL;

qeo_mgmt_client_locator_type_t _get_locator_type(const char* stringValue);
const char* _get_locator_string(qeo_mgmt_client_locator_type_t type);

typedef struct
{
    int operationflag;
    EVP_PKEY *pkey;
    X509 *cert;
    STACK_OF(X509) *ca;
    FILE *outputfile;
    int verbose;
    int nrForwarders;
    qeo_mgmt_client_forwarder_t **forwarders;
    qeo_mgmt_client_retcode_t result; /* Used for async testing. */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} _client_cb_helper;

static qeo_mgmt_client_retcode_t my_ssl_cb(SSL_CTX *ctx, void *cookie)
{
    _client_cb_helper *helper = (_client_cb_helper*)cookie;
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    int i = 0;

    if (helper && (helper->operationflag == OPERATION_FLAG_POLICY  || helper->operationflag == OPERATION_FLAG_FWD_REGISTER
            || helper->operationflag == OPERATION_FLAG_FWD_GETLIST)) {

        if (!SSL_CTX_use_certificate(ctx, helper->cert)) {
            fprintf(stderr, "SSL_CTX_use_certificate issue\n");
            goto error;
        }
        if (helper->verbose)
            X509_print_fp(stdout, helper->cert);
        for (i = sk_X509_num(helper->ca) - 1; i >= 0; i--) {
            X509 *cert = sk_X509_value(helper->ca, i);
            if (!SSL_CTX_add_extra_chain_cert(ctx, X509_dup(cert))) {
                fprintf(stderr, "Failed to add additional chain cert\n");
                goto error;
            }
            if (helper->verbose)
                X509_print_fp(stdout, cert);
        }
        if (!SSL_CTX_use_PrivateKey(ctx, helper->pkey)) {
            fprintf(stderr, "SSL_CTX_use_PrivateKey issue\n");
            goto error;
        }

        if (!SSL_CTX_check_private_key(ctx)) {
            fprintf(stderr, "SSL_CTX_check_private_key issue\n");
            goto error;
        }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL );
        SSL_CTX_set_quiet_shutdown(ctx, 1);

        //***To be considered if needed later on***** //
        // SSL_CTX_set_cipher_list(ctx, "RC4-MD5");   //
        // SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);//
        //******************************************* //

        //X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx), sk_X509_value(helper->ca, sk_X509_num(helper->ca)-1) );

    }
    else if (helper && (helper->operationflag == OPERATION_FLAG_DEVICE)) {
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL );
        SSL_CTX_set_quiet_shutdown(ctx, 1);
    }
    ret = QMGMTCLIENT_OK;
    error: return ret;
}
static qeo_mgmt_client_retcode_t my_data_cb(char *ptr, size_t sz, void *cookie)
{
    _client_cb_helper *helper = (_client_cb_helper*)cookie;

    if (!helper || !helper->outputfile)
        return QMGMTCLIENT_EFAIL;
    if (fwrite(ptr, sz, 1, helper->outputfile) <= 0) {
        return QMGMTCLIENT_EFAIL;
    }
    return QMGMTCLIENT_OK;
}

static void _init_lib()
{

    /* Add algorithms and init random pool */
    OpenSSL_add_all_algorithms();
    //ERR_load_crypto_strings();
    SSL_library_init();
}

static void _exit_lib()
{
    //http://www.openssl.org/support/faq.html#PROG13
    ERR_remove_thread_state(NULL);
    ENGINE_cleanup();
    CONF_modules_unload(1);
    ERR_free_strings();
    OBJ_cleanup();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
}

static void usage()
{
    fprintf(stdout,
            "Usage: %s OPERATION [OPTIONS]\n"
            "\nAvailable OPERATIONs are\n"
            "  device            Get a device certificate\n"
            "  policy            Fetch a policy file from the server\n"
            "  check_policy      Check whether the policy file based on a sequence nr is still up to date\n"
            "  register_fwd      Registers the device (retrieved as json from -f <file>) as a forwarder to the management server\n"
            "  list_fwd          Retrieves the list of forwarders from the management server and write it to -f <file>\n"
            "\nOPTIONS\n"
            "  -u <url>          SCEP server URL (without the scep suffix)\n"
            "  -o <otp>          One time password to use (for device) \n"
            "  -d <device>       String identifying the device (for device) \n"
            "  -c <file>         PKCS#12 file that contains the certificates and the private key (write in case of device)\n"
            "  -p <psswd>        Password needed to read the pkcs#12 file\n"
            "  -f <file>         Output/Input file to write the results to or retrieve input from\n"
            "  -m <multiplicity> How many times we have to repeat the action\n"
            "  -s <sequencenr>   Sequence number used for policy_check\n"
            "  -r <realmid>      Realmid to use"
            "  -v                Verbose operation\n",
            pname);
}

void print_cert(X509* cert, FILE *fp, char* prefix)
{
    char buffer[4096];
    fprintf(fp, "%s: Printing out certificate with\n"
            "  subject: '%s'\n",
            prefix, X509_NAME_oneline(X509_get_subject_name(cert), buffer, sizeof(buffer)));
    fprintf(fp, "  issuer: %s\n", X509_NAME_oneline(X509_get_issuer_name(cert), buffer, sizeof(buffer)));
}

qeo_mgmt_client_retcode_t my_fwd_cb(qeo_mgmt_client_forwarder_t* forwarder, void *cookie) {
    _client_cb_helper *cb_helper = (_client_cb_helper *) cookie;

    cb_helper->nrForwarders++;
    cb_helper->forwarders=realloc(cb_helper->forwarders, sizeof(qeo_mgmt_client_forwarder_t*)*cb_helper->nrForwarders);
    cb_helper->forwarders[cb_helper->nrForwarders-1]=forwarder;
    return QMGMTCLIENT_OK;
}

void my_result_cb(qeo_mgmt_client_retcode_t result, void *cookie) {
    _client_cb_helper *helper = (_client_cb_helper*)cookie;
    printf("my_fwd_result_cb called\n");
    helper->result = result;
    pthread_mutex_lock(&helper->mutex);
    pthread_cond_signal(&helper->cond);
    pthread_mutex_unlock(&helper->mutex);
}

static int _setup_client_authentication(_client_cb_helper *cb_helper, char *pkcs12file, char *password){
    FILE *fp = NULL;
    int rc = -1;
    PKCS12* p12 = NULL;
    if (!(fp = fopen(pkcs12file, "rb"))) {
        fprintf(stderr, "Error opening file %s\n", pkcs12file);
        goto exit;
    }

    p12 = d2i_PKCS12_fp(fp, NULL );

    if (!p12) {
        fprintf(stderr, "Error reading PKCS#12 file\n");
        goto exit;
    }

    if (!PKCS12_parse(p12, password, &(cb_helper->pkey), &(cb_helper->cert), &(cb_helper->ca))) {
        fprintf(stderr, "Error parsing PKCS#12 file \n");
        goto exit;
    }
    rc = 0;
exit:
    if (p12) PKCS12_free(p12);
    if (fp) fclose(fp);
    return rc;
}


static void _free_client_authentication(_client_cb_helper *cb_helper){
    if (cb_helper->pkey) EVP_PKEY_free(cb_helper->pkey);
    if (cb_helper->cert) X509_free(cb_helper->cert);
    if (cb_helper->ca) sk_X509_pop_free(cb_helper->ca, X509_free);
    cb_helper->pkey=NULL;
    cb_helper->cert=NULL;
    cb_helper->ca=NULL;
}

/*
 * {
 *   "forwarders": [
 *     {
 *       "id": "device_id",
 *       "locators": [
 *         {
 *           "type": "TCPv4",
 *           "address": "212.118.224.153",
 *           "port": 7400
 *         }
 *       ]
 *     }
 *   ]
 * }
 */
static void _write_forwarders_to_file(const char *file, qeo_mgmt_client_forwarder_t **forwarders, int nrForwarders){
    int i = 0, j = 0;
    //TODO: should there be a limitation on the number of locators that can be registered?
    json_t* msg = NULL;
    json_t* forwarder_array = json_array();
    json_error_t json_error = {0};

    for (i = 0; i < nrForwarders; i++) {
        json_t* locators = json_array();
        for (j = 0; j < forwarders[i]->nrOfLocators; j++) {
            qeo_mgmt_client_locator_t *locator = &forwarders[i]->locators[j];
            const char* typeString = _get_locator_string(locator->type);
            json_t* json_locator = json_pack_ex(&json_error, 0, "{sssssi}", "type", typeString, "address",
                                                locator->address, "port", (int)locator->port);
            if (!json_locator) fprintf(stderr, "Failed to pack json locator %s (%s:%d:%d)\n", json_error.text, json_error.source, json_error.line, json_error.column);

            json_array_append_new(locators, json_locator);
        }
        char devicestring[64];
        sprintf(devicestring, "%" PRIx64 "", forwarders[i]->deviceID);
        json_t* json_forwarder = json_pack_ex(&json_error, 0, "{ssso}", "id", devicestring, "locators", locators);
        if (!json_forwarder) fprintf(stderr, "Failed to pack json forwarder %s (%s:%d:%d)\n", json_error.text, json_error.source, json_error.line, json_error.column);
        json_array_append_new(forwarder_array, json_forwarder);
    }

    msg = json_pack_ex(&json_error, 0, "{so}", "forwarders", forwarder_array);
    if (!msg) fprintf(stderr, "Failed to pack msg %s (%s:%d:%d)\n", json_error.text, json_error.source, json_error.line, json_error.column);

    if (json_dump_file(msg, file, JSON_INDENT(2)) == -1) {
        fprintf(stderr, "Failed to write forwarders json to file");
        exit(-1);
    }
    json_decref(msg);
}

static qeo_mgmt_client_locator_t *_read_locators_from_file(const char *file, int *nrLocators){
    json_error_t json_error = {0};
    json_t *json = json_load_file(file, JSON_REJECT_DUPLICATES, &json_error);
    int rc = 0;
    json_t *json_locators = NULL;
    int i = 0;
    qeo_mgmt_client_locator_t *locators = NULL;

    if (!json){
        fprintf(stderr, "Failed to parse json message %s (%s:%d:%d)\n", json_error.text, json_error.source, json_error.line, json_error.column);
        exit(-1);
    }
    rc = json_unpack_ex(json, &json_error, 0, "{so}", "locators", &json_locators);
    if (0 != rc) {
        fprintf(stderr, "Unable to parse locators:%s\n", json_error.text);
        exit(-1);
    }
    *nrLocators = json_array_size(json_locators);
    if (*nrLocators == 0) {
        return locators;
    }
    locators = calloc(*nrLocators, sizeof(qeo_mgmt_client_locator_t));
    for (i = 0; i < *nrLocators; ++i) {
        char *type = NULL;
        int port;
        rc = json_unpack_ex(json_array_get(json_locators, i), &json_error, 0, "{sssssi}", "type", &type, "address",&(locators[i].address), "port", &port);
        if (0 != rc) {
            fprintf(stderr, "Unable to parse locator:%s\n", json_error.text);
            exit(-1);
        }

        locators[i].address = strdup(locators[i].address);
        locators[i].type = _get_locator_type(type);
        locators[i].port = port;
    }
    json_decref(json);
    return locators;
}

static void _free_locators(qeo_mgmt_client_locator_t *locators, int nrlocators){
    int i = 0;

    for (i = 0; i < nrlocators; ++i) {
        free(locators[i].address);
    }
    free(locators);
}

int main(int argc, char **argv)
{
    int c;
    char *url = NULL, *otp = NULL, *device = NULL, *pkcs12file = NULL, *password = NULL, *file = NULL;
    int64_t realmid = 0;
    int64_t sequencenr = 0;
    FILE *fp = NULL;
    int operation_type = 0, verbose = 0;
    int multiplicity = 1;
    qeo_platform_device_info info;
    qeo_mgmt_client_ctx_t *ctx = NULL;
    STACK_OF(X509) *certs = sk_X509_new(NULL);
    X509 *cert = NULL;
    int i = 0;
    qeo_mgmt_client_retcode_t ret = QMGMTCLIENT_EFAIL;
    _client_cb_helper cb_helper = { 0 };
    int rc = -1;
    bool helperinit = false;
    EVP_PKEY *rsa = NULL;
    PKCS12 *p12 = NULL;

    /* Initialize scep layer */
    _init_lib();

    /* Set program name */
    pname = argv[0];

    /* Check operation parameter */
    if (!argv[1]) {
        usage();
        rc=0;
        goto exit;
    }
    else if (!strncmp(argv[1], "device", 7)) {
        operation_type = OPERATION_FLAG_DEVICE;
    }
    else if (!strncmp(argv[1], "policy", 7)) {
        operation_type = OPERATION_FLAG_POLICY;
    }
    else if (!strncmp(argv[1], "check_policy", 13)) {
        operation_type = OPERATION_FLAG_POLICY_CHECK;
    }
    else if (!strncmp(argv[1], "register_fwd", 13)) {
        operation_type = OPERATION_FLAG_FWD_REGISTER;
    }
    else if (!strncmp(argv[1], "list_fwd", 9)) {
        operation_type = OPERATION_FLAG_FWD_GETLIST;
    }
    else if (!strncmp(argv[1], "do_nothing", 10)) {
        operation_type = OPERATION_FLAG_DO_NOTHING;
    }
    else {
        fprintf(stderr, "%s: missing or illegal operation parameter\n", pname);
        usage();
        goto exit;
    }
    /* Skip first parameter and parse the rest of the command */
    optind++;
    while ((c = getopt(argc, argv, "s:r:o:d:u:c:p:f:m:hv")) != -1)
        switch (c) {
            case 'r':
                if (sscanf(optarg, "%" PRIx64, &realmid) != 1){
                    fprintf(stderr, "%s: could not parse realmid based on <%s>\n", pname, optarg);
                    exit(-1);
                }
                break;
            case 's':
                if (sscanf(optarg, "%" PRId64, &sequencenr) != 1){
                    fprintf(stderr, "%s: could not parse sequencenr based on <%s>\n", pname, optarg);
                    exit(-1);
                }
                break;
            case 'o':
                otp = optarg;
                break;
            case 'd':
                device = optarg;
                break;
            case 'u':
                url = optarg;
                break;
            case 'c':
                pkcs12file = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            case 'f':
                file = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'm':
                multiplicity = strtol(optarg, NULL, 10);
                break;
            case 'h':
            default:
                printf("argv: %s\n", argv[optind]);
                usage();
        }
    argc -= optind;
    argv += optind;

    cb_helper.operationflag = operation_type;
    cb_helper.verbose = verbose;
    cb_helper.result = QMGMTCLIENT_EFAIL;
    pthread_mutex_init(&cb_helper.mutex, NULL);
    pthread_cond_init(&cb_helper.cond, NULL);
    helperinit = true;

    if (verbose)
        fprintf(stdout, "%s: starting\n", pname);

    ctx = qeo_mgmt_client_init();
    if (ctx == NULL ) {
        fprintf(stderr, "%s: Failed to create management client\n", pname);
        goto exit;
    }

    if ((operation_type == OPERATION_FLAG_POLICY) || (operation_type == OPERATION_FLAG_FWD_GETLIST) || (operation_type == OPERATION_FLAG_FWD_REGISTER)){
        rc = _setup_client_authentication(&cb_helper, pkcs12file, password);
        if (rc != 0){
            goto exit;
        }
    }

    if (operation_type == OPERATION_FLAG_POLICY){
        if (!(cb_helper.outputfile = fopen(file, "w"))) {
            fprintf(stderr, "Error opening file %s for writing\n", file);
            goto exit;
        }
    }
    if (operation_type == OPERATION_FLAG_DEVICE){
        rsa = keygen_create(1024);
    }

    while (multiplicity > 0) {
        switch (operation_type) {
            case OPERATION_FLAG_DEVICE: {
                qeo_mgmt_cert_contents device_cert_info = { -1, -1, -1 };

                if (rsa == NULL ) {
                    fprintf(stderr, "%s: Failed to load private key file.\n", pname);
                    goto exit;
                }

                memset(&info, 0, sizeof(info));
                info.userFriendlyName = device;

                ret = qeo_mgmt_client_enroll_device(ctx, url, rsa, otp, &info, my_ssl_cb, &cb_helper, certs);
                if (ret != QMGMTCLIENT_OK) {
                    fprintf(stderr, "%s: Failed to enroll device (rc=%d).\n", pname, (int) ret);
                    rc = ret;
                    goto exit;
                }
                fprintf(stdout, "%s: Printing out the stack...\n", pname);
                for (i = 0; i < sk_X509_num(certs); i++) {
                    print_cert(sk_X509_value(certs, i), stdout, pname);
                }

                if (qeo_mgmt_cert_parse(certs, &device_cert_info) != QCERT_OK) {
                    fprintf(stderr, "%s: Failed to retrieve ids from device cert.\n", pname);
                    goto exit;
                }
                fprintf(stdout, "%s: Realm=0x%x (%d), Device=0x%x (%d) and User=0x%x (%d)\n", pname,
                        (unsigned int)device_cert_info.realm, (int)device_cert_info.realm,
                        (unsigned int)device_cert_info.device, (int)device_cert_info.device,
                        (unsigned int)device_cert_info.user, (int)device_cert_info.user);
                //Remove the device certificate from the stack
                cert = sk_X509_delete(certs, 0);
                if (cert == NULL ) {
                    fprintf(stderr, "%s: Failed to pop device certificate\n", pname);
                    goto exit;
                }
                p12 = PKCS12_create(password, "devicePrivate", rsa, cert, certs, 0, 0, 0, 0, 0);
                X509_free(cert);
                if (p12 == NULL ) {
                    fprintf(stderr, "%s: Failed to create pkcs12 file (%s)\n", pname,
                            ERR_error_string(ERR_get_error(), NULL ));
                    goto exit;
                }
                if (!(fp = fopen(pkcs12file, "w"))) {
                    fprintf(stderr, "%s: cannot open "
                            "file for writing\n",
                            pname);
                    goto exit;
                }
                if (i2d_PKCS12_fp(fp, p12) != 1) {
                    fprintf(stderr, "%s: error while "
                            "writing pkcs12 file\n",
                            pname);
                    ERR_print_errors_fp(stderr);
                    goto exit;
                }

            }
                break;
            case OPERATION_FLAG_POLICY: {
                ret = qeo_mgmt_client_get_policy(ctx, url, my_ssl_cb, my_data_cb, &cb_helper);
                if (ret != QMGMTCLIENT_OK) {
                    fprintf(stderr, "%s: Failed to retrieve policy file (rc=%d).\n", pname, (int) ret);
                    rc = ret;
                    goto exit;
                }
            }
                break;
            case OPERATION_FLAG_POLICY_CHECK: {
                bool result = false;
                ret = qeo_mgmt_client_check_policy(ctx, my_ssl_cb, &cb_helper, url, sequencenr, realmid, &result);
                if (ret != QMGMTCLIENT_OK) {
                    fprintf(stderr, "%s: Failed to check for policy file (rc=%d).\n", pname, (int) ret);
                    rc = ret;
                    goto exit;
                }
                printf("%s: Check for policy file status indicates that %s.\n", pname, (result == true) ? "it is still valid" : "it is not valid anymore");
                if (result == false){
                    rc = 66;
                    goto exit;
                }
            }
                break;
            case OPERATION_FLAG_FWD_REGISTER: {
                int nrLocators = 0;
                qeo_mgmt_client_locator_t *locators = _read_locators_from_file(file, &nrLocators);

                ret = qeo_mgmt_client_register_forwarder(ctx, url, locators, nrLocators, my_ssl_cb, &cb_helper);
                _free_locators(locators, nrLocators);
                if (ret != QMGMTCLIENT_OK) {
                    fprintf(stderr, "%s: Failed to register forwarder (rc=%d).\n", pname, (int) ret);
                    rc = ret;
                    goto exit;
                }
            }
                break;
            case OPERATION_FLAG_FWD_GETLIST: {
                cb_helper.nrForwarders = 0;
                cb_helper.forwarders = NULL;

                pthread_mutex_lock(&cb_helper.mutex);
                ret = qeo_mgmt_client_get_forwarders(ctx, url, my_fwd_cb, my_result_cb, &cb_helper, my_ssl_cb, &cb_helper);
                if (ret != QMGMTCLIENT_OK) {
                    fprintf(stderr, "%s: Failed to retrieve forwarders file (rc=%d).\n", pname, (int) ret);
                    rc = ret;
                    goto exit;
                }
                pthread_cond_wait(&cb_helper.cond, &cb_helper.mutex);
                pthread_mutex_unlock(&cb_helper.mutex);
                ret = cb_helper.result;
                if (ret != QMGMTCLIENT_OK) {
                    fprintf(stderr, "%s: Error retrieving forwarders file (rc=%d).\n", pname, (int)ret);
                    rc = ret;
                    goto exit;
                }

                _write_forwarders_to_file(file, cb_helper.forwarders, cb_helper.nrForwarders);
                for (i = 0; i < cb_helper.nrForwarders; ++i) {
                    qeo_mgmt_client_free_forwarder(cb_helper.forwarders[i]);
                }
                free(cb_helper.forwarders);
            }
                break;
            case OPERATION_FLAG_DO_NOTHING:
                break;
            default:
                fprintf(stderr, "%s: Operation not supported.\n", pname);
                goto exit;
                break;
        }
        multiplicity--;
     }
    rc = 0;
exit:
    if (ctx != NULL ) {
        qeo_mgmt_client_clean(ctx);
    }
    if (rc == 0) fprintf(stdout, "%s: SUCCESS\n", pname);

    if (helperinit){
        _free_client_authentication(&cb_helper);
        if (cb_helper.outputfile) fclose(cb_helper.outputfile);
        pthread_mutex_destroy(&cb_helper.mutex);
        pthread_cond_destroy(&cb_helper.cond);
    }
    sk_X509_pop_free(certs, X509_free);
    if (rsa){
        EVP_PKEY_free(rsa);
    }
    if (p12){
        PKCS12_free(p12);
    }
    if (fp){
        fclose(fp);
    }
    _exit_lib();
    exit(rc);
}

