#include "monitor_neighbors.h"
#include "linkstate.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include <inttypes.h>

#define MSG_LEN                100
#define HEARTBEAT_TIMEOUT   1000000

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

extern char *logfile_path;
extern struct ForwardingTableEntry forwarding_table[256];
extern char globalLSP[LSP_MAX_LEN];
extern unsigned int globalLSPLength;
/*extern pthread_mutex_t lock;*/

unsigned int getInfoFromSendBuf(unsigned char *buf, uint16_t *destID, char *msg, unsigned int buf_length)
{
    unsigned int msg_length = buf_length - 4 - sizeof(uint16_t);
    buf += 4;
    memcpy(destID, buf, sizeof(uint16_t));
    *destID = ntohs(*destID);
    memcpy(msg, buf + sizeof(uint16_t), msg_length);
    return msg_length;
}

void getInfoFromCostBuf(unsigned char *buf, uint16_t *neighborID, uint32_t *newCost)
{
    buf += 4;
    memcpy(neighborID, buf, sizeof(uint16_t));
    memcpy(newCost, buf + sizeof(uint16_t), sizeof(uint32_t));
    *neighborID = ntohs(*neighborID);
    *newCost = ntohl(*newCost);
}

void writeLog(char *buf, unsigned int buf_length)
{
    FILE *fp = fopen(logfile_path, "a");
    fwrite(buf, sizeof(char), buf_length, fp);
    fclose(fp);
}

void writeForwardLog(uint16_t dest, uint16_t nexthop, char *msg, unsigned int msg_length)
{
    char buf[1000];
    unsigned int count;

    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "forward packet dest %"PRIu16" nexthop %"PRIu16" message ", dest, nexthop);
    strncpy(buf + count, msg, msg_length);
    buf[count + msg_length] = '\n';

    writeLog(buf, count + msg_length + 1);
}

void writeSendingLog(uint16_t dest, uint16_t nexthop, char *msg, unsigned int msg_length)
{
    char buf[1000];
    unsigned int count;

    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "sending packet dest %"PRIu16" nexthop %"PRIu16" message ", dest, nexthop);
    strncpy(buf + count, msg, msg_length);
    buf[count + msg_length] = '\n';

    writeLog(buf, count + msg_length + 1);
}

void writeReceiveLog(char *msg, unsigned int msg_length)
{
    char buf[1000];
    unsigned int count;

    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "receive packet message ");
    strncpy(buf + count, msg, msg_length);
    buf[count + msg_length] = '\n';

    writeLog(buf, count + msg_length + 1);
}

void writeUnreachableLog(uint16_t dest)
{
    char buf[1000];
    unsigned int count;

    memset(buf, '\0', 1000);
    count = (unsigned int)sprintf(buf, "unreachable dest %"PRIu16"\n", dest);

    writeLog(buf, count);
}

void forwardSend(int nexthop, char *buf, unsigned int buf_length)
{
    sendto(globalSocketUDP, buf, buf_length, 0, (struct sockaddr*)&globalNodeAddrs[nexthop], sizeof(globalNodeAddrs[nexthop]));
}

unsigned int getTimeDiff(struct timeval after, struct timeval before)
{
    return (after.tv_sec - before.tv_sec) * 1000000 + after.tv_usec - before.tv_usec;
}

void* checkNeighbor(void* unusedParam)
{
    int i;
    struct timeval current;
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 500 * 1000 * 1000; //500 ms

    while (1) {
        int has_disconnect = 0;
        for (i = 0; i < 256; i ++) {
            if (globalMyID == i) {
                continue;
            }
            if (isConnected(globalMyID, i)) {
                gettimeofday(&current, 0);
                if (getTimeDiff(current, globalLastHeartbeat[i]) > HEARTBEAT_TIMEOUT) {
                    setCost(globalMyID, i, 0);
                    has_disconnect = 1;
                }
            }
        }
        //TODO: broadcast to others that the node loses a connection
        if (has_disconnect) {
            /*[>flushSequenceNumber();<]*/
            /*broadcastTopoLSP(globalMyID);*/
        }
        /*pthread_mutex_lock(&lock);*/
        /*broadcastTopoLSP(globalMyID);*/
        /*pthread_mutex_unlock(&lock);*/

		nanosleep(&sleepFor, 0);
    }
}

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
// hackyBroadcast will broadcast a message to all other nodes
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
        forwarding_table[globalMyID].sequence_number ++;
        /*pthread_mutex_lock(&lock);*/
        /*globalLSPLength = setLinkStatePacketBuf(globalLSP, globalMyID);*/
        /*pthread_mutex_unlock(&lock);*/
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		theirAddrLen = sizeof(theirAddr);
        recvBuf[0] = '\0';
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0,
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
            // heardFrom is the id of the node from where recvFrom() gets the msg
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);

			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
            if (!isConnected(globalMyID, heardFrom)) {
                setCost(globalMyID, heardFrom, forwarding_table[heardFrom].direct_cost);
                //TODO: broadcast to other nodes that the node comes up a connection
                /*broadcastTopoLSP(heardFrom);*/
                /*sendWholeTopology(heardFrom);*/
            }

			//record that we heard from heardFrom just now.
            gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}

		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
            dijkstra();
            uint16_t destID;
            char msg[MSG_LEN];
            memset(msg, '\0', MSG_LEN);
            unsigned int msg_length = getInfoFromSendBuf(recvBuf, &destID, msg, bytesRecvd);

			//TODO send the requested message to the requested destination node
			// ...

            if (globalMyID == (int)destID) {
                // receive message
                writeReceiveLog(msg, msg_length);
            } else {
                int nexthop = forwarding_table[destID].next_hop;

                if (nexthop == -1) {
                    // unreachable
                    writeUnreachableLog(destID);
                } else {
                    forwardSend(forwarding_table[destID].next_hop, recvBuf, bytesRecvd);
                    if (strncmp(fromAddr, "10.0.0.10", 9) == 0) {
                        writeSendingLog(destID, (uint16_t)nexthop, msg, msg_length);
                    } else {
                        writeForwardLog(destID, (uint16_t)nexthop, msg, msg_length);
                    }
                }
            }
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
            uint16_t neighbor;
            uint32_t newCost;

            getInfoFromCostBuf(recvBuf, &neighbor, &newCost);

			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
            forwarding_table[neighbor].direct_cost = newCost;
            if (isConnected(globalMyID, neighbor)) {
                setCost(globalMyID, neighbor, newCost);
            }
            /*broadcastTopoLSP(globalMyID);*/
		}

		//TODO now check for the various types of packets you use in your own protocol
        else if(!strncmp(recvBuf, "LSP", 3))
        {
            uint16_t node;
            uint32_t sequence_number;
            int i, count = (bytesRecvd - 3 - sizeof(uint16_t) * 2) / COST_PAIR_SIZE;
            uint16_t *neighbors = malloc(count * sizeof(uint16_t));
            uint32_t *costs = malloc(count * sizeof(uint32_t));
            getInfoFromLSPBuf(recvBuf, &node, neighbors, costs, count, &sequence_number, bytesRecvd);

            /*puts("============");*/
            /*puts(fromAddr);*/
            /*printf("node = %"PRIu16"\n", node);*/
            /*printf("sequence_number = %"PRIu32"\n", sequence_number);*/
            /*printf("count = %d\n", count);*/
            /*for (i = 0; i < count; i ++) {*/
                /*printf("%"PRIu16"\t%"PRIu32"\n", neighbors[i], costs[i]);*/
            /*}*/

            // forward LSP
            if (sequence_number > forwarding_table[node].sequence_number) {
                int j;
                /*int j, adjacency_array_need_flush = 0;*/
                // check if the connections still exist
                for (i = 0; i < 256; i ++) {
                    if (i == node) {
                        continue;
                    }
                    if (isConnected(node, i)) {
                        int other_connect_still_exist = 0;
                        for (j = 0; j < count; j ++) {
                            if (neighbors[j] == (uint16_t)i) {
                                other_connect_still_exist = 1;
                            }
                        }
                        if (!other_connect_still_exist) {
                            /*adjacency_array_need_flush = 1;*/
                            setCost(node, i, 0);
                        }
                    }
                }
                for (i = 0; i < count; i ++) {
                    setCost(neighbors[i], node, costs[i]);
                }
                forwarding_table[node].sequence_number = sequence_number;
                /*if (adjacency_array_need_flush) {*/
                    /*[>flushSequenceNumber();<]*/
                /*}*/
                broadcastLSP(heardFrom, recvBuf, bytesRecvd);
            }
        }
	}
	//(should never reach here)
	close(globalSocketUDP);
}

void* broadcastLSPPeriodically(void* unusedParam)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    srand((unsigned int)(t.tv_sec * 1000000 + t.tv_usec));
	struct timespec startSleep;
	startSleep.tv_sec = 0;
    startSleep.tv_nsec = (rand() % 1000) * 1000 * 1000;
	nanosleep(&startSleep, 0);

	struct timespec sleepFor;
	sleepFor.tv_sec = 1;
	sleepFor.tv_nsec = 0;
	while(1)
	{
        /*pthread_mutex_lock(&lock);*/
        globalLSPLength = setLinkStatePacketBuf(globalLSP, globalMyID);
        broadcastTopoLSP(globalMyID);
        /*pthread_mutex_unlock(&lock);*/
		nanosleep(&sleepFor, 0);
	}
}
