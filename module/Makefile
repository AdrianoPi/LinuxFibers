all:
	./make.sh ""
clean:
	./make.sh "clean"
test:
	sudo rmmod main && sudo insmod main.ko 
	cd ../client/ && make && cd ../module
	./../client/main
