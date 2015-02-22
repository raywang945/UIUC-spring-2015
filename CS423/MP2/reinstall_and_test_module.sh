rmmod mp2
make
insmod mp2.ko
./userapp 10 &
./userapp 15 &
sleep 6
cat /proc/mp2/status
