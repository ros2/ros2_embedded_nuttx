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

####Building

```bash
    cd nuttx/
    make menuconfig # optional
    make 
```


####Programming

```bash
make program2
```

####Debugging

```bash
cd nuttx/
make gdb_server3
```
In another terminal (same directory):
```bash
make gdb3
```

###UDP testing
If NSH is launched with the right debug options, it can be used to test UDP traffic. On the remote machine do:
```bash
 sudo mz eth0 -c 10 -A 192.168.0.2 -B 192.168.0.3 -t udp -p 100
```
You should see incoming packages in the NSH.

![](http://www.osrfoundation.org/wordpress/wp-content/uploads/2014/07/osrf_masthead.png)
