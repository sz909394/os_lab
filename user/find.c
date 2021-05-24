#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *dir, char *file)
{
    int fd;
    struct stat st;
    struct dirent de;
    char buf[512], *p;

    if ((fd = open(dir, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", dir);
        exit(1);
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", dir);
        close(fd);
        exit(1);
    }

    if (st.type != T_DIR)
    {
        fprintf(2, "find: %s is not a dir\n", dir);
        close(fd);
        exit(1);
    }

    if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        printf("find: path too long\n");
        close(fd);
        exit(1);
    }
    strcpy(buf, dir);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0)
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        if ((st.type == T_FILE) && (strcmp(de.name, file) == 0))
            printf("%s\n", buf);

        if ((st.type == T_DIR) && (strcmp(de.name, ".") != 0) && (strcmp(de.name, "..") != 0))
        {
            find(buf, file);
        }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(2, "USAGE: find dirxx filexx\n");
        exit(1);
    }

    char *dir = argv[1];
    char *file = argv[2];
    find(dir, file);
    exit(0);
}