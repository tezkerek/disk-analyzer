#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <client/arg_parse.h>


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
    int valid_args = 0;
    int id;
    char* help = "Usage: da [OPTION]... [DIR]...\n"
    "Analyze the space occupied by the directory at [DIR]\n"
    "-a, --add analyze a new directory path for disk usage\n"
    "-p, --priority set priority for the new analysis (works only with -a argument)\n"
    "-S, --suspend <id> suspend task with <id>\n"
    "-R, --resume <id> resume task with <id>\n"
    "-r, --remove <id> remove the analysis with the given <id>\n"
    "-i, --info <id> print status about the analysis with <id> (pending, progress, done)\n"
    "-l, --list list all analysis tasks, with their ID and the corresponding root path\n"
    "-p, --print <id> print analysis report for those tasks that are done\n";

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
        if ((valid_args == 1 && arg_a == 0 && arg_p == 0)
            || (valid_args == 1 && arg_a == 1 && c != 'p')
            || (valid_args == 1 && arg_p == 1 && c != 'a'))
        {
            printf("%s",help);
            exit(EXIT_FAILURE);
            break;
        }

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
            ret->cmd = 2;
            break;

        case 'R':            // RESUME
            ret->cmd = 3;
            break;

        case 'r':            // REMOVE
            ret->cmd = 4;
            break;

        case 'i':            // INFO
            ret->cmd = 5;
            break;

        case 'l':            // LIST
            ret->cmd = 6;
            break;

        case '?':
            printf("%s", help);
            exit(EXIT_FAILURE);
            break;
        }

        if (ret->cmd == 6)
        {
            ret->payload = "";
            ret->payload_len = 0;
        }
        else if (ret->cmd>=2 && ret->cmd <=5)
        {
            ret->payload = malloc(strlen(optarg));
            ret->payload_len = strlen(optarg);
        }
        valid_args = 1;
    }

    if (arg_a == 1) { //ADD
        ret->cmd = 1;
        ret->payload = malloc(strlen(path)+2);
        ret->payload[0] = ((int8_t)*p_value)-((int8_t)'0');
        ret->payload++;
        strcpy(ret->payload, path);
        ret->payload--;
        ret->payload_len = strlen(path)+2;

    } else if (arg_p == 1) { //PRINT
        ret->cmd = 7;
        ret->payload = malloc(strlen(p_value));
        strcpy(ret->payload, p_value);
        ret->payload_len = strlen(p_value);
    }

    if (optind < argc) {
        printf("%s", help);
        exit(EXIT_FAILURE);
    }
    return ret;

    exit(EXIT_SUCCESS);
}
