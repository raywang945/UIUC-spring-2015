#ifndef __MONITOR_NEIGHBORS_INCLUDE__
#define __MONITOR_NEIGHBORS_INCLUDE__

#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>

struct ForwardingTableEntry {
    unsigned int direct_cost;
    uint32_t sequence_number;
    int cost;
    int next_hop;
};

void* checkNeighbor(void* unusedParam);
void* announceToNeighbors(void* unusedParam);
void hackyBroadcast(const char* buf, int length);
void listenForNeighbors();
void* broadcastLSPPeriodically(void* unusedParam);

#endif
