rmmod mp2
make clean
make
insmod mp2.ko
./userapp 100 200 3 &
./userapp 300 400 6 &
sleep 1
#dmesg | tail -n 20
cat /proc/mp2/status
