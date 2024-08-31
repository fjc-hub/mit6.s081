#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

/*
    you CAN NOT change the order of "kernel/stat.h" and "user/user.h"
    because "user/user.h" depends on struct stat in "kernel/stat.h"
*/

/*
    test case 1
    $ echo hello too | xargs echo bye
    bye hello too
    $

    test case 2
    $ find . b | xargs grep hello
    
*/

// try to read a line including newline char
int readline(int fd, char *buf, int maxsz) {
    int n, sz=0;
    char tbuf[1];

    while((n = read(fd, tbuf, 1)) == 1) {
        buf[sz++] = tbuf[0];
        if (tbuf[0] == '\n') {
            break;
        }
        if (sz > maxsz) {
            fprintf(2, "readline fail: exceed maxsz\n");
            return -2;
        }
    }

    if (n < 0) {
        fprintf(2, "readline failed\n");
        return -1;
    }

    return sz;
}

int main(int argc, char *argv[]) {
    int sz, maxline=512, fk;
    char buf[maxline];

    if (argc <= 2) {
        fprintf(2, "invalid argument number\n");
        exit(0);
    }

    while((sz = readline(0, buf, maxline)) > 0) { // read from stdin
        // 1.remove newline character
        buf[--sz] = '\0';
        // 2.fork
        if ((fk = fork()) == 0) {
            // child
            // construct argv
            int i, x, y;
            char *cargv[MAXARG];
            cargv[0] = argv[1];
            for (i=1; i + 1 < argc; i++) {
                cargv[i] = argv[i+1];
            }
            for (x=0, y=0; y < sz; y++) {
                if (buf[y] == ' ') {
                    cargv[i++] = &buf[x];
                    buf[y] = '\0';
                    x = y + 1;
                }
            }
            if (y > x) {
                cargv[i++] = &buf[x];
                buf[y] = '\0';
            }
            cargv[i] = 0;
            if (exec(argv[1], cargv) < 0)
                fprintf(2, "exec failed\n");
        } else if (fk > 0) {
            // parent
            wait(0);
        } else {
            fprintf(2, "fork failed\n");
            exit(1);
        }
    }

    exit(0);
}