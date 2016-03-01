make clean
rm logfiles/*
pkill ls_router
make
trap "pkill ls_router" SIGINT

topology_file="create_config_files/topology"
cost_file_dir="create_config_files/cost_files"
cost_format="costs_"
#topology_file="example_topology/testtopo.txt"
#cost_file_dir="example_topology"
#cost_format="testinitcosts"

logfile_dir="logfiles"

perl make_topology.pl ${topology_file}

nodes=(`find ${cost_file_dir}/* -name ${cost_format}* -printf "%f\n" | sed s/${cost_format}//g`)
#test_node=1
#for i in ${!nodes[@]} ; do
    #if [[ ${nodes[$i]} = ${test_node} ]]; then
        #nodes[$i]=''
    #fi
#done
for node in ${nodes[@]} ; do
    ./ls_router ${node} ${cost_file_dir}/${cost_format}${node} ${logfile_dir}/log_${node} >/dev/null 2>/dev/null &
done
#valgrind ./ls_router ${test_node} ${cost_file_dir}/${cost_format}${test_node} logfiles/log_${test_node}
#./ls_router ${test_node} ${cost_file_dir}/${cost_format}${test_node} logfiles/log_${test_node}

#sleep 5
#exit

#./disconnect.sh 1 4
#./kill.sh 0
#./kill.sh 7
#sleep 5
#./launch.sh 0 >/dev/null 2>/dev/null &
#./launch.sh 7 >/dev/null 2>/dev/null &
#./kill.sh 5
#./kill.sh 8
#./disconnect.sh 0 4
#./kill.sh 9
#sleep 5
#./launch.sh 5 >/dev/null 2>/dev/null &
#./launch.sh 8 >/dev/null 2>/dev/null &
#./launch.sh 9 >/dev/null 2>/dev/null &
#./disconnect.sh 0 3
#./disconnect.sh 0 7
#sleep 5
#./connect.sh 0 7
#./connect.sh 1 9
#./connect.sh 0 3
#./connect.sh 0 4
#./setCost.sh 0 8 100
#sleep 5
#./setCost.sh 0 8 1
#./setCost.sh 8 9 100
#./kill.sh 0
#./setCost.sh 8 9 1
#sleep 5
#./launch.sh 0 >/dev/null 2>/dev/null &
#sleep 5

sleep 5
./manager_send 33 send 90 "33->90"
#echo "after manager_send"
sleep 1
logfiles=(`ls ${logfile_dir}`)
for logfile in ${logfiles[@]} ; do
    echo "=====${logfile}====="
    cat ${logfile_dir}/${logfile}
done

exit

# ./manager_send <destnode> cost <destID> <newCost>
# ./manager_send <destnode> send <destID> "the message"
./manager_send 2 send 1 "2->1"
./manager_send 0 send 6 "0->6"
./manager_send 4 send 2 "4->2"
./manager_send 5 send 1 "5->1"
./manager_send 7 send 2 "7->2"
sleep 1
logfiles=(`ls ${logfile_dir}`)
for logfile in ${logfiles[@]} ; do
    echo "=====${logfile}====="
    cat ${logfile_dir}/${logfile}
done

echo "#####################"
./setCost.sh 1 6 20
./setCost.sh 5 6 35
rm ${logfile_dir}/*
sleep 5
./manager_send 2 send 1 "2->1"
./manager_send 0 send 6 "0->6"
./manager_send 4 send 2 "4->2"
./manager_send 5 send 1 "5->1"
./manager_send 7 send 2 "7->2"
sleep 1
logfiles=(`ls ${logfile_dir}`)
for logfile in ${logfiles[@]} ; do
    echo "=====${logfile}====="
    cat ${logfile_dir}/${logfile}
done

pkill ls_router
