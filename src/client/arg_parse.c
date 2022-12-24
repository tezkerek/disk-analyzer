#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <client/arg_parse.h>
#include <common/ipc.h>

struct return_struct *get_args(int argc, char **argv) {
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
    int valid_args = 0;
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
        if ((valid_args == 1 && arg_a == 0 && arg_p == 0) ||
            (valid_args == 1 && arg_a == 1 && c != 'p') ||
            (valid_args == 1 && arg_p == 1 && c != 'a')) {
            printf("%s", help);
            return NULL;
            break;
        }

        switch (c) {

        case 'a':
            arg_a = 1;
            path = malloc(strlen(optarg));
            path = optarg;
            break;

        case 'p':
            arg_p = 1;
            strcpy(p_value, optarg);
            break;

        case 'S': 
            ret->cmd = CMD_SUSPEND;
            break;

        case 'R': 
            ret->cmd = CMD_RESUME;
            break;

        case 'r': 
            ret->cmd = CMD_REMOVE;
            break;

        case 'i': 
            ret->cmd = CMD_INFO;
            break;

        case 'l': 
            ret->cmd = CMD_LIST;
            break;

        case '?':
            printf("%s", help);
            return NULL;
            break;
        }

        if (ret->cmd == CMD_LIST) {
            ret->uni.job_id = -1;
        } else if (ret->cmd >= 2 && ret->cmd <= 5) {
            ret->uni.job_id = atoi(optarg);
        }
        valid_args = 1;
    }

    if (arg_a == 1) { 
        ret->cmd = CMD_ADD;
        ret->uni.path = malloc(strlen(path) + 2);
        ret->uni.path[0] = ((int8_t)*p_value) - ((int8_t)'0');
        ret->uni.path++;
        strcpy(ret->uni.path, path);
        ret->uni.path--;

    } else if (arg_p == 1) { 
        ret->cmd = CMD_PRINT;
        ret->uni.job_id = atoi(p_value);
    }

    if (optind < argc) {
        printf("%s", help);
    }
    return ret;
}
