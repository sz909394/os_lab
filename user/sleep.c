#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int i = 1;

    if(argc < 2){
        fprintf(2, "Usage: sleep NUMBER\n");
        exit(1);
    }

    for(i = 1; i < argc; i++){
        int time = atoi(argv[i]);
        if(sleep(time) < 0)
        {
            fprintf(2, "sleep failed\n");
            exit(1);
        }
    }
    exit(0);
}