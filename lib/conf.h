#ifndef conf_h
#define conf_h

struct conf;

struct conf *conf_new(void);
void conf_free(struct conf *conf);
void conf_dump(struct conf *conf);

int conf_read(struct conf *conf, const char *path);

void conf_set_str(struct conf *conf, const char *section, const char *key, const char *val);
void conf_set_int(struct conf *conf, const char *section, const char *key, int val);

const char *conf_get_str(struct conf *conf, const char *section, const char *key);
int conf_get_int(struct conf *conf, const char *section, const char *key);

#endif
