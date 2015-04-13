#ifndef ducrc_h
#define ducrc_h

enum ducrc_type {
	DUCRC_TYPE_BOOL,
	DUCRC_TYPE_INT,
	DUCRC_TYPE_FLOAT,
	DUCRC_TYPE_STRING,
};

struct ducrc_option {
	const char *longopt;
	char shortopt;
	enum ducrc_type type;
	const char *defval;
	const char *description;
};


struct ducrc;

struct ducrc *ducrc_new(void);
void ducrc_free(struct ducrc *ducrc);
void ducrc_dump(struct ducrc *ducrc);

int ducrc_read(struct ducrc *ducrc, const char *path);

void ducrc_set_str(struct ducrc *ducrc, const char *section, const char *key, const char *val);
void ducrc_set_int(struct ducrc *ducrc, const char *section, const char *key, int val);

const char *ducrc_get_str(struct ducrc *ducrc, const char *section, const char *key);
int ducrc_get_int(struct ducrc *ducrc, const char *section, const char *key);

#endif
