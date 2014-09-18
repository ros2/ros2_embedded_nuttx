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

/* sec_main.c -- Security checks for DDS. */

#include <stdio.h>
#include "log.h"
#include "error.h"
#include "nmatch.h"
#include "disc.h"
#include "security.h"
#include "sec_data.h"
#include "sec_id.h"
#include "sec_perm.h"
#include "sec_auth.h"
#include "sec_access.h"
#include "sec_crypto.h"
#include "sec_log.h"

#ifdef DDS_SECURITY

#define	LOG_DATE

static int ready_to_reeval = 1;

int dds_security = 1;

DDS_TypeSupport_meta BinaryProperty_tsm [] = {
	{CDR_TYPECODE_STRUCT,   TSMFLAG_DYNAMIC, "BinaryProperty", sizeof (DDS_BinaryProperty), 0, 2, 0, NULL},
	{CDR_TYPECODE_CSTRING,  TSMFLAG_DYNAMIC, "key", 0, offsetof (DDS_BinaryProperty, key), 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "value", 0, offsetof (DDS_BinaryProperty, value), 0, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL }
};

DDS_TypeSupport_meta Property_tsm [] = {
	{CDR_TYPECODE_STRUCT,  TSMFLAG_DYNAMIC, "Property", sizeof (DDS_Property), 0, 2, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_DYNAMIC, "key", 0, offsetof (DDS_Property, key), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING, TSMFLAG_DYNAMIC, "value", 0, offsetof (DDS_Property, value), 0, 0, NULL}
};

DDS_TypeSupport_meta DataHolder_tsm [] = {
	{CDR_TYPECODE_STRUCT,   TSMFLAG_DYNAMIC | TSMFLAG_MUTABLE, "DataHolder", sizeof (DDS_DataHolder), 0, 7, 0, NULL},
	{CDR_TYPECODE_CSTRING,  TSMFLAG_DYNAMIC, "class_id", 0, offsetof (DDS_DataHolder, class_id), 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_OPTIONAL, "string_properties", 0, offsetof (DDS_DataHolder, string_properties), 0, 0, NULL},
	{CDR_TYPECODE_TYPEREF,  0, NULL, 0, 0, 0, 0, Property_tsm },
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_OPTIONAL, "binary_properties", 0, offsetof (DDS_DataHolder, binary_properties), 0, 0, NULL},
	{CDR_TYPECODE_TYPEREF,  0, NULL, 0, 0, 0, 0, BinaryProperty_tsm },
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_OPTIONAL, "string_values", 0, offsetof (DDS_DataHolder, string_values), 0, 0, NULL},
	{CDR_TYPECODE_CSTRING,  TSMFLAG_DYNAMIC, NULL, 0, 0, 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_OPTIONAL, "binary_value1", 0, offsetof (DDS_DataHolder, binary_value1), 0, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL },
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_OPTIONAL, "binary_value2", 0, offsetof (DDS_DataHolder, binary_value2), 0, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL },
	{CDR_TYPECODE_SEQUENCE, TSMFLAG_OPTIONAL, "longlongs_value", 0, offsetof (DDS_DataHolder, longlongs_value), 0, 0, NULL},
	{CDR_TYPECODE_LONGLONG,    0, NULL, 0, 0, 0, 0, NULL }
};

DDS_TypeSupport_meta MessageIdentity_tsm [] = {
	{CDR_TYPECODE_STRUCT,   0, "MessageIdentity", sizeof (DDS_MessageIdentity), 0, 2, 0, NULL},
	{CDR_TYPECODE_ARRAY,    0, "source_guid", 0, offsetof (DDS_MessageIdentity, source_guid), 16, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL },
	{CDR_TYPECODE_LONGLONG, 0, "sequence_number", 0, offsetof (DDS_MessageIdentity, sequence_number), 0, 0, NULL}
};

DDS_TypeSupport_meta ParticipantGenericMessage_tsm [] = {
	{CDR_TYPECODE_STRUCT,   0, "ParticipantGenericMessage", sizeof (DDS_ParticipantGenericMessage), 0, 7, 0, NULL},
	{CDR_TYPECODE_TYPEREF,  0, "message_identity", 0, offsetof (DDS_ParticipantGenericMessage, message_identity), 0, 0, MessageIdentity_tsm},
	{CDR_TYPECODE_TYPEREF,  0, "related_message_identity", 0, offsetof (DDS_ParticipantGenericMessage, related_message_identity), 0, 0, MessageIdentity_tsm},
	{CDR_TYPECODE_ARRAY,    0, "destination_participant_key", 0, offsetof (DDS_ParticipantGenericMessage, destination_participant_key), 16, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL },
	{CDR_TYPECODE_ARRAY,    0, "destination_endpoint_key", 0, offsetof (DDS_ParticipantGenericMessage, destination_endpoint_key), 16, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL },
	{CDR_TYPECODE_ARRAY,    0, "source_endpoint_key", 0, offsetof (DDS_ParticipantGenericMessage, source_endpoint_key), 16, 0, NULL},
	{CDR_TYPECODE_OCTET,    0, NULL, 0, 0, 0, 0, NULL },
	{CDR_TYPECODE_CSTRING,  0, "message_class_id", 0, offsetof (DDS_ParticipantGenericMessage, message_class_id), 0, 0, NULL},
	{CDR_TYPECODE_SEQUENCE, 0, "message_data", 0, offsetof (DDS_ParticipantGenericMessage, message_data), 0, 0, NULL},
	{CDR_TYPECODE_TYPEREF,  0, NULL, 0, 0, 0, 0, DataHolder_tsm}
};

DDS_TypeSupport_meta ParticipantStatelessMessage_tsm [] = {
	{CDR_TYPECODE_TYPEREF,  0, "ParticipantStatelessMessage", 0, 0, 0, 0, ParticipantGenericMessage_tsm}
};

DDS_TypeSupport_meta ParticipantVolatileSecureMessage_tsm [] = {
	{CDR_TYPECODE_TYPEREF,  0, "ParticipantVolatileSecureMessage", 0, 0, 0, 0, ParticipantGenericMessage_tsm}
};

DDS_TypeSupport		Property_ts;
DDS_TypeSupport		BinaryProperty_ts;
DDS_TypeSupport		DataHolder_ts;
DDS_TypeSupport		MessageIdentity_ts;
DDS_TypeSupport		ParticipantGenericMessage_ts;
DDS_TypeSupport		ParticipantStatelessMessage_ts;
DDS_TypeSupport		ParticipantVolatileSecureMessage_ts;

Identity_t		local_identity;
Permissions_t		local_permissions;
DDS_SecurityPolicy	plugin_policy;

static DDS_Credentials	*local_credentials;
static size_t		local_credentials_size;
static char		*local_sec_name;

static DDS_SecurityPluginFct	plugin_fct [2];

/* sec_register_types -- Register the security types. */

DDS_ReturnCode_t sec_register_types (void)
{
	BinaryProperty_ts = DDS_DynamicType_register (BinaryProperty_tsm);
	if (!BinaryProperty_ts) {
		fatal_printf ("Can't register BinaryProperty type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	Property_ts = DDS_DynamicType_register (Property_tsm);
	if (!Property_ts) {
		fatal_printf ("Can't register Property type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_DynamicType_set_type (DataHolder_tsm, 3, Property_ts);
	DDS_DynamicType_set_type (DataHolder_tsm, 5, BinaryProperty_ts);
	DataHolder_ts = DDS_DynamicType_register (DataHolder_tsm);
	if (!DataHolder_ts) {
		fatal_printf ("Can't register BinaryProperty type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	MessageIdentity_ts = DDS_DynamicType_register (MessageIdentity_tsm);
	DDS_DynamicType_set_type (ParticipantGenericMessage_tsm, 1, MessageIdentity_ts);
	DDS_DynamicType_set_type (ParticipantGenericMessage_tsm, 2, MessageIdentity_ts);
	DDS_DynamicType_set_type (ParticipantGenericMessage_tsm, 11, DataHolder_ts);
	ParticipantGenericMessage_ts = DDS_DynamicType_register (ParticipantGenericMessage_tsm);
	if (!ParticipantGenericMessage_ts) {
		fatal_printf ("Can't register ParticipantGenericMessage type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_DynamicType_set_type (ParticipantStatelessMessage_tsm, 0, ParticipantGenericMessage_ts);
	ParticipantStatelessMessage_ts = DDS_DynamicType_register (ParticipantStatelessMessage_tsm);
	if (!ParticipantStatelessMessage_ts) {
		fatal_printf ("Can't register ParticipantStatelessMessage type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	DDS_DynamicType_set_type (ParticipantVolatileSecureMessage_tsm, 0, ParticipantGenericMessage_ts);
	ParticipantVolatileSecureMessage_ts = DDS_DynamicType_register (ParticipantVolatileSecureMessage_tsm);
	if (!ParticipantVolatileSecureMessage_ts) {
		fatal_printf ("Can't register ParticipantVolatileSecureMessage type!");
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	return (DDS_RETCODE_OK);
}

/* sec_unregister_types -- Unregister the security types. */

void sec_unregister_types (void)
{
	DDS_DynamicType_free (ParticipantStatelessMessage_ts);
	DDS_DynamicType_free (ParticipantVolatileSecureMessage_ts);
	DDS_DynamicType_free (ParticipantGenericMessage_ts);
	DDS_DynamicType_free (MessageIdentity_ts);
	DDS_DynamicType_free (DataHolder_ts);
	DDS_DynamicType_free (Property_ts);
	DDS_DynamicType_free (BinaryProperty_ts);
}

#ifndef CDR_ONLY

void DDS_Security_set_library_init (int val)
{
	DDS_SecurityReqData	data;

	data.secure = val;
	sec_aux_request (DDS_SET_LIBRARY_INIT, &data);
}

void DDS_Security_set_library_lock (void)
{
	sec_aux_request (DDS_SET_LIBRARY_LOCK, NULL);
}

void DDS_Security_unset_library_lock (void)
{
	sec_aux_request (DDS_UNSET_LIBRARY_LOCK, NULL);
}

/* DDS_Security_set_policy -- Sets the security policy.  This should be done
			      only once and before any credentials are assigned
			      or any DomainParticipants are created! */

DDS_ReturnCode_t DDS_Security_set_policy (DDS_SecurityPolicy    policy,
					  DDS_SecurityPluginFct fct)
{
	static const char *policyr [] = {
		"No security",
		"Local security checks",
		"Agent-based security checks"
	};
	DDS_ReturnCode_t	ret;

	sec_log_fct ("DDS_Security_set_policy");
	if (policy > DDS_SECURITY_AGENT ||
	    (policy && !fct) || 
	    policy < plugin_policy) {
		sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (policy) {
		log_printf (DDS_ID, 0, "DDS: Security policy update: %s -> %s\r\n", 
					policyr [plugin_policy], policyr [policy]);
		DDS_Security_log (DDS_INFO_LEVEL, "API", "Policy updated");
		plugin_policy = policy;
		plugin_fct [0] = fct;
		(*fct) (DDS_SC_INIT, 0, NULL);
		ret = sec_auth_init ();
		if (!ret)
			ret = sec_perm_init ();
	}
	else
		ret = DDS_RETCODE_OK;

	sec_log_ret ("%d", ret);
	return (ret);
}

/* DDS_Security_set_crypto -- Set the crypto plugin library. */

DDS_ReturnCode_t DDS_Security_set_crypto (DDS_SecurityPluginFct	plugin)
{
	DDS_ReturnCode_t	ret;

	sec_log_fct ("DDS_Security_set_crypto");
	if (!plugin) {
		sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	if (plugin_fct [1]) {
		if (plugin == plugin_fct [1])
			return (DDS_RETCODE_OK);

		DDS_Security_log (DDS_NOTICE_LEVEL, "API", "Crypto library update denied");
		sec_log_ret ("%d", DDS_RETCODE_ILLEGAL_OPERATION);
		return (DDS_RETCODE_ILLEGAL_OPERATION);
	}
	plugin_fct [1] = plugin;
	(*plugin) (DDS_SC_INIT, 0, NULL);
	ret = sec_crypto_init (32, ~0);
	sec_log_ret ("%d", ret);
	return (ret);
}

#define	MAX_CERTIFICATES	10	/* Max. # of certificates that following function
					   accepts. */

DDS_Credentials *DDS_Credentials__copy (DDS_Credentials  *crp,
					size_t           *length,
					DDS_ReturnCode_t *error)
{
	char		*dp;
	const char	*sp;
	unsigned	i, nbOfCert;
	size_t		n, xlength;
	X509            *cert_ori = NULL, *cert_cpy = NULL;
	DDS_Credentials	*credentials;

	if (crp->credentialKind == DDS_FILE_BASED) {
		log_printf (SEC_ID, 0, "Security: set file based credentials\n\r");
		if (!crp->info.filenames.private_key_file ||
	            !crp->info.filenames.certificate_chain_file) {
	 		sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
		xlength = sizeof (DDS_Credentials) +
		          strlen (crp->info.filenames.private_key_file) +
			  strlen (crp->info.filenames.certificate_chain_file) +
			  2;
		credentials = xmalloc (xlength);
		if (!credentials) {
	 		sec_log_ret ("%d", DDS_RETCODE_OUT_OF_RESOURCES);
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
		credentials->credentialKind = DDS_FILE_BASED;
		dp = (char *) (credentials + 1);
		sp = crp->info.filenames.private_key_file;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.filenames.private_key_file = dp;
		dp += n;
		sp = crp->info.filenames.certificate_chain_file;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.filenames.certificate_chain_file = dp;
	}
	else if (crp->credentialKind == DDS_ENGINE_BASED) {
		log_printf (SEC_ID, 0, "Security: set engine based credentials\n\r");
		if (!crp->info.engine.engine_id ||
		    !crp->info.engine.cert_id ||
		    !crp->info.engine.priv_key_id) {
			sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
		xlength = sizeof (DDS_Credentials) +
		          strlen (crp->info.engine.engine_id) +
			  strlen (crp->info.engine.cert_id) +
			  strlen (crp->info.engine.priv_key_id) +
			  3;
		credentials = xmalloc (xlength);
		if (!credentials) {
	 		sec_log_ret ("%d", DDS_RETCODE_OUT_OF_RESOURCES);
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
		credentials->credentialKind = DDS_ENGINE_BASED;
		dp = (char *) (credentials + 1);
		sp = crp->info.engine.engine_id;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.engine.engine_id = dp;
		dp += n;
		sp = crp->info.engine.cert_id;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.engine.cert_id = dp;
		dp += n;
		sp = crp->info.engine.priv_key_id;
		n = strlen (sp) + 1;
		memcpy (dp, sp, n);
		credentials->info.engine.priv_key_id = dp;

		log_printf (SEC_ID, 0, "Security: credentials \n\r cert path = %s \n\r key path = %s \n\r engine_id = %s\n\r", credentials->info.engine.cert_id, credentials->info.engine.priv_key_id, credentials->info.engine.engine_id);
	} 
	else if (crp->credentialKind == DDS_SSL_BASED) {
		log_printf (SEC_ID, 0, "Security: set SSL based credentials\r\n");
		if (!crp->info.sslData.certificate_list ||
		    !crp->info.sslData.private_key) {
			sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
		xlength = sizeof (DDS_Credentials) +
			sizeof (crp->info.sslData.private_key) +
			sizeof (crp->info.sslData.certificate_list);
		credentials = xmalloc (xlength);
		if (!credentials) {
	 		sec_log_ret ("%d", DDS_RETCODE_OUT_OF_RESOURCES);
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
		credentials->credentialKind = DDS_SSL_BASED;

		/* Make a new empty stack object. */
		credentials->info.sslData.certificate_list = sk_X509_new_null ();
		
		nbOfCert = sk_num ((const _STACK*) crp->info.sslData.certificate_list);

		/* Copy every certificate to the new stack. */
		for (i = 0; i < (unsigned) nbOfCert; i++) {
			cert_ori = sk_X509_value (crp->info.sslData.certificate_list, i);
			cert_cpy = X509_dup (cert_ori);
			sk_X509_push (credentials->info.sslData.certificate_list,
				      cert_cpy);
		}

		/* There is no OpenSSL function to copy a private key
		   so just reference to it and then up the reference count. */
		credentials->info.sslData.private_key = crp->info.sslData.private_key;
		credentials->info.sslData.private_key->references++;
		log_printf (SEC_ID, 0, "Security: the private key and certificates are set.\r\n");
	}
	else {
		log_printf (SEC_ID, 0, "Security: set data based credentials\n\r");
		if (!crp->info.data.private_key.data ||
		    !crp->info.data.private_key.length ||
		    !crp->info.data.num_certificates ||
		    !crp->info.data.num_certificates > MAX_CERTIFICATES ||
		    !crp->info.data.certificates [0].data ||
		    !crp->info.data.certificates [0].length) {
			sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
			*error = DDS_RETCODE_BAD_PARAMETER;
			return (NULL);
		}
		xlength = sizeof (DDS_Credentials) +
			  crp->info.data.num_certificates * sizeof (DDS_Credential) +
			  crp->info.data.private_key.length +
			  crp->info.data.certificates [0].length;
		for (i = 1; i < crp->info.data.num_certificates; i++) {
			if (!crp->info.data.certificates [i].data ||
			    !crp->info.data.certificates [i].length) {
				sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
				*error = DDS_RETCODE_BAD_PARAMETER;
				return (NULL);
			}
			xlength += crp->info.data.certificates [i].length;
		}
		credentials = xmalloc (xlength);
		if (!credentials) {
	 		sec_log_ret ("%d", DDS_RETCODE_OUT_OF_RESOURCES);
			*error = DDS_RETCODE_OUT_OF_RESOURCES;
			return (NULL);
		}
		credentials->credentialKind = DDS_DATA_BASED;
		credentials->info.data.private_key.format = crp->info.data.private_key.format;
		n = crp->info.data.private_key.length;
		credentials->info.data.private_key.length = n;
		dp = (char *) (credentials + 1);
		sp = crp->info.data.private_key.data;
		n = crp->info.data.private_key.length;
		memcpy (dp, sp, n);
		credentials->info.data.private_key.data = dp;
		dp += n;
		for (i = 0; i < crp->info.data.num_certificates; i++) {
			credentials->info.data.certificates [i].format = crp->info.data.certificates [i].format;
			n = crp->info.data.certificates [i].length;
			credentials->info.data.certificates [i].length = n;
			sp = crp->info.data.certificates [i].data;
			memcpy (dp, sp, n);
			credentials->info.data.certificates [i].data = dp;
			dp += n;
		}
	}
	if (length)
		*length = xlength;
	return (credentials);
}

void DDS_Credentials__free (DDS_Credentials *crp)
{
	if (crp->credentialKind == DDS_SSL_BASED) {
		sk_X509_pop_free (crp->info.sslData.certificate_list, X509_free);
		EVP_PKEY_free (crp->info.sslData.private_key);
	}
	xfree (crp);
}

/* DDS_Security_set_credentials -- Set the security policy.  This should only
				   be done once and before any credentials are
				   assigned, or any DomainParticipants are
				   created! */

DDS_ReturnCode_t DDS_Security_set_credentials (const char      *name,
					       DDS_Credentials *crp)
{
	DDS_Credentials	 *prev_creds;
	char		 *prev_name; 
	size_t		 xlength;
	DDS_ReturnCode_t ret;

	sec_log_fct ("DDS_Security_set_credentials");
	if (!plugin_policy) {
		sec_log_ret ("%d", DDS_RETCODE_PRECONDITION_NOT_MET);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	if (!crp) {
		sec_log_ret ("%d", DDS_RETCODE_BAD_PARAMETER);
		return (DDS_RETCODE_BAD_PARAMETER);
	}
	prev_name = local_sec_name;
	prev_creds = local_credentials;
	local_sec_name = strdup (name);
	if (!local_sec_name) {
		local_sec_name = prev_name;
		return (DDS_RETCODE_OUT_OF_RESOURCES);
	}
	local_credentials = DDS_Credentials__copy (crp, &xlength, &ret);
	if (!local_credentials) {
		local_credentials = prev_creds;
		free (local_sec_name);
		local_sec_name = prev_name;
		return (ret);
	}
	if (prev_creds) {
		DDS_Credentials__free (prev_creds);
		free (prev_name);
	}
	local_credentials_size = xlength;
	local_identity = 0;
	sec_log_ret ("%d", DDS_RETCODE_OK);
	return (DDS_RETCODE_OK);
}

void DDS_Security_cleanup_credentials (void)
{
	if (local_sec_name) {
		free (local_sec_name);
		local_sec_name = NULL;
		DDS_Credentials__free (local_credentials);
		local_credentials = NULL;
	}
	local_identity = 0;
}

/* authenticate_participant -- Authenticate user credentials for a new
			       Domain Participant. */

DDS_ReturnCode_t authenticate_participant (GuidPrefix_t *prefix)
{
	unsigned char	   part_key [16];
	AuthState_t	   r;
	DDS_ReturnCode_t   error;

	sec_log_fct ("authenticate_participant");
	if (!local_credentials) {
		sec_log_ret ("%d", DDS_RETCODE_PRECONDITION_NOT_MET);
		return (DDS_RETCODE_PRECONDITION_NOT_MET);
	}
	memcpy (part_key, prefix->prefix, GUIDPREFIX_SIZE);
	memset (&part_key [GUIDPREFIX_SIZE], 0, 4);
	r = sec_validate_local_id (local_sec_name, part_key,
				   local_credentials, local_credentials_size,
				   &local_identity, &error);
	memcpy (prefix->prefix, part_key, GUIDPREFIX_SIZE);
	sec_log_ret ("%d", r);
	return ((r != AS_OK) ? error : DDS_RETCODE_OK);
}

/* sec_authentication_request -- Call the authentication plugin for the given
				 request function (r) and request data (d). */

DDS_ReturnCode_t sec_authentication_request (unsigned r, DDS_SecurityReqData *d)
{
	if (plugin_fct [0])
		return ((*plugin_fct [0]) (DDS_SC_AUTH, r, d));
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

/* sec_access_control_request -- Call the access control plugin for the given
			         request function (r) and request data (d). */

DDS_ReturnCode_t sec_access_control_request (unsigned r, DDS_SecurityReqData *d)
{
	if (plugin_fct [0])
		return ((*plugin_fct [0]) (DDS_SC_ACCESS, r, d));
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

/* sec_certificate_request -- Call the certificate support functions plugin
			      for the given request function (r) and data (d).*/

DDS_ReturnCode_t sec_certificate_request (unsigned r, DDS_SecurityReqData *d)
{
	if (plugin_fct [1])
		return ((*plugin_fct [1]) (DDS_SC_CERTS, r, d));
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

/* sec_crypto_request -- Call the low-level crypto functions plugin for the
			 given request function (r) and request data (d). */

DDS_ReturnCode_t sec_crypto_request (unsigned r, DDS_SecurityReqData *d)
{
	if (plugin_fct [1])
		return ((*plugin_fct [1]) (DDS_SC_CRYPTO, r, d));
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

/* sec_aux_request -- Call the auxiliary functions plugin for the given request
		      function (r) and request data (d). */

DDS_ReturnCode_t sec_aux_request (unsigned r, DDS_SecurityReqData *d)
{
	if (plugin_fct [1])
		return ((*plugin_fct [0]) (DDS_SC_AUX, r, d));
	else
		return (DDS_RETCODE_UNSUPPORTED);
}

/* topic_rematch -- Force calling of the access control match function for a
		    given topic. */

static void topic_rematch (Domain_t *dp, Topic_t *tp, DDS_InstanceHandle_t peer)
{
	Endpoint_t	*ep;
	Writer_t	*wp;
	Reader_t	*rp;

	for (ep = tp->writers; ep; ep = ep->next)
		if (entity_local (ep->entity.flags) &&
		    (ep->entity.flags & (EF_ENABLED | EF_SUSPEND)) == EF_ENABLED) {
			wp = (Writer_t *) ep;
#ifdef RW_LOCKS
			lock_take (wp->w_lock);
#endif
			if (!peer || dp->participant.p_handle == peer)
				hc_qos_update (wp->w_cache);
			if (dp->participant.p_handle != peer)
				disc_writer_update (dp, wp, 0, peer);
#ifdef RW_LOCKS
			lock_release (wp->w_lock);
#endif
		}

	for (ep = tp->readers; ep; ep = ep->next)
		if (entity_local (ep->entity.flags) &&
		    (ep->entity.flags & (EF_ENABLED | EF_SUSPEND)) == EF_ENABLED) {
			rp = (Reader_t *) ep;
#ifdef RW_LOCKS
			lock_take (rp->r_lock);
#endif
			/* Don't call the local match function twice!
			if (!peer || dp->participant.p_handle == peer)
				hc_qos_update (rp->r_cache); */
			if (dp->participant.p_handle != peer)
				disc_reader_update (dp, rp, 0, peer);
#ifdef RW_LOCKS
			lock_release (rp->r_lock);
#endif
		}
}

typedef struct topic_reeval_st {
	Domain_t		*dp;
	DDS_InstanceHandle_t	handle;
	const char		*topic_name;
} TopicReeval_t;

static int topic_reevaluate (Skiplist_t *list, void *np, void *arg)
{
	Topic_t		*tp, **tpp = (Topic_t **) np;
	TopicReeval_t	*rp = (TopicReeval_t *) arg;

	ARG_NOT_USED (list)

	tp = *tpp;
	if ((tp->entity.flags & EF_BUILTIN) == 0 &&
	    !nmatch (rp->topic_name, str_ptr (tp->name), 0)) {
		lock_take (tp->lock);
		topic_rematch (rp->dp, tp, rp->handle);
		lock_release (tp->lock);
	}
	return (1);
}

static int check_handshake_done (Skiplist_t *list, void *node, void *arg)
{
	Participant_t	*pp, **ppp = (Participant_t **) node;

	ARG_NOT_USED (list)
	ARG_NOT_USED (arg)

	pp = *ppp;

	if (pp->p_auth_state == AS_FAILED || 
	    pp->p_auth_state == AS_OK)
		return (1);

	ready_to_reeval = 0;
	return (0);
}

static void reeval_topic_to (uintptr_t user)
{
	TopicReeval_t *reeval = (TopicReeval_t *) user;
	DDS_DomainParticipant dp = reeval->dp;
	DDS_InstanceHandle_t handle = reeval->handle;
	const char *topic_name = reeval->topic_name;

	log_printf (SEC_ID, 0, 
		    "SEC_MAIN: Topics Reeval timeout\r\n");

	DDS_Security_topics_reevaluate (dp, handle, topic_name);

	xfree (reeval);
}

static Timer_t reeval_topic_timer;

void DDS_Security_topics_reevaluate (DDS_DomainParticipant dp,
				     DDS_InstanceHandle_t handle,
				     const char *topic_name)
{
	TopicReeval_t *reeval;
	
	/* Get Domain Participant. */
	if (!domain_ptr (dp, 1, NULL)) {
		log_printf (SEC_ID, 0, "topics_reevaluate(): domain participant not found!\r\n");
		return;
	}
	if (!dp->security || !dp->access_protected) {
		lock_release (dp->lock);
		return;
	}

	/* We must check here that all the handshakes are either OK or FAILED */

	ready_to_reeval = 1;
	sl_walk (&dp->peers, check_handshake_done, dp);
	if (!(reeval = xmalloc (sizeof (TopicReeval_t))))
		return;

	reeval->dp = dp;
	reeval->handle = handle;
	reeval->topic_name = topic_name;
	if (!ready_to_reeval) {
		log_printf (SEC_ID, 0, 
			    "SEC_MAIN: not all handshakes are completed, wait to start topics reeval\r\n");
		tmr_start (&reeval_topic_timer, TICKS_PER_SEC, (uintptr_t) reeval, reeval_topic_to);
		lock_release (dp->lock);
		return;
	}
	log_printf (SEC_ID, 0, 
		    "SEC_MAIN: All handshakes completed, start topic reevaluation\r\n");
	sl_walk (&dp->participant.p_topics, topic_reevaluate, reeval);
	lock_release (dp->lock);
	xfree (reeval);
}

#ifdef DDS_DEBUG

void sec_cache_dump (void)
{
	id_dump ();
	perm_dump ();
}

#endif
#endif

#else
int dds_security = 0;
#endif

