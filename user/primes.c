#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
    you CAN NOT change the order of "kernel/stat.h" and "user/user.h"
    because "user/user.h" depends on struct stat in "kernel/stat.h"
*/

int child_sieve(int p[2]) {
    close(p[1]);
    // 1.read all bytes from pipe
    int n=0, nleft=34, rn, child_id;
    char rbuf[34];
    while ((rn=read(p[0], &rbuf[n], 34)) != 0 && nleft > 0) {
        if (rn < 0) {
            printf("child read failed\n");
            close(p[0]);
            exit(1);
        }
        nleft -= rn;
        n += rn;
    }
    close(p[0]);

    // 2.manipulate data set in rbuf
    if (n == 0) {
        exit(0);
    }
    int i, wbuf_sz=0, d=(int) rbuf[0];
    char wbuf[34];
    printf("prime %d\n", d); // the first int in pipe must be prime
    for (i=1; i < n; i++) {
        if (((int) rbuf[i]) % d != 0) {
            wbuf[wbuf_sz++] = rbuf[i];
        }
    }

    // 3.feeds wbuf to another process by new pipe
    int np[2];
    pipe(np);
    if ((child_id = fork()) == 0) {
        child_sieve(np);
    } else if (child_id > 0) {
        if (write(np[1], wbuf, wbuf_sz) < 0) {
            printf("child write new pipe failed\n");
            exit(1);
        }
        close(np[0]);
        close(np[1]);
        // 4.wait child stop
        wait(0);
    } else {
        printf("fork failed\n");
        exit(1);
    }

    exit(0);
}

int main(int argc, char *argv[]) {
    int p[2], child_id;
    pipe(p);

    if ((child_id = fork()) == 0) {
        // child
        child_sieve(p);
    } else if (child_id > 0) {
        // parent
        // 1.write data set into pipe
        int i;
        char wbuf[34];
        for (i=2; i <= 35; i++) {
            wbuf[i-2] = (char) i;
        }
        if (write(p[1], wbuf, 34) < 0) {
            printf("parent write failed\n");
            exit(1);
        }
        close(p[0]);
        close(p[1]);

        // 2.wait child stop
        wait(0);
    } else {
        printf("fork failed\n");
        exit(1);
    }
    
    exit(0);
}