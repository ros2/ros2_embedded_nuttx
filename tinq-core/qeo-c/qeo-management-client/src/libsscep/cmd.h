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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *   Copyright (c) Jarkko Turkulainen 2003. All rights reserved. 
 *   See the 'sscep License' chapter in the file COPYRIGHT for copyright notice
 *   and original licensing information.
 */


/*
 * Command line options
 * These are defined globally for easy access from all functions.
 * For each command line option 'x', there is int x_flag and
 * char *x_char or int x_num if the option requires parameter.
 */

/* CA certificate */
int c_flag;
char *c_char;

/* Debug? */
int d_flag;

/* CA encryption certificate */
int e_flag;
char *e_char;

/* Encryption algorithm */
char *E_char; 
int E_flag;

/* Configuration file */
int f_flag;
char *f_char;

/* Fingerprint algorithm */
char *F_char; 
int F_flag;

/* Local certificate  */
char *l_char;
int l_flag;

/* Local selfsigned certificate  (generated automaticatally) */
char *L_char;
int L_flag;

/* CA identifier */
char *i_char;
int i_flag;

/* Private key */
char *k_char;
int k_flag;

/* Private key of already existing certificate */
char *K_char;
int K_flag;

/* Request count */
int n_flag;
int n_num;

/* Already existing certificate (to be renewed) */
char *O_char; 
int O_flag;

/* Proxy */
char *p_char; 
int p_flag;

/* GetCrl CRL file */
char *r_char; 
int r_flag;

/* Resume */
int R_flag;

/* Certificate serial number */
char *s_char; 
int s_flag;

/* Signature algorithm */
char *S_char; 
int S_flag;

/* Polling interval */
int t_num; 
int t_flag;

/* Max polling time */
int T_num; 
int T_flag;

/* URL */
int u_flag;
char *url_char;

/* Verbose? boolean */
int v_flag;

/* GetCert certificate */
int w_flag;
char *w_char;

/* End of command line options */

