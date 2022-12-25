#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <client/arg_parse.h>
#include <common/ipc.h>

int get_args(int argc, char **argv, struct return_struct* ret ) {
    int c;
    int digit_optind = 0;
    ret->cmd = -1;

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"add",      required_argument, 0, 'a'},
            {"priority", required_argument, 0, 'p'},
            {"suspend",  required_argument, 0, 'S'},
            {"resume",   required_argument, 0, 'R'},
            {"remove",   required_argument, 0, 'r'},
            {"info",     required_argument, 0, 'i'},
            {"list",     no_argument,       0, 'l'},
            {"print",    required_argument, 0, 'p'},
            {0,          0,                 0, 0  }
        };

        c = getopt_long(argc, argv, "a:p:S:R:r:i:lp:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {

        case 'a':
            if (ret->cmd != -1){
                return -1;
            }
            ret->cmd = CMD_ADD;
            ret->path = malloc(strlen(optarg) + 1);
            strcpy(ret->path, optarg);
            ret->priority = (int8_t)2;
            break;

        case 'p':
            if (ret->cmd == CMD_ADD){
                ret->priority = atoi(optarg);
            }
            else if (ret->cmd == -1){
                ret->cmd = CMD_PRINT;
                ret->job_id = atoi(optarg);
            }
            else{
                printf("%s", help);
                return -1;
            }
            break;

        case 'S':
            if (ret->cmd != -1){
                printf("%s", help);
                return -1;
            }             
            ret->cmd = CMD_SUSPEND;
            ret->job_id = atoi(optarg);
            break;

        case 'R':
            if (ret->cmd != -1){
                printf("%s", help);
                return -1;
            } 
            ret->cmd = CMD_RESUME;
            ret->job_id = atoi(optarg);
            break;

        case 'r':
            if (ret->cmd != -1){
                printf("%s", help);
                return -1;
            } 
            ret->cmd = CMD_REMOVE;
            ret->job_id = atoi(optarg);
            break;

        case 'i':
            if (ret->cmd != -1){
                printf("%s", help);
                return -1;
            } 
            ret->cmd = CMD_INFO;
            ret->job_id = atoi(optarg);
            break;

        case 'l':
            if (ret->cmd != -1){
                printf("%s", help);
                return -1;
            } 
            ret->cmd = CMD_LIST;
            ret->job_id = -1;
            break;

        case '?':
            printf("%s", help);
            return -1;
            break;
        }
    }

    if (optind < argc) {
        printf("%s", help);
        return -1; 
    }
    return 0;
//sad linux noises :(
}
