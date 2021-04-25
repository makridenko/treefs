KDIR = /lib/modules/`uname -r`/build
CFLAGS_treefs.o := -DDEBUG

kbuild:
	make -C $(KDIR) M=`pwd`
clean:
	make -C $(KDIR) M=`pwd` clean