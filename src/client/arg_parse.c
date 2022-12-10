#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client/arg_parse.h"


struct return_struct* get_args(int argc, char **argv) {
    struct return_struct *ret;
    ret = malloc(sizeof(struct return_struct)); 
    int c;
    int digit_optind = 0;
    int arg_a = 0;
    int arg_p = 0;
    char *path;
    char p_value[10];
    int priority;
    p_value[0] = '2';
    int id;

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
            arg_a = 1;
            path = malloc(strlen(optarg));
            strcpy(path, optarg);
            break;

        case 'p':
            arg_p = 1;
            strcpy(p_value, optarg);
            break;

        case 'S':            // SUSPEND
            id = atoi(optarg);
            ret->cmd = 2;
            ret->payload = malloc(strlen(optarg));
            strcpy(ret->payload, optarg);
            ret->payload_len = strlen(optarg);
            return ret;
            break;

        case 'R':            // RESUME
            id = atoi(optarg);
            ret->cmd = 3;
            ret->payload = malloc(strlen(optarg));
            strcpy(ret->payload, optarg);
            ret->payload_len = strlen(optarg);
            return ret;
            break;

        case 'r':            // REMOVE
            id = atoi(optarg);
            ret->cmd = 4;
            ret->payload = malloc(strlen(optarg));
            strcpy(ret->payload, optarg);
            ret->payload_len = strlen(optarg);
            return ret;
            break;

        case 'i':            // INFO
            id = atoi(optarg);
            ret->cmd = 5;
            ret->payload = malloc(strlen(optarg));
            strcpy(ret->payload, optarg);
            ret->payload_len = strlen(optarg);
            return ret;
            break;

        case 'l':            // LIST
            ret->cmd = 6;
            ret->payload = "";
            ret->payload_len = 0;
            return ret;
            break;

        case '?':
            perror("Invalid argument");
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (arg_a == 1) {
        ret->cmd = 1;
        ret->payload = malloc(strlen(path)+1);
        strcpy(ret->payload,p_value);
        ret->payload++;
        strcpy(ret->payload, path);
        ret->payload--;
        ret->payload_len = strlen(path)+1;
        return ret;
        // ADD
    } else if (arg_p == 1) {
        ret->cmd = 7;
        ret->payload = malloc(strlen(p_value));
        strcpy(ret->payload, p_value);
        ret->payload_len = strlen(p_value);
        return ret;
        // PRINT
    }

    if (optind < argc) {
        // printf("non-option ARGV-elements: ");
        while (optind < argc)
            printf("%s ", argv[optind++]);
        // printf("\n");
    }

    exit(EXIT_SUCCESS);
}
