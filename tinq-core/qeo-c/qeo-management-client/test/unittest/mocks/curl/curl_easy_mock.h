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

#ifndef CURL_EASY_MOCK_H_
#define CURL_EASY_MOCK_H_
#include <stdbool.h>
#include <curl/curl.h>

#define CURL_EASY_MOCK_CHECK_CALLED 999

/*
 * Function that is used to make sure that curl_easy functions are called the right amount of times.
 * To just make sure that a function is called at least once, pass CURL_EASY_MOCK_CHECK_CALLED as a value.
 */
void curl_easy_mock_expect_called(int init, int setopt, int getinfo, int perform, int reset, int clean);

/*
 * Indicate what to return when we get a call to the mock
 */
void curl_easy_mock_ignore_and_return(CURLcode global_init, bool easy_init, CURLcode setopt, CURLcode getinfo, CURLcode perform);

/*
 * When getinfo is called on curl, return the provided statuscode starting from the 'cnt'ed time it is called
 */
void curl_easy_mock_return_getinfo(long httpstatuscode, int cnt);

/*
 * Return the passed data when we do a get and maybe skip the first time it is asked
 */
void curl_easy_mock_return_data(char* message, bool skipfirst);

/*
 * Retrieve the data that was uploaded using curl
 */
char* curl_easy_mock_get_uploaded_data(void);

/*
 * Reinit everything
 */
void curl_easy_mock_clean(void);
#endif /* CURL_EASY_MOCK_H_ */
