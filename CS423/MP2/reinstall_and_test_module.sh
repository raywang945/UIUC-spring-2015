rm log
rmmod mp2
make
insmod mp2.ko
# ./userapp <period> <computation> <loop_cycle>
./userapp 1000 200 10 &
./userapp 500 100 40 &
./userapp 100 10 70 &
./userapp 150 15 60 &
./userapp 150 50 60 &
./userapp 1000 300 8 &
sleep 0.1
cat /proc/mp2/status
less log
