This directory contains all sources related to the qeo management client library, which is the 
library used by Qeo to talk to the management server. 

Contents
========
* api: The api of the library, you can find documentation on how to use it inside it.
* src: The actual sources of the library
* eclipse: Custom run configurations to easily use in eclipse
* test: All stuff that we need for testing
  * client_app: cli application (see below for more information)
  * testdata: Data that can be used for testing
  * unittest: All unittest code and used mocks can be found here
  * qeo_mgmt_client_test_run.sh: Script that is used when calculating coverage results.

qeo-mgmt-client-app
===================
This command line application can be used to test the server without the need for Qeo. 
It is used by the functional tests of the management server to make sure interop is respected.
It can also be used to test whether you have a valid connection towards the server. 

* Wrapper scripts exist for ease of use of different functionality.
* A valgrind script exists that can be used to test the stuff with valgrind enabled.

Implementation Details
======================
* There are a number of curl options we use by default
  * We enable redirects with a max of 20
* Currently we only use the curl easy api. When dealing with things in an async way, it makes sense to start using the multi api to do things in a more optimal way.
* When initializing libcurl, we specify the flag CURL_GLOBAL_NOTHING to make sure that openssl is not initialized as this is already done inside qeo.

More Info
=========
* [Design Documentation](http://confluence.technicolor.com/display/QEO/Qeo+Management+Client+c+library)

