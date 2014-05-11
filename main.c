
#include <stdio.h>

#include "db.h"

int dump(struct db *db, const char *path);
off_t ps_index(struct db *db, const char *path);

int main(int argc, char **argv)
{

	if(argv[1][0] == 'd') {
		char *path = ".";
		if(argc > 2) path = argv[2];
		struct db *db = db_open("r");
		dump(db, path);
		db_close(db);
	}

	if(argv[1][0] == 'i') {
		struct db *db = db_open("wc");
		off_t size = 0;
		size = ps_index(db, argv[2]);
		printf("%jd %.2f\n", size, size / (1024.0*1024.0*1024.0));
		db_close(db);
	}


	return 0;
}

/*
 * End
 */
