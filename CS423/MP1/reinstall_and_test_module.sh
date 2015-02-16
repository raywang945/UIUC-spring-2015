rmmod mp1
make
insmod mp1.ko
./userapp 10 &
./userapp 15 &
sleep 6
cat /proc/mp1/status
