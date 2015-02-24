rmmod mp2
make
insmod mp2.ko
./userapp 100 200
dmesg | tail -n 20
make clean
