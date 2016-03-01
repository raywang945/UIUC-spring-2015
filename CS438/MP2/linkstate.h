#ifndef __LINKSTATE_INCLUDE__
#define __LINKSTATE_INCLUDE__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>

#define ADJACENCY_ARRAY_LEN   255 * 256 / 2
#define COST_PAIR_SIZE        (sizeof(uint16_t) + sizeof(uint32_t))
#define PRINT_LIMIT           13
#define LSP_MAX_LEN           1539

struct DijkstraElement {
    int dest;
    unsigned int cost;
    int dest_prev;
    int next_hop;
};

void adjacencyArrayInit();
unsigned int getCost(int i, int j);
void setCost(int i, int j, unsigned int newCost);
int isConnected(int i, int j);
unsigned int setLinkStatePacketBuf(char *buf, int target);
void broadcastLSP(int fromNode, char *LSP, unsigned int LSP_length);
//void broadcastTopoLSP();
void broadcastTopoLSP(int block);
void getInfoFromLSPBuf(char *buf, uint16_t *node, uint16_t *neighbors, uint32_t *costs, int count, uint32_t *sequence_number, unsigned int buf_length);
void sendWholeTopology(int target);
void flushSequenceNumber();
void dijkstra();
void printForwardingTableOnce();
void *printAdjacencyArray(void *bound);
void *printForwardingTable(void *unusedParam);

#endif
