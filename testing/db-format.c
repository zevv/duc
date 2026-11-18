/*
 * Tests for databases that this version of Duc must refuse or report:
 *
 * - a database with another layout version (duc_db_version),
 * - an unversioned database that already holds indexed paths,
 * - an index report or directory record that is truncated, or that holds
 *   a count above DUC_HISTOGRAM_BUCKETS_MAX or DUC_TOPN_CNT_MAX.
 *
 * The records are encoded here from the layout, not with the buffer_put_*
 * functions, so that the tests do not share mistakes with the code.
 *
 * 'make check' runs this program from the top build directory. The cli_*
 * cases run ./duc from that directory: 'duc info', 'duc histogram' and
 * 'duc topn' list the reports, and 'duc ls' opens the indexed path. Each
 * case runs in a child process, so a crash fails only that case.
 */

#include "config.h"

#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include "duc.h"
#include "private.h"
#include "db.h"
#include "varint.h"


/*
 * Failure accounting
 */

static int failures = 0;

#define EXPECT(cond, what) expect(!!(cond), __LINE__, #cond, what)

static void expect(int ok, int line, const char *cond, const char *what)
{
	if(ok) return;
	fprintf(stderr, "  line %d: %s: %s\n", line, what, cond);
	failures++;
}


static void die(const char *what)
{
	perror(what);
	exit(2);
}


/*
 * Byte builder for records
 */

struct bytes {
	uint8_t *data;
	size_t len;
	size_t max;
};


static struct bytes bytes_new(void)
{
	struct bytes b;
	b.data = NULL;
	b.len = 0;
	b.max = 0;
	return b;
}


static void bytes_free(struct bytes *b)
{
	free(b->data);
	b->data = NULL;
	b->len = b->max = 0;
}


static void put_raw(struct bytes *b, const void *p, size_t n)
{
	if(b->len + n > b->max) {
		b->max = (b->len + n) * 2;
		b->data = realloc(b->data, b->max);
		if(b->data == NULL) die("realloc");
	}
	memcpy(b->data + b->len, p, n);
	b->len += n;
}


static void put_varint(struct bytes *b, uint64_t v)
{
	uint8_t buf[9];
	put_raw(b, buf, PutVarint64(buf, v));
}


/* A string is one length byte followed by the bytes, without a NUL */

static void put_string(struct bytes *b, const char *s)
{
	size_t n = strlen(s);
	if(n > 255) {
		fprintf(stderr, "string longer than 255 bytes: %s\n", s);
		exit(2);
	}
	uint8_t l = n;
	put_raw(b, &l, 1);
	put_raw(b, s, n);
}


/*
 * Index report layout. Every value differs, so a swapped field shows.
 * SIZE_COUNT is the last value of the Duc 1.4 layout; 1.5.0-rc1 reused it
 * as the histogram bucket count and overflowed the histogram array.
 */

enum {
	DEV            = 11,
	INO            = 12,
	T_START_SEC    = 1001,
	T_START_USEC   = 1002,
	T_STOP_SEC     = 1003,
	T_STOP_USEC    = 1004,
	FILE_COUNT     = 1005,
	DIR_COUNT      = 1006,
	SIZE_APPARENT  = 1007,
	SIZE_ACTUAL    = 1008,
	SIZE_COUNT     = 192617,
	TOPN_MIN_SIZE  = 1009,
	DIR_MTIME      = 1010,
};


static uint64_t histogram_value(uint64_t i)
{
	return i * 3 + 1;
}


static void topn_name(uint64_t i, char *buf, size_t len)
{
	snprintf(buf, len, "big-%ju", (uintmax_t)i);
}


static uint64_t topn_size(uint64_t i)
{
	return 100000 + i;
}


/* The Duc 1.4 report: path, devino, times, counts and total size */

static void put_report_head(struct bytes *b, const char *path)
{
	put_string(b, path);
	put_varint(b, DEV);
	put_varint(b, INO);
	put_varint(b, T_START_SEC);
	put_varint(b, T_START_USEC);
	put_varint(b, T_STOP_SEC);
	put_varint(b, T_STOP_USEC);
	put_varint(b, FILE_COUNT);
	put_varint(b, DIR_COUNT);
	put_varint(b, SIZE_APPARENT);
	put_varint(b, SIZE_ACTUAL);
	put_varint(b, SIZE_COUNT);
}


/*
 * The fields that Duc 1.5 adds after the head. The counts in the record
 * and the number of entries written differ only in the out-of-range cases.
 */

struct report_tail {
	uint64_t buckets;
	uint64_t buckets_written;
	uint64_t topn;
	uint64_t topn_written;
};


static struct report_tail report_tail(uint64_t buckets, uint64_t topn)
{
	struct report_tail t;
	t.buckets = buckets;
	t.buckets_written = buckets;
	t.topn = topn;
	t.topn_written = topn;
	return t;
}


static void put_report_tail(struct bytes *b, struct report_tail t)
{
	uint64_t i;

	put_varint(b, TOPN_MIN_SIZE);
	put_varint(b, t.topn);
	put_varint(b, DUC_TOPN_CNT_MAX);
	put_varint(b, t.buckets);

	for(i=0; i<t.buckets_written; i++) {
		put_varint(b, histogram_value(i));
	}

	for(i=0; i<t.topn_written; i++) {
		char name[32];
		topn_name(i, name, sizeof name);
		put_varint(b, strlen(name));
		put_string(b, name);
		put_varint(b, topn_size(i));
	}
}


/* Directory record: parent devino and mtime, then the dirents */

static void put_dir_head(struct bytes *b)
{
	put_varint(b, 0);
	put_varint(b, 0);
	put_varint(b, DIR_MTIME);
}


static void put_dirent(struct bytes *b, const char *name, uint64_t size, duc_file_type type)
{
	put_string(b, name);
	put_varint(b, size);
	put_varint(b, size * 2);
	put_varint(b, 1);
	put_varint(b, type);
	if(type == DUC_FILE_TYPE_DIR) {
		put_varint(b, DEV);
		put_varint(b, INO + size);
	}
}


/*
 * Scratch directory. Its canonical path is also the indexed path that the
 * reports describe, because duc_dir_open() takes a path.
 */

static char root[DUC_PATH_MAX];
static char *indexed_path;


static void make_root(void)
{
	const char *tmp = getenv("TMPDIR");
	if(tmp == NULL || *tmp == '\0') tmp = P_tmpdir;
	snprintf(root, sizeof root, "%s/duc-db-format.XXXXXX", tmp);
	if(mkdtemp(root) == NULL) die("mkdtemp");
	indexed_path = duc_canonicalize_path(root);
}


static int remove_one(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
	if(remove(path) != 0) perror(path);
	return 0;
}


static void remove_root(void)
{
	nftw(root, remove_one, 16, FTW_DEPTH | FTW_PHYS);
	free(indexed_path);
}


static void db_file(char *buf, size_t len, const char *name)
{
	snprintf(buf, len, "%s/%s.db", root, name);
}


/*
 * Database access below duc_open(), to write what an older Duc wrote and
 * to look at what duc_open() left behind
 */

static struct db *raw_open(const char *path, int flags)
{
	duc_errno e = DUC_OK;
	struct db *db = db_open(path, flags, &e);
	if(db == NULL) {
		fprintf(stderr, "db_open(%s) failed: %d\n", path, e);
		exit(2);
	}
	return db;
}


static void put_index_list(struct db *db, const char *path)
{
	char entry[DUC_PATH_MAX];
	memset(entry, 0, sizeof entry);
	snprintf(entry, sizeof entry, "%s", path);
	db_put(db, "duc_index_reports", 17, entry, sizeof entry);
}


static void put_record(struct db *db, const char *key, const struct bytes *b, size_t len)
{
	db_put(db, key, strlen(key), b->data, len);
}


static void dir_key(char *buf, size_t len)
{
	snprintf(buf, len, "%jx/%jx", (uintmax_t)DEV, (uintmax_t)INO);
}


/* want == NULL means that no version must be stored */

static int stored_version_is(const char *path, const char *want)
{
	struct db *db = raw_open(path, DUC_OPEN_RO);
	size_t len = 0;
	char *v = db_get(db, "duc_db_version", 14, &len);
	int ok;
	if(want) {
		ok = v && len == strlen(want) && memcmp(v, want, len) == 0;
	} else {
		ok = (v == NULL);
	}
	free(v);
	db_close(db);
	return ok;
}


static duc *open_rw(const char *path)
{
	duc *duc = duc_new();
	if(duc_open(duc, path, DUC_OPEN_RW) != 0) {
		fprintf(stderr, "duc_open(%s) failed: %s\n", path, duc_strerror(duc));
		exit(2);
	}
	return duc;
}


static void free_report(struct duc_index_report *r)
{
	int i;
	for(i=0; i<r->topn_cnt && i<DUC_TOPN_CNT_MAX; i++) {
		free(r->topn_array[i]);
	}
	duc_index_report_free(r);
}


/* duc_open() must fail read-only and read-write with a version mismatch */

static void expect_refused(const char *path, const char *what)
{
	int flags[] = { DUC_OPEN_RO, DUC_OPEN_RW };
	size_t i;

	for(i=0; i<sizeof(flags)/sizeof(flags[0]); i++) {
		char msg[256];
		snprintf(msg, sizeof msg, "%s, %s", what, flags[i] == DUC_OPEN_RO ? "read-only" : "read-write");
		duc *duc = duc_new();
		int r = duc_open(duc, path, flags[i]);
		EXPECT(r == -1, msg);
		EXPECT(duc_error(duc) == DUC_E_DB_VERSION_MISMATCH, msg);
		EXPECT(duc->db == NULL, msg);
		duc_del(duc);
	}
}


/*
 * Version cases
 */

static void case_fresh_database_is_stamped(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "fresh");

	duc *duc = duc_new();
	EXPECT(duc_open(duc, path, DUC_OPEN_RW) == 0, "open a new database read-write");
	duc_del(duc);

	EXPECT(stored_version_is(path, DUC_DB_VERSION), "a new database holds the layout version");
}


static void case_empty_unversioned_database_opens(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "empty");
	db_close(raw_open(path, DUC_OPEN_RW));

	duc *duc = duc_new();
	EXPECT(duc_open(duc, path, DUC_OPEN_RO) == 0, "open an empty unversioned database read-only");
	duc_del(duc);
}


static void case_other_version_is_refused(void)
{
	const char *versions[] = { "17", "1", DUC_DB_VERSION "0" };
	size_t i;

	for(i=0; i<sizeof(versions)/sizeof(versions[0]); i++) {
		const char *v = versions[i];
		char path[DUC_PATH_MAX], name[64], msg[128];
		snprintf(name, sizeof name, "version-%s", v);
		db_file(path, sizeof path, name);

		struct db *db = raw_open(path, DUC_OPEN_RW);
		db_put(db, "duc_db_version", 14, v, strlen(v));
		db_close(db);

		snprintf(msg, sizeof msg, "database with version '%s'", v);
		expect_refused(path, msg);
		EXPECT(stored_version_is(path, v), "a refused open leaves the stored version alone");
	}
}


static void case_unversioned_database_with_reports_is_refused(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "unversioned");

	struct db *db = raw_open(path, DUC_OPEN_RW);
	put_index_list(db, indexed_path);
	db_close(db);

	expect_refused(path, "unversioned database with an index list");
	EXPECT(stored_version_is(path, NULL), "a refused open does not stamp a version");
}


/*
 * Report cases
 */

static void case_report_round_trip(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "report");
	duc *duc = open_rw(path);

	struct bytes b = bytes_new();
	put_report_head(&b, indexed_path);
	put_report_tail(&b, report_tail(48, 2));
	put_record(duc->db, indexed_path, &b, b.len);
	put_index_list(duc->db, indexed_path);

	struct duc_index_report *r = duc_get_report(duc, 0);
	EXPECT(r != NULL, "read a well-formed report");
	if(r) {
		int i;
		EXPECT(strcmp(r->path, indexed_path) == 0, "path");
		EXPECT(r->devino.dev == DEV, "dev");
		EXPECT(r->devino.ino == INO, "ino");
		EXPECT(r->time_start.tv_sec == T_START_SEC, "time_start.tv_sec");
		EXPECT(r->time_start.tv_usec == T_START_USEC, "time_start.tv_usec");
		EXPECT(r->time_stop.tv_sec == T_STOP_SEC, "time_stop.tv_sec");
		EXPECT(r->time_stop.tv_usec == T_STOP_USEC, "time_stop.tv_usec");
		EXPECT(r->file_count == FILE_COUNT, "file_count");
		EXPECT(r->dir_count == DIR_COUNT, "dir_count");
		EXPECT(r->size.apparent == SIZE_APPARENT, "size.apparent");
		EXPECT(r->size.actual == SIZE_ACTUAL, "size.actual");
		EXPECT(r->size.count == SIZE_COUNT, "size.count");
		EXPECT(r->topn_min_size == TOPN_MIN_SIZE, "topn_min_size");
		EXPECT(r->topn_cnt == 2, "topn_cnt");
		EXPECT(r->topn_cnt_max == DUC_TOPN_CNT_MAX, "topn_cnt_max");
		EXPECT(r->histogram_buckets == 48, "histogram_buckets");
		for(i=0; i<48; i++) {
			EXPECT(r->histogram[i] == histogram_value(i), "histogram entry");
		}
		for(i=0; i<2 && i<r->topn_cnt; i++) {
			char name[32];
			topn_name(i, name, sizeof name);
			EXPECT(strcmp(r->topn_array[i]->name, name) == 0, "topn name");
			EXPECT(r->topn_array[i]->size == topn_size(i), "topn size");
		}
		free_report(r);
	}

	EXPECT(duc_get_report(duc, 1) == NULL, "one report in the list");
	EXPECT(duc_error(duc) == DUC_OK, "the end of the report list is not an error");

	bytes_free(&b);
	duc_del(duc);
}


static void case_report_at_limits_is_read(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "limits");
	duc *duc = open_rw(path);

	struct bytes b = bytes_new();
	put_report_head(&b, indexed_path);
	put_report_tail(&b, report_tail(DUC_HISTOGRAM_BUCKETS_MAX, DUC_TOPN_CNT_MAX));
	put_record(duc->db, indexed_path, &b, b.len);
	put_index_list(duc->db, indexed_path);

	struct duc_index_report *r = duc_get_report(duc, 0);
	EXPECT(r != NULL, "read a report with counts at the limits");
	if(r) {
		EXPECT(r->histogram_buckets == DUC_HISTOGRAM_BUCKETS_MAX, "histogram_buckets");
		EXPECT(r->histogram[DUC_HISTOGRAM_BUCKETS_MAX-1] == histogram_value(DUC_HISTOGRAM_BUCKETS_MAX-1), "last histogram entry");
		EXPECT(r->topn_cnt == DUC_TOPN_CNT_MAX, "topn_cnt");
		EXPECT(r->topn_array[DUC_TOPN_CNT_MAX-1]->size == topn_size(DUC_TOPN_CNT_MAX-1), "last topn size");
		free_report(r);
	}

	bytes_free(&b);
	duc_del(duc);
}


static void expect_corrupt_report(duc *duc, const char *what)
{
	struct duc_index_report *r = duc_get_report(duc, 0);
	EXPECT(r == NULL, what);
	EXPECT(duc_error(duc) == DUC_E_DB_CORRUPT, what);
	if(r) free_report(r);

	duc_dir *dir = duc_dir_open(duc, indexed_path);
	EXPECT(dir == NULL, what);
	EXPECT(duc_error(duc) == DUC_E_DB_CORRUPT, what);
	if(dir) duc_dir_close(dir);
}


static void case_truncated_report_is_corrupt(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "truncated");
	duc *duc = open_rw(path);

	struct bytes b = bytes_new();
	put_report_head(&b, indexed_path);
	put_report_tail(&b, report_tail(48, 2));
	put_index_list(duc->db, indexed_path);

	size_t n;
	for(n=0; n<b.len; n++) {
		char msg[64];
		snprintf(msg, sizeof msg, "report cut to %zu of %zu bytes", n, b.len);
		put_record(duc->db, indexed_path, &b, n);
		expect_corrupt_report(duc, msg);
	}

	/* After an error, a good report and the end of the list are no error */

	put_record(duc->db, indexed_path, &b, b.len);
	struct duc_index_report *r = duc_get_report(duc, 0);
	EXPECT(r != NULL, "read the whole report after the cut ones");
	if(r) free_report(r);
	EXPECT(duc_get_report(duc, 1) == NULL, "one report in the list");
	EXPECT(duc_error(duc) == DUC_OK, "the end of the report list is not an error");

	bytes_free(&b);
	duc_del(duc);
}


static void expect_report_tail_corrupt(duc *duc, struct report_tail t, const char *what)
{
	struct bytes b = bytes_new();
	put_report_head(&b, indexed_path);
	put_report_tail(&b, t);
	put_record(duc->db, indexed_path, &b, b.len);
	expect_corrupt_report(duc, what);
	bytes_free(&b);
}


static void case_out_of_range_counts_are_corrupt(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "range");
	duc *duc = open_rw(path);
	put_index_list(duc->db, indexed_path);

	struct report_tail t;

	t = report_tail(DUC_HISTOGRAM_BUCKETS_MAX + 1, 0);
	expect_report_tail_corrupt(duc, t, "histogram_buckets one above the limit");

	t = report_tail(0, DUC_TOPN_CNT_MAX + 1);
	expect_report_tail_corrupt(duc, t, "topn_cnt one above the limit");

	/* Counts that an int would truncate to 1 */

	t = report_tail(0, 0);
	t.buckets = (1ull << 32) + 1;
	t.buckets_written = 1;
	expect_report_tail_corrupt(duc, t, "histogram_buckets above 32 bits");

	t = report_tail(0, 0);
	t.topn = (1ull << 32) + 1;
	t.topn_written = 1;
	expect_report_tail_corrupt(duc, t, "topn_cnt above 32 bits");

	duc_del(duc);
}


/*
 * Directory record cases
 */

static void case_truncated_dir_record_is_corrupt(void)
{
	char path[DUC_PATH_MAX], key[32];
	db_file(path, sizeof path, "dir");
	dir_key(key, sizeof key);
	duc *duc = open_rw(path);

	struct bytes report = bytes_new();
	put_report_head(&report, indexed_path);
	put_report_tail(&report, report_tail(0, 0));
	put_record(duc->db, indexed_path, &report, report.len);
	put_index_list(duc->db, indexed_path);

	/* ends[k] is the record length that holds exactly k dirents */

	struct bytes d = bytes_new();
	size_t ends[4];
	size_t n_ends = 0;
	put_dir_head(&d);
	ends[n_ends++] = d.len;
	put_dirent(&d, "alpha", 100, DUC_FILE_TYPE_REG);
	ends[n_ends++] = d.len;
	put_dirent(&d, "bravo", 200, DUC_FILE_TYPE_DIR);
	ends[n_ends++] = d.len;
	put_dirent(&d, "charlie", 300, DUC_FILE_TYPE_REG);
	ends[n_ends++] = d.len;

	size_t n;
	for(n=0; n<=d.len; n++) {
		char msg[64];
		size_t k;
		int whole = -1;
		for(k=0; k<n_ends; k++) {
			if(ends[k] == n) whole = k;
		}

		put_record(duc->db, key, &d, n);
		duc_dir *dir = duc_dir_open(duc, indexed_path);

		if(whole >= 0) {
			snprintf(msg, sizeof msg, "record of %zu bytes with %d dirents", n, whole);
			EXPECT(dir != NULL, msg);
			if(dir) EXPECT(duc_dir_get_count(dir) == (size_t)whole, msg);
		} else {
			snprintf(msg, sizeof msg, "record cut to %zu of %zu bytes", n, d.len);
			EXPECT(dir == NULL, msg);
			EXPECT(duc_error(duc) == DUC_E_DB_CORRUPT, msg);
		}
		if(dir) duc_dir_close(dir);
	}

	bytes_free(&report);
	bytes_free(&d);
	duc_del(duc);
}


/*
 * Command line cases
 */

static void put_dir_record(struct db *db)
{
	char key[32];
	dir_key(key, sizeof key);
	struct bytes d = bytes_new();
	put_dir_head(&d);
	put_dirent(&d, "alpha", 100, DUC_FILE_TYPE_REG);
	put_record(db, key, &d, d.len);
	bytes_free(&d);
}


/* Run ./duc with stdout to /dev/null; return the wait status and stderr */

static int run_duc(char *const argv[], char *err, size_t errlen)
{
	int fd[2];
	if(pipe(fd) != 0) die("pipe");

	fflush(stdout);
	fflush(stderr);

	pid_t pid = fork();
	if(pid < 0) die("fork");
	if(pid == 0) {
		int null = open("/dev/null", O_WRONLY);
		dup2(null, 1);
		dup2(fd[1], 2);
		close(fd[0]);
		close(fd[1]);
		close(null);
		execv("./duc", argv);
		perror("./duc");
		_exit(127);
	}

	close(fd[1]);
	size_t used = 0;
	ssize_t r;
	while((r = read(fd[0], err + used, errlen - 1 - used)) > 0) {
		used += r;
		if(used == errlen - 1) break;
	}
	err[used] = '\0';
	close(fd[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	return status;
}


static void expect_cli_error(char *const argv[], const char *message)
{
	char err[4096];
	char what[256];
	int status = run_duc(argv, err, sizeof err);

	snprintf(what, sizeof what, "'duc %s' says '%s'", argv[1], message);

	if(WIFSIGNALED(status)) {
		fprintf(stderr, "  'duc %s' was killed by signal %d\n", argv[1], WTERMSIG(status));
	}
	EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE, what);
	EXPECT(strstr(err, message) != NULL, what);
	if(strstr(err, message) == NULL) {
		fprintf(stderr, "  stderr was: %s\n", err);
	}
}


/* The subcommands that list the index reports, and 'duc ls' */

static void expect_cli_errors(char *path, const char *message)
{
	char *info[] = { "duc", "info", "-d", path, NULL };
	char *histogram[] = { "duc", "histogram", "-d", path, NULL };
	char *topn[] = { "duc", "topn", "-d", path, NULL };
	char *ls[] = { "duc", "ls", "-d", path, indexed_path, NULL };
	expect_cli_error(info, message);
	expect_cli_error(histogram, message);
	expect_cli_error(topn, message);
	expect_cli_error(ls, message);
}


static void case_cli_duc_1_4_database_is_refused(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "cli-1.4");

	struct db *db = raw_open(path, DUC_OPEN_RW);
	struct bytes b = bytes_new();
	put_report_head(&b, indexed_path);
	put_record(db, indexed_path, &b, b.len);
	put_index_list(db, indexed_path);
	put_dir_record(db);
	db_close(db);
	bytes_free(&b);

	expect_cli_errors(path, "Database version mismatch");
}


static void case_cli_truncated_report_is_corrupt(void)
{
	char path[DUC_PATH_MAX];
	db_file(path, sizeof path, "cli-truncated");

	duc *duc = open_rw(path);
	struct bytes b = bytes_new();
	put_report_head(&b, indexed_path);
	put_record(duc->db, indexed_path, &b, b.len);
	put_index_list(duc->db, indexed_path);
	put_dir_record(duc->db);
	duc_del(duc);
	bytes_free(&b);

	expect_cli_errors(path, "Database corrupt and not usable");
}


/*
 * Runner
 */

struct test {
	const char *name;
	void (*fn)(void);
};

#define TEST(fn) { #fn, case_ ## fn }

static const struct test tests[] = {
	TEST(fresh_database_is_stamped),
	TEST(empty_unversioned_database_opens),
	TEST(other_version_is_refused),
	TEST(unversioned_database_with_reports_is_refused),
	TEST(report_round_trip),
	TEST(report_at_limits_is_read),
	TEST(truncated_report_is_corrupt),
	TEST(out_of_range_counts_are_corrupt),
	TEST(truncated_dir_record_is_corrupt),
	TEST(cli_duc_1_4_database_is_refused),
	TEST(cli_truncated_report_is_corrupt),
};


int main(void)
{
	size_t i;
	int failed = 0;

	make_root();

	for(i=0; i<sizeof(tests)/sizeof(tests[0]); i++) {
		const struct test *t = &tests[i];

		fflush(stdout);
		fflush(stderr);

		pid_t pid = fork();
		if(pid < 0) die("fork");
		if(pid == 0) {
			t->fn();
			exit(failures ? 1 : 0);
		}

		int status = 0;
		waitpid(pid, &status, 0);

		if(WIFSIGNALED(status)) {
			printf("FAIL %s: killed by signal %d\n", t->name, WTERMSIG(status));
			failed++;
		} else if(WEXITSTATUS(status) != 0) {
			printf("FAIL %s\n", t->name);
			failed++;
		} else {
			printf("ok   %s\n", t->name);
		}
	}

	remove_root();

	printf("%d of %zu cases failed\n", failed, sizeof(tests)/sizeof(tests[0]));
	return failed ? 1 : 0;
}

/*
 * End
 */
