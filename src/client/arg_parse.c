#include <client/arg_parse.h>
#include <common/ipc.h>
#include <common/utils.h>
#include <getopt.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PRIORITY 2

static struct option long_options[] = {
    {"add",      required_argument, 0, 'a'},
    {"priority", required_argument, 0, 'p'},
    {"suspend",  required_argument, 0, 'S'},
    {"resume",   required_argument, 0, 'R'},
    {"remove",   required_argument, 0, 'r'},
    {"info",     required_argument, 0, 'i'},
    {"list",     no_argument,       0, 'l'},
    {"print",    required_argument, 0, 'P'},
    {"help",     no_argument,       0, 'h'},
    {0,          0,                 0, 0  }
};

int parse_args(int argc, char **argv, struct da_args *result) {
    result->cmd = -1;
    result->priority = DEFAULT_PRIORITY;

    while (1) {
        int c = getopt_long(argc, argv, "a:p:S:R:r:i:lP:h", long_options, NULL);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_ADD;
            result->path = da_malloc(PATH_MAX);
            realpath(optarg, result->path);
            break;

        case 'p':
            int8_t priority = atoi(optarg);
            if (priority < 1 || priority > 3) {
                fputs("Priority must be between 1 and 3\n", stderr);
                return -1;
            }
            result->priority = priority;
            break;

        case 'P':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_PRINT;
            result->job_id = atoll(optarg);
            break;

        case 'S':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_SUSPEND;
            result->job_id = atoll(optarg);
            break;

        case 'R':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_RESUME;
            result->job_id = atoll(optarg);
            break;

        case 'r':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_REMOVE;
            result->job_id = atoll(optarg);
            break;

        case 'i':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_INFO;
            result->job_id = atoll(optarg);
            break;

        case 'l':
            if (result->cmd != -1) {
                return -1;
            }
            result->cmd = CMD_LIST;
            result->job_id = -1;
            break;

        case 'h':
            fputs(DA_USAGE_HELP, stdout);
            exit(EXIT_SUCCESS);

        case '?':
            return -1;
            break;
        }
    }

    if (result->cmd == -1) {
        // Missing command
        return -1;
    }

    if (optind < argc) {
        return -1;
    }

    return 0;
}
