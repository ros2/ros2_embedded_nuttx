default: gccarm.installed openocd

gccarm.installed:
	sudo add-apt-repository ppa:terry.guo/gcc-arm-embedded
	sudo apt-get update
	sudo apt-get install gcc-arm-none-eabi
	touch gccarm.installed

openocd-source:
	git clone git://git.code.sf.net/p/openocd/code openocd-source

openocd: openocd-source
	cd openocd-source && ./bootstrap && ./configure --prefix=$(shell pwd)/openocd --enable-ftdi && make && make install
