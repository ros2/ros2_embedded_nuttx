ROS 2.0 NuttX prototype
-------------

This repository prototypes ROS 2.0 for embedded systems using NuttX in the STM32F4Discovery board.

### Milestones

- [x] Quick overview/understand fo the OMG DDS standart 
- [x] Evaluate different Open Source DDS implementations and select one meant for embedded devices (Tinq selected)
- [x] Prototype with FreeRTOS (discarded)
- [x] Prototype with Riot (discarded)
- [x] Prototype with NuttX (current prototype)
- [x] Use the network stack to create a simple UDP/IP example over Ethernet
- [x] NSH (NuttX Shell) infraestructure set up
- [x] Adjust DDS interfaces to match with NuttX (pseudo-POSIX)
- [x] DDS compiling and linking on top of NuttX
- [x] Code small enough to fit in RAM and ROM (112 KB and 1 MB respectively)
- [x] DDS chat application running
- [x] DDS Debug Shell available
- [ ] Tinq-embedded <-> Tinq Desktop interoperability (DDS embedded - DDS Desktop)
- [ ] Tinq-embedded <-> OpenSplice Desktop interoperability 
- [ ] RCL construction
- [ ] Real Time assessment
- [ ] Hardware frontier



###Hardware
The hardware used for this prototype is the `STM32F4Discovery board` together with the `STM32F4-BB`.

The board is connected to the computer using USB. This connection is used to power up the board, program and debug (through STLINK). `PD5`, `PD6` and `GND` are used as the serial connection (for development and debugging purposes, NSH, etc). An Ethernet cable is connected from the `STM32F4-BB` to the working station.


###Setting it up

#### Installing `menuconfig`
```bash
git clone http://ymorin.is-a-geek.org/git/kconfig-frontends
cd kconfig-frontends/
sudo apt-get install gperf
sudo apt-get install flex
sudo apt-get install bison
sudo apt-get install libncurses5-dev
./bootstrap
./configure
make
sudo make install
sudo /sbin/ldconfig -v
```

#### Selecting a configuration
There're are several configurations for the different applications. The following example shows how to use the hello world one:
```bash
cd nuttx/tools
./configure stm32f4discovery/hello
```
For DDS the one you need to use is:
```bash
cd nuttx/tools
./configure stm32f4discovery/dds
```

####Building

```bash
    cd nuttx/
    make menuconfig # optional
    make 
```


####Programming

```bash
make program
```

####Debugging

```bash
cd nuttx/
make gdb_server
```
In another terminal (same directory):
```bash
make gdb
```

####UDP testing
If NSH is launched with the right debug options, it can be used to test UDP traffic. On the remote machine do:
```bash
 sudo mz eth0 -c 10 -A 192.168.0.2 -B 192.168.0.3 -t udp -p 100
```
You can also use mz to send packages from a specific port and to a particular destiny port:
```bash
sudo mz eth0 -c 1 -A 192.168.0.2 -B 192.168.0.3 -t udp "sp=12,dp=5471" -P "Hola que tal"
```

####Updating NuttX
This prototype relies heavily on NuttX. It's recommended to rebase the code frequently with the master branch of NuttX git://git.code.sf.net/p/nuttx/git. The following steps show picture how to do it:
```bash
git remote add nuttx git://git.code.sf.net/p/nuttx/git
git fetch nuttx
git checkout master
git rebase nuttx/master
```

###Running in Linux
NuttX includes a simulator that allows to run the applications (with some resctrictions, refer to [nuttx/configs/sim/README.txt](nuttx/configs/sim/README.txt)) directly in Linux. A simple setup can be achieved through:
```
cd nuttx
make distclean # this is an important step to clean previous builds
cd tools
./configure.sh sim/nsh # make sure you have all the requirements in your Linux machine before compiling (e.g. zlib installed, ...)
cd -
make
./nuttx # you should see the NuttX shell ;)
```

---

*IGMPv2*

NuttX supports only IGMPv2 thereby in order to put force your Linux machine in this mode the following should
be done:
```bash

sudo bash
echo "2" > /proc/sys/net/ipv4/conf/eth0/force_igmp_version
```
If you wish to set if to the default value:
```bash
sudo bash
echo "0" > /proc/sys/net/ipv4/conf/eth0/force_igmp_version
```

---


### Communication
- [development discussion](https://github.com/brunodebus/tinq-core/issues/7)

![](http://www.osrfoundation.org/wordpress/wp-content/uploads/2014/07/osrf_masthead.png)
