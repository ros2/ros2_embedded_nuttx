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

#define _GNU_SOURCE
#include "unittest/unittest.h"

#include "security.h"
#include <qeo/error.h>
#include <qeo/log.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <string.h>

#include <qeo/platform.h>

#include "security_fixtures.h"
#include "Mockmgmt_client.h"
#include "Mockmgmt_cert_parser.h"
#include "Mockplatform_security.h"
#include "Mockremote_registration.h"

/*########################################################################
 #                                                                       #
 #  MOCK IMPLEMENTATION#
 #                                                                       #
 ########################################################################*/
static void callback(int p, int n, void *arg)
{
    char c='B';

    if (p == 0) c='.';
    if (p == 1) c='+';
    if (p == 2) c='*';
    if (p == 3) c='\n';
    fputc(c,stderr);
}
/* Add extension using V3 code: we can set the config file as NULL
 *  * because we wont reference any other sections.
 *   */

static int add_ext(X509 *cert, int nid, char *value)
{
    X509_EXTENSION *ex;
    X509V3_CTX ctx;
    /* This sets the 'context' of the extensions. */
    /* No configuration database */
    X509V3_set_ctx_nodb(&ctx);
    /* Issuer and subject certs: both the target since it is self signed,
 *      * no request and no CRL
 *           */
    X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
    ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);
    if (!ex)
        return 0;

    X509_add_ext(cert,ex,-1);
    X509_EXTENSION_free(ex);
    return 1;
}


static const char *_cert_cn = "ab cd ef";
static EVP_PKEY *_used_key = NULL;
static int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days)
	{
	X509 *x;
	EVP_PKEY *pk;
	RSA *rsa;
	X509_NAME *name=NULL;
	
	if ((pkeyp == NULL) || (*pkeyp == NULL))
		{
		if ((pk=EVP_PKEY_new()) == NULL)
			{
			abort(); 
			return(0);
			}
		}
	else
		pk= *pkeyp;

	if ((x509p == NULL) || (*x509p == NULL))
		{
		if ((x=X509_new()) == NULL)
			goto err;
		}
	else
		x= *x509p;

    if (bits != 0){
        rsa=RSA_generate_key(bits,RSA_F4,callback,NULL);
        if (!EVP_PKEY_assign_RSA(pk,rsa))
        {
            abort();
            goto err;
        }
        rsa=NULL;
    }

	X509_set_version(x,2);
	ASN1_INTEGER_set(X509_get_serialNumber(x),serial);
	X509_gmtime_adj(X509_get_notBefore(x),0);
	X509_gmtime_adj(X509_get_notAfter(x),(long)60*60*24*days);
	X509_set_pubkey(x,pk);

	name=X509_get_subject_name(x);

	/* This function creates and adds the entry, working out the
	 * correct string type and performing checks on its length.
	 * Normally we'd check the return value for errors...
	 */
	X509_NAME_add_entry_by_txt(name,"C",
				MBSTRING_ASC,(const unsigned char *) "UK", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name,"CN",
				MBSTRING_ASC, (const unsigned char *) _cert_cn, -1, -1, 0);

	/* Its self signed so set the issuer name to be the same as the
 	 * subject.
	 */
	X509_set_issuer_name(x,name);

	/* Add various extensions: standard extensions */
	add_ext(x, NID_basic_constraints, "critical,CA:TRUE");
	add_ext(x, NID_key_usage, "critical,keyCertSign,cRLSign");

	add_ext(x, NID_subject_key_identifier, "hash");

	/* Some Netscape specific extensions */
	add_ext(x, NID_netscape_cert_type, "sslCA");

	add_ext(x, NID_netscape_comment, "example comment extension");


#ifdef CUSTOM_EXT
	/* Maybe even add our own extension based on existing */
	{
		int nid;
		nid = OBJ_create("1.2.3.4", "MyAlias", "My Test Alias Extension");
		X509V3_EXT_add_alias(nid, NID_netscape_comment);
		add_ext(x, nid, "example comment alias");
	}
#endif
	
	if (!X509_sign(x,pk,EVP_sha1()))
		goto err;

	*x509p=x;
	*pkeyp=pk;
	return(1);
err:
	return(0);
}

static qeo_mgmt_client_retcode_t _enroll_ret[2];

static qeo_mgmt_client_ctx_t* qeo_mgmt_client_init_cb(int cmock_num_calls)
{
    return (qeo_mgmt_client_ctx_t*)0xdeadbabe; // TODO
}

static void qeo_mgmt_client_clean_cb(qeo_mgmt_client_ctx_t* ctx, int cmock_num_calls)
{
    // TODO
}

static bool _ignore_otp_check;
static const char *_expected_otp;
static const char *_expected_url;
static qeo_mgmt_client_retcode_t enroll_scep_cb(
                                       qeo_mgmt_client_ctx_t *ctx,
                                       const char *url,
                                       const EVP_PKEY *pkey,
                                       const char *otp,
                                       const qeo_platform_device_info *info,
                                       qeo_mgmt_client_ssl_ctx_cb cb,
                                       void *cookie,
                                       STACK_OF(X509) *certificate_chain, int cmock_num_calls){

    //create stack of certicates and add created certificate
    
    if (_ignore_otp_check == true){
        ck_assert_str_eq(otp, _expected_otp);
        ck_assert_str_eq(url, _expected_url);
    }
    
    _used_key = (EVP_PKEY *)pkey;
    if (_enroll_ret[cmock_num_calls] == QMGMTCLIENT_OK){
        X509 *x509=NULL;
        if (mkcert(&x509, (EVP_PKEY **)&pkey, 0 /* does not apply, we have our own key */, 6666, 10) == 0){
            printf("mkcert() failed\r\n");
            return QMGMTCLIENT_EFAIL;
        }
        

        if (sk_X509_push(certificate_chain, x509) == 0) {
            printf("sk_X509_push failed\r\n");
            return QMGMTCLIENT_EFAIL;
        }
    }

    return _enroll_ret[cmock_num_calls]; 
}

static bool _reg_cred_cancel;
static qeo_retcode_t _set_reg_cred_cancel_expected;
static qeo_platform_security_registration_credentials _reg_cred;
static qeo_retcode_t _set_reg_cred_expected;
static qeo_util_retcode_t security_get_registration_credentials_cb(qeo_platform_security_context_t context,
                                                                   qeo_platform_security_set_registration_credentials_cb set_reg_cred_cb, 
                                                                   qeo_platform_security_registration_credentials_cancelled_cb set_reg_cred_cancel_cb, 
                                                                   int cmock_num_calls){

    if (_reg_cred_cancel == true){
        ck_assert_int_eq(set_reg_cred_cancel_cb(context), _set_reg_cred_cancel_expected);

    } else {
        ck_assert_int_eq(set_reg_cred_cb(context, &_reg_cred), _set_reg_cred_expected);
    }

    return QEO_UTIL_OK;
}

static char *get_common_name_from_certificate(X509 *cert, char *buf, size_t len)
{
    X509_NAME *cert_subject = NULL;

    cert_subject = X509_get_subject_name(cert);
    if (cert_subject == NULL) {
        qeo_log_e("Certificate subject was not set");
        return NULL;
    }

    /* WARNING: THIS FUNCTION CAN ONLY BE USED IF YOU ARE SURE THE COMMON NAME IS A PLAIN ASCII STRING - NO FANCY BMPSTRING OR UTF8STRING */
    if (X509_NAME_get_text_by_NID(cert_subject, NID_commonName, buf, len) == 0) {
        qeo_log_e("Could not copy certifcate subject");
        return NULL;
    }

    return buf;
}

static bool get_ids_from_certificate(X509 *cert, int64_t *realm_id, int64_t *device_id, int64_t *user_id)
{
    char  own_cert_common_name[50];
    char  *ptr = NULL;

    if (get_common_name_from_certificate(cert, own_cert_common_name, sizeof(own_cert_common_name)) == NULL) {
        qeo_log_e("Could not get common name");
        return false;
    }

    ptr       = own_cert_common_name;
    *realm_id = strtoll(ptr, NULL, 16);
    ptr       = strchr(ptr, ' ');
    if (ptr == NULL) {
        qeo_log_e("Could not find device id");
        return false;
    }

    ++ptr;
    *device_id  = strtoll(ptr, NULL, 16);
    ptr         = strchr(ptr, ' ');
    if (ptr == NULL) {
        qeo_log_e("Could not find device id");
        return false;
    }
    ++ptr;
    *user_id = strtoll(ptr, NULL, 16);

    return true;
}

static qeo_mgmt_cert_retcode_t qeo_mgmt_cert_parse_cb(STACK_OF(X509)* chain, qeo_mgmt_cert_contents* contents, int cmock_num_calls){

    /* will this work ?*/
/*
    contents->realm = 0xab;
    contents->device = 0xcd;
    contents->user = 0xef;
*/
    get_ids_from_certificate(sk_X509_value(chain,0), &contents->realm,&contents->device,&contents->user);

    return QCERT_OK;

}

static qeo_platform_security_state_reason _failure_reason ;
static void platform_security_update_state_cb(qeo_platform_security_context_t context, qeo_platform_security_state state, qeo_platform_security_state_reason state_reason, int cmock_num_calls){

    if (state == QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE){
        ck_assert_int_eq(state_reason, _failure_reason);
    }

}

static qeo_remote_registration_init_cfg_t _remote_reg_init_cfg;
static qeo_retcode_t remote_registration_init_cb(const qeo_remote_registration_init_cfg_t* init_cfg, int cmock_num_calls){
    
    memcpy(&_remote_reg_init_cfg, init_cfg, sizeof(_remote_reg_init_cfg));

    return QEO_OK;

}

static qeo_remote_registration_cfg_t _remote_reg_cfg;
static int _remote_reg_construct_called;
static int _remote_reg_construct_expected;
static const char *_suggested_username_expected;
static unsigned long _registration_window_expected;
static qeo_retcode_t remote_registration_construct_cb(const qeo_remote_registration_cfg_t *cfg, qeo_remote_registration_hndl_t *remote_reg, int cmock_num_calls){

    memcpy(&_remote_reg_cfg, cfg, sizeof(_remote_reg_cfg));
    *remote_reg = (qeo_remote_registration_hndl_t)0xdeadbabe;
    ++_remote_reg_construct_called;
    ck_assert_str_eq(cfg->suggested_username, _suggested_username_expected);
    ck_assert_int_eq(cfg->registration_window, _registration_window_expected);

    return QEO_OK;
}

static int _remote_reg_start_called;
static int _remote_reg_start_expected;
static qeo_retcode_t remote_registration_start_cb(qeo_remote_registration_hndl_t remote_reg, int cmock_num_calls){

    ck_assert_int_eq(remote_reg,0xdeadbabe);
    ++_remote_reg_start_called;


    return QEO_OK;
}

static qeo_retcode_t remote_registration_get_user_data_cb(qeo_remote_registration_hndl_t remote_reg, uintptr_t *user_data, int cmock_num_calls){

    ck_assert_int_eq(remote_reg,0xdeadbabe);
    *user_data = _remote_reg_cfg.user_data;
    return QEO_OK;

}

static qeo_remote_registration_status_t _rr_states[8];
static qeo_remote_registration_failure_reason_t _rr_failure_reasons[8];
static int _remote_reg_update_called;
static int _remote_reg_update_expected;
static char *_remote_reg_otp="222333444";
static char *_remote_reg_url="https://myurl";
static char *_remote_reg_realm_name="mytestrealm";
static const char *_expected_realm_name;

static void simulate_remote_registration_credentials_received(){

    pthread_detach(pthread_self());
    _remote_reg_init_cfg.on_qeo_registration_credentials((qeo_remote_registration_hndl_t)0xdeadbabe, _remote_reg_otp, _remote_reg_realm_name, _remote_reg_url);

}

static bool _no_remote_reg_cred;
static qeo_retcode_t update_remote_registration_status_cb(qeo_remote_registration_hndl_t remote_reg, qeo_remote_registration_status_t status, qeo_remote_registration_failure_reason_t reason, int cmock_num_calls){

    
    ck_assert_int_eq(remote_reg,0xdeadbabe);
    ck_assert_int_eq(status, _rr_states[cmock_num_calls]);
    ck_assert_int_eq(status, _rr_states[cmock_num_calls]);

    _remote_reg_update_called= cmock_num_calls + 1;

    if (_no_remote_reg_cred){
        return QEO_OK;
    }

    _expected_otp = _remote_reg_otp;
    _expected_url = _remote_reg_url;
    _expected_realm_name = _remote_reg_realm_name;

    pthread_t key_gen_thread;
        
    if (status == QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED){
        /* have to do it from different thread */
        ck_assert_int_eq(pthread_create(&key_gen_thread, NULL, (void *(*)(void *))simulate_remote_registration_credentials_received, NULL), 0);
    }

    return QEO_OK;

}

static int _remote_reg_cred_confirmation_called;
static int _remote_reg_cred_confirmation_expected;
static bool _remote_reg_confirmation_feedback[8];
static qeo_util_retcode_t security_get_confirmation_cb(qeo_platform_security_context_t context, const qeo_platform_security_remote_registration_credentials_t* rrcred, qeo_platform_security_remote_registration_credentials_feedback_cb cb, int cmock_num_calls){

    ck_assert_str_eq(rrcred->realm_name, _expected_realm_name);
    ck_assert_str_eq(rrcred->url, _expected_url);
    _remote_reg_cred_confirmation_called = cmock_num_calls + 1;

    return cb(context, _remote_reg_confirmation_feedback[cmock_num_calls]);

}

/*########################################################################
 #                                                                       #
 #  CALLBACK IMPLEMENTATION#
 #                                                                       #
 ########################################################################*/

static int _state_counter;
static qeo_security_state _sec_states[20];
static int _security_status_cb_counter;
static void security_status_cb(qeo_security_hndl qeoSec, qeo_security_state status){

    ++_security_status_cb_counter;
    ck_assert_int_eq(status, _sec_states[_state_counter++]);

}


static void initMock(void){

    Mockmgmt_client_Init();
    qeo_mgmt_client_init_StubWithCallback(qeo_mgmt_client_init_cb);
    qeo_mgmt_client_clean_StubWithCallback(qeo_mgmt_client_clean_cb);
    qeo_mgmt_client_enroll_device_StubWithCallback(enroll_scep_cb);
    qeo_platform_security_registration_credentials_needed_StubWithCallback(security_get_registration_credentials_cb);
    qeo_platform_security_update_state_StubWithCallback(platform_security_update_state_cb);
    qeo_platform_security_remote_registration_confirmation_needed_StubWithCallback(security_get_confirmation_cb);

    Mockmgmt_cert_parser_Init();
    qeo_mgmt_cert_parse_StubWithCallback(qeo_mgmt_cert_parse_cb);


    qeo_remote_registration_init_StubWithCallback(remote_registration_init_cb);
    qeo_remote_registration_construct_StubWithCallback(remote_registration_construct_cb);
    qeo_remote_registration_start_StubWithCallback(remote_registration_start_cb);
    qeo_remote_registration_get_user_data_StubWithCallback(remote_registration_get_user_data_cb);
    qeo_remote_registration_update_registration_status_StubWithCallback(update_remote_registration_status_cb);
    qeo_remote_registration_enable_using_new_registration_credentials_IgnoreAndReturn(QEO_OK);
}

static void destroyMock(void){

    ck_assert_int_eq(_remote_reg_construct_called, _remote_reg_construct_expected);
    ck_assert_int_eq(_remote_reg_start_called, _remote_reg_start_expected);
    ck_assert_int_eq(_remote_reg_update_called, _remote_reg_update_expected);
    ck_assert_int_eq(_remote_reg_cred_confirmation_called, _remote_reg_cred_confirmation_expected);

    Mockmgmt_client_Verify();
    Mockmgmt_client_Destroy();

    Mockmgmt_cert_parser_Verify();
    Mockmgmt_cert_parser_Destroy();
}

/*########################################################################
 #                                                                       #
 #  FIXTURE IMPLEMENTATION#
 #                                                                       #
 ########################################################################*/

static qeo_security_hndl _qeo_security;
static bool _destruct_possible;

static void testConstructNoRealm(void)
{
    const qeo_security_config cfg = {
        .id = {
            .realm_id = 0,
            .device_id = 0,
            .user_id = 0,
            .friendly_name = ""
        },
        .security_status_cb = security_status_cb,
    };

    ck_assert_int_eq(qeo_security_construct(&cfg, &_qeo_security), QEO_OK);
    ck_assert_int_ne(_qeo_security, NULL);
};

static void testConstructWithValidRealm(void)
{
    const qeo_security_config cfg = {
        .id= {
            .realm_id = 0xab,
            .device_id = 0xcd,
            .user_id = 0xef,
            .friendly_name = ""
        },
        .security_status_cb = security_status_cb,
    };

    ck_assert_int_eq(qeo_security_construct(&cfg, &_qeo_security), QEO_OK);
    ck_assert_int_ne(_qeo_security, NULL);
};
    
static void testConstructWithInvalidRealm(void)
{
    const qeo_security_config cfg = {
        .id = {
            .realm_id = 0xdeadbeef,
            .device_id = 0xac1dbabc,
            .user_id = 0xac1dbabe,
            .friendly_name = ""
        },
        .security_status_cb = security_status_cb,
    };

    ck_assert_int_eq(qeo_security_construct(&cfg, &_qeo_security), QEO_OK);
    ck_assert_int_ne(_qeo_security, NULL);
};
    
static void testDestruct(void)
{                                                        
    if (_destruct_possible == true){
        ck_assert_int_eq(qeo_security_destruct(&_qeo_security), QEO_OK);
        ck_assert_int_eq(_qeo_security, NULL);
    } else {
        ck_assert_int_eq(qeo_security_destruct(&_qeo_security), QEO_EBADSTATE);
        ck_assert_int_ne(_qeo_security, NULL);
    }
}

static qeo_security_hndl _qeo_security_fixture;
/* this only works with the 'current' linux implementation of platform api */
static void clean_qeo_credentials(void)
{
    char* path = NULL;

    fail_if(QEO_UTIL_OK != qeo_platform_get_device_storage_path("truststore.p12", &path), "Could not open truststore");
    ck_assert_int_eq(qeo_security_destruct(&_qeo_security_fixture), QEO_OK);
    unlink(path);
    free(path);
}


/* copied from authentication_scep_successful */
static void authenticate_successfully(void){

    clean_qeo_credentials();

    const qeo_security_config cfg = {
        .id = {
            .realm_id = 0,
            .device_id = 0,
            .user_id = 0,
            .friendly_name = ""
        },
        .security_status_cb = security_status_cb,
    };

    ck_assert_int_eq(qeo_security_construct(&cfg, &_qeo_security_fixture), QEO_OK);
    ck_assert_int_ne(_qeo_security_fixture, NULL);

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[3] = QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS;
    _sec_states[4] = QEO_SECURITY_STORING_QEO_CREDENTIALS;
    _sec_states[5] = QEO_SECURITY_AUTHENTICATED;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP;
    _reg_cred.u.otp.url = "https://blablaa";
    _reg_cred.u.otp.otp = "12345";
    _set_reg_cred_expected = QEO_OK;
    _enroll_ret[0] = QMGMTCLIENT_OK;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security_fixture), QEO_OK);
    while (_security_status_cb_counter < 6) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */
    ck_assert_int_eq(_security_status_cb_counter, 6);

    /* return to pristine state */
    _security_status_cb_counter = 0;
    _state_counter = 0;
    memset(_sec_states, 0, sizeof(_sec_states));
    _used_key = NULL;


}

/*########################################################################
 #                                                                       #
 #  TEST IMPLEMENTATION#
 #                                                                       #
 ########################################################################*/


START_TEST(authentication_inv_args)
{
    ck_assert_int_eq(qeo_security_authenticate(NULL), QEO_EINVAL);
    _destruct_possible = true;
}
END_TEST

START_TEST(authentication_otp_cancel)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity  *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_AUTHENTICATION_FAILURE;
    _reg_cred_cancel = true;
    _set_reg_cred_cancel_expected = QEO_OK;
    _failure_reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_CANCELLED;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    sleep(1);
    _destruct_possible = true;
    while (_security_status_cb_counter < 3) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */

    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_EBADSTATE);
    ck_assert_int_eq(key, NULL);
    ck_assert_int_eq(certs, NULL);
    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_EBADSTATE);
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_EINVAL);
}
END_TEST

START_TEST(authentication_scep_failure_otp_invalid)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[3] = QEO_SECURITY_AUTHENTICATION_FAILURE;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP;
    _expected_url = _reg_cred.u.otp.url = "https://blablaa";
    _expected_otp = _reg_cred.u.otp.otp = "12345";
    _set_reg_cred_expected = QEO_OK;
    _ignore_otp_check = true;
    _failure_reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_INVALID_OTP;
    _enroll_ret[0] = QMGMTCLIENT_EOTP;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    while (_security_status_cb_counter < 4) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */
    _destruct_possible = true;
    ck_assert_int_eq(_security_status_cb_counter, 4);

    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_EBADSTATE);
    ck_assert_int_eq(key, NULL);
    ck_assert_int_eq(certs, NULL);
    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_EBADSTATE);
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_EINVAL);
}
END_TEST

START_TEST(authentication_scep_successful)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[3] = QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS;
    _sec_states[4] = QEO_SECURITY_STORING_QEO_CREDENTIALS;
    _sec_states[5] = QEO_SECURITY_AUTHENTICATED;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP;
    _expected_url = _reg_cred.u.otp.url = "https://blablaa";
    _expected_otp = _reg_cred.u.otp.otp = "12345";
    _set_reg_cred_expected = QEO_OK;
    _enroll_ret[0] = QMGMTCLIENT_OK;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    while (_security_status_cb_counter < 6) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */
    _destruct_possible = true;
    ck_assert_int_eq(_security_status_cb_counter, 6);

    
    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_OK);
    ck_assert_int_ne(key, NULL);
    ck_assert_int_ne(certs, NULL);
    ck_assert_int_eq(sk_X509_num(certs), 1); /* self-signed */
    ck_assert_int_eq(EVP_PKEY_cmp(key, _used_key), 1); /* YES, IT RETURNS 1 IF IT IS EQUAL !!! */

    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_OK);
    ck_assert_int_eq(id->realm_id, 0xab);
    ck_assert_int_eq(id->device_id, 0xcd);
    ck_assert_int_eq(id->user_id, 0xef);
    ck_assert_str_eq(id->friendly_name, "<rid:ab><did:cd><uid:ef>");
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_OK);

    clean_qeo_credentials();
    

}
END_TEST

START_TEST(remote_reg_authentication_scep_successful)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[3] = QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS;
    _sec_states[4] = QEO_SECURITY_STORING_QEO_CREDENTIALS;
    _sec_states[5] = QEO_SECURITY_AUTHENTICATED;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION;
    _registration_window_expected = _reg_cred.u.remote_registration.registration_window = 10;
    _suggested_username_expected = _reg_cred.u.remote_registration.suggested_username = "john doe";
    _set_reg_cred_expected = QEO_OK;
    _remote_reg_construct_expected = 1;
    _remote_reg_start_expected = 1;
    _remote_reg_cred_confirmation_expected = 1;

    _rr_states[0] = QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED;
    _rr_failure_reasons[0] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE;
    _rr_states[1] = QEO_REMOTE_REGISTRATION_STATUS_REGISTERING;
    _rr_failure_reasons[1] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE;
    _remote_reg_update_expected = 2;

    _enroll_ret[0] = QMGMTCLIENT_OK;
    _remote_reg_confirmation_feedback[0] = true;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    while (_security_status_cb_counter < 6) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */
    _destruct_possible = true;
    ck_assert_int_eq(_security_status_cb_counter, 6);

    
    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_OK);
    ck_assert_int_ne(key, NULL);
    ck_assert_int_ne(certs, NULL);
    ck_assert_int_eq(sk_X509_num(certs), 1); /* self-signed */
    ck_assert_int_eq(EVP_PKEY_cmp(key, _used_key), 1); /* YES, IT RETURNS 1 IF IT IS EQUAL !!! */

    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_OK);
    ck_assert_int_eq(id->realm_id, 0xab);
    ck_assert_int_eq(id->device_id, 0xcd);
    ck_assert_int_eq(id->user_id, 0xef);
    ck_assert_str_eq(id->friendly_name, "<rid:ab><did:cd><uid:ef>");
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_OK);

    clean_qeo_credentials();
    

}
END_TEST

START_TEST(remote_reg_timeout)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity  *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_AUTHENTICATION_FAILURE;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION;
    _registration_window_expected = _reg_cred.u.remote_registration.registration_window = 1;
    _suggested_username_expected = _reg_cred.u.remote_registration.suggested_username = "john doe";
    _set_reg_cred_expected = QEO_OK;
    _remote_reg_construct_expected = 1;
    _remote_reg_start_expected = 1;
    _no_remote_reg_cred = true;
    
    _rr_states[0] = QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED;
    _rr_failure_reasons[0] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE;
    _rr_states[1] = QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED;
    _rr_failure_reasons[1] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT;
    _remote_reg_update_expected = 2;

    _failure_reason = QEO_PLATFORM_SECURITY_AUTHENTICATION_FAILURE_REASON_REMOTE_REGISTRATION_TIMEOUT;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    sleep(1);
    
    _remote_reg_init_cfg.on_qeo_registration_timeout((qeo_remote_registration_hndl_t)0xdeadbabe);
    _destruct_possible = true;
    while (_security_status_cb_counter < 3) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */

    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_EBADSTATE);
    ck_assert_int_eq(key, NULL);
    ck_assert_int_eq(certs, NULL);
    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_EBADSTATE);
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_EINVAL);
}
END_TEST

/* 1. first otp is rejected with negative feedback 
 * 2. second otp is okay but scep fails
 * 3. third otp is okay and scep passes
 */

START_TEST(remote_reg_authentication_negative_feedback_scep_successful_after_retry)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[2] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[3] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[4] = QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS;
    _sec_states[5] = QEO_SECURITY_STORING_QEO_CREDENTIALS;
    _sec_states[6] = QEO_SECURITY_AUTHENTICATED;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_REMOTE_REGISTRATION;
    _registration_window_expected = _reg_cred.u.remote_registration.registration_window = 10;
    _suggested_username_expected = _reg_cred.u.remote_registration.suggested_username = "john doe";
    _set_reg_cred_expected = QEO_OK;
    _remote_reg_construct_expected = 1;
    _remote_reg_start_expected = 1;
    _remote_reg_cred_confirmation_expected = 3;
    _rr_states[0] = QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED;
    _rr_failure_reasons[0] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE;
    _rr_states[1] = QEO_REMOTE_REGISTRATION_STATUS_REGISTERING;
    _rr_failure_reasons[1] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NONE;
    _rr_states[2] = QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED;
    _rr_failure_reasons[2] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NEGATIVE_CONFIRMATION;
    _rr_states[3] = QEO_REMOTE_REGISTRATION_STATUS_REGISTERING;
    _rr_failure_reasons[3] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_NEGATIVE_CONFIRMATION;
    _rr_states[4] = QEO_REMOTE_REGISTRATION_STATUS_UNREGISTERED;
    _rr_failure_reasons[4] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_INVALID_OTP;
    _rr_states[5] = QEO_REMOTE_REGISTRATION_STATUS_REGISTERING;
    _rr_failure_reasons[5] = QEO_REMOTE_REGISTRATION_FAILURE_REASON_INVALID_OTP;
    _remote_reg_update_expected = 6;


    _enroll_ret[0] = QMGMTCLIENT_EOTP;
    _enroll_ret[1] = QMGMTCLIENT_OK;
    _remote_reg_confirmation_feedback[0] = false;
    _remote_reg_confirmation_feedback[1] = true;
    _remote_reg_confirmation_feedback[2] = true;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    while (_security_status_cb_counter < 7) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */
    _destruct_possible = true;
    ck_assert_int_eq(_security_status_cb_counter, 7);

    
    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_OK);
    ck_assert_int_ne(key, NULL);
    ck_assert_int_ne(certs, NULL);
    ck_assert_int_eq(sk_X509_num(certs), 1); /* self-signed */
    ck_assert_int_eq(EVP_PKEY_cmp(key, _used_key), 1); /* YES, IT RETURNS 1 IF IT IS EQUAL !!! */

    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_OK);
    ck_assert_int_eq(id->realm_id, 0xab);
    ck_assert_int_eq(id->device_id, 0xcd);
    ck_assert_int_eq(id->user_id, 0xef);
    ck_assert_str_eq(id->friendly_name, "<rid:ab><did:cd><uid:ef>");
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_OK);

    clean_qeo_credentials();
    

}
END_TEST

START_TEST(authentication_from_storage_successful)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS;
    _sec_states[1] = QEO_SECURITY_VERIFYING_LOADED_QEO_CREDENTIALS;
    _sec_states[2] = QEO_SECURITY_AUTHENTICATED;
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    sleep(3); /*  even better is to use proper synchronization mechanisms */
    _destruct_possible = true;
    ck_assert_int_eq(_security_status_cb_counter, 3);


    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_OK);
    ck_assert_int_ne(key, NULL);
    ck_assert_int_ne(certs, NULL);
    ck_assert_int_eq(sk_X509_num(certs), 1); /* self-signed */
    
    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_OK);
    ck_assert_int_eq(id->realm_id, 0xab);
    ck_assert_int_eq(id->device_id, 0xcd);
    ck_assert_int_eq(id->user_id, 0xef);
    ck_assert_str_eq(id->friendly_name, "<rid:ab><did:cd><uid:ef>");
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_OK);


}
END_TEST

START_TEST(authentication_from_storage_scep_fallback)
{
    EVP_PKEY *key = NULL;
    STACK_OF(X509) *certs = NULL;
    qeo_security_identity *id = NULL;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    _sec_states[0] = QEO_SECURITY_TRYING_TO_LOAD_STORED_QEO_CREDENTIALS;
    _sec_states[1] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_GENERATING_KEY;
    _sec_states[2] = QEO_SECURITY_RETRIEVING_REGISTRATION_CREDENTIALS_KEY_GENERATED;
    _sec_states[3] = QEO_SECURITY_WAITING_FOR_SIGNED_CERTIFICATE;
    _sec_states[4] = QEO_SECURITY_VERIFYING_RECEIVED_QEO_CREDENTIALS;
    _sec_states[5] = QEO_SECURITY_STORING_QEO_CREDENTIALS;
    _sec_states[6] = QEO_SECURITY_AUTHENTICATED;
    _reg_cred.reg_method = QEO_PLATFORM_SECURITY_REGISTRATION_METHOD_OTP;
    _expected_url = _reg_cred.u.otp.url = "https://blablaa";
    _expected_otp = _reg_cred.u.otp.otp = "12345";
    _set_reg_cred_expected = QEO_OK;
    _enroll_ret[0] = QMGMTCLIENT_OK;
    _cert_cn = "deadbeef babecafe ac1dbabe";
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_OK);
    ck_assert_int_eq(qeo_security_authenticate(_qeo_security), QEO_EBADSTATE);
    while (_security_status_cb_counter < 7) { sleep(1); } /* key generation may take quite some time; even better is to use proper synchronization mechanisms */
    _destruct_possible = true;
    ck_assert_int_eq(_security_status_cb_counter, 7);

    ck_assert_int_eq(qeo_security_get_credentials(_qeo_security, &key, &certs), QEO_OK);
    ck_assert_int_ne(key, NULL);
    ck_assert_int_ne(certs, NULL);
    ck_assert_int_eq(sk_X509_num(certs), 1); /* self-signed */
    ck_assert_int_eq(EVP_PKEY_cmp(key, _used_key), 1); /* YES, IT RETURNS 1 IF IT IS EQUAL !!! */

    ck_assert_int_eq(qeo_security_get_identity(_qeo_security, &id), QEO_OK);
    ck_assert_int_eq(id->realm_id, 0xdeadbeef);
    ck_assert_int_eq(id->device_id, 0xbabecafe);
    ck_assert_int_eq(id->user_id, 0xac1dbabe);
    ck_assert_str_eq(id->friendly_name, "<rid:deadbeef><did:babecafe><uid:ac1dbabe>");
    ck_assert_int_eq(qeo_security_free_identity(&id), QEO_OK);
}
END_TEST

START_TEST(retrieve_realms_realms_present){

    
    qeo_security_identity *id;
    unsigned int len;

    qeocore_remote_registration_init_cond_Ignore();
    qeocore_remote_registration_set_key_Ignore();
    ck_assert_int_eq(qeo_security_get_realms(&id, &len), QEO_OK);
    ck_assert_int_eq(len, 1);
    ck_assert_str_eq(id[0].friendly_name, "<rid:ab><did:cd><uid:ef>");
    ck_assert_int_eq(id[0].realm_id, 0xab);
    ck_assert_int_eq(id[0].device_id, 0xcd);
    ck_assert_int_eq(id[0].user_id, 0xef);

    
    ck_assert_int_eq(qeo_security_free_realms(&id, len), QEO_OK);
    _destruct_possible = true;

}
END_TEST

START_TEST(retrieve_realms_realms_none_present){

    
    qeo_security_identity *id;
    unsigned int len;

    ck_assert_int_eq(qeo_security_get_realms(&id, &len), QEO_OK);
    ck_assert_int_eq(len, 0);

    
    ck_assert_int_eq(qeo_security_free_realms(&id, len), QEO_OK);
    _destruct_possible = true;

}
END_TEST

void register_authenticationtests(Suite *s)
{
	TCase *testCase = NULL;

    testCase = tcase_create("Security Lib authentication tests");
    tcase_add_checked_fixture(testCase, initGlobalMocks, destroyGlobalMocks);
    tcase_add_checked_fixture(testCase, initMock, destroyMock);
    tcase_add_checked_fixture(testCase, testInitLib, testDestroyLib);
    tcase_add_checked_fixture(testCase, testConstructNoRealm, testDestruct);
    tcase_add_test(testCase, authentication_inv_args);
    tcase_add_test(testCase, authentication_otp_cancel);
    tcase_add_test(testCase, authentication_scep_failure_otp_invalid);
    tcase_add_test(testCase, retrieve_realms_realms_none_present); 
    tcase_add_test(testCase, authentication_scep_successful);
    tcase_add_test(testCase, remote_reg_authentication_scep_successful);
    tcase_add_test(testCase, remote_reg_authentication_negative_feedback_scep_successful_after_retry);
    tcase_add_test(testCase, remote_reg_timeout);
    suite_add_tcase(s, testCase);

    testCase = tcase_create("Security Lib authentication tests with valid stored credentials");
    tcase_add_checked_fixture(testCase, initGlobalMocks, destroyGlobalMocks);
    tcase_add_checked_fixture(testCase, initMock, destroyMock);
    tcase_add_checked_fixture(testCase, testInitLib, testDestroyLib);
    tcase_add_checked_fixture(testCase, testConstructWithValidRealm, testDestruct);
    tcase_add_checked_fixture(testCase, authenticate_successfully, clean_qeo_credentials);
    tcase_add_test(testCase, authentication_from_storage_successful);
    tcase_add_test(testCase, retrieve_realms_realms_present);
    suite_add_tcase(s, testCase);

    testCase = tcase_create("Security Lib authentication tests with invalid stored credentials");
    tcase_add_checked_fixture(testCase, initGlobalMocks, destroyGlobalMocks);
    tcase_add_checked_fixture(testCase, initMock, destroyMock);
    tcase_add_checked_fixture(testCase, testInitLib, testDestroyLib);
    tcase_add_checked_fixture(testCase, testConstructWithInvalidRealm, testDestruct);
    tcase_add_checked_fixture(testCase, authenticate_successfully, clean_qeo_credentials);
    tcase_add_test(testCase, authentication_from_storage_scep_fallback);
    suite_add_tcase(s, testCase);

}
