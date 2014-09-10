Nuttx STM32F4 ROS prototype
-------------

This repository prototypes a ROS 2 Embedded system using NuttX.

Installing menuconfig
--------------------

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


Building
---------
```bash
    cd nuttx/
    #make menuconfig
    make 
```


Programming
------------

```bash
cd nuttx/
make program2
```

Debugging
-------

```bash
cd nuttx/
make gdb_server3
```
In another terminal (same directory):
```bash
make gdb3
```

UDP
-------
Test UDP traffic. On the remote machine do:
```bash
 sudo mz eth0 -c 10 -A 192.168.0.2 -B 192.168.0.3 -t udp -p 100
```
