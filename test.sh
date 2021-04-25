#!/bin/bash

make clean
make kbuild
sudo insmod treefs.ko
dmesg | tail -2
echo "#####"
sudo rmmod treefs.ko
make clean