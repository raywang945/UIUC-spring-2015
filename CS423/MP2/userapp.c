#include "userapp.h"
#include <stdlib.h>

#define BUFLEN        1000

int main(int argc, char* argv[])
{
    int loop_cycle = atoi(argv[3]), i, len, offset;
    int fact_iteration = get_iteration(MAPPINGFILE, atoi(argv[2]));
    unsigned int pid = getpid();
    char buf[BUFLEN];
    struct timeval t0, start, end;

    if (argc != 4) {
        puts("error in argc");
        return 1;
    }

    // register process through /proc/mp2/status
    register_process(pid, argv[1], argv[2]);

    // read /proc/mp2/status and check if the process registered successfully
    if (!is_process_exist(pid)) {
        puts("error in is_process_exist");
        exit(1);
    }

    // record when the task start
    gettimeofday(&t0, NULL);

    // prepare for the buffer which will be written to the log file
    offset = sprintf(buf, "%u\t%s\t%s\t", pid, argv[1], argv[2]);

    // write yield to /proc/mp2/status
    yield_process(pid);

    for (i = 0; i < loop_cycle; i ++) {
        gettimeofday(&start, NULL);
        do_job(fact_iteration);
        gettimeofday(&end, NULL);

        // write to the log file
        len = sprintf(buf + offset, "%f\t%f\t%f\n", time_diff(start, end), time_diff(t0, start), time_diff(t0, end));
        buf[offset + len] = '\0';
        write_log(buf);

        // write yield to /proc/mp2/status
        yield_process(pid);
    }

    // unregister process through /proc/mp2/status
    unregister_process(pid);

	return 0;
}
