#include "userapp.h"
#include <stdlib.h>

int main(int argc, const char *argv[])
{
    double tmp;
    struct timeval t0, current;
    int ceil, i, j, avg_time = 10, len;
    char buf[200];
    FILE *fp;

    if (argc != 2) {
        puts("Wrong usage");
        puts("./calculate_mapping <map ceiling of iteration>");
        exit(1);
    }

    ceil = atoi(argv[1]);

    // write result to mapping.csv
    fp = fopen(MAPPINGFILE, "w");
    for (i = 1; i <= ceil; i ++) {
        tmp = 0;
        for (j = 0; j < avg_time; j ++) {
            gettimeofday(&t0, NULL);
            do_job(i);
            gettimeofday(&current, NULL);
            tmp += time_diff(t0, current);
        }
        len = sprintf(buf, "%d,%d\n", i, (int)(tmp / avg_time));
        buf[len] = '\0';
        printf("%s", buf);
        fwrite(buf, sizeof(char), len, fp);
    }
    fclose(fp);

    return 0;
}
