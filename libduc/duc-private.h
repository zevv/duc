#ifndef duc_internal_h
#define duc_internal_h

struct duc {
	struct db *db;
};


struct ducdir *ducdir_new(struct duc *duc, size_t ent_max);
void ducdir_add_ent(struct ducdir *dir, const char *name, size_t size, mode_t mode, dev_t dev, ino_t ino);

int ducdir_write(struct ducdir *dir, dev_t dev, ino_t ino);
struct ducdir *ducdir_read(struct duc *duc, dev_t dev, ino_t ino);

int duc_root_write(struct duc *duc, const char *path, dev_t dev, ino_t ino);

#endif

