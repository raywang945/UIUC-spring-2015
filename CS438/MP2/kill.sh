if [[ $# != 1 ]]; then
    echo "Usage: ./kill <nodeID>"
    exit
fi
kill `ps -C ls_router -o pid,command= | grep "ls_router $1" | cut -d' ' -f 1,2`
