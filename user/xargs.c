#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/fs.h"

void fork_exec(char *cmd, char **args)
{
    int child = fork();
    if (child < 0)
    {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (child > 0) // parent
    {
        wait(0);
    }
    else
    { // child
        exec(cmd, args);
        fprintf(2, "child exec error\n");
        exit(1);
    }
}
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "USAGE: xargs command\n");
        exit(1);
    }

    char *args[MAXARG];

    int i;
    for (i = 0; i < argc; i++)
    {
        args[i] = argv[i];
    }

    char cmdline[DIRSIZ];
    memset(cmdline, 0, sizeof(cmdline));

    char *p = cmdline;
    while ((p < cmdline + DIRSIZ) && read(0, p, 1) != 0)
    {
        if (*p == '\n')
        {
            *p = '\0';
            args[i] = cmdline;
            char **next = args;
            next++;
            fork_exec(args[1], next);
            p = cmdline;
            continue;
        }
        p++;
    }
    exit(0);
}