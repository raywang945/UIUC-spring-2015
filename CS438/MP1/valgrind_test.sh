#!/bin/bash

ip="127.0.0.1"
http_port=8888
https_port=9999
python_simple_server_port=8000
wget_out="wget_out"
wget="wget -O $wget_out"
valgrind="valgrind --leak-check=yes -q"
output="output"
http_urls=( "http://illinois.edu/index.html" "http://illinois.edu/" )
https_urls=( "https://www.google.com/?gws_rd=ssl" "https://www.google.com/" )
files=( `echo input/*` )

make &> /dev/null

# test http_client
for url in "${http_urls[@]}" ; do
    $wget $url >> log 2>&1
    $valgrind ./http_client $url
    if [ $(diff $output $wget_out | wc -l) != 0 ]; then
        echo "=========ERROR========"
        echo "./http_client $url"
        echo "======================"
        exit
    fi
    rm $wget_out
done

# test http_client tls
for url in ${https_urls[@]} ; do
    $wget $url >> log 2>&1
    $valgrind ./http_client $url
    if [ $(cat $output | wc -l) != $(cat $wget_out | wc -l) ]; then
        echo "=========ERROR========"
        echo "./http_client $url"
        echo "======================"
        exit
    fi
    rm $wget_out
done

make clean &> /dev/null
