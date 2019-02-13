#!/bin/bash


if [[ $1 == "clean" ]]; then
	cd build
	make clean
	exit 0
fi

cd build
make 
cp *.ko ../
cd ../
