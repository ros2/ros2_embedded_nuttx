printf "\r\n***** Cred test started *****\r\n\r\n"
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

if [ $1 -eq 1 ]
then
        make reallyclean > /dev/null 2>&1
        make > /dev/null 2>&1
else
        make reallyclean
        make
fi
./cred ../certs/first.pem ../certs/first-key.pem
printf "\r\n***** Cred tests finished *****\r\n\r\n"
