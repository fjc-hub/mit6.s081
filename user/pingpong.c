#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
    you CAN NOT change the order of "kernel/stat.h" and "user/user.h"
    because "user/user.h" depends on struct stat in "kernel/stat.h"
*/

int main(int argc, char *argv[]) {
    int p[2];
    char wbuf[1], rbuf[1];
    pipe(p);

    if (fork() == 0) {
        // child
        int t;
        while ((t = read(p[0], rbuf, 1)) == 0);
        if (t < 0) {
            printf("child read failed\n");
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        if (write(p[1], &wbuf, 1) < 0) {
            printf("child write failed\n");
            exit(1);
        }
    } else {
        // parent
        if (write(p[1], &wbuf, 1) < 0) {
            printf("parent write failed\n");
            exit(1);
        }
        int t;
        while ((t = read(p[0], rbuf, 1)) == 0);
        if (t < 0) {
            printf("parent read failed\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
    }
    close(p[0]);
    close(p[1]);
    exit(0);
}