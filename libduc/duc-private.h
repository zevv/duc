#ifndef duc_internal_h
#define duc_internal_h

enum duc_loglevel {
	LG_FTL,
	LG_WRN,
	LG_INF,
	LG_DBG,
	LG_DMP
};

struct duc {
	struct db *db;
	enum duc_errno err;
	enum duc_loglevel loglevel;
};

void duc_log(struct duc *duc, enum duc_loglevel lvl, const char *fmt, ...);

struct ducdir *ducdir_new(struct duc *duc, size_t ent_max);
int ducdir_add_ent(struct ducdir *dir, const char *name, off_t size, mode_t mode, dev_t dev, ino_t ino);

int ducdir_write(struct ducdir *dir, dev_t dev, ino_t ino);
struct ducdir *ducdir_read(struct duc *duc, dev_t dev, ino_t ino);

int duc_root_write(struct duc *duc, const char *path, dev_t dev, ino_t ino);

#endif

