#ifndef CLIENT_HEADER
#define CLIENT_HEADER

    struct return_struct
	{
		int8_t cmd;
		int64_t payload_len;
		char *payload;
	};

struct return_struct* get_args(int argc, char** argv);

#endif
