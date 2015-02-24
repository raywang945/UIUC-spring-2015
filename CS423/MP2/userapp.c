#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define DIRECTORY    "mp2"
#define FILENAME     "status"
#define COMMANDLEN   1000

void register_process(unsigned int pid, char *period, char *computation)
{
    char command[COMMANDLEN];
    memset(command, '\0', COMMANDLEN);
    sprintf(command, "echo \"R, %u, %s, %s\" > /proc/%s/%s", pid, period, computation, DIRECTORY, FILENAME);
    system(command);
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        puts("error in argc");
        return 1;
    }

    register_process(getpid(), argv[1], argv[2]);

	return 0;
}
