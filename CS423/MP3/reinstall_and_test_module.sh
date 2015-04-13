pkill work
make clean
rm /dev/node
rmmod mp3
make
insmod mp3.ko
#mknod /dev/node c `cat /proc/devices | grep mp3_character_device | sed s/mp3_character_device//g` 0

# profile1.data
#nice ./work 1024 R 50000 &
#nice ./work 1024 R 10000 &
#exit
#sleep 50
#./monitor > data/profile1.data

# profile2.data
#nice ./work 1024 R 50000 &
#nice ./work 1024 L 10000 &
#exit
#sleep 50
#./monitor > data/profile2.data

# profile3.data
#node_num=20
#for (( i = 19; i < ${node_num}; i++ )); do
    #for (( j = 0; j < ${i}; j++ )); do
        #nice ./work 200 R 10000 &
    #done
    #sleep 50
    #./monitor > data/profile3_${i}.data
#done
