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

/*########################################################################
#                                                                       #
#  HEADER (INCLUDE) SECTION                                             #
#                                                                       #
########################################################################*/
#ifndef DEBUG
#define NDEBUG
#endif
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <qeo/log.h>
#include "policy.h"
#include "security_util.h"
#include "policy_parser.h"
#include "policy_cache.h"
#include "policy_dds_sp.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <dds/dds_security.h>
#include <qeo/mgmt_client.h>
#include <qeo/platform.h>
#include <assert.h>
#include <time.h>
#include <qeocore/api.h>
#include "qeo_Policy.h"

#include "core.h"
#include "core_util.h"

/*########################################################################
#                                                                       #
#  TYPES and DEFINES SECTION                                            #
#                                                                       #
########################################################################*/
#define POLICY_SUFFIX     "policy.mime"
#define MAX_BUF_SIZE      (256*1024)
#define DUMMY_SEQNR       (0)

typedef struct {
    uint64_t  seqnr;
    char      *content;
    size_t    size;
    BIO       *bio;
} policy_t;

struct qeo_security_policy {
    qeo_security_policy_config  cfg;
    char                        *policy_path;
    int                         rand;               /* random number for temporary file (only used when reading) */
    policy_t                    published_policy;   /* policy that we published */
    policy_t                    new_policy;         /* remote policy from server or from QEO peers; becomes published if verification and enforcement is successful. */
    qeo_policy_cache_hndl_t     cache;
};

struct policy_parser_userdata {
    qeo_security_policy_hndl qeo_policy_handle;
};

struct policy_cache_userdata {
    uintptr_t cb;
    uintptr_t cookie;
};

/*########################################################################
#                                                                       #
#  STATIC VARIABLE SECTION                                              #
#                                                                       #
########################################################################*/
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

static bool _initialized = false;
static const policy_t _policy_null = { DUMMY_SEQNR, NULL , 0, NULL};

/*########################################################################
#                                                                       #
#  STATIC FUNCTION PROTOTYPES                                           #
#                                                                       #
########################################################################*/
static bool isInitialized(void);
static void free_policy(policy_t *policy);
static void notify_policy_update(qeo_security_policy_hndl qeo_policy_handle);
static bool exists_local_policy_file(qeo_security_policy_hndl qeo_policy_handle);
static bool open_local_policy_file(qeo_security_policy_hndl qeo_policy_handle);
static void close_local_policy_file(qeo_security_policy_hndl qeo_policy_handle);
static bool new_remote_policy(qeo_security_policy_hndl qeo_policy_handle,
                                 const char *content,
                                 size_t length);
static void close_remote_policy(qeo_security_policy_hndl qeo_policy_handle,
                                bool remove);
static bool update_policy(qeo_security_policy_hndl qeo_policy_handle);
static bool verify_policy(const policy_t  *policy,
                          STACK_OF(X509)  *certs,
                          char            **body);
static bool enforce_policy(qeo_security_policy_hndl qeo_policy_handle,
                           const char               *body);
static bool publish_policy(qeo_security_policy_hndl qeo_policy_handle);
static bool cfg_qeo_policy_handle(qeo_security_policy_hndl          qeo_policy_handle,
                          const qeo_security_policy_config  *cfg);
static void free_qeo_policy_handle(qeo_security_policy_hndl qeo_policy_handle);
static qeo_mgmt_client_retcode_t ssl_ctx_cb(SSL_CTX *ctx,
                                            void    *cookie);
static qeo_mgmt_client_retcode_t data_cb(char   *data,
                                         size_t size,
                                         void   *cookie);

static DDS_ReturnCode_t on_policy_content(uintptr_t  userdata,
                                          uint64_t   *seqnr,
                                          char       *content,
                                          size_t     *length,
                                          int        set);
static int bio_to_mem(char  **out,
                      int   maxlen,
                      BIO   *in);
static int smime_cb(int             ok,
                    X509_STORE_CTX  *ctx);
static void on_policy_parser_participant_found_cb(policy_parser_hndl_t  parser,
                                                  uintptr_t             *user_data,
                                                  const char            *participant_id);
static void on_policy_parser_coarse_grained_rule_found_cb(policy_parser_hndl_t              parser,
                                                          uintptr_t                         user_data,
                                                          const char                        *topic_name,
                                                          const policy_parser_permission_t  *perm);
static void on_policy_parser_fine_grained_rule_section_found_cb(policy_parser_hndl_t              parser,
                                                                uintptr_t                         parser_cookie,
                                                                const char                        *topic_name,
                                                                const char                        *participant_id,
                                                                const policy_parser_permission_t  *perm);
static void on_policy_parser_sequence_number_found_cb(policy_parser_hndl_t  parser,
                                                      uint64_t              sequence_number);
static void find_and_replace(char *haystack,
                             char *needle,
                             char *replacement);
static int replace(char       *toreplace,
                   int        toreplace_len,
                   const char *replacement,
                   int        rep_len);
static void policy_cache_participant_only_cb(qeo_policy_cache_hndl_t cache,
                                             const char              *participant);
static void policy_cache_participant_cb(qeo_policy_cache_hndl_t cache,
                                        const char              *participant);
static void policy_cache_update_topic_cb(qeo_policy_cache_hndl_t           cache,
                                         uintptr_t                         cookie,
                                         const char                        *participant_tag,
                                         const char                        *topic_name,
                                         unsigned int                      selector,
                                         struct topic_participant_list_node *read_participant_list,
                                         struct topic_participant_list_node *write_participant_list);
static void get_fine_grained_rules_cb(qeo_policy_cache_hndl_t            cache,
                                      uintptr_t                          cookie,
                                      const char                         *participant_tag,
                                      const char                         *topic_name,
                                      unsigned int                       selector,
                                      struct topic_participant_list_node *read_participant_list,
                                      struct topic_participant_list_node *write_participant_list);
int32_t bio_write(BIO *const    pBio,
                  const char    *data,
                  const int32_t len);
static void api_lock(void);
static void api_unlock(void);

/*########################################################################
#                                                                       #
#  STATIC SECTION                                                       #
#                                                                       #
########################################################################*/
static bool isInitialized(void)
{
    bool ret = false;

    api_lock();
    if (_initialized == true) {
        ret = true;
    }
    api_unlock();

    return ret;
}

static void api_lock(void)
{
    lock(&_mutex);
}

static void api_unlock(void)
{
    unlock(&_mutex);
}

static void free_policy(policy_t *policy)
{
    if (policy != NULL) {
        if (policy->bio != NULL) {
            BIO_free_all(policy->bio);
        }
        if (policy->content != NULL) {
            free(policy->content);
        }
        memcpy(policy, &_policy_null, sizeof(policy_t));
    }
}

static void notify_policy_update(qeo_security_policy_hndl qeo_policy_handle)
{
    DDS_Security_permissions_changed();
    DDS_Security_qeo_write_policy_version();
    DDS_Security_topics_reevaluate(qeo_policy_handle->cfg.factory->dp, 0, "*");
    
    if (qeo_policy_handle->cfg.update_cb != NULL) {
        qeo_policy_handle->cfg.update_cb(qeo_policy_handle);
    }
}

static bool exists_local_policy_file(qeo_security_policy_hndl qeo_policy_handle)
{
    bool ret = false;

    if (access(qeo_policy_handle->policy_path, R_OK | W_OK) == 0) {
        ret = true;
    }

    return ret;
}

static bool open_local_policy_file(qeo_security_policy_hndl qeo_policy_handle)
{
    bool ret = false;
    char *tmp_file = NULL;

    do {
        qeo_policy_handle->rand = rand();
        if (asprintf(&tmp_file, "%s.%u.%d_r.tmp", qeo_policy_handle->policy_path, getpid(), qeo_policy_handle->rand) == -1) {
            qeo_log_e("Could not allocate tmp_file string");
            break;
        }

        qeo_log_d("Link %s", tmp_file);
        if (link(qeo_policy_handle->policy_path, tmp_file) != 0){
            qeo_log_e("Could not hardlink %s to %s %d(%s)", tmp_file, qeo_policy_handle->policy_path, errno, strerror(errno));
            break;
        }

        qeo_policy_handle->new_policy.bio = BIO_new_file(qeo_policy_handle->policy_path, "r");
        if (qeo_policy_handle->new_policy.bio == NULL) {
            qeo_log_e("Can't open local policy file for reading");
            break;
        }

        while (1) {
            int len = 0;

            qeo_policy_handle->new_policy.content = realloc(qeo_policy_handle->new_policy.content,
                                                            qeo_policy_handle->new_policy.size + 1024);
            len = BIO_read(qeo_policy_handle->new_policy.bio,
                           qeo_policy_handle->new_policy.content + qeo_policy_handle->new_policy.size, 1024);
            if (len <= 0) {
                qeo_policy_handle->new_policy.content = realloc(qeo_policy_handle->new_policy.content,
                                                                qeo_policy_handle->new_policy.size);
                break;
            }
            qeo_policy_handle->new_policy.size += len;
        }
        qeo_log_d("Policy size: %d", qeo_policy_handle->new_policy.size);
        //qeo_log_d("Policy content: %s", qeo_policy_handle->new_policy.content);
        (void) BIO_reset(qeo_policy_handle->new_policy.bio);

        ret = true;
    } while (0);

    if (tmp_file != NULL) {
        free(tmp_file);
    }

    return ret;
}

static void close_local_policy_file(qeo_security_policy_hndl qeo_policy_handle)
{
    char *tmp_file = NULL;

    do {
        if (asprintf(&tmp_file, "%s.%u.%d_r.tmp", qeo_policy_handle->policy_path, getpid(), qeo_policy_handle->rand) == -1) {
            qeo_log_e("Could not allocate tmp_file string");
            break;
        }

        qeo_log_d("Unlink %s", tmp_file);
        if (unlink(tmp_file) != 0){
            qeo_log_e("Could not unlink %s: error %d(%s)", tmp_file, errno, strerror(errno));
            break;
        }
    } while (0);

    free_policy(&qeo_policy_handle->new_policy);

    if (tmp_file != NULL) {
        free(tmp_file);
    }
}

static qeo_mgmt_client_retcode_t ssl_ctx_cb(SSL_CTX *ctx, void *cookie)
{
    qeo_mgmt_client_retcode_t client_ret  = QMGMTCLIENT_EFAIL;
    qeo_retcode_t             qeo_ret     = QEO_OK;
    qeo_security_policy_hndl  qeo_policy_handle   = (qeo_security_policy_hndl)cookie;
    EVP_PKEY                  *key        = NULL;

    STACK_OF(X509) * certs = NULL;
    X509  *user_cert  = NULL;
    X509  *cert       = NULL;
    int   i           = 0;

    do {
        if (qeo_policy_handle == NULL) {
            qeo_log_e("qeo_policy_handle == NULL");
            break;
        }

        qeo_ret = qeo_security_get_credentials(qeo_policy_handle->cfg.sec, &key, &certs);
        if (qeo_ret != QEO_OK) {
            qeo_log_e("Failed to get credentials");
            break;
        }

        if (sk_X509_num(certs) <= 1) {
            qeo_log_e("Not enough certificates in chain");
            break;
        }

        user_cert = sk_X509_value(certs, 0);
        if (user_cert == NULL) {
            qeo_log_e("User_cert == NULL");
            break;
        }

        if (!SSL_CTX_use_certificate(ctx, user_cert)) {
            qeo_log_e("SSL_CTX_use_certificate failed");
            break;
        }
        if (!SSL_CTX_use_PrivateKey(ctx, key)) {
            qeo_log_e("SSL_CTX_use_PrivateKey failed");
            break;
        }

        if (!SSL_CTX_check_private_key(ctx)) {
            qeo_log_e("SSL_CTX_check_private_key failed");
            break;
        }

        security_util_configure_ssl_ctx(ctx);
        for (i = 1; i < sk_X509_num(certs); i++) {
            qeo_log_i("Add cert: %d", i);
            cert = sk_X509_value(certs, i);
            if (cert == NULL) {
                qeo_log_e("Cert == NULL");
                break;
            }

            if (!X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx), cert)) {
                dump_openssl_error_stack("X509_STORE_add_cert failed");
                break;
            }
        }

        client_ret = QMGMTCLIENT_OK;
    } while (0);

    return client_ret;
}

static qeo_mgmt_client_retcode_t data_cb(char *data, size_t size, void *cookie)
{
    qeo_mgmt_client_retcode_t client_ret  = QMGMTCLIENT_EFAIL;
    qeo_security_policy_hndl  qeo_policy_handle   = (qeo_security_policy_hndl)cookie;
    int                       ret_size    = 0;

    do {
        if (qeo_policy_handle == NULL) {
            qeo_log_e("qeo_policy_handle == NULL");
            break;
        }

        qeo_log_i("Data [%zu]:\r\n%s", size, data);
        qeo_policy_handle->new_policy.content = realloc(qeo_policy_handle->new_policy.content,
                                                        qeo_policy_handle->new_policy.size + size);
        memcpy(qeo_policy_handle->new_policy.content + qeo_policy_handle->new_policy.size, data, size);
        qeo_policy_handle->new_policy.size += size;
        if ((ret_size = bio_write(qeo_policy_handle->new_policy.bio, data, size)) != size) {
            qeo_log_e("write_bio failed, requested size: %d, returned size: %d", size, ret_size);
            break;
        }

        client_ret = QMGMTCLIENT_OK;
    } while (0);

    return client_ret;
}

static bool new_remote_policy(qeo_security_policy_hndl qeo_policy_handle, const char *content, size_t length)
{
    bool                      ret         = false;
    int                       opensslret  = 0;
    qeo_mgmt_client_retcode_t clientret;
    qeo_mgmt_client_ctx_t     *ctx    = NULL;
    qeo_retcode_t             qeoret  = QEO_OK;
    qeo_security_identity     *id     = NULL;

    char *tmp_file = NULL;

    do {
        qeo_policy_handle->rand = rand();
        if (asprintf(&tmp_file, "%s.%u.%d_w.tmp", qeo_policy_handle->policy_path, getpid(), qeo_policy_handle->rand) == -1) {
            qeo_log_e("Could not allocate tmp_file string");
            break;
        }

        if (qeo_policy_handle->new_policy.bio != NULL) {
            qeo_log_e("new_policy.bio != NULL");
            break;
        }

        if (qeo_policy_handle->new_policy.content != NULL) {
            qeo_log_e("new_policy.content != NULL");
            break;
        }

        qeo_policy_handle->new_policy.bio = BIO_new_file(tmp_file, "w");
        if (qeo_policy_handle->new_policy.bio == NULL) {
            qeo_log_e("Can't open temporary policy file for writing");
            break;
        }

        if (content == NULL) {
            qeoret = qeo_security_get_mgmt_client_ctx(qeo_policy_handle->cfg.sec, &ctx);
            if (qeoret != QEO_OK) {
                qeo_log_e("Can't get QEO management client context");
                break;
            }

            if ((qeoret = qeo_security_get_identity(qeo_policy_handle->cfg.sec, &id)) != QEO_OK) {
                qeo_log_e("qeo_security_get_identity failed with error: %d", qeoret);
                break;
            }

            clientret = qeo_mgmt_client_get_policy(ctx, id->url, ssl_ctx_cb, data_cb, qeo_policy_handle);
            if (clientret != QMGMTCLIENT_OK) {
                qeo_log_e("qeo_mgmt_client_get_policy failed with error: %d", clientret);
                break;
            }

            qeo_policy_handle->new_policy.content = realloc(qeo_policy_handle->new_policy.content,
                                                            qeo_policy_handle->new_policy.size + 1);
            memcpy(qeo_policy_handle->new_policy.content + qeo_policy_handle->new_policy.size, "\0", 1);
            qeo_policy_handle->new_policy.size += 1;
            opensslret = BIO_printf(qeo_policy_handle->new_policy.bio, "%c", '\0');
            if (opensslret <= 0) {
                qeo_log_e("Failed to add 0 at the end of the policy file (BIO_printf)");
                break;
            }
        }
        else {
            qeo_policy_handle->new_policy.content = calloc(length, sizeof(char));
            memcpy(qeo_policy_handle->new_policy.content, content, length);
            qeo_policy_handle->new_policy.size = length;
            if (bio_write(qeo_policy_handle->new_policy.bio, content, length) <= 0) {
                qeo_log_e("Failed to write the temporary policy file (bio_write)\r\n");
                break;
            }
        }

        (void) BIO_flush(qeo_policy_handle->new_policy.bio);
        BIO_free_all(qeo_policy_handle->new_policy.bio);

        qeo_policy_handle->new_policy.bio = BIO_new_file(tmp_file, "r");
        if (qeo_policy_handle->new_policy.bio == NULL) {
            qeo_log_e("Can't open temporary policy file for reading");
            break;
        }

        ret = true;
    } while (0);

    if (ret == false) {
        free_policy(&qeo_policy_handle->new_policy);
        if (remove(tmp_file) != 0) {
            qeo_log_e("unable to delete %s (%s)\n", tmp_file, strerror(errno));
        }
    }

    if (id != NULL) {
        qeo_security_free_identity(&id);
    }

    if (tmp_file != NULL) {
        free(tmp_file);
    }

    return ret;
}

static void close_remote_policy(qeo_security_policy_hndl qeo_policy_handle, bool success)
{
    char *tmp_file = NULL;

    do {
        if (asprintf(&tmp_file, "%s.%u.%d_w.tmp", qeo_policy_handle->policy_path, getpid(), qeo_policy_handle->rand) == -1) {
            qeo_log_e("Could not allocate tmp_file string");
            break;
        }

        if (success) {
            qeo_log_d("Renaming %s to %s", tmp_file, qeo_policy_handle->policy_path);
            if (rename(tmp_file, qeo_policy_handle->policy_path) != 0){
                qeo_log_e("Rename() from %s to %s failed (%s)", tmp_file, qeo_policy_handle->policy_path, strerror(errno));
                break;
            }
        }
        else {
            qeo_log_d("Removing %s", tmp_file);
            if (remove(tmp_file) != 0) {
                qeo_log_e("Unable to delete %s \n", tmp_file);
                break;
            }
        }
    } while (0);

    free_policy(&qeo_policy_handle->new_policy);
    if (tmp_file != NULL) {
        free(tmp_file);
    }
}

static bool update_policy(qeo_security_policy_hndl qeo_policy_handle)
{
    bool          ret     = false;
    qeo_retcode_t qeo_ret = QEO_OK;
    char          *body   = NULL;
    EVP_PKEY      *key    = NULL;

    STACK_OF(X509) * certs = NULL;

    qeo_log_i("Verify and update the new policy");
    do {
        qeo_ret = qeo_security_get_credentials(qeo_policy_handle->cfg.sec, &key, &certs);
        if (qeo_ret != QEO_OK) {
            qeo_log_e("failed to get credentials");
            break;
        }

        ret = verify_policy(&qeo_policy_handle->new_policy, certs, &body);
        if (ret == false) {
            qeo_log_e("verify_policy failed");
            break;
        }

        if (body == NULL) {
            qeo_log_e("body == NULL");
            break;
        }

        ret = enforce_policy(qeo_policy_handle, body);
        if (ret == false) {
            break;
        }

        ret = publish_policy(qeo_policy_handle);
        if (ret == false) {
            break;
        }
        ret = true;
    } while (0);

    if (body != NULL) {
        OPENSSL_free(body);
    }

    return ret;
}

static bool verify_policy(const policy_t *policy, STACK_OF(X509)   *certs, char **body)
{
    bool        ret     = true;
    X509        *cert   = NULL;
    X509_STORE  *store  = NULL;
    PKCS7       *p7     = NULL;
    BIO         *out    = NULL;
    BIO         *indata = NULL;
    int         flags   = PKCS7_DETACHED;
    int         bret    = 0;
    int         i       = 0;

    do {
        p7 = SMIME_read_PKCS7(policy->bio, &indata);
        if (p7 == NULL) {
            dump_openssl_error_stack("SMIME_read_PKCS7 failed");
            ret = false;
            break;
        }

        store = X509_STORE_new();
        if (store == NULL) {
            qeo_log_e("X509_STORE_new failed");
            ret = false;
            break;
        }

        for (i = 1; i < sk_X509_num(certs); i++) {
            qeo_log_i("add cert: %d", i);
            cert = sk_X509_value(certs, i);
            if (cert == NULL) {
                qeo_log_e("cert == NULL");
                ret = false;
                break;
            }

            if (!X509_STORE_add_cert(store, cert)) {
                dump_openssl_error_stack("X509_STORE_add_cert failed");
                ret = false;
                break;
            }
        }

        out = BIO_new(BIO_s_mem());
        if (out == NULL) {
            dump_openssl_error_stack("BIO_new failed");
            ret = false;
            break;
        }

        X509_STORE_set_verify_cb(store, smime_cb);
        if (PKCS7_verify(p7, NULL, store, indata, out, flags) != 1) {
            dump_openssl_error_stack("Verification failure");
            ret = false;
            break;
        }

        qeo_log_i("Verification successful\n");
        BIO_printf(out, "%c", '\0');
        bret = bio_to_mem(body, MAX_BUF_SIZE, out);
        if (bret <= 0) {
            qeo_log_e("Getting BIO's content failed");
            ret = false;
            break;
        }
        if (getenv("POLICY_DUMP") != NULL){
            qeo_log_i("data available: %d", bret);
            qeo_log_i("policy body:\r\n%s", *body);
        }

        (void) BIO_reset(policy->bio);
    } while (0);

    if (p7 != NULL) {
        PKCS7_free(p7);
    }

    if (store != NULL) {
        X509_STORE_free(store);
    }

    if (out != NULL) {
        BIO_free(out);
    }

    if (indata != NULL) {
        BIO_free(indata);
    }

    return ret;
}

static bool enforce_policy(qeo_security_policy_hndl qeo_policy_handle, const char *body)
{
    bool                  retval          = false;
    policy_parser_hndl_t  parser          = NULL;
    qeo_retcode_t         ret             = QEO_OK;
    qeocore_domain_id_t   domain_id       = 0;
    bool                  dds_sp_failure  = false;
    struct policy_parser_userdata ud = { qeo_policy_handle };
    policy_parser_cfg_t cfg =
    {
        .buf        = body,
        .user_data  = (uintptr_t) &ud
    };

    qeo_log_d("Configure DDS security database");
    do {
        domain_id = qeo_policy_handle->cfg.factory->domain_id;

        if ((ret = policy_parser_construct(&cfg, &parser)) != QEO_OK) {
            qeo_log_e("Policy parser construct failed");
            break;
        }

        if ((ret = policy_parser_run(parser)) != QEO_OK) {
            qeo_log_e("Policy parsing failed");
            break;
        }

        if ((ret = qeo_policy_cache_finalize(qeo_policy_handle->cache)) != QEO_OK) {
            qeo_log_e("Policy parser finalization failed");
            break;
        }

        // Configure DDS security plugin database
        policy_dds_sp_update_start();
        if ((ret = policy_dds_sp_add_domain(domain_id) != QEO_OK)) {
            qeo_log_e("policy_dds_sp_add_domain() failed");
            dds_sp_failure = true;
            break;
        }
        // First configure all participant rules in DDS (needed for fine-grained topic rules)
        if (qeo_policy_cache_get_participants(qeo_policy_handle->cache, policy_cache_participant_only_cb) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_get_participants failed");
            dds_sp_failure = true;
            break;
        }
        // Second configure all topic rules (coarse and fine-grained)
        if (qeo_policy_cache_get_participants(qeo_policy_handle->cache, policy_cache_participant_cb) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_get_participants failed");
            dds_sp_failure = true;
            break;
        }
        policy_dds_sp_update_done();

        retval = true;
    } while (0);

    if (dds_sp_failure == true) {
        policy_dds_sp_update_done();
        policy_dds_sp_flush();
    }

    policy_parser_destruct(&parser);

    return retval;
}

static bool publish_policy(qeo_security_policy_hndl qeo_policy_handle)
{
    bool ret = false;

    qeo_log_i("Saving sequence number of new policy");
    do {
        if (qeo_policy_handle->new_policy.bio == NULL) {
            qeo_log_e("new_policy.bio == NULL");
            break;
        }

        qeo_policy_handle->published_policy.seqnr = qeo_policy_handle->new_policy.seqnr;
        /* Free the content buffer in the published_policy, move the content buffer from
         * the new_policy to the published_policy and NULL it in the new_policy
         * to make sure it is not accidently freed */
        if (qeo_policy_handle->published_policy.content != NULL) {
            free(qeo_policy_handle->published_policy.content);
        }
        qeo_policy_handle->published_policy.content = qeo_policy_handle->new_policy.content;
        qeo_policy_handle->new_policy.content = NULL;
        qeo_policy_handle->published_policy.size = qeo_policy_handle->new_policy.size;
        free_policy(&qeo_policy_handle->new_policy);

        ret = true;
    } while (0);

    return ret;
}

static DDS_ReturnCode_t on_policy_content(uintptr_t  userdata,
                                          uint64_t   *seqnr,
                                          char       *content,
                                          size_t     *length,
                                          int        set)
{
    qeo_security_policy_hndl  qeo_policy_handle = (qeo_security_policy_hndl)userdata;
    DDS_ReturnCode_t ret = DDS_RETCODE_ERROR;

    api_lock();
    do {
        if (set) {
            uint64_t sequence_number = 0;
            char *tmp = NULL;

            if (content == NULL) {
                qeo_log_e("Content pointer NULL");
                return DDS_RETCODE_BAD_PARAMETER;
            }

            tmp = calloc(*length, sizeof(char));
            memcpy(tmp, content, *length);
            sequence_number = policy_parser_get_sequence_number(tmp);
            free(tmp);

            qeo_log_d("Policy update seq nr = %" PRIu64 " ( local enforced seq nr = %" PRIu64 ")\r\n",
                      sequence_number, qeo_policy_handle->published_policy.seqnr);
            //qeo_log_d("Content: %s", content);
            if (sequence_number > qeo_policy_handle->published_policy.seqnr) {
                qeo_log_d("Policy instance has newer sequence number (%" PRIu64 ") as local enforced policy (%" PRIu64 ")\r\n",
                          sequence_number, qeo_policy_handle->published_policy.seqnr);
                if (new_remote_policy(qeo_policy_handle, content, *length) == false) {
                    qeo_log_e("new_remote_policy failed");
                    break;
                }

                if (update_policy(qeo_policy_handle) == false) {
                    qeo_log_e("update_policy failed");
                    close_remote_policy(qeo_policy_handle, false);
                    break;
                }
                close_remote_policy(qeo_policy_handle, true);
                api_unlock();
                notify_policy_update(qeo_policy_handle);
                api_lock();
            }
         } else {
            if (seqnr != NULL) {
                qeo_log_d("Get policy sequence nr = %" PRIu64, qeo_policy_handle->published_policy.seqnr);
                *seqnr = qeo_policy_handle->published_policy.seqnr;
            }
            if (length != NULL) {
                qeo_log_d("Get policy length = %d", qeo_policy_handle->published_policy.size);
                *length = qeo_policy_handle->published_policy.size;
            }
            if (content != NULL) {
                //qeo_log_d("Get policy content: %s", qeo_policy_handle->published_policy.content);
                qeo_log_d("Get policy content");
                memcpy(content, qeo_policy_handle->published_policy.content, qeo_policy_handle->published_policy.size);
            }
        }

        ret = DDS_RETCODE_OK;
    } while (0);
    api_unlock();

    return ret;
}

static bool cfg_qeo_policy_handle(qeo_security_policy_hndl qeo_policy_handle, const qeo_security_policy_config *cfg)
{
    bool                  ret       = true;
    char                  *file     = NULL;
    qeo_util_retcode_t    util_ret  = QEO_UTIL_OK;
    qeo_security_identity *id       = NULL;
    qeo_retcode_t         rc        = QEO_EFAIL;

    do {
        memcpy(&qeo_policy_handle->cfg, cfg, sizeof(qeo_policy_handle->cfg));

        if ((rc = qeo_security_get_identity(cfg->sec, &id)) != QEO_OK) {
            qeo_log_e("qeo_security_get_identity failed with error: %d", rc);
            ret = false;
            break;
        }

        if (asprintf(&file, "%" PRIx64 "_%s", id->realm_id, POLICY_SUFFIX) == -1) {
            qeo_log_e("Could not allocate");
            ret = false;
            break;
        }

        util_ret = qeo_platform_get_device_storage_path(file, &qeo_policy_handle->policy_path);
        if (util_ret != QEO_UTIL_OK) {
            qeo_log_e("Could not set absolute policy file path");
            ret = false;
            break;
        }

        qeo_policy_handle->published_policy = _policy_null;
        qeo_policy_handle->new_policy       = _policy_null;

        qeo_log_d("qeo_policy_handle[%p]", qeo_policy_handle);
        qeo_log_d("realm_id: %" PRIx64 "", id->realm_id);
        qeo_log_d("url: %s", id->url);
    } while (0);

    if (file != NULL) {
        free(file);
    }

    if (id != NULL) {
        qeo_security_free_identity(&id);
    }

    return ret;
}

static void free_qeo_policy_handle(qeo_security_policy_hndl qeo_policy_handle)
{
    free_policy(&qeo_policy_handle->new_policy);
    free_policy(&qeo_policy_handle->published_policy);
    if (qeo_policy_handle->cache != NULL) {
        if (qeo_policy_cache_destruct(&qeo_policy_handle->cache) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_destruct failed");
        }
    }
    if (qeo_policy_handle->policy_path != NULL) {
        free(qeo_policy_handle->policy_path);
        qeo_policy_handle->policy_path = NULL;
    }

    free(qeo_policy_handle);
}

/* Read whole contents of a BIO into an allocated memory buffer and
 * return it.
 */
static int bio_to_mem(char **out, int maxlen, BIO *in)
{
    BIO   *mem;
    int   len;
    int   ret = 0;
    char  tbuf[1024];

    do {
        mem = BIO_new(BIO_s_mem());
        if (!mem) {
            dump_openssl_error_stack("BIO_new failed");
            return -1;
        }

        while (1) {
            if ((maxlen != -1) && (maxlen < 1024)) {
                len = maxlen;
            }
            else {
                len = 1024;
            }
            len = BIO_read(in, tbuf, len);
            if (len <= 0) {
                break;
            }
            if (BIO_write(mem, tbuf, len) != len) {
                dump_openssl_error_stack("BIO_write failed");
                ret = -1;
                break;
            }
            maxlen -= len;
            if (maxlen == 0) {
                break;
            }
        }
        ret = BIO_get_mem_data(mem, (char **)out);
        if (ret <= 0) {
            dump_openssl_error_stack("BIO_get_mem_data failed");
            break;
        }
        BIO_set_flags(mem, BIO_FLAGS_MEM_RDONLY);
    } while (0);

    BIO_free(mem);

    return ret;
}

int32_t bio_write(BIO *const pBio, const char *data, const int32_t len)
{
    int32_t nPos = 0;
    int32_t nNumberOfBytesWritten = 0;

    for (nPos = 0; nPos < len; nPos += nNumberOfBytesWritten) {
        if ((nNumberOfBytesWritten = BIO_write(pBio, data + nPos, len - nPos)) <= 0) {
            if (BIO_should_retry(pBio)) {
                nNumberOfBytesWritten = 0;
                continue;
            }

            qeo_log_e("BIO_write failed");
            return -1;
        }
    }

    return nPos;
}

static void nodes_print(BIO *out, const char *name,
                        STACK_OF(X509_POLICY_NODE) *nodes)
{
    X509_POLICY_NODE  *node;
    int               i;

    BIO_printf(out, "%s Policies:", name);
    if (nodes) {
        BIO_puts(out, "\n");
        for (i = 0; i < sk_X509_POLICY_NODE_num(nodes); i++) {
            node = sk_X509_POLICY_NODE_value(nodes, i);
            X509_POLICY_NODE_print(out, node, 2);
        }
    }
    else {
        BIO_puts(out, " <empty>\n");
    }
}

void policies_print(BIO *out, X509_STORE_CTX *ctx)
{
    X509_POLICY_TREE  *tree;
    int               explicit_policy;

    tree            = X509_STORE_CTX_get0_policy_tree(ctx);
    explicit_policy = X509_STORE_CTX_get_explicit_policy(ctx);

    BIO_printf(out, "Require explicit Policy: %s\n",
               explicit_policy ? "True" : "False");

    nodes_print(out, "Authority", X509_policy_tree_get0_policies(tree));
    nodes_print(out, "User", X509_policy_tree_get0_user_policies(tree));
}

static int smime_cb(int ok, X509_STORE_CTX *ctx)
{
    BIO   *out                = NULL;
    X509  *policy_cert        = NULL;
    int   err                 = 0;
    int   depth               = 0;
    int   verify_depth        = 0;
    int   verify_return_error = 0;


    qeo_log_d("enter");

    do {
        out = BIO_new_fp(stderr, BIO_NOCLOSE);
        if (out == NULL) {
            dump_openssl_error_stack("BIO_new_fp failed");
            break;
        }

        policy_cert = X509_STORE_CTX_get_current_cert(ctx);
        if (policy_cert == NULL) {
            BIO_puts(out, "<no policy cert>\n");
            ok = 0;
            break;
        }

        depth = X509_STORE_CTX_get_error_depth(ctx);
#ifdef DEBUG
        BIO *dbg_out = BIO_new(BIO_s_mem());
        BIO_printf(dbg_out, "depth=%d X509_STORE_CTX_get_current_cert: ", depth);
        X509_NAME_print_ex(dbg_out, X509_get_subject_name(policy_cert), 0, XN_FLAG_ONELINE);

        BIO_printf(dbg_out, "%c", '\0');
        char * dbg_out_string = NULL;
        bio_to_mem(&dbg_out_string, MAX_BUF_SIZE, dbg_out);
        qeo_log_d("%s", dbg_out_string);
        OPENSSL_free(dbg_out_string);
        BIO_free(dbg_out);
#endif

        //only continue in case this is the policy signing certificate
        if (depth != 0) {
            ok = 1;
            break;
        }

        //check based on key usage check in openssl/crypto/x509v3/v3_purp.c
        /*
         * Check the optional key usage field:
         * if Key Usage is present, it must be digitalSignature
         * (other values are not consistent and shall
         * be rejected).
         */

        if ((policy_cert->ex_flags & EXFLAG_KUSAGE) &&
            ((policy_cert->ex_kusage & ~(KU_DIGITAL_SIGNATURE)) ||
             !(policy_cert->ex_kusage & (KU_DIGITAL_SIGNATURE)))) {
            qeo_log_e("invalid key usage");
            ok = 0;
            break;
        }

        err = X509_STORE_CTX_get_error(ctx);
        if (!ok) {
            qeo_log_e("verify error:num=%d:%s\n", err,
                      X509_verify_cert_error_string(err));
            if (verify_depth >= depth) {
                if (!verify_return_error) {
                    ok = 1;
                }
            }
            else {
                ok = 0;
            }
        }
        switch (err) {
            case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
                BIO_puts(out, "issuer= ");
                X509_NAME_print_ex(out, X509_get_issuer_name(policy_cert),
                                   0, XN_FLAG_ONELINE);
                BIO_puts(out, "\n");
                break;

            case X509_V_ERR_CERT_NOT_YET_VALID:
            case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
                BIO_printf(out, "notBefore=");
                ASN1_TIME_print(out, X509_get_notBefore(policy_cert));
                BIO_printf(out, "\n");
                break;

            case X509_V_ERR_CERT_HAS_EXPIRED:
            case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
                BIO_printf(out, "notAfter=");
                ASN1_TIME_print(out, X509_get_notAfter(policy_cert));
                BIO_printf(out, "\n");
                break;

            case X509_V_ERR_NO_EXPLICIT_POLICY:
                policies_print(out, ctx);
                break;

            case X509_V_ERR_INVALID_PURPOSE:
                //ok = 0;
                break;
        }
        if ((err == X509_V_OK) && (ok == 2)) {
            policies_print(out, ctx);
        }

        qeo_log_d("verify return:%d\n", ok);
    } while (0);

    BIO_free(out);
    qeo_log_d("exit");

    return ok;
}

static void find_and_replace(char *haystack, char *needle, char *replacement)
{
    int   toreplace_len = strlen(needle);
    int   rep_len       = strlen(replacement);
    char  *toreplace    = NULL;

    while ((toreplace = strstr(haystack, needle)) != NULL) {
        replace(toreplace, toreplace_len, replacement, rep_len);
        haystack = toreplace;
    }
}

/* This function does not take care of reallocating so this function only works if you make the string smaller.! */
static int replace(char *toreplace, int toreplace_len, const char *replacement, int rep_len)
{
    assert(toreplace_len >= rep_len); /* check contract */
    if (toreplace_len != rep_len) {   /* if the lengths don't match, we have to move first... */
        int len = strlen(toreplace);
        memmove(toreplace + toreplace_len + (rep_len - toreplace_len), toreplace + toreplace_len, len + 1 - toreplace_len);
    }
    memcpy(toreplace, replacement, rep_len);
    return rep_len - toreplace_len;
}

static void on_policy_parser_participant_found_cb(policy_parser_hndl_t parser, uintptr_t *parser_cookie, const char *participant_id)
{
    struct policy_parser_userdata *ud = NULL;

    do {
        if (policy_parser_get_user_data(parser, (uintptr_t *)(void *)&ud) != QEO_OK) {
            qeo_log_e("Could not retrieve user data");
            break;
        }

        *parser_cookie = (uintptr_t) participant_id;
        if (qeo_policy_cache_add_participant_tag(ud->qeo_policy_handle->cache, participant_id) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_add_participant_tag failed");
        }
    } while (0);
}

static void on_policy_parser_coarse_grained_rule_found_cb(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const policy_parser_permission_t *perm)
{
    struct policy_parser_userdata *ud             = NULL;
    const char                    *participant_id = (const char *) parser_cookie;

    do {
        if (policy_parser_get_user_data(parser, (uintptr_t *)(void *)&ud) != QEO_OK) {
            qeo_log_e("Could not retrieve user data");
            break;
        }
        //Replace all "::" with ".", because there's a mismatch in topic definitions found in the TSM structs, with "." and the QDM topic definitions with "::"
        find_and_replace((char *) topic_name, "::", ".");
        if (qeo_policy_cache_add_coarse_grained_rule(ud->qeo_policy_handle->cache, participant_id, topic_name, perm) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_add_coarse_grained_rule failed");
        }
    } while (0);
}

static void on_policy_parser_fine_grained_rule_section_found_cb(policy_parser_hndl_t parser, uintptr_t parser_cookie, const char *topic_name, const char *participant_id, const policy_parser_permission_t *perm)
{
    struct policy_parser_userdata *ud = NULL;
    const char                    *participant_tag = (const char *) parser_cookie;

    do {
        if (policy_parser_get_user_data(parser, (uintptr_t *)(void *)&ud) != QEO_OK) {
            qeo_log_e("Could not retrieve user data");
            break;
        }
        //Replace all "::" with ".", because there's a mismatch in topic definitions found in the TSM structs, with "." and the QDM topic definitions with "::"
        find_and_replace((char *) topic_name, "::", ".");
        if (qeo_policy_cache_add_fine_grained_rule_section(ud->qeo_policy_handle->cache, participant_tag, topic_name, perm, participant_id) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_add_fine_grained_rule_section failed");
        }
    } while (0);
}

static void on_policy_parser_sequence_number_found_cb(policy_parser_hndl_t parser, uint64_t sequence_number)
{
    struct policy_parser_userdata *ud = NULL;

    do {
        if (policy_parser_get_user_data(parser, (uintptr_t *)(void *)&ud) != QEO_OK) {
            qeo_log_e("Could not retrieve user data");
            break;
        }

        ud->qeo_policy_handle->new_policy.seqnr = sequence_number;

        if (qeo_policy_cache_reset(ud->qeo_policy_handle->cache) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_reset failed");
            break;
        }

        if (qeo_policy_cache_set_seq_number(ud->qeo_policy_handle->cache, sequence_number) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_reset failed");
            break;
        }
    } while (0);
}

static void policy_cache_participant_only_cb(qeo_policy_cache_hndl_t cache, const char *participant)
{
    char      participant_wildcard[64];
    uintptr_t cookie = 0;

    do {
        snprintf(participant_wildcard, sizeof(participant_wildcard), "*<%s>*", participant);
        if ((policy_dds_sp_add_participant(&cookie, participant_wildcard) != 0)) {
            qeo_log_e("policy_dds_sp_add_participant failed\r\n");
            break;
        }
    } while (0);
}

static void policy_cache_participant_cb(qeo_policy_cache_hndl_t cache, const char *participant)
{
    char      participant_wildcard[64];
    uintptr_t cookie = 0;

    do {
        snprintf(participant_wildcard, sizeof(participant_wildcard), "*<%s>*", participant);
        if ((policy_dds_sp_add_participant(&cookie, participant_wildcard) != 0)) {
            qeo_log_e("policy_dds_sp_add_participant failed\r\n");
            break;
        }

        if ((qeo_policy_cache_get_topic_rules(cache, cookie, participant, NULL,
                                              TOPIC_PARTICIPANT_SELECTOR_READ | TOPIC_PARTICIPANT_SELECTOR_WRITE,
                                              policy_cache_update_topic_cb)) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_get_partition_string failed\r\n");
            break;
        }
    } while (0);
}

static void policy_cache_update_topic_cb(qeo_policy_cache_hndl_t cache,
                                         uintptr_t cookie,
                                         const char *participant_tag,
                                         const char *topic_name,
                                         unsigned int selector,
                                         struct topic_participant_list_node *read_participant_list,
                                         struct topic_participant_list_node *write_participant_list)
{
    policy_dds_sp_perms_t              perms       = { false };

    do {
        if (selector & TOPIC_PARTICIPANT_SELECTOR_READ) {
            perms.read = true;
        }

        if (selector & TOPIC_PARTICIPANT_SELECTOR_WRITE) {
            perms.write = true;
        }

        if (policy_dds_sp_add_topic(cookie, topic_name, &perms) != 0) {
            qeo_log_e("policy_dds_sp_add_topic failed\r\n");
            break;
        }

        if (policy_dds_sp_add_topic_fine_grained(cookie, topic_name, &perms, read_participant_list, write_participant_list)) {
            qeo_log_e("policy_dds_sp_add_topic_fine_grained failed\r\n");
            break;
        }
    } while (0);
}

static void get_fine_grained_rules_cb(qeo_policy_cache_hndl_t cache,
                                      uintptr_t cookie,
                                      const char *participant_tag,
                                      const char *topic_name,
                                      unsigned int selector,
                                      struct topic_participant_list_node *read_participant_list,
                                      struct topic_participant_list_node *write_participant_list)
{
    struct policy_cache_userdata                     *ud         = (struct policy_cache_userdata *) cookie;
    qeo_security_policy_hndl                         qeo_policy_handle   = NULL;
    uintptr_t                                        tmp_cookie  = 0;
    qeo_security_policy_update_fine_grained_rules_cb update_cb   = NULL;

    do {
        if (ud == NULL) {
            qeo_log_e("ud == NULL");
            break;
        }
        update_cb = (qeo_security_policy_update_fine_grained_rules_cb) ud->cb;

        if (update_cb == NULL) {
            qeo_log_e("update_cb == NULL");
            break;
        }

        if (qeo_policy_cache_get_cookie(cache, &tmp_cookie) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_get_cookie failed\r\n");
        }

        qeo_policy_handle = (qeo_security_policy_hndl) tmp_cookie;
        update_cb(qeo_policy_handle, ud->cookie, topic_name, selector, read_participant_list, write_participant_list);
    } while (0);
}

/*########################################################################
#                                                                       #
#  PUBLIC FUNCTION SECTION                                              #
#                                                                       #
########################################################################*/
qeo_retcode_t qeo_security_policy_init(void)
{
    qeo_retcode_t ret = QEO_OK;

    if (isInitialized() == true) {
        return QEO_OK;
    }

    api_lock();
    do {
        policy_parser_init_cfg_t init_cfg =
        {
            /* callbacks */
            .on_participant_found_cb                = on_policy_parser_participant_found_cb,
            .on_coarse_grained_rule_found_cb        = on_policy_parser_coarse_grained_rule_found_cb,
            .on_fine_grained_rule_section_found_cb  = on_policy_parser_fine_grained_rule_section_found_cb,
            .on_sequence_number_found_cb            = on_policy_parser_sequence_number_found_cb
        };

        /* Okay, I admit I am not sure this is the right place .. */
        if ((ret = policy_dds_sp_init()) != QEO_OK) {
            qeo_log_e("policy_dds_sp_init() failed");
            break;
        }

        if ((ret = policy_parser_init(&init_cfg)) != QEO_OK) {
            qeo_log_e("Could not init policy parser...");
            break;
        }

        _initialized = true;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_policy_construct(const qeo_security_policy_config *cfg, qeo_security_policy_hndl *qeo_policy_handle)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if ((cfg == NULL) || (qeo_policy_handle == NULL)) {
        return QEO_EINVAL;
    }

    if (isInitialized() == false) {
        qeo_log_d("not initialized");
        return QEO_EBADSTATE;
    }

    api_lock();
    do {
        *qeo_policy_handle = calloc(1, sizeof(**qeo_policy_handle));
        if (*qeo_policy_handle == NULL) {
            qeo_log_e("*qeo_policy_handle == NULL");
            ret = QEO_ENOMEM;
            break;
        }

        if (cfg_qeo_policy_handle(*qeo_policy_handle, cfg) == false) {
            qeo_log_e("cfg_qeo_policy_handle failed");
            break;
        }

        ret = qeo_policy_cache_construct((uintptr_t) *qeo_policy_handle, &(*qeo_policy_handle)->cache);
        if (ret != QEO_OK) {
            qeo_log_e("qeo_policy_cache_construct failed");
            break;
        }
        ret = QEO_EFAIL; //reset ret

        policy_dds_sp_set_policy_cb(on_policy_content, (uintptr_t) *qeo_policy_handle);

        if (exists_local_policy_file(*qeo_policy_handle)) {
            if (open_local_policy_file(*qeo_policy_handle) == false) {
                qeo_log_e("open_local_policy_file failed");
                break;
            }
        }
        else {
            if (new_remote_policy(*qeo_policy_handle, NULL, 0) == false) {
                qeo_log_e("new_remote_policy failed");
                break;
            }
        }
        if (update_policy(*qeo_policy_handle) == false) {
            qeo_log_d("update_policy failed");
            if (exists_local_policy_file(*qeo_policy_handle)) {
                close_local_policy_file(*qeo_policy_handle);
            }
            else {
                close_remote_policy(*qeo_policy_handle, false);
            }
            break;
        }
        if (exists_local_policy_file(*qeo_policy_handle)) {
            close_local_policy_file(*qeo_policy_handle);
        }
        else {
            close_remote_policy(*qeo_policy_handle, true);
        }

        ret = QEO_OK;
        api_unlock();
        //not sure whether we need it here, but it will not harm
        notify_policy_update(*qeo_policy_handle);
        api_lock();
    } while (0);
    api_unlock();

    if (ret != QEO_OK) {
        if (*qeo_policy_handle != NULL) {
            free_qeo_policy_handle(*qeo_policy_handle);
            *qeo_policy_handle = NULL;
        }
    }

    return ret;
}

qeo_retcode_t qeo_security_policy_get_config(qeo_security_policy_hndl qeo_policy_handle, qeo_security_policy_config *cfg)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if (isInitialized() == false) {
        return QEO_EBADSTATE;
    }

    if (qeo_policy_handle == NULL) {
        return QEO_EINVAL;
    }

    if (cfg == NULL) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        *cfg = qeo_policy_handle->cfg;

        ret = QEO_OK;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_policy_get_sequence_number(qeo_security_policy_hndl qeo_policy_handle, uint64_t *sequence_number)
{
    qeo_retcode_t ret = QEO_EFAIL;

    if (isInitialized() == false) {
        return QEO_EBADSTATE;
    }

    if (qeo_policy_handle == NULL) {
        return QEO_EINVAL;
    }

    if (sequence_number == NULL) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        *sequence_number = qeo_policy_handle->published_policy.seqnr;

        ret = QEO_OK;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_policy_refresh(qeo_security_policy_hndl qeo_policy_handle)
{
    qeo_retcode_t             ret         = QEO_EFAIL;
    qeo_mgmt_client_ctx_t     *ctx        = NULL;
    qeo_mgmt_client_retcode_t client_ret  = QMGMTCLIENT_EFAIL;
    qeo_security_identity     *id         = NULL;

    if (isInitialized() == false) {
        return QEO_EBADSTATE;
    }

    if (qeo_policy_handle == NULL) {
        return QEO_EINVAL;
    }

    api_lock();
    do {
        qeo_log_i("policy refresh...");

        // HACK : to be removed once policy poller is not needed anymore
        bool result = false;
        if (qeo_policy_handle->published_policy.seqnr != DUMMY_SEQNR) {
            qeo_log_i("currently enforced policy sequence nr:  %" PRIi64 "", qeo_policy_handle->published_policy.seqnr);

            ret = qeo_security_get_mgmt_client_ctx(qeo_policy_handle->cfg.sec, &ctx);
            if (ret != QEO_OK) {
                qeo_log_e("Can't get qeo mgmt client context");
                break;
            }

            ret = qeo_security_get_identity(qeo_policy_handle->cfg.sec, &id);
            if (ret != QEO_OK) {
                qeo_log_e("qeo_security_get_identity failed with error: %d", ret);
                break;
            }

            qeo_log_i("Checking whether policy is still valid");
            client_ret = qeo_mgmt_client_check_policy(ctx, ssl_ctx_cb, qeo_policy_handle, id->url, qeo_policy_handle->published_policy.seqnr, id->realm_id, &result);
            if (client_ret != QMGMTCLIENT_OK) {
                ret = QEO_EFAIL;
                qeo_log_e("qeo_mgmt_client_check_policy failed");
                break;
            }

            if (true == result) {
                qeo_log_i("policy still valid");
                ret = QEO_OK;
                break;
            }
        }
        // END OF HACK

        if (new_remote_policy(qeo_policy_handle, NULL, 0) == false) {
            ret = QEO_EFAIL;
            qeo_log_e("new_remote_policy failed");
            break;
        }

        if (update_policy(qeo_policy_handle) == false) {
            ret = QEO_EFAIL;
            qeo_log_e("update_policy failed");
            close_remote_policy(qeo_policy_handle, false);
            break;
        }
        close_remote_policy(qeo_policy_handle, true);
        api_unlock();
        notify_policy_update(qeo_policy_handle);
        api_lock();

        ret = QEO_OK;
    } while (0);
    api_unlock();

    if (id != NULL) {
        qeo_security_free_identity(&id);
    }


    return ret;
}

qeo_retcode_t qeo_security_policy_get_fine_grained_rules(qeo_security_policy_hndl qeo_policy_handle, uintptr_t cookie, const char *topic_name, unsigned int selector_mask, qeo_security_policy_update_fine_grained_rules_cb update_cb)
{
    qeo_retcode_t                 ret           = QEO_EFAIL;
    char                          *participant  = NULL;
    struct policy_cache_userdata  ud            = {};

    if (isInitialized() == false) {
        return QEO_EBADSTATE;
    }

    if (qeo_policy_handle == NULL) {
        qeo_log_e("qeo_policy_handle == NULL");
        return QEO_EINVAL;
    }

    if (update_cb == NULL) {
        qeo_log_e("update_cb == NULL");
        return QEO_EINVAL;
    }

    api_lock();
    do {
        if (asprintf(&participant, "uid:%" PRIx64 "", (qeo_policy_handle->cfg.factory)->qeo_id.user_id) == -1) {
            qeo_log_e("Could not allocate");
            ret = QEO_ENOMEM;
            break;
        }

        ud.cb     = (uintptr_t)update_cb;
        ud.cookie = cookie;
        if ((qeo_policy_cache_get_topic_rules(qeo_policy_handle->cache, (uintptr_t)&ud, participant, topic_name, selector_mask, get_fine_grained_rules_cb)) != QEO_OK) {
            qeo_log_e("qeo_policy_cache_get_partition_string failed\r\n");
            break;
        }

        ret = QEO_OK;
    } while (0);
    api_unlock();

    free(participant);

    return ret;
}

qeo_retcode_t qeo_security_policy_destruct(qeo_security_policy_hndl *qeo_policy_handle)
{
    qeo_retcode_t ret = QEO_OK;

    if (isInitialized() == false) {
        return QEO_EBADSTATE;
    }

    api_lock();
    do {
        if ((qeo_policy_handle == NULL) || (*qeo_policy_handle == NULL)) {
            ret = QEO_OK; /* same behaviour like standard free() */
            break;
        }

        free_qeo_policy_handle(*qeo_policy_handle);
        *qeo_policy_handle = NULL;
    } while (0);
    api_unlock();

    return ret;
}

qeo_retcode_t qeo_security_policy_destroy(void)
{
    qeo_retcode_t ret = QEO_OK;

    if (isInitialized() == true) {
        api_lock();
        policy_dds_sp_destroy();
        policy_parser_destroy();
        _initialized = false;
        api_unlock();
    }

    return ret;
}
