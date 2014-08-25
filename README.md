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
```
    cd nuttx/
    make menuconfig
    make CROSSDEV=arm-none-eabi-
```


Programming
------------

```
make program
```

(old instructions)

    openocd -f board/stm32f4discovery.cfg -c "init" -c "reset halt" -c "flash write_image erase nuttx.bin 0x08000000 bin" -c "reset run" -c "exit"


Debugging
-------

(old instructions)

    openocd -f board/stm32f4discovery.cfg -c "init"
    arm-none-eabi-gdb -tui nuttx -ex 'target remote localhost:3333' -ex 'monitor reset halt' -ex 'load'  -ex 'continue'



