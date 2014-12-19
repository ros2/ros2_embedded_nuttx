Qeo credential builder
===============
This directory contains scripts to recreate a qeo storage directory from scratch without the need to go to a management server.
To do this, the scripts will generate root, realm, device and policy certificates using openssl that contain the correct properties.
Using these scripts it is also possible to generate multiple qeo credential directories in different constallations (different realms, different users but same realm, different devices for same user, ...).

**Disclaimer: The use of these scripts is for local testing purposes only. When doing integration testing real interaction with the server is required.**

Instructions
---------------
Execute the `generate_qeo_storage.sh` script to create a working Qeo directory.
You can run this script multiple times with the same truststore location to generate device credentials belonging to the same user/realm.
The script will call the generate_truststore.sh script to create the necessary certificates if not already present.

Using the generated credentials on android
---------------
It is possible to use the generated credentials on an android device. To use it, you however need to do the following things.

* Have a rooted android device
* Generate the credentials on your linux host device
* change the file urls inside the generated credentials directory:
  * Change the url file to point to `file:///data/data/org.qeo.android.service/files/root_resource.json`
  * Change the forwarder url inside the root_resource.json file to `file:///data/data/org.qeo.android.service/files/forwarders.json`
* cp the credentials to the correct location `adb push <credentialdir> /data/data/org.qeo.android.service/files`
* make sure all the credentials run as the correct user and group `chown <uid>.<group> <files> on your android device`
