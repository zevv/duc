#ifndef ducrc_h
#define ducrc_h

enum ducrc_type {
	DUCRC_TYPE_BOOL,
	DUCRC_TYPE_INT,
	DUCRC_TYPE_DOUBLE,
	DUCRC_TYPE_STRING,
	DUCRC_TYPE_FUNC,
};

struct ducrc_option {
	void *ptr;
	const char *longopt;
	char shortopt;
	enum ducrc_type type;
	const char *descr_short;
	const char *descr_long;
};


struct ducrc;

struct ducrc *ducrc_new(const char *section);
void ducrc_free(struct ducrc *ducrc);

void ducrc_add_options(struct ducrc *ducrc, struct ducrc_option *option_list);

int ducrc_read(struct ducrc *ducrc, const char *path);
int ducrc_getopt(struct ducrc *ducrc, int *argc, char **argv[]);

#endif
