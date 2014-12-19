# Tinq Open Source Project - OSRF fork #

Welcome to the Tinq Open Source Project.
Tinq is completely based on the Qeo publish/subcribe framework produced by Technicolor as explained in the license section.

## About Tinq ##

Tinq is a software framework that allows devices to easily exchange data with other devices on the same network based on a publish-subscribe paradigm.

- Break the silos. Tinq defines a set of standard datamodels that allows your application to interact with a wide range of devices, regardless of their manufacturer.
- Secure. All communication between different devices is encrypted.
- Access control. The end-user has full control over what data can be accessed by which other user/device/application.
- Beyond the local network. Devices that are not in the local network can still exchange data with that network by connecting to a forwarder.

## Supported Platforms ##
Before building the source, make sure your system meets the following requirements:

- A 32-bit or 64-bit Linux system.
- 300MB of free disk space.

## Building ##

You can build the open source version using the `build.sh` script. This script takes one argument to specify the directory to which the resulting artifacts will be copied.

    $ ./build.sh install

## Documentation ##
The Tinq Open Source project Documentation depends heavily on the Qeo Open Source Project Documentation which is made available under the [GNU Free Documentation License V1.3](http://www.gnu.org/licenses/fdl-1.3.en.html).

## License ##
Tinq is made available under the Clear BSD license as defined by the Qeo Open Source Project.
Project components are therefore licensed under [Clear BSD License](http://directory.fsf.org/wiki/License:ClearBSD).

Since it depends on the Qeo Open Source Project it also includes third party open source software components. See the Qeo LICENSE file for more details.

## Trademarks ##
Qeo is a Registered Trademark. For more information and terms of use, contact <opensource@qeo.org>.
