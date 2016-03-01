#include "monitor_neighbors.h"
#include "linkstate.h"

unsigned int *adjacency_array;
extern int globalMyID;
extern int globalSocketUDP;
extern struct sockaddr_in globalNodeAddrs[256];
extern struct ForwardingTableEntry forwarding_table[256];
char globalLSP[LSP_MAX_LEN];
unsigned int globalLSPLength;

int getIndex(int i, int j)
{
    return ((i - 1) * i) / 2 + j;
}

void adjacencyArrayInit()
{
    int i, j;
    adjacency_array = (unsigned int *)malloc(ADJACENCY_ARRAY_LEN * sizeof(int));
    for (i = 0; i < ADJACENCY_ARRAY_LEN; i ++) {
        adjacency_array[i] = 0;
    }
}

unsigned int getCost(int i, int j)
{
    if (i < j) {
        int tmp = i;
        i = j;
        j = tmp;
    }
    return adjacency_array[getIndex(i, j)];
}

int isConnected(int i, int j)
{
    if (getCost(i, j) == 0) {
        return 0;
    } else {
        return 1;
    }
}

void setCost(int i, int j, unsigned int newCost)
{
    if (i < j) {
        int tmp = i;
        i = j;
        j = tmp;
    }
    adjacency_array[getIndex(i, j)] = newCost;
}

// return buf length
/*unsigned int setLinkStatePacketBuf(char *buf)*/
/*{*/
    /*uint16_t node = htons((uint16_t)globalMyID);*/
    /*uint32_t sequence_number = htonl(forwarding_table[globalMyID].sequence_number);*/
    /*int i, count = 0;*/

    /*sprintf(buf, "LSP");*/
    /*memcpy(buf + 3, &node, sizeof(uint16_t));*/
    /*for (i = 0; i < 256; i ++) {*/
        /*if (i == globalMyID) {*/
            /*continue;*/
        /*}*/
        /*if (isConnected(globalMyID, i)) {*/
            /*uint16_t neighbor = htons((uint16_t)i);*/
            /*uint32_t cost = htonl((uint32_t)getCost(globalMyID, i));*/
            /*memcpy(buf + 3 + sizeof(uint16_t) + COST_PAIR_SIZE * count, &neighbor, sizeof(uint16_t));*/
            /*memcpy(buf + 3 + sizeof(uint16_t) * 2 + COST_PAIR_SIZE * count, &cost, sizeof(uint32_t));*/
            /*count ++;*/
        /*}*/
    /*}*/
    /*if (count == 0) {*/
        /*return 0;*/
    /*}*/
    /*memcpy(buf + 3 + sizeof(uint16_t) + COST_PAIR_SIZE * count, &sequence_number, sizeof(uint32_t));*/
    /*return 3 + COST_PAIR_SIZE * (count + 1);*/
/*}*/

unsigned int setLinkStatePacketBuf(char *buf, int target)
{
    uint16_t node = htons((uint16_t)target);
    uint32_t sequence_number = htonl(forwarding_table[target].sequence_number);
    int i, count = 0;

    sprintf(buf, "LSP");
    memcpy(buf + 3, &node, sizeof(uint16_t));
    for (i = 0; i < 256; i ++) {
        if (i == target) {
            continue;
        }
        if (isConnected(target, i)) {
            uint16_t neighbor = htons((uint16_t)i);
            uint32_t cost = htonl((uint32_t)getCost(target, i));
            memcpy(buf + 3 + sizeof(uint16_t) + COST_PAIR_SIZE * count, &neighbor, sizeof(uint16_t));
            memcpy(buf + 3 + sizeof(uint16_t) * 2 + COST_PAIR_SIZE * count, &cost, sizeof(uint32_t));
            count ++;
        }
    }
    if (count == 0) {
        return 0;
    }
    memcpy(buf + 3 + sizeof(uint16_t) + COST_PAIR_SIZE * count, &sequence_number, sizeof(uint32_t));
    return 3 + COST_PAIR_SIZE * (count + 1);
}

void broadcastLSP(int fromNode, char *LSP, unsigned int LSP_length)
{
    int i;
    for (i = 0; i < 256; i ++) {
        if (i != fromNode && i != globalMyID) {
            if (isConnected(i, globalMyID)) {
                sendto(globalSocketUDP, LSP, LSP_length, 0, (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
            }
        }
    }
}

void broadcastTopoLSP(int block)
{
    /*char *buf = (char *)malloc(LSP_MAX_LEN);*/
    /*forwarding_table[globalMyID].sequence_number ++;*/
    /*unsigned int len = setLinkStatePacketBuf(buf, globalMyID);*/
    /*broadcastLSP(block, buf, len);*/
    broadcastLSP(block, globalLSP, globalLSPLength);
    /*free(buf);*/
    /*buf = NULL;*/
}

void getInfoFromLSPBuf(char *buf, uint16_t *node, uint16_t *neighbors, uint32_t *costs, int count, uint32_t *sequence_number, unsigned int buf_length)
{
    int i;

    memcpy(node, buf + 3, sizeof(uint16_t));
    *node = ntohs(*node);
    memcpy(sequence_number, buf + 3 + sizeof(uint16_t) + COST_PAIR_SIZE * count, sizeof(uint32_t));
    *sequence_number = ntohl(*sequence_number);

    for (i = 0; i < count; i ++) {
        memcpy(&neighbors[i], buf + 3 + sizeof(uint16_t) + COST_PAIR_SIZE * i, sizeof(uint16_t));
        memcpy(&costs[i], buf + 3 + sizeof(uint16_t) * 2 + COST_PAIR_SIZE * i, sizeof(uint32_t));
        neighbors[i] = ntohs(neighbors[i]);
        costs[i] = ntohl(costs[i]);
    }
}

unsigned int *DFS(int *count)
{
    unsigned int queue[256];
    int i, j, queue_count = 1;
    queue[0] = globalMyID;
    unsigned int dfs_result[256];
    *count = 0;
    unsigned int target;

    while (queue_count > 0) {
        queue_count --;
        target = queue[queue_count];
        int isAdded = 0;
        for (i = 0; i < *count; i ++) {
            if (target == (int)dfs_result[i]) {
                isAdded = 1;
                break;
            }
        }
        if (isAdded) {
            continue;
        }
        dfs_result[*count] = target;
        (*count) ++;
        for (i = 0; i < 256; i ++) {
            if ((int)target == i) {
                continue;
            }
            if (isConnected((int)target, i)) {
                int isAdded = 0;
                for (j = 0; j < *count; j ++) {
                    if (i == (int)dfs_result[j]) {
                        isAdded = 1;
                        break;
                    }
                }
                if (!isAdded) {
                    queue[queue_count] = i;
                    queue_count ++;
                }
            }
        }
    }

    unsigned int *result = (unsigned int *)malloc(*count * sizeof(unsigned int));
    for (i = 0; i < *count; i ++) {
        result[i] = dfs_result[i];
    }

    return result;
}

void sendWholeTopology(int target)
{
    char *LSP = (char *)malloc(LSP_MAX_LEN);
    unsigned int *dfs;
    int i, count;
    dfs = DFS(&count);
    for (i = 0; i < count; i ++) {
        unsigned int LSP_length = setLinkStatePacketBuf(LSP, (int)dfs[i]);
        sendto(globalSocketUDP, LSP, LSP_length, 0, (struct sockaddr*)&globalNodeAddrs[target], sizeof(globalNodeAddrs[target]));
    }
    free(LSP);
    LSP = NULL;
}

void flushSequenceNumber()
{
    unsigned int *dfs;
    int i, j, count;
    dfs = DFS(&count);
    for (i = 0; i < 256; i ++) {
        if (i == globalMyID) {
            continue;
        }
        for (j = 1; j < count; j ++) {
            if (i == dfs[j]) {
                break;
            }
        }
        forwarding_table[i].sequence_number = 0;
    }
}

struct DijkstraElement getTentative(struct DijkstraElement *tentative_array, int tentative_num, int target)
{
    struct DijkstraElement result = { .dest = -1, .cost = 0, .next_hop = -1 };
    int i;
    for (i = 0; i < tentative_num; i ++) {
        if (tentative_array[i].dest == target) {
            return tentative_array[i];
        }
    }
    return result;
}

void setTentativeElement(struct DijkstraElement *tentative_array, int tentative_num, struct DijkstraElement sample)
{
    int i;
    for (i = 0; i < tentative_num; i ++) {
        if (tentative_array[i].dest == sample.dest) {
            tentative_array[i] = sample;
            break;
        }
    }
}

struct DijkstraElement *updateTentativeArray(struct DijkstraElement *tentative_array, int *tentative_num, int source_node)
{
    int source_node_cost = forwarding_table[source_node].cost;
    int i;
    for (i = 0; i < 256; i ++) {
        if (i == source_node) {
            continue;
        }
        if (isConnected(source_node, i) && forwarding_table[i].cost == -1) {
            int linkCost = getCost(source_node, i);
            struct DijkstraElement tmp = getTentative(tentative_array, *tentative_num, i);
            if (tmp.dest != -1) {
                // dest i exist in tentative_array
                // update i
                if (tmp.cost > source_node_cost + linkCost) {
                    // update node i in tentative_array
                    tmp.cost = source_node_cost + linkCost;
                    tmp.dest_prev = source_node;
                    tmp.next_hop = forwarding_table[source_node].next_hop;
                    setTentativeElement(tentative_array, *tentative_num, tmp);
                } else if (tmp.cost == source_node_cost + linkCost) {
                    // tie break
                    if (tmp.dest_prev > source_node) {
                        tmp.cost = source_node_cost + linkCost;
                        tmp.dest_prev = source_node;
                        tmp.next_hop = forwarding_table[source_node].next_hop;
                        setTentativeElement(tentative_array, *tentative_num, tmp);
                    }
                }
            } else {
                // add i into tentative_array
                if (*tentative_num == 0) {
                    tentative_array = (struct DijkstraElement*)malloc(sizeof(struct DijkstraElement));
                } else {
                    tentative_array = (struct DijkstraElement*)realloc(tentative_array, (*tentative_num + 1) * sizeof(struct DijkstraElement));
                }
                tentative_array[*tentative_num].dest = i;
                tentative_array[*tentative_num].cost = source_node_cost + linkCost;
                tentative_array[*tentative_num].dest_prev = source_node;
                if (forwarding_table[source_node].next_hop == -1) {
                    tentative_array[*tentative_num].next_hop = i;
                } else {
                    tentative_array[*tentative_num].next_hop = forwarding_table[source_node].next_hop;
                }
                *tentative_num = *tentative_num + 1;
            }
        }
    }
    return tentative_array;
}

struct DijkstraElement getSmallestCostElement(struct DijkstraElement *tentative_array, int tentative_num)
{
    struct DijkstraElement result = tentative_array[0];
    int i;
    for (i = 0; i < tentative_num; i ++) {
        if (result.cost > tentative_array[i].cost) {
            result = tentative_array[i];
        }
    }
    return result;
}

struct DijkstraElement *updateForwardingTable(struct DijkstraElement *tentative_array, int *tentative_num, int *source_node)
{
    if (*tentative_num == 0) {
        *source_node = -1;
        return tentative_array;
    }
    struct DijkstraElement smallestCostElement = getSmallestCostElement(tentative_array, *tentative_num);
    *tentative_num = *tentative_num - 1;
    *source_node = smallestCostElement.dest;

    // set forwarding_table
    forwarding_table[smallestCostElement.dest].cost = smallestCostElement.cost;
    forwarding_table[smallestCostElement.dest].next_hop = smallestCostElement.next_hop;

    if (*tentative_num == 0) {
        return NULL;
    }
    struct DijkstraElement *result = (struct DijkstraElement*)malloc(*tentative_num * sizeof(struct DijkstraElement));
    int i, count = 0;
    for (i = 0; i < *tentative_num + 1; i ++) {
        if (smallestCostElement.dest != tentative_array[i].dest) {
            memcpy(&result[count], &(tentative_array[i]), sizeof(struct DijkstraElement));
            count ++;
        }
    }
    free(tentative_array);
    tentative_array = NULL;
    return result;
}

void printTentativeArray(struct DijkstraElement *tentative_array, int tentative_num)
{
    puts("==============");
    int i;
    for (i = 0; i < tentative_num; i ++) {
        printf("%d\t%u\t%d\t->\t%d\n", tentative_array[i].dest, tentative_array[i].cost, tentative_array[i].next_hop, tentative_array[i].dest_prev);
    }
}

void dijkstra()
{
    struct DijkstraElement *tentative_array;
    int i, tentative_num = 0, sourceNode = globalMyID;

    // reset forwarding_table
    for (i = 0; i < 256; i ++) {
        forwarding_table[i].next_hop = -1;
        forwarding_table[i].cost = -1;
    }
    forwarding_table[globalMyID].cost = 0;

    while (sourceNode != -1) {
        tentative_array = updateTentativeArray(tentative_array, &tentative_num, sourceNode);
        /*printTentativeArray(tentative_array, tentative_num);*/
        /*printForwardingTableOnce();*/
        tentative_array = updateForwardingTable(tentative_array, &tentative_num, &sourceNode);
        /*printTentativeArray(tentative_array, tentative_num);*/
        /*printForwardingTableOnce();*/
    }
    return;
}

void printAdjacencyArrayOnce()
{
    int i, j;
    puts("************");
    for (i = 1; i < PRINT_LIMIT; i ++) {
        for (j = 0; j < i; j ++) {
            printf("%u\t", getCost(i, j));
        }
        printf("\n");
    }
}

void* printAdjacencyArray(void *bound)
{
    struct timeval current;
    struct timespec sleepFor;
    sleepFor.tv_sec = 1;
    sleepFor.tv_nsec = 0;

    while (1) {
        printAdjacencyArrayOnce();
        nanosleep(&sleepFor, 0);
    }
}

void printForwardingTableOnce()
{
    puts("$$$$$$$$$$$$$$$$");
    int i;
    for (i = 0; i < PRINT_LIMIT; i ++) {
        /*printf("%d\t%d\t%d\n", i, forwarding_table[i].cost, forwarding_table[i].next_hop);*/
        printf("forwarding_table[%d].sequence_number = %"PRIu32"\n", i, forwarding_table[i].sequence_number);
    }
}

void *printForwardingTable(void *unusedParam)
{
    struct timeval current;
    struct timespec sleepFor;
    sleepFor.tv_sec = 1;
    sleepFor.tv_nsec = 0;

    while (1) {
        printForwardingTableOnce();
        nanosleep(&sleepFor, 0);
    }
}
