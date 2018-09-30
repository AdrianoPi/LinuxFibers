obj-m+=chardev.o
 
all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
load:
	sudo rmmod chardev.ko
	sudo insmod chardev.ko 
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
