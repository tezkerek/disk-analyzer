#ifndef ARG_PARSE_HEADER
#define ARG_PARSE_HEADER

static char *help =
    "Usage: da [OPTION]... [DIR]...\n"
    "Analyze the space occupied by the directory at [DIR]\n"
    "-a, --add analyze a new directory path for disk usage\n"
    "-p, --priority set priority for the new analysis (works only with -a argument)\n"
    "-S, --suspend <id> suspend task with <id>\n"
    "-R, --resume <id> resume task with <id>\n"
    "-r, --remove <id> remove the analysis with the given <id>\n"
    "-i, --info <id> print status about the analysis with <id> (pending, progress, done)\n"
    "-l, --list list all analysis tasks, with their ID and the corresponding root path\n"
    "-p, --print <id> print analysis report for those tasks that are done\n";
	

struct return_struct {
    int8_t cmd;
	union struct_union{
		int64_t job_id;
		char* path;
	} uni;
};

struct return_struct *get_args(int argc, char **argv);

#endif
