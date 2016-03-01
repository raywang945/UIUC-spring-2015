if [[ $# != 3 ]]; then
    echo "Usage: ./setCost.sh <node1> <node2> <newCost>"
    exit
fi
./manager_send $1 cost $2 $3
./manager_send $2 cost $1 $3
