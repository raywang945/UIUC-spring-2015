#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define DIRECTORY     "mp2"
#define FILENAME      "status"
#define COMMANDLEN    1000
#define FILENAMELEN   500
#define BUFFERLEN     5000
#define PIDLISTLEN    100

void register_process(unsigned int pid, char *period, char *computation)
{
    char command[COMMANDLEN];
    memset(command, '\0', COMMANDLEN);
    sprintf(command, "echo \"R, %u, %s, %s\" > /proc/%s/%s", pid, period, computation, DIRECTORY, FILENAME);
    system(command);
}

void yield_process(unsigned int pid)
{
    char command[COMMANDLEN];
    memset(command, '\0', COMMANDLEN);
    sprintf(command, "echo \"Y, %u\" > /proc/%s/%s", pid, DIRECTORY, FILENAME);
    system(command);
}

void unregister_process(unsigned int pid)
{
    char command[COMMANDLEN];
    memset(command, '\0', COMMANDLEN);
    sprintf(command, "echo \"D, %u\" > /proc/%s/%s", pid, DIRECTORY, FILENAME);
    system(command);
}

int is_exist_in_array(unsigned int *array, int array_len, int number)
{
    int i;
    for (i = 0; i < array_len; i ++) {
        if (array[i] == number) {
            return 1;
        }
    }
    return 0;
}

int is_process_exist(unsigned int pid)
{
    char filename[FILENAMELEN], buf[BUFFERLEN];
    FILE *fd;
    unsigned int pid_list[PIDLISTLEN], tmp_pid;
    int i, len, pid_list_num = 0;
    char *tmp;

    len = sprintf(filename, "/proc/%s/%s", DIRECTORY, FILENAME);
    filename[len] = '\0';

    fd = fopen(filename, "r");
    len = fread(buf, sizeof(char), BUFFERLEN - 1, fd);
    buf[len] = '\0';
    fclose(fd);

    tmp = strtok(buf, "\n");
    sscanf(tmp, "%u", &tmp_pid);
    while (!is_exist_in_array(pid_list, pid_list_num, tmp_pid)) {
        pid_list[pid_list_num] = tmp_pid;
        pid_list_num ++;
        tmp = strtok(NULL, "\n");
        sscanf(tmp, "%u", &tmp_pid);
    }

    for (i = 0; i < pid_list_num; i ++) {
        if (pid_list[i] == pid) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int expire = atoi(argv[3]);
    time_t start_time = time(NULL);
    unsigned int pid = getpid();

    if (argc != 4) {
    /*if (argc != 3) {*/
        puts("error in argc");
        return 1;
    }

    register_process(pid, argv[1], argv[2]);

    if (!is_process_exist(pid)) {
        puts("error in is_process_exist");
        exit(1);
    }

    /*yield_process(pid);*/

    // break out the while loop if the time is expired
    while (1) {
        if ((int)(time(NULL) - start_time) > expire) {
            break;
        }
    }

    unregister_process(pid);

	return 0;
}
