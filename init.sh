#!/bin/bash

cd nuttx/tools  
./configure.sh stm32f4discovery/bb
cd ..
# make CROSSDEV=arm-none-eabi-