#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <inttypes.h>

#include "monitor_neighbors.h"
#include "linkstate.h"

int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

char *logfile_path;
struct ForwardingTableEntry forwarding_table[256];
/*pthread_mutex_t lock;*/

int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}

	//initialization: get this process's node ID, record what time it is,
	//and set up our sockaddr_in's for sending to the other nodes.
	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<256;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);

		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}

    logfile_path = argv[3];

	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
    for (i = 0; i < 256; i ++) {
        forwarding_table[i].direct_cost = 1;
        forwarding_table[i].sequence_number = 0;
        forwarding_table[i].next_hop = -1;
        forwarding_table[i].cost = -1;
    }
    char buf[100];
    int nodeID, cost;
    FILE *cost_file = fopen(argv[2], "r");
    while (fgets(buf, sizeof(buf), cost_file) != NULL) {
        sscanf(buf, "%d %d", &nodeID, &cost);
        forwarding_table[nodeID].direct_cost = cost;
    }
    fclose(cost_file);

    if (strcmp(argv[0], "./ls_router") == 0) {
        adjacencyArrayInit();
        /*pthread_mutex_init(&lock, NULL);*/
    }

	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}

	//start threads... feel free to add your own, and to remove the provided ones.
    pthread_t announcerThread, checkNeighborThread, broadcastLSPThread;
    /*pthread_t announcerThread, checkNeighborThread;*/
    pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);
    pthread_create(&checkNeighborThread, 0, checkNeighbor, (void*)0);
    pthread_create(&broadcastLSPThread, 0, broadcastLSPPeriodically, (void*)0);
    /*pthread_t printThread;*/
    /*pthread_create(&printThread, 0, printAdjacencyArray, (void*)0);*/
    /*pthread_create(&printThread, 0, printForwardingTable, (void*)0);*/
    /*pthread_create(&printThread, 0, printDFS, (void*)0);*/

	//good luck, have fun!
	listenForNeighbors();
}
