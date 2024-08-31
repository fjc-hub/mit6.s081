#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

/*
    you CAN NOT change the order of "kernel/stat.h" and "user/user.h"
    because "user/user.h" depends on struct stat in "kernel/stat.h"
*/

char* getNameFromPath(char *path) {
    static char buf[DIRSIZ+1];
    char *p;

    // get the last slash in path, if no slash, p point to the place before path
    for(p=path+strlen(path); p >= path && (*p) != '/'; p--);

    p++;

    int sz = strlen(p);
    memmove(buf, p, sz);
    buf[sz] = '\0'; // *p = '\0'
    return buf;
}

void find(char *path, char *file) {
    // printf("find(%s, %s)\n", path, file);
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find failed: cannot open %s;\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find failed: cannot stat %s\n", path);
        close(fd);
        return;
    }

    int path_sz = strlen(path);
    memmove(buf, path, path_sz); // strcpy(buf, path);
    p = buf + path_sz;
    *p++ = '/';

    switch (st.type)
    {
    case T_FILE:
        char *name = getNameFromPath(path);
        if (strcmp(file, name) == 0) {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        // check all files in dir
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) // avoid dead loop
                continue;
            
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            // printf("dir path=%s; next path=%s; de.name=%s;\n", path, buf, de.name);
            find(buf, file);
        }
        break;
    default:
        break;
    }
    close(fd);
    return;
}

int main(int argc, char *argv[]) {
    if(argc != 3){
        printf("invalid argument number\n");
        exit(0);
    }

    find(argv[1], argv[2]);
    exit(0);
}