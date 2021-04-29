# treefs

Build
```sh
make
```

Test
```sh
sudo insmod treefs.ko
sudo mkdir -p /mnt/treefs
sudo mount -t treefs none /mnt/treefs
stat -f /mnt/treefs
sudo mkdir /mnt/treefs/test
sudo touch /mnt/treefs/foo.bar
```

End
```sh
sudo umount /mnt/treefs
sudo rm -rf /mnt/treefs
sudo rmmod treefs.ko
make clean
```
