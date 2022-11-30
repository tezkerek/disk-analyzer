#include <stdio.h>   
#include <stdlib.h>    
#include <getopt.h>
#include <string.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
int
main(int argc, char **argv)
{
    int c;
    int digit_optind = 0;
    int arg_a = 0;
    int arg_p = 0;
    char path[1000];
    char p_value[10];
    int priority;
    p_value[0] = '2';
    int id;

   while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"add",     required_argument, 0, 'a'},
            {"priority",required_argument, 0, 'p'},
            {"suspend", required_argument, 0, 'S'},
            {"resume",  required_argument, 0, 'R'},
            {"remove",  required_argument, 0, 'r'},
            {"info",    required_argument, 0, 'i'},
            {"list", 	 no_argument,       0, 'l'},
            {"print",   required_argument, 0, 'p'},
            {0,         0,                 0,  0 }
        };

       c = getopt_long(argc, argv, "a:p:S:R:r:i:lp:",
                 long_options, &option_index);
        if (c == -1)
            break;

       switch (c) {

       case 'a':
            arg_a = 1;
            strcpy(path, optarg);
            break;

       case 'p':
            arg_p = 1;
            strcpy(p_value, optarg);
            break;

       case 'S':
            id = atoi(optarg);
            //printf("option S with value '%d'\n", id);
            //SUSPEND
            break;

       case 'R':
            id = atoi(optarg);
            //printf("option R with value '%d'\n", id);
            //RESUME
            break;
            
       case 'r':
            id = atoi(optarg);
            //printf("option r with value '%d'\n", id);
            //REMOVE
            break;
         
       case 'i':
            id = atoi(optarg);
            //printf("option i with value '%d'\n", id);
            //INFO
            break;
       
       case 'l':
            //printf("option l");
            //LIST
            break;

       case '?':
            printf("invalid option!\n");
            exit(EXIT_FAILURE);
            break;
            
        }
    }
    
    if (arg_a == 1)
    {
    	priority = atoi(p_value);
    	//printf("option a with path '%s' and priority '%d'\n", path,  priority);
    	//ADD
    }
    else if (arg_p == 1)
    {
    	id = atoi(p_value);
    	//printf("option p with value '%d'\n", id);
    	//PRINT
    }

   if (optind < argc) {
        //printf("non-option ARGV-elements: "); //vrem sa facem ceva cu argumentele random? 
        while (optind < argc)
            printf("%s ", argv[optind++]);
        //printf("\n");
    }

   exit(EXIT_SUCCESS);
}
