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

        case 'S':
            id = atoi(optarg);
            (*ret).cmd = 2;
            (*ret).payload = malloc(strlen(optarg));
            strcpy((*ret).payload, optarg);
            (*ret).payload_len = strlen(optarg);
            return (void*) ret;
            // printf("option S with value '%d'\n", id);
            // SUSPEND
            break;

        case 'R':
            id = atoi(optarg);
            (*ret).cmd = 3;
            (*ret).payload = malloc(strlen(optarg));
            strcpy((*ret).payload, optarg);
            (*ret).payload_len = strlen(optarg);
            return (void*) ret;
            // printf("option R with value '%d'\n", id);
            // RESUME
            break;

        case 'r':
            id = atoi(optarg);
            (*ret).cmd = 4;
            (*ret).payload = malloc(strlen(optarg));
            strcpy((*ret).payload, optarg);
            (*ret).payload_len = strlen(optarg);
            return (void*) ret;
            // printf("option r with value '%d'\n", id);
            // REMOVE
            break;

        case 'i':
            id = atoi(optarg);
            (*ret).cmd = 5;
            (*ret).payload = malloc(strlen(optarg));
            strcpy((*ret).payload, optarg);
            (*ret).payload_len = strlen(optarg);
            return (void*) ret;
            // printf("option i with value '%d'\n", id);
            // INFO
            break;

        case 'l':
            (*ret).cmd = 6;
            (*ret).payload = NULL;
            (*ret).payload_len = 0;
            return (void*) ret;
            // printf("option l");
            // LIST
            break;

        case '?':
            perror("Invalid argument");
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (arg_a == 1) {
        (*ret).cmd = 1;
        (*ret).payload = malloc(strlen(optarg)+1);
        strcpy((*ret).payload, path);
        (*ret).payload_len = strlen(path);
        return (void*) ret;
        // priority = atoi(p_value);
        // printf("option a with path '%s' and priority '%d'\n", path,  priority);
        // ADD
    } else if (arg_p == 1) {
        (*ret).cmd = 7;
        (*ret).payload = malloc(strlen(p_value));
        strcpy((*ret).payload, path);
        (*ret).payload_len = strlen(p_value);
        return (void*) ret;
        // id = atoi(p_value);
        // printf("option p with value '%d'\n", id);
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
