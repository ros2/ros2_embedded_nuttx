#!/bin/bash
# Copyright (c) 2014 - Qeo LLC
#
# The source code form of this Qeo Open Source Project component is subject
# to the terms of the Clear BSD license.
#
# You can redistribute it and/or modify it under the terms of the Clear BSD
# License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
# for more details.
#
# The Qeo Open Source Project also includes third party Open Source Software.
# See LICENSE file for more details.

# Script that is used to create a mini Qeo PKI at a certain directory on your file system. 
# You can use this script multiple times with the same directory. In this case it will reuse
# the root certificate and realm certificates if they have the same id.
# The script will also create qeo policy files to use for this set of credentials.

SELF_DIR=$(cd $(dirname $0) > /dev/null; pwd)
# We should make sure that the normal system libraries are used for tools like openssl
unset LD_LIBRARY_PATH

function syntax()
{
    echo "Usage: $(basename $0) <workingdir> <realm-id> <device-id> <user-id> [<policy-rule>]"
    exit 1
}   

if [ $# -lt 4 ]; then
    syntax
fi 
CA_DIR="${1}/ca"
WORKING_DIR="${1}"

function create_openssl_config()
{
    cat <<- EOF > ${CA_DIR}/openssl.cnf
	[ ca ]
	default_ca      = CA_root

	[ policy_any ]
	countryName            = optional
	stateOrProvinceName    = optional
	organizationName       = supplied
	organizationalUnitName = optional
	commonName             = supplied
	emailAddress           = optional

	[ req ]
	default_bits            = 1024
	distinguished_name      = req_dn
	req_extensions		    = device_req # The 

	[ req_dn ]
	countryName                     = Country Name (2 letter code)

	[ root_req ]
	basicConstraints=critical,CA:TRUE, pathlen:2
	keyUsage=keyCertSign, cRLSign

	[ realm_req ]
	basicConstraints=critical,CA:TRUE, pathlen:1
	keyUsage=keyCertSign, cRLSign

	[ device_req ]
	basicConstraints=critical,CA:TRUE, pathlen:0
	keyUsage=digitalSignature, keyEncipherment, dataEncipherment, keyAgreement, keyCertSign, cRLSign
	extendedKeyUsage=serverAuth, clientAuth
                                        
	[ policy_req ]
	keyUsage=digitalSignature
	EOF
}

function add_ca_config()
{
    local name=$1
    
    mkdir -p ${dir}/private
    mkdir -p ${dir}/newcerts
    echo "01" > ${dir}/serial
    touch ${dir}/index.txt

    cat <<- EOF >> ${CA_DIR}/openssl.cnf

	[ CA_${name} ]
	dir              = ${CA_DIR}/CA_${name}
	database         = \$dir/index.txt
	new_certs_dir    = \$dir/newcerts

	certificate      = \$dir/cacert.pem
	serial           = \$dir/serial
	private_key      = \$dir/private/cakey.pem
	RANDFILE         = \$dir/private/.rand

	default_days     = ${VALIDITY}
	default_crl_days = 30
	default_md       = sha256

	policy           = policy_any
	email_in_dn      = no
	copy_extensions  = none
	EOF
    #    name_opt         = ca_default
    #cert_opt         = ca_default
}

function setup_ca()
{
    mkdir -p ${CA_DIR}

    create_openssl_config   
}

function create_root_ca()
{
    local name=$1
    local dir=${CA_DIR}/CA_${name}
    local subject='/CN=join.qeodev.org/OU=Qeo/O=Technicolor/ST=Antwerp/C=BE'
    
    add_ca_config ${name}

    # create self-signed certificate
    openssl req -x509 -newkey rsa:2048 -days ${VALIDITY} -nodes -subj "${subject}" \
        -keyout ${dir}/private/cakey.pem -out ${dir}/cacert.pem -config ${CA_DIR}/openssl.cnf -reqexts root_req
}

function create_realm_ca()
{
    local realm=$1
    local dir=${CA_DIR}/CA_${realm}
    local subject="/CN=join.qeodev.org ${realm}/OU=${realm}/O=Qeo"
    local chain="chain.pem"

    add_ca_config ${realm}
    
    # create realm key and certificate
    openssl req -new -newkey rsa:2048 -nodes -subj "${subject}" \
        -out ${dir}/realm.csr -keyout ${dir}/private/cakey.pem
    openssl ca -in ${dir}/realm.csr -days ${VALIDITY} -config ${CA_DIR}/openssl.cnf \
        -name CA_root -batch -out ${dir}/cacert.pem -notext -extfile ${CA_DIR}/openssl.cnf -extensions realm_req
    
    cat ${dir}/cacert.pem > "${dir}/${chain}"
    cat ${CA_DIR}/CA_root/cacert.pem >> "${dir}/${chain}"
}  

function update_policy_file()
{
    local realm=$1
    local dir="${WORKING_DIR}/${realm}_policy"
    local certdir="${WORKING_DIR}/${realm}_policy_cert"
    local fname="${realm}_policy"
    local nrusers=$(ls "${WORKING_DIR}/realms/${realm}/users/"|wc -l)

    mkdir -p ${dir}
    
    printf "[meta]\nversion=1.0\nseqnr=${nrusers}\n\n" > ${dir}/${fname}
    cat "${WORKING_DIR}/realms/${realm}/policy.users" >> ${dir}/${fname}
    printf "[rid:${realm}]\n*=rw\n" >> ${dir}/${fname}

    openssl smime -sign -signer ${certdir}/${fname}.crt -inkey ${certdir}/${fname}.key -in ${dir}/${fname} -out ${dir}/${fname}.mime
}

function create_policy_cert()
{
    local realm=$1
    local subject="/CN=${realm} policy/O=Qeo"
    local dir="${WORKING_DIR}/${realm}_policy_cert"
    local fname="${realm}_policy"
    local nrusers=$(ls "${WORKING_DIR}/realms/${REALM_ID}/users/"|wc -l)

    mkdir -p ${dir}
    openssl req -new -newkey rsa:1024 -sha1 -nodes -subj "${subject}" \
        -out ${dir}/${fname}.csr -keyout ${dir}/${fname}.key
    openssl ca -in ${dir}/${fname}.csr -days ${VALIDITY} -config ${CA_DIR}/openssl.cnf \
        -name CA_${realm} -batch -out ${dir}/${fname}.crt -extfile ${CA_DIR}/openssl.cnf -extensions policy_req 
    
    echo "[meta]\nversion=1.0\nseqnr=${nrusers}\n" > ${dir}/${fname}
    cat "${WORKING_DIR}/realms/${REALM_ID}/policy.users" >> ${dir}/${fname}
    echo "[rid:${realm}]\n*=rw\n" >> ${dir}/${fname}

    cp ${SELF_DIR}/policy.template ${dir}/${fname}
    sed -i "s/xxxxxx/${realm}/g" ${dir}/${fname} 
    openssl smime -sign -signer ${dir}/${fname}.crt -inkey ${dir}/${fname}.key -in ${dir}/${fname} -out ${dir}/${fname}.mime
} 

function create_truststore()
{
    local realm=$1
    local did=$2
    local uid=$3
    local subject="/CN=${realm} ${did} ${uid}/O=Qeo"
    local dir="${WORKING_DIR}/${realm}_${did}_${uid}"
    local fname="${realm}_${did}_${uid}"
    local name="<rid:${realm}><did:${did}><uid:${uid}>"

    mkdir -p ${dir}
    # create user key and certificate
    openssl req -new -newkey rsa:1024 -sha1 -nodes -subj "${subject}" \
        -out ${dir}/${fname}.csr -keyout ${dir}/${fname}.key
    openssl ca -in ${dir}/${fname}.csr -days ${VALIDITY} -config ${CA_DIR}/openssl.cnf \
        -name CA_${realm} -batch -out ${dir}/${fname}.crt -extfile ${CA_DIR}/openssl.cnf -extensions device_req

    openssl pkcs12 -export -in ${dir}/${fname}.crt -inkey ${dir}/${fname}.key -passout pass:secret \
        -out ${dir}/truststore.p12 -name "${name}" \
        -certfile ${CA_DIR}/CA_${realm}/chain.pem 
}

REALM_ID=$2
DEVICE_ID=$3
USER_ID=$4
POLICY_RULE=${5:-*=rw}

mkdir ${WORKING_DIR} 2>/dev/null
mkdir -p "${WORKING_DIR}/realms/${REALM_ID}/users" 2>/dev/null

VALIDITY=$(( 365 * 100 ))

if [ ! -d ${CA_DIR} ]; then
    echo "Creating root certificate"
    setup_ca
    create_root_ca "root"
fi

if [ ! -d ${CA_DIR}/CA_${REALM_ID} ]; then
    echo "Creating realm certificate ${REALM_ID}"
    create_realm_ca "${REALM_ID}"
fi
if [ ! -f "${WORKING_DIR}/realms/${REALM_ID}/users/${USER_ID}" ]; then
    touch "${WORKING_DIR}/realms/${REALM_ID}/users/${USER_ID}"
    printf "[uid:${USER_ID}]\n${POLICY_RULE}\n\n" >> "${WORKING_DIR}/realms/${REALM_ID}/policy.users"
fi

if [ ! -d "${WORKING_DIR}/${REALM_ID}_policy_cert" ]; then
    echo "Creating policy file for realm ${REALM_ID}"
    create_policy_cert "${REALM_ID}"
fi                    
update_policy_file "${REALM_ID}" ${POLICY_RULE}

if [ ! -d "${WORKING_DIR}/${REALM_ID}_${DEVICE_ID}_${USER_ID}" ]; then
    echo "Creating user certificate for ${REALM_ID}_${DEVICE_ID}_${USER_ID}"
    create_truststore "${REALM_ID}" "${DEVICE_ID}" "${USER_ID}"
fi
echo "trustore is up to date for ${REALM_ID} ${DEVICE_ID} ${USER_ID}"
