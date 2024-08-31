#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
    you CAN NOT change the order of "kernel/stat.h" and "user/user.h"
    because "user/user.h" depends on struct stat in "kernel/stat.h"
*/

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("invalid argument number\n");
        exit(1);
    }
    int d = atoi(argv[1]);
    if (d < 0) {
        printf("invalid argument\n");
        exit(1);
    }
    if (sleep(d) < 0) {
        printf("syscall error\n");
        exit(1);
    }

    exit(0);
}