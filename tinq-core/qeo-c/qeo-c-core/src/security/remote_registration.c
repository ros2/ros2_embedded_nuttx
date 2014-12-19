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

/*#######################################################################
 # HEADER (INCLUDE) SECTION                                              #
 ########################################################################*/
#ifndef DEBUG
#define NDEBUG
#endif

#include <dds/dds_xtypes.h>
#include <dds/dds_aux.h>
#include "qeocore/remote_registration.h"
#include "core_util.h"
#include "remote_registration.h"
#include "qdm/qeo_RegistrationRequest.h"
#include "qdm/qeo_RegistrationCredentials.h"
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <qeo/log.h>
#include "core.h"
#include <assert.h>
#include "utlist.h"
/*#######################################################################
 # TYPES SECTION                                                         #
 ########################################################################*/
#define QEO_REMOTE_REGISTRATION_MAGIC    0xdeadcafe
#define OTC_ENCRYPTION_VERSION 1

struct qeo_remote_registration_s {
#ifndef NDEBUG
    unsigned long                         magic;
#endif
    qeo_remote_registration_cfg_t         cfg;
    org_qeo_system_RegistrationRequest_t  reg_request;
    DDS_Timer                             timer;
    bool                                  started; /* indicates if registration window is open */
    bool                                  reg_cred_in_use; /* indicates whether some given credentials are being used for registration or not*/
    struct qeo_remote_registration_s      *next;
};

/*#######################################################################
 # STATIC FUNCTION DECLARATION                                           #
 ########################################################################*/
static void on_reg_cred_update(const qeocore_reader_t *reader,
                               const qeocore_data_t   *data,
                               uintptr_t              userdata);

/*#######################################################################
 # STATIC VARIABLE SECTION                                               #
 ########################################################################*/
static bool                               _initialized;
static const qeo_platform_device_info           *_device_info;
static qeocore_writer_t                   *_reg_req_writer;
static qeocore_reader_t                   *_reg_cred_reader;
static qeocore_type_t                     *_reg_req_type;
static qeocore_type_t                     *_reg_cred_type;
static struct qeo_remote_registration_s   *_reg_list;
static qeo_remote_registration_init_cfg_t _cfg;
static EVP_PKEY * _key = NULL;
static pthread_cond_t _key_cond;

#ifdef __USE_GNU
/* compile with -D_GNU_SOURCE */
#ifndef NDEBUG
static pthread_mutex_t _mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#else
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
#else
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static const qeocore_reader_listener_t _listener =
{
    .on_data          = on_reg_cred_update,
    .on_policy_update = NULL,
    .userdata         = 0
};

/*#######################################################################
 # STATIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/
#ifndef NDEBUG
static ptrdiff_t cmp_shallow(const struct qeo_remote_registration_s *p1,
                             const struct qeo_remote_registration_s *p2)
{
    return p1 - p2;
}
#endif

#if 0
static int cmp_by_device_id(const struct qeo_remote_registration_s  *p1,
                            const struct qeo_remote_registration_s  *p2)
{
    if ((p1->cfg.device_info->qeoDeviceId.lowerId == p2->cfg.device_info->qeoDeviceId.lowerId) &&
        (p1->cfg.device_info->qeoDeviceId.upperId == p2->cfg.device_info->qeoDeviceId.upperId)) {
        return 0;
    }
    return 1;
}
#endif

static void print_crypto_error(void)
{
    char err[256];

    ERR_load_crypto_strings();
    ERR_error_string(ERR_get_error(), err);
    qeo_log_e("Error decrypting otp: %s\n", err);
}

static bool is_valid_remote_reg_cfg(const qeo_remote_registration_cfg_t *cfg)
{
    if (cfg->pkey == NULL) {
        return false;
    }

    return true;
}

#ifndef NDEBUG
static bool is_valid_remote_reg_obj(const struct qeo_remote_registration_s *remote_reg)
{
    qeo_remote_registration_hndl_t el = NULL;

    if (remote_reg->magic != QEO_REMOTE_REGISTRATION_MAGIC) {
        qeo_log_e("*** 1 ***");
        return false;
    }

    /* see if it is in the list */
    LL_SEARCH(_reg_list, el, remote_reg, cmp_shallow);
    if (el != remote_reg) {
        qeo_log_e("*** 2 ***");
        return false;
    }

    if (is_valid_remote_reg_cfg(&remote_reg->cfg) == false) {
        qeo_log_e("*** 3 ***");
        return false;
    }

    if (!((_device_info->qeoDeviceId.upperId == remote_reg->reg_request.deviceId.upper) &&
          (_device_info->qeoDeviceId.lowerId == remote_reg->reg_request.deviceId.lower))) {
        qeo_log_e("*** 4 ***");
        return false;
    }

    return true;
}
#endif

#if 0
static bool encrypt(EVP_PKEY        *pkey,
                    const char      *otp,
                    qeo_sequence_t  *seq)
{
    char encrypted_otp[EVP_PKEY_size(size)];

    int len = RSA_public_encrypt(strlen(otp), otp, encrypted_otp, pkey->pkey.rsa, RSA_PKCS1_OAEP_PADDING);

    if (len == -1) {
        print_crypto_error();
        return false;
    }
    if (dds_seq_from_array(seq, encrypted_otp, sizeof(encrypted_otp) / sizeof(encrypted_otp[0])) != DDS_RETCODE_OK) {
        qeo_log_e("Could not copy encrypted OTP into sequence");
        return false;
    }

    return true;
}
#endif

/* allocates memory ! */
static char *decrypt(EVP_PKEY                                                     *pkey,
                     unsigned char * encrypted_data,
                     int encrypted_length)
{
    unsigned char decrypted_data[EVP_PKEY_size(pkey)];
    char          *decrypted_data_string;

    int len = RSA_private_decrypt(encrypted_length, encrypted_data, decrypted_data, pkey->pkey.rsa, RSA_PKCS1_OAEP_PADDING);
    if (len == -1) {
        print_crypto_error();
        return NULL;
    }
    
    /* do not use strdup here as 'otp' is not NULL-terminated! */
    decrypted_data_string = malloc(len + 1);
    if (decrypted_data_string == NULL){
        qeo_log_e("no mem");
        return NULL;
    }

    memcpy(decrypted_data_string, decrypted_data, len);
    decrypted_data_string[len] = '\0';
    
    return decrypted_data_string;
}

/* allocates memory ! */
static char *decrypt_otp_seq(EVP_PKEY                                                     *pkey,
                     org_qeo_system_RegistrationCredentials_encryptedOtc_seq  *encrypted_otp_seq)
{
    int encrypted_length = DDS_SEQ_LENGTH(*encrypted_otp_seq);
    unsigned char        encrypted_otp[encrypted_length];

    dds_seq_to_array(encrypted_otp_seq, encrypted_otp, encrypted_length); /* values now holds a copy of the sequence contents */
    return decrypt(pkey, encrypted_otp, encrypted_length);
}

static void on_reg_cred_update(const qeocore_reader_t *reader,
                               const qeocore_data_t   *data,
                               uintptr_t              userdata)
{
    qeo_remote_registration_hndl_t            remote_reg      = NULL;
    org_qeo_system_RegistrationCredentials_t  *reg_cred       = NULL;
    char                                      *decrypted_otp  = NULL;
    char                                      *url            = NULL;
    char                                      *realm_name     = NULL;

    if (qeocore_data_get_status(data) != QEOCORE_DATA) {
        return;
    }

    reg_cred = (org_qeo_system_RegistrationCredentials_t *)qeocore_data_get_data(data);
    assert(reg_cred != NULL);

    lock(&_mutex);
    do {
        /* For now, no need to look in the list: there will only be max 1 remote reg object */
        if ((_reg_list != NULL) &&
            (reg_cred->deviceId.lower == _device_info->qeoDeviceId.lowerId) &&
            (reg_cred->deviceId.upper == _device_info->qeoDeviceId.upperId) &&
            (strcmp(_reg_list->reg_request.rsaPublicKey, reg_cred->requestRSAPublicKey) == 0)) {
            remote_reg = _reg_list;
        }

        if (remote_reg == NULL) {
            qeo_log_d("Update not for us");
            break;
        }
        assert(is_valid_remote_reg_obj(remote_reg));

        if (true == remote_reg->reg_cred_in_use) {
            qeo_log_i("Previous registration credentials are in use already...ignoring newly received ones: ");
            qeo_log_i("Ignored registration credentials are for realm: %s", reg_cred->realmName);
            break;
        }

        if (remote_reg->started == false) {
            qeo_log_w("registration window is closed");
            break;
        }

        url           = reg_cred->url;
        realm_name    = reg_cred->realmName;
        decrypted_otp = decrypt_otp_seq(remote_reg->cfg.pkey, &reg_cred->encryptedOtc);
        if (decrypted_otp == NULL) {
            qeo_log_e("No mem");
            break;
        }

        remote_reg->reg_cred_in_use = true;

        unlock(&_mutex);
        qeo_log_d("Received OTP: %s, realm name: %s, URL: %s", decrypted_otp, realm_name, url);
        _cfg.on_qeo_registration_credentials(remote_reg, decrypted_otp, realm_name, url);
        lock(&_mutex);
    } while (0);
    unlock(&_mutex);

    free(decrypted_otp);
}

static void close_reader_and_writer(void)
{
    if (_reg_req_writer != NULL) {
        core_delete_writer(_reg_req_writer, true);
        _reg_req_writer = NULL;
    }
    if (_reg_cred_reader != NULL) {
        core_delete_reader(_reg_cred_reader, true);
        _reg_cred_reader = NULL;
    }

    qeocore_type_free(_reg_cred_type);
    _reg_cred_type = NULL;

    qeocore_type_free(_reg_req_type);
    _reg_req_type = NULL;
}

static bool create_reader_writer(void)
{
    qeo_factory_t *open_domain_factory = NULL;
    bool          retval = false;

    if (_reg_req_type != NULL) {
        return true;
    }


    do {
        open_domain_factory = core_get_open_domain_factory();
        if (open_domain_factory == NULL) {
            qeo_log_e("Could not get open domain factory");
            break;
        }

        _reg_req_type = qeocore_type_register_tsm(open_domain_factory, org_qeo_system_RegistrationRequest_type, org_qeo_system_RegistrationRequest_type->name);
        if (_reg_req_type == NULL) {
            qeo_log_e("Could not register registration request type");
            break;
        }

        _reg_cred_type = qeocore_type_register_tsm(open_domain_factory, org_qeo_system_RegistrationCredentials_type, org_qeo_system_RegistrationCredentials_type->name);
        if (_reg_cred_type == NULL) {
            qeo_log_e("Could not register registration credentials type");
            break;
        }

        _reg_req_writer = core_create_writer(open_domain_factory, _reg_req_type, NULL,
                                             QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, NULL, NULL);
        if (_reg_req_writer == NULL) {
            qeo_log_e("Could not open registration request writer");
            break;
        }

        _reg_cred_reader = core_create_reader(open_domain_factory, _reg_cred_type, NULL,
                                              QEOCORE_EFLAG_STATE_DATA | QEOCORE_EFLAG_ENABLE, &_listener, NULL);
        if (_reg_cred_reader == NULL) {
            qeo_log_e("Could not open registration request reader");
            break;
        }
        retval = true;
    } while (0);

    if (retval == false) {
        close_reader_and_writer();
    }

    return retval;
}

/* allocates memory !! */
static char *public_key_to_pem(EVP_PKEY *key)
{
    BIO   *bio  = NULL;
    char  *ret  = NULL;
    char  *tmp  = NULL;
    int   len;

    do {
        bio = BIO_new(BIO_s_mem());
        if (!bio) {
            qeo_log_e("BIO_new failed");
            break;
        }

        if (PEM_write_bio_PUBKEY(bio, key) != 1) {
            qeo_log_e("Could not write public key to BIO");
            break;
        }

        len = BIO_get_mem_data(bio, &tmp);
        if (len <= 0) {
            qeo_log_e("Cannot get memdata");
            break;
        }

        /* don't use strdup() here.. */
        ret = malloc(len + 1);
        if (ret == NULL) {
            qeo_log_e("nomem (%u bytes)", len);
            break;
        }
        memcpy(ret, tmp, len + 1);
        ret[len] = '\0';
    } while (0);

    BIO_free(bio);

    return ret;
}

static EVP_PKEY *pem_to_public_key(const char *key)
{
    BIO       *bio  = NULL;
    EVP_PKEY  *ret  = NULL;

    do {
        bio = BIO_new(BIO_s_mem());
        if (!bio) {
            qeo_log_e("BIO_new failed");
            break;
        }

        if (0 >= BIO_puts(bio, key)) {
            qeo_log_e("BIO puts failed");
            break;
        }

        ret = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
        if (NULL == ret) {
            print_crypto_error();
            qeo_log_e("Read public key from PEM failed");
            break;
        }

    } while (0);

    if (NULL != bio) {
        BIO_free(bio);
    }

    return ret;
}

static void dump_reg_request(const org_qeo_system_RegistrationRequest_t *reg_request)
{
    qeo_log_d("Writing %" PRIx64 ":%" PRIx64 "\r\n%s,%s,%s,%s %d \r\n%s", reg_request->deviceId.upper, reg_request->deviceId.lower,
              reg_request->manufacturer, reg_request->modelName, reg_request->userFriendlyName, reg_request->userName,
              reg_request->registrationStatus,
              reg_request->rsaPublicKey);
}

static bool publish_registration_request(qeo_remote_registration_hndl_t           remote_reg,
                                         qeo_remote_registration_status_t         status,
                                         qeo_remote_registration_failure_reason_t failure_reason)
{
    bool          retval = false;
    qeo_retcode_t ret;

    if (status == remote_reg->reg_request.registrationStatus){
        return true; /* ignore */
    }

    remote_reg->reg_request.registrationStatus  = status;
    remote_reg->reg_request.errorCode      = failure_reason;
    remote_reg->reg_request.errorMessage        = NULL; /* not sure if NULL pointers work actually */

    do {
        ret = qeocore_writer_write(_reg_req_writer, &remote_reg->reg_request);
        if (ret != QEO_OK) {
            qeo_log_e("Writing of registration request failed\r\n");
            break;
        }

        dump_reg_request(&remote_reg->reg_request);

        retval = true;
    } while (0);

    return retval;
}

static void at_timer_expiration(uintptr_t userdata)
{
    qeo_log_d("Timer has expired (%p)", userdata);

    qeo_remote_registration_hndl_t remote_reg = (qeo_remote_registration_hndl_t)userdata;
    _cfg.on_qeo_registration_timeout(remote_reg);
}

static void free_remote_reg(qeo_remote_registration_hndl_t remote_reg)
{
    if (remote_reg == NULL){
        return;
    }
    free(remote_reg->reg_request.rsaPublicKey);
    DDS_ReturnCode_t ddsret = DDS_Timer_delete(remote_reg->timer);
    if (ddsret != DDS_RETCODE_OK){
        qeo_log_e("Could not delete timer (%d)", ddsret);
    }

#ifndef NDEBUG
    memset(remote_reg, 0xAA, sizeof(*remote_reg));
#endif

    free(remote_reg);
}

static bool stop_timer(qeo_remote_registration_hndl_t remote_reg)
{
    DDS_ReturnCode_t ddsret = DDS_Timer_stop(remote_reg->timer);

    if (ddsret != DDS_RETCODE_OK){
        qeo_log_e("Could not stop timer: %s", strerror(errno));
        return false;
    }

    return true;
}

/*#######################################################################
 # PUBLIC FUNCTION IMPLEMENTATION                                        #
 ########################################################################*/


qeo_retcode_t qeo_remote_registration_init(const qeo_remote_registration_init_cfg_t *cfg)
{
    qeo_retcode_t ret = QEO_EFAIL;

    lock(&_mutex);
    do {
        if (_initialized == true) {
            ret = QEO_OK;
            break;
        }

        if ((_device_info = qeo_platform_get_device_info()) == NULL) {
            ret = QEO_EFAIL;
            qeo_log_e("Could not retrieve device info");
            break;
        }

        memcpy(&_cfg, cfg, sizeof(_cfg));
        _initialized = true;

        ret = QEO_OK;
    } while (0);

    unlock(&_mutex);


    return ret;
}

void qeo_remote_registration_destroy(void)
{
    qeo_remote_registration_hndl_t  tmp;
    unsigned int                    count = 0;

    lock(&_mutex);
    LL_COUNT(_reg_list, tmp, count);
    if (count != 0) {
        qeo_log_w("Still %u remote registration objects !", count);
    }
    _initialized = false;
    unlock(&_mutex);
}

qeo_retcode_t qeo_remote_registration_construct(const qeo_remote_registration_cfg_t *cfg,
                                                qeo_remote_registration_hndl_t      *premote_reg)
{
    qeo_retcode_t                   ret = QEO_EFAIL;
    qeo_remote_registration_hndl_t  remote_reg = NULL;
    qeo_remote_registration_hndl_t  tmp;
    unsigned int                    count = 0;

    if ((cfg == NULL) || (premote_reg == NULL)) {
        return QEO_EINVAL;
    }

    if (is_valid_remote_reg_cfg(cfg) == false) {
        return QEO_EINVAL;
    }

    lock(&_mutex);
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }

        *premote_reg = remote_reg = calloc(1, sizeof(*remote_reg));
        if (*premote_reg == NULL) {
            ret = QEO_ENOMEM;
            break;
        }


#ifndef NDEBUG
        remote_reg->magic = QEO_REMOTE_REGISTRATION_MAGIC;
#endif
        memcpy(&remote_reg->cfg, cfg, sizeof(*cfg));

        remote_reg->reg_request = (org_qeo_system_RegistrationRequest_t) {
            .deviceId.upper   = _device_info->qeoDeviceId.upperId,
            .deviceId.lower   = _device_info->qeoDeviceId.lowerId,
            .version          = OTC_ENCRYPTION_VERSION,
            .manufacturer     = (char *)_device_info->manufacturer,
            .modelName        = (char *)_device_info->modelName,
            .userFriendlyName = (char *)_device_info->userFriendlyName,
            .userName         = remote_reg->cfg.suggested_username,
            .registrationStatus = -1 /* initialize to an invalid state */
        };

        if ((remote_reg->reg_request.rsaPublicKey = public_key_to_pem(remote_reg->cfg.pkey)) == NULL) {
            qeo_log_e("Could not convert to PEM");
            break;
        }

        /* possible alternative is to use DDS-timers */
        char timer_name[32];
        snprintf(timer_name, sizeof(timer_name), "rr_%p", remote_reg);
        if ((remote_reg->timer = DDS_Timer_create(timer_name)) == NULL){
            qeo_log_e("Could not create timer");
            break;
        }

        /* create reader and writer in case this is the first remote registration object (see also DE2894) */
        LL_COUNT(_reg_list, tmp, count);
        if (count == 0) {
            if (create_reader_writer() == false) {
                ret = QEO_EFAIL;
                qeo_log_e("Could not create readers and writers");
                break;
            }
        }

        /* prepend is O(1), append is O(n) */
        LL_PREPEND(_reg_list, remote_reg);

        assert(is_valid_remote_reg_obj(remote_reg));

        ret = QEO_OK;
    } while (0);

    if (ret != QEO_OK) {
        free_remote_reg(remote_reg);
        *premote_reg = NULL;
    }
    unlock(&_mutex);

    return ret;
}

qeo_retcode_t qeo_remote_registration_get_user_data(qeo_remote_registration_hndl_t  remote_reg,
                                                    uintptr_t                       *user_data)
{
    if ((remote_reg == NULL) || (user_data == NULL)) {
        return QEO_EINVAL;
    }

    lock(&_mutex);
    if (_initialized == false) {
        unlock(&_mutex);
        return QEO_EBADSTATE;
    }
    assert(is_valid_remote_reg_obj(remote_reg));
    *user_data = remote_reg->cfg.user_data;

    unlock(&_mutex);
    return QEO_OK;
}

qeo_retcode_t qeo_remote_registration_destruct(qeo_remote_registration_hndl_t *premote_reg)
{
    qeo_remote_registration_hndl_t  remote_reg;
    qeo_remote_registration_hndl_t  tmp;
    unsigned int                    count = 0;
    qeo_retcode_t                   ret;

    if ((premote_reg == NULL) || (*premote_reg == NULL)) {
        return QEO_OK;
    }

    remote_reg = *premote_reg;
    lock(&_mutex);
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }

        assert(is_valid_remote_reg_obj(remote_reg));
        stop_timer(remote_reg);

        qeo_log_i("Disposing registration request");
        qeocore_writer_remove(_reg_req_writer, &remote_reg->reg_request);

        LL_DELETE(_reg_list, remote_reg);

        free_remote_reg(remote_reg);

        *premote_reg = NULL;

        LL_COUNT(_reg_list, tmp, count);
        if (count == 0) {
            close_reader_and_writer();
        }

        ret = QEO_OK;
    } while (0);

    unlock(&_mutex);

    return ret;
}

qeo_retcode_t qeo_remote_registration_start(qeo_remote_registration_hndl_t remote_reg)
{
    qeo_retcode_t     ret = QEO_EFAIL;
    DDS_ReturnCode_t ddsret;

    if (remote_reg == NULL) {
        return QEO_EINVAL;
    }

    lock(&_mutex);
    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }
        assert(is_valid_remote_reg_obj(remote_reg));

        if (remote_reg->started == true) {
            qeo_log_w("Already started !");
            ret = QEO_OK;
            break;
        }


        if ((ddsret = DDS_Timer_start(remote_reg->timer, remote_reg->cfg.registration_window * 1000, (uintptr_t)remote_reg, at_timer_expiration)) != DDS_RETCODE_OK){
            qeo_log_e("Could not start timer: %d", ddsret);
            ret = ddsrc_to_qeorc(ddsret);
            break;
        }

        ret = QEO_OK;
        remote_reg->started = true;
    } while (0);

    unlock(&_mutex);
    return ret;
}

qeo_retcode_t qeo_remote_registration_update_registration_status(qeo_remote_registration_hndl_t           remote_reg,
                                                                 qeo_remote_registration_status_t         status,
                                                                 qeo_remote_registration_failure_reason_t reason)
{
    qeo_retcode_t ret = QEO_EFAIL;

    lock(&_mutex);

    do {
        if (_initialized == false) {
            ret = QEO_EBADSTATE;
            break;
        }
        assert(is_valid_remote_reg_obj(remote_reg));
        if (publish_registration_request(remote_reg, status, reason) == false) {
            qeo_log_e("Could not update registration request");
            ret = QEO_EFAIL;
            break;
        }

        ret = QEO_OK;
    } while (0);
    unlock(&_mutex);

    return ret;
}

qeo_retcode_t qeo_remote_registration_enable_using_new_registration_credentials(qeo_remote_registration_hndl_t remote_reg)
{
    if (NULL == remote_reg) {
        return QEO_EINVAL;
    }

    lock(&_mutex);
    remote_reg->reg_cred_in_use = false;
    qeo_log_i("Using new registration credentials has been enabled...");
    unlock(&_mutex);

    return QEO_OK;
}

int qeocore_remote_registration_encrypt_otc(const char *otc,
                                              const char *public_key,
                                              unsigned char ** encrypted_otc)
{
    EVP_PKEY *key = NULL;
    int size = -1;

    // check input parameters
    if ((NULL == public_key) || (NULL == otc)) {
        return -1;
    }

    // perform encryption
    do {
        key = pem_to_public_key(public_key);
        if (NULL == key) {
            qeo_log_e("RSA key allocation failed");
            break;
        }
        //encrypted data will always be same size as rsa key size, data has to be less than RSA_size(rsa) - 41
        *encrypted_otc = calloc(RSA_size(EVP_PKEY_get1_RSA(key)), sizeof(char));
        size = RSA_public_encrypt(strlen(otc), (unsigned char*)otc, *encrypted_otc, EVP_PKEY_get1_RSA(key), RSA_PKCS1_OAEP_PADDING);
        if (size == -1) {
            qeo_log_e("RSA encrypt failed");
            break;
        }
    } while (0);

    // free up resources
    if (NULL != key) {
        EVP_PKEY_free(key);
    }

    return size;
}

// allocates memory!
char * qeocore_remote_registration_decrypt_otc(unsigned char *encrypted_otc,
                                              int enrypted_otc_size)
{
    if (NULL == _key) {
        qeo_log_w("private key not known, cannot decrypt otc");
        return NULL;
    }
    return decrypt(_key, encrypted_otc, enrypted_otc_size);
}

void qeocore_remote_registration_free_encrypted_otc(unsigned char *encrypted_otc)
{
    if (NULL != encrypted_otc) {
        free(encrypted_otc);
    }
}

void qeocore_remote_registration_init_cond()
{
    _key_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    _key = NULL;
}

void qeocore_remote_registration_set_key(EVP_PKEY * key)
{
    qeo_log_d("set private key");
    lock(&_mutex);
    _key = key;
    pthread_cond_signal(&_key_cond);
    unlock(&_mutex);
}

char * qeocore_remote_registration_get_pub_key_pem()
{
    qeo_log_d("get private key");
    lock(&_mutex);
    if (NULL == _key) {
        qeo_log_d("get private key was null, waiting");
        if (pthread_cond_wait(&_key_cond, &_mutex) < 0) {
            qeo_log_e("Error while waiting for condition variable(%s)", strerror(errno));
        }
    }
    unlock(&_mutex);
    qeo_log_d("convert private key to pem");
    return public_key_to_pem(_key);
}


