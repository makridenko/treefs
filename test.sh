#!/bin/bash

sudo rmmod treefs.ko
make clean
make
echo -e "\n ### TEST ###\n"
sudo dmesg -C
sudo insmod treefs.ko
sudo rmmod treefs.ko
dmesg
echo -e "\n#############\n"
make clean