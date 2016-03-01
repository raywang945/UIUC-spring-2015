if [[ $# != 1 ]]; then
    echo "Usage: ./launch.sh <nodeID>"
    exit
fi
#topology_file="create_config_files/topology"
#cost_file_dir="create_config_files/cost_files"
#cost_format="costs_"
topology_file="example_topology/testtopo.txt"
cost_file_dir="example_topology"
cost_format="testinitcosts"
./ls_router $1 ${cost_file_dir}/${cost_format}$1 logfiles/log_$1
