#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define COMMANDLEN   1000

void register_process(unsigned int pid)
{
    char command[COMMANDLEN];
    memset(command, '\0', COMMANDLEN);
    sprintf(command, "echo %u > /proc/mp1/status", pid);
    system(command);
}

int main(int argc, char* argv[])
{
    int expire = 10;
    time_t start_time = time(NULL);

    if (argc == 2) {
        expire = atoi(argv[1]);
    }

    register_process(getpid());

    // break out the while loop if the time is expired
    while (1) {
        if ((int)(time(NULL) - start_time) > expire) {
            break;
        }
    }

	return 0;
}
