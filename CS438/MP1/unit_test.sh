#!/bin/bash

ip="127.0.0.1"
http_port=8888
https_port=9999
python_simple_server_port=8000
wget_out="wget_out"
wget="wget -O $wget_out"
output="output"
http_urls=( "http://illinois.edu/index.html" "http://illinois.edu/" )
https_urls=( "https://www.google.com/?gws_rd=ssl" "https://www.google.com/" )
files=( `echo input/*` )

make &> /dev/null

# test http_client
for url in "${http_urls[@]}" ; do
    $wget $url >> log 2>&1
    ./http_client $url &> /dev/null
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
    ./http_client $url &> /dev/null
    if [ $(cat $output | wc -l) != $(cat $wget_out | wc -l) ]; then
        echo "=========ERROR========"
        echo "./http_client $url"
        echo "======================"
        exit
    fi
    rm $wget_out
done

# test http_client python simple server
(python -m SimpleHTTPServer $python_simple_server_port &) &> /dev/null
sleep 1
for file in ${files[@]} ; do
    ./http_client http://$ip:$python_simple_server_port/$file &> /dev/null
    if [ $(diff $output $file | wc -l) != 0 ]; then
        echo "=========ERROR========"
        echo "./http_client http://$ip:$python_simple_server_port/$file"
        echo "======================"
        exit
    fi
done
kill $(ps -C python -o pid=)

# test http_server
(./http_server $http_port &) > /dev/null
sleep 1
for file in ${files[@]} ; do
    ./http_client http://$ip:$http_port/$file &> /dev/null
    if [ $(diff $output $file | wc -l) != 0 ]; then
        echo "=========ERROR========"
        echo "./http_client http://$ip:$http_port/$file"
        echo "======================"
        exit
    fi
done
kill $(ps -C http_server -o pid=)

# test https_server
(./https_server $https_port &) > /dev/null
sleep 1
for file in ${files[@]} ; do
    ./http_client https://$ip:$https_port/$file &> /dev/null
    if [ $(diff $output $file | wc -l) != 0 ]; then
        echo "=========ERROR========"
        echo "./http_client https://$ip:$https_port/$file"
        echo "======================"
        exit
    fi
done
kill $(ps -C https_server -o pid=)

# remove redundant files
rm $output
make clean &> /dev/null
