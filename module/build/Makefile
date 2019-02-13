obj-m += main.o
main-y := ../src/main.o ../src/driver.o

ccflags-y := -I$(src)/../include

all:
	make  -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make  -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
