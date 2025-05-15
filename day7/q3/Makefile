obj-m := pchar.o
CC = x86_64-linux-gnu-gcc-12
HOSTCC = x86_64-linux-gnu-gcc-12

all:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

clean:
	make -C /lib/modules/`uname -r`/build M=`pwd` clean
