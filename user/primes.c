#include "kernel/types.h"
#include "user/user.h"

void primes_sieve(int left_pipe)
{
    int primes = 0;
    int pipe_child[2] = {0, 0};
    int ret = -1;
    read(left_pipe, &primes, 4);
    printf("prime %d\n", primes);

    int number = 0;
    while (read(left_pipe, &number, 4) != 0)
    {
        if ((number % primes) != 0)
        {
            if (pipe_child[0] == 0)
            {
                if (pipe(pipe_child) < 0)
                {
                    fprintf(2, "create pipe_child failed\n");
                    close(left_pipe);
                    exit(1);
                }
                ret = fork();
                if (ret < 0)
                {
                    fprintf(2, "fork failed\n");
                    close(left_pipe);
                    close(pipe_child[0]);
                    close(pipe_child[1]);
                    exit(1);
                }
                if (ret == 0) // child
                {
                    close(pipe_child[1]);
                    primes_sieve(pipe_child[0]);
                    exit(0);
                }
            }
            write(pipe_child[1], &number, 4);
        }
    }
    close(pipe_child[1]);
    if (ret > 0)
        wait(0);
}

int main(int argc, char *argv[])
{
    int pi[2];
    int ret;

    if (pipe(pi) < 0)
    {
        fprintf(2, "create pipe failed\n");
        exit(1);
    }

    ret = fork();
    if (ret < 0)
    {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    if (ret > 0) // parent write
    {
        close(pi[0]);
        printf("prime 2\n");
        for (int i = 3; i <= 35; i++)
        {
            if ((i % 2) != 0)
                write(pi[1], &i, 4);
        }
        close(pi[1]);
        wait(0);
    }
    else if (ret == 0) // child
    {
        close(pi[1]);
        primes_sieve(pi[0]);
        exit(0);
    }
    exit(0);
}