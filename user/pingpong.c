#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int P0[2];
    int P1[2];
    int ret = 0;

    if(pipe(P0) < 0)
    {
        fprintf(2, "create pipe0 failed\n");
        exit(1);
    }
    if(pipe(P1) < 0)
    {
        fprintf(2, "create pipe1 failed\n");
        exit(1);
    }

    ret = fork();
    if(ret < 0)
    {
        fprintf(2, "fork failed\n");
        exit(1);       
    }

    if(ret > 0) // parent
    {
        char p_send = 'a';
        char buf;
        close(P0[0]);
        close(P1[1]);
        write(P0[1], &p_send, sizeof(p_send));
        close(P0[1]);
        wait((int *) 0);
        read(P1[0], &buf, sizeof(buf));
        printf("%d: received pong\n", getpid());
        close(P1[0]);
    }
    else if (ret == 0)  // child
    {
        char buf;
        close(P0[1]);
        close(P1[0]);
        read(P0[0], &buf, sizeof(buf));
        close(P0[0]);
        printf("%d: received ping\n", getpid());
        char c_send = 'b';
        write(P1[1], &c_send, sizeof(c_send));
        close(P1[1]);
        exit(0);
    }
    exit(0);
}