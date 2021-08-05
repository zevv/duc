
/*
 * To whom it may concern: http://womble.decadent.org.uk/readdir_r-advisory.html
 */

#include "config.h"

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#include "db.h"
#include "duc.h"
#include "private.h"
#include "uthash.h"
#include "utlist.h"
#include "buffer.h"

struct stack_node {
	size_t length;
	struct stack_node *next;
	struct scanner* scanner;
};

unsigned int WORKER_COUNT = 1;
unsigned int CUTOFF_DEPTH = 2;
pthread_t *worker_threads;
struct stack_node **worker_stacks;
pthread_rwlock_t *worker_rw_locks;
pthread_mutex_t database_write_lock;

/*
 * Maintains a count of the number of workers that are polling other workers for work, checking
 * if they need to terminate. Once this count reaches <WORKER_COUNT>, it is a signal for all
 * the workers to terminate.
 */
_Atomic unsigned int num_workers_terminating;

struct hard_link {
	struct duc_devino devino;
	UT_hash_handle hh;
};

struct fstype {
	char *path;
	char *type;
	UT_hash_handle hh;
};

struct exclude {
	char *name;
	struct exclude *next;
};

struct duc_index_req {
	duc *duc;
	struct exclude *exclude_list;
	duc_dev_t dev;
	duc_index_flags flags;
	_Atomic int maxdepth;
	uid_t uid;
	const char *username;
	duc_index_progress_cb progress_fn;
	void *progress_fndata;
	_Atomic int progress_n;
	struct timeval progress_interval;
	struct timeval progress_time;
	struct hard_link *hard_link_map;
	struct fstype *fstypes_mounted;
	struct fstype *fstypes_include;
	struct fstype *fstypes_exclude;
};

struct scanner {
	struct scanner *parent;
	_Atomic int depth;
	struct buffer *buffer;
	struct duc *duc;
	struct duc_index_req *req;
	struct duc_index_report *rep;
	struct duc_dirent ent;
	const char *absolute_path;                /* Absolute path of the dirent this scanner refers to */
	_Atomic unsigned int completed_children;  /* Number of children of this node that completed */
	_Atomic unsigned int num_children;        /* Number of children that this node has */
	_Atomic bool done_processing;             /* Indicates whether the node finished processing */
};


void stack_push(unsigned int worker_num, struct scanner *scanner)
{
	struct stack_node *old_head = worker_stacks[worker_num];
	struct stack_node *new_head = duc_malloc(sizeof(struct stack_node));
	new_head->next = old_head;
	new_head->scanner = scanner;
	new_head->length = old_head == NULL ? 1 : old_head->length + 1;
	worker_stacks[worker_num] = new_head;
}


struct scanner *stack_pop(unsigned int worker_num)
{
	struct stack_node *old_head = worker_stacks[worker_num];
	if (!old_head) {
		return NULL;
	}

	struct scanner *scanner = old_head->scanner;
	worker_stacks[worker_num] = old_head->next;

	duc_free(old_head);
	return scanner;
}


size_t stack_len(struct stack_node *head)
{
	return head == NULL ? 0 : head->length;
}


bool stack_empty(struct stack_node *head)
{
	return !head;
}


duc_index_req *duc_index_req_new(duc *duc)
{
	struct duc_index_req *req = duc_malloc0(sizeof(struct duc_index_req));

	req->duc = duc;
	req->progress_interval.tv_sec = 0;
	req->progress_interval.tv_usec = 100 * 1000;
	req->hard_link_map = NULL;

	return req;
}


int duc_index_req_free(duc_index_req *req)
{
	struct hard_link *h, *hn;
	struct fstype *f, *fn;
	struct exclude *e, *en;

	HASH_ITER(hh, req->hard_link_map, h, hn) {
		HASH_DEL(req->hard_link_map, h);
		free(h);
	}
	
	HASH_ITER(hh, req->fstypes_mounted, f, fn) {
		duc_free(f->type);
		duc_free(f->path);
		HASH_DEL(req->fstypes_mounted, f);
		free(f);
	}
	
	HASH_ITER(hh, req->fstypes_include, f, fn) {
		duc_free(f->type);
		HASH_DEL(req->fstypes_include, f);
		free(f);
	}
	
	HASH_ITER(hh, req->fstypes_exclude, f, fn) {
		duc_free(f->type);
		HASH_DEL(req->fstypes_exclude, f);
		free(f);
	}

	LL_FOREACH_SAFE(req->exclude_list, e, en) {
		free(e->name);
		free(e);
	}

	free(req);

	return 0;
}


int duc_index_req_add_exclude(duc_index_req *req, const char *patt)
{
	struct exclude *e = duc_malloc(sizeof(struct exclude));
	e->name = duc_strdup(patt);
	LL_APPEND(req->exclude_list, e);
	return 0;
}


static struct fstype *add_fstype(duc_index_req *req, const char *types, struct fstype *list)
{
	char *types_copy = duc_strdup(types);
	char *type = strtok(types_copy, ",");
	while(type) {
		struct fstype *fstype;
		fstype = duc_malloc(sizeof(*fstype));
		fstype->type = duc_strdup(type);
		HASH_ADD_KEYPTR(hh, list, fstype->type, strlen(fstype->type), fstype);
		type = strtok(NULL, ",");
	}
	duc_free(types_copy);
	return list;
}


int duc_index_req_add_fstype_include(duc_index_req *req, const char *types)
{
	req->fstypes_include = add_fstype(req, types, req->fstypes_include);
	return 0;
}


int duc_index_req_add_fstype_exclude(duc_index_req *req, const char *types)
{
	req->fstypes_exclude = add_fstype(req, types, req->fstypes_exclude);
	return 0;
}


int duc_index_req_set_maxdepth(duc_index_req *req, int maxdepth)
{
	req->maxdepth = maxdepth;
	return 0;
}


int duc_index_set_worker_count(unsigned int worker_count)
{
	WORKER_COUNT = worker_count;
	worker_threads = duc_malloc(sizeof(pthread_t) * WORKER_COUNT);
	worker_stacks = duc_malloc(sizeof(struct stack_node *) * WORKER_COUNT);
	worker_rw_locks = duc_malloc(sizeof(pthread_rwlock_t) * WORKER_COUNT);
	return 0;
}


int duc_index_set_cutoff_depth(unsigned int cutoff_depth)
{
	CUTOFF_DEPTH = cutoff_depth;
	return 0;
}


/* 
 * We set both uid and username, since we cannot use -1 UID to check wether we're 
 * limiting the search to just a specific UID, but we use UID for quicker compares.
 */

int duc_index_req_set_username(duc_index_req *req, const char *username )
{
    struct passwd *pass;
    pass = getpwnam(username);
    req->username = username;
    req->uid = pass->pw_uid;
    return 0;
}

int duc_index_req_set_uid(duc_index_req *req, int uid)
{
    struct passwd *pass;
    pass = getpwuid(uid);
    req->uid = uid;
    req->username = pass->pw_name;
    return 0;
}


int duc_index_req_set_progress_cb(duc_index_req *req, duc_index_progress_cb fn, void *ptr)
{
	req->progress_fn = fn;
	req->progress_fndata = ptr;
	return DUC_OK;
}


static int match_exclude(const char *name, struct exclude *list)
{
	struct exclude *e;
	LL_FOREACH(list, e) {
#ifdef HAVE_FNMATCH_H
		if(fnmatch(e->name, name, 0) == 0) return 1;
#else
		if(strstr(name, e->name) == 0) return 1;
#endif
	}
	return 0;
}


/*
 * Convert st_mode to DUC_FILE_TYPE_* type
 */

duc_file_type st_to_type(mode_t mode)
{
	if(S_ISBLK(mode))  return DUC_FILE_TYPE_BLK;
	if(S_ISCHR(mode))  return DUC_FILE_TYPE_CHR;
	if(S_ISDIR(mode))  return DUC_FILE_TYPE_DIR;
	if(S_ISFIFO(mode)) return DUC_FILE_TYPE_FIFO;
	if(S_ISLNK(mode))  return DUC_FILE_TYPE_LNK;
	if(S_ISREG(mode )) return DUC_FILE_TYPE_REG;
	if(S_ISSOCK(mode)) return DUC_FILE_TYPE_SOCK;

	return DUC_FILE_TYPE_UNKNOWN;
}


static void st_to_size(struct stat *st, struct duc_size *s)
{
	s->apparent = st->st_size;
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
	s->actual = st->st_blocks * 512;
#else
	s->actual = s->apparent;
#endif
	s->count = 1;
}


/*
 * Convert struct stat to duc_devino. Windows does not support inodes
 * and will always put 0 in st_ino. We fake inodes here by simply using
 * an incrementing counter. This *will* cause problems when re-indexing
 * existing databases. If anyone knows a better method to simulate
 * inodes on windows, please tell me
 */

static void st_to_devino(struct stat *st, struct duc_devino *devino)
{
#ifdef WIN32
	static duc_ino_t ino_seq = 0;
	devino->dev = st->st_dev;
	devino->ino = ++ino_seq;
#else
	devino->dev = st->st_dev;
	devino->ino = st->st_ino;
#endif
}


/*
 * Check if the given node is a duplicate. returns 1 if this dev/inode
 * was seen before
 */

static int is_duplicate(struct duc_index_req *req, struct duc_devino *devino)
{
	struct hard_link *h;
	HASH_FIND(hh, req->hard_link_map, devino, sizeof(*devino), h);
	if(h) return 1;

	h = duc_malloc(sizeof(*h));
	h->devino = *devino;
	HASH_ADD(hh, req->hard_link_map, devino, sizeof(h->devino), h);
	return 0;
}


static void report_skip(struct duc *duc, const char *name, const char *fmt, ...)
{
	char path_full[DUC_PATH_MAX];
	char msg[DUC_PATH_MAX + 128];
	char *res = realpath(name, path_full);
	if (res == NULL) {
	    duc_log(duc, DUC_LOG_WRN, "Cannot determine realpath of: %s", name);
	}
	va_list va;
	va_start(va, fmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	duc_log(duc, DUC_LOG_WRN, "skipping %s: %s", path_full, msg);
	va_end(va);
}


/* 
 * Given a parent scanner and a child's name, this function returns the
 * absolute path to the child.
 */

char *get_absolute_path(struct scanner *scanner_parent, char *child_name)
{
	char *absolute_path;
	size_t path_suffix_size = strlen(child_name);

	if (!scanner_parent) {
		absolute_path = duc_malloc0(sizeof(char) * (path_suffix_size + 1));
		strcpy(absolute_path, child_name);
		return absolute_path;
	}

	/*
	 * The absolute path referenced by a child scanner should be the absolute
	 * path referenced by the parent scanner + '/' + the name of the file
	 */

	size_t parent_absolute_path_size = strlen(scanner_parent->absolute_path);
	absolute_path = duc_malloc0(sizeof(char) * (parent_absolute_path_size + path_suffix_size + 2));
	sprintf(absolute_path, "%s/%s", scanner_parent->absolute_path, child_name);
	return absolute_path;
}


/*
 * Check if this file system type should be scanned, depending on the
 * fstypes_include and fstypes_exclude lists. If neither has any entries, all
 * fs types are allowed. return 0 to skip, or 1 to scan
 */

static int is_fstype_allowed(struct duc_index_req *req, const char *name)
{
	struct duc *duc = req->duc;

	if((req->fstypes_include == NULL) && (req->fstypes_exclude == NULL)) {
		return 1;
	}

	/* Find file system type */
	char path_full[DUC_PATH_MAX];
        char *res = realpath(name, path_full);
	if (res == NULL) {
	    report_skip(duc, name, "Cannot determine realpath result");
            return 0;
	}
	struct fstype *fstype = NULL;
	HASH_FIND_STR(req->fstypes_mounted, path_full, fstype);
	if(fstype == NULL) {
		report_skip(duc, name, "Unable to determine fs type");
		return 0;
	}
	const char *type = fstype->type;

	/* Check if excluded */

	if(req->fstypes_exclude) {
		HASH_FIND_STR(req->fstypes_exclude, type, fstype);
		if(fstype) {
			report_skip(duc, name, "File system type '%s' is excluded", type);
			return 0;
		}
	}

	/* Check if included */

	if(req->fstypes_include) {
		HASH_FIND_STR(req->fstypes_include, type, fstype);
		if(!fstype) {
			report_skip(duc, name, "File system type '%s' is not included", type);
			return 0;
		}
	}

	return 1;
}


/* 
 * Open dir and read file status 
 */

static struct scanner *scanner_new(struct duc *duc, struct scanner *scanner_parent, const char *path, const char *dirent_name, struct stat *st)
{
	struct scanner *scanner;
	scanner = duc_malloc0(sizeof(*scanner));

	struct stat st2;
	struct duc_devino devino_parent = { 0, 0 };

	scanner->absolute_path = path;
	scanner->done_processing = false;
	scanner->num_children = 0;
	scanner->completed_children = 0;

	if(scanner_parent) {
		scanner->depth = scanner_parent->depth + 1;
		scanner->duc = scanner_parent->duc;
		scanner->req = scanner_parent->req;
		scanner->rep = scanner_parent->rep;
		devino_parent = scanner_parent->ent.devino;
	} else {
		int r = lstat(scanner->absolute_path, &st2);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", path, strerror(errno));
			if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
			goto err;
		}
		st = &st2;
	}
	

	scanner->duc = duc;
	scanner->parent = scanner_parent;
	scanner->buffer = buffer_new(NULL, 32768);

	scanner->ent.name = duc_strdup(dirent_name);
	scanner->ent.type = DUC_FILE_TYPE_DIR,
	st_to_devino(st, &scanner->ent.devino);
	st_to_size(st, &scanner->ent.size);
	scanner->ent.size.apparent = 0;

	buffer_put_dir(scanner->buffer, &devino_parent, st->st_mtime);

	duc_log(duc, DUC_LOG_DMP, ">> %s", scanner->ent.name);

	return scanner;

err:
	if(scanner) free(scanner);
	return NULL;
}


static void scanner_free(struct scanner *scanner)
{
	struct duc *duc = scanner->duc;
	struct duc_index_req *req = scanner->req;
	struct duc_index_report *report = scanner->rep; 	

	duc_log(duc, DUC_LOG_DMP, "<< %s actual:%jd apparent:%jd", 
			scanner->ent.name, scanner->ent.size.apparent, scanner->ent.size.actual);

	if(scanner->parent) {
		duc_size_accum(&scanner->parent->ent.size, &scanner->ent.size);

		if((req->maxdepth == 0) || (scanner->depth < req->maxdepth)) {
			buffer_put_dirent(scanner->parent->buffer, &scanner->ent);
		}
	}

	/* Progress reporting */
	if(req->progress_fn) {
		if((!scanner->parent) || (req->progress_n++ == 100)) {
			
			struct timeval t_now;
			gettimeofday(&t_now, NULL);

			if(!scanner->parent || timercmp(&t_now, &req->progress_time, > )) {
				req->progress_fn(report, req->progress_fndata);
				timeradd(&t_now, &req->progress_interval, &req->progress_time);
			}
			req->progress_n = 0;
		}
	}

	if(!(req->flags & DUC_INDEX_DRY_RUN)) {
		char key[32];
		struct duc_devino *devino = &scanner->ent.devino;
		size_t keyl = snprintf(key, sizeof(key), "%jx/%jx", (uintmax_t)devino->dev, (uintmax_t)devino->ino);
		pthread_mutex_lock(&database_write_lock);
		int r = db_put(duc->db, key, keyl, scanner->buffer->data, scanner->buffer->len);
		pthread_mutex_unlock(&database_write_lock);
		if(r != 0) duc->err = r;
	}

	duc_free((void *)scanner->absolute_path);

	buffer_free(scanner->buffer);
	duc_free(scanner->ent.name);
	duc_free(scanner);
}


static void read_mounts(duc_index_req *req)
{
	FILE *f;

	f = fopen("/proc/mounts", "r");

	if(f == NULL) {
		f = fopen("/etc/mtab", "r");
	}

	if(f == NULL) {
		duc_log(req->duc, DUC_LOG_FTL, "Unable to get list of mounted file systems");
		return;
	}

	char buf[DUC_PATH_MAX];

	while(fgets(buf, sizeof(buf)-1, f) != NULL) {
		(void)strtok(buf, " ");
		char *path = strtok(NULL, " ");
		char *type = strtok(NULL, " ");
		if(path && type) {
			struct fstype *fstype;
			fstype = duc_malloc(sizeof(*fstype));
			fstype->type = duc_strdup(type);
			fstype->path = duc_strdup(path);
			HASH_ADD_KEYPTR(hh, req->fstypes_mounted, fstype->path, strlen(fstype->path), fstype);
		}
	}
	fclose(f);
}


void write_lock_worker_stack(unsigned int worker_num)
{
	assert(worker_num < WORKER_COUNT);

	pthread_rwlock_wrlock(&worker_rw_locks[worker_num]);
}


void read_lock_worker_stack(unsigned int worker_num)
{
	assert(worker_num < WORKER_COUNT);

	pthread_rwlock_rdlock(&worker_rw_locks[worker_num]);
}


void unlock_worker_stack(unsigned int worker_num)
{
	assert(worker_num < WORKER_COUNT);

	pthread_rwlock_unlock(&worker_rw_locks[worker_num]);
}


void free_nodes(struct scanner *scanner)
{
	/*
	 * LOGIC:
	 * This bottom-up algorithm to free nodes. The worker starts freeing at node <scanner> and walks
	 * up the tree, ensuring that topological order is maintained. The algorithm is as follows:

	 * LOOP: The worker frees this node and keeps track of its parent.
	 * The <completed_children> count of the node's parent is incremented (since the child was freed).
	 * If the freed node's parent is not done processing:
	 * 		The parent obviously cannot be freed because it hasn't finished processing. And so
	 * 		the loop is broken.
	 * Otherwise:
	 * 		The freeing of the parent was left as a duty to the LAST CHILD to finish.
	 * 		So the worker checks if this node is the last child. If it is, LOOP is repeated with
	 * 		the parent being the new node. Otherwise, the loop terminates.
	 */

	struct scanner* s = scanner->parent;
	scanner_free(scanner);
	while(s) {
		s->completed_children++;

		/*
		 * If the parent hasn't finished processing, then this child cannot free it.
		 * So the child can ignore its parent in this scenario.
		 */
		if(! s->done_processing) break;
		s->done_processing = true;

		/*
		 * If the parent is done processing but this child is not the last child to process,
		 * then the child cannot free the parent (top sort demands ALL children finish before parent).
		 */
		if (s->completed_children != s->num_children) break;

		/* Free the parent */
		struct scanner* new_parent = s->parent;
		scanner_free(s);

		s = new_parent;
	}
}


/*
 * Adds all relevant children that need to be processed onto <worker_num>'s DFS
 * stack, and returns whether <scanner> is a leaf node i.e whether it has any children.
 */

bool process_node(unsigned int worker_num, struct scanner *scanner)
{
	DIR *dr = opendir(scanner->absolute_path);
	if (dr == NULL) return true;

	struct dirent *de;
	bool is_leaf = true;
	while( (de = readdir(dr)) != NULL ) {
		char *dirent_name = de->d_name;
		struct duc *duc = scanner->duc;
		struct duc_index_req *req = scanner->req;
		struct duc_index_report *report = scanner->rep;

		if(strcmp(dirent_name, ".") == 0 || strcmp(dirent_name, "..") == 0) {
			continue;
		}
		
		if(match_exclude(dirent_name, req->exclude_list)) {
			report_skip(duc, dirent_name, "Excluded by user");
			continue;
		}

		/* 
		 * Get file info. Derive the file type from st.st_mode. It
		 * seems that we cannot trust e->d_type because it is not
		 * guaranteed to contain a sane value on all file system types.
		 * See the readdir() man page for more details
		 */
		char *absolute_path = get_absolute_path(scanner, dirent_name);
		struct stat st_ent;
		int r = lstat(absolute_path, &st_ent);
		if(r == -1) {
			duc_log(duc, DUC_LOG_WRN, "Error statting %s: %s", absolute_path, strerror(errno));
			duc_free(absolute_path);
			continue;
		}

		/* 
		 * If this dirent lies on a different device, check the file system type of the new
		 * device and skip if it is not on the list of approved types
		 */
		if(st_ent.st_dev != scanner->ent.devino.dev) {
			if(!is_fstype_allowed(req, dirent_name)) {
				duc_free(absolute_path);
				continue;
			}
		}

		/* Are we looking for data for only a specific user? */
		if(req->username) {
			if(st_ent.st_uid != req->uid) {
				duc_free(absolute_path);
				continue;
			}
		}
		
		/* Create duc_dirent from readdir() and fstatat() results */
		struct duc_dirent ent;
		ent.name = dirent_name;
		ent.type = st_to_type(st_ent.st_mode);
		st_to_devino(&st_ent, &ent.devino);
		st_to_size(&st_ent, &ent.size);

		/* Skip hard link duplicates for any files with more then one hard link */
		if((ent.type != DUC_FILE_TYPE_DIR) && (req->flags & DUC_INDEX_CHECK_HARD_LINKS) &&
			(st_ent.st_nlink > 1) && is_duplicate(req, &ent.devino)) {
			duc_free(absolute_path);
			continue;
		}


		/* Check if we can cross file system boundaries */
		if((ent.type == DUC_FILE_TYPE_DIR) && (req->flags & DUC_INDEX_XDEV) &&
			(st_ent.st_dev != req->dev)) {
			report_skip(duc, dirent_name, "Not crossing file system boundaries");
			duc_free(absolute_path);
			continue;
		}
		

		if(ent.type == DUC_FILE_TYPE_DIR) {

			/* Open and scan child directory */
			struct scanner *child_scanner = scanner_new(duc, scanner, absolute_path, dirent_name, &st_ent);

			if(child_scanner == NULL) {
				duc_free(absolute_path);
				continue;
			}

			/* Obtain the write lock and push the child onto the stack */
			write_lock_worker_stack(worker_num);
			stack_push(worker_num, child_scanner);
			unlock_worker_stack(worker_num);

			is_leaf = false;
			++scanner->num_children;
			++report->dir_count;
		} else {
			duc_size_accum(&scanner->ent.size, &ent.size);
			duc_size_accum(&report->size, &ent.size);

			++report->file_count;

			duc_log(duc, DUC_LOG_DMP, "  %c %jd %jd %s", 
					duc_file_type_char(ent.type), ent.size.apparent, ent.size.actual, dirent_name);

			duc_free(absolute_path);

			/* Optionally hide file names */
			if(req->flags & DUC_INDEX_HIDE_FILE_NAMES) ent.name = "<FILE>";
		

			/* Store record */
			if((req->maxdepth == 0) || (scanner->depth < req->maxdepth)) {
				buffer_put_dirent(scanner->buffer, &ent);
			}
		}
	}
	closedir(dr);
	return is_leaf;
}


void dfs_worker(unsigned int worker_num)
{
	/*
	 * LOGIC:
	 * Our goal is to ensure a concurrent topological sort, so that parent nodes can aggregate stats of their children.
	 * Let us define a node "processing" as the process whereby a worker puts the node's relevant children onto its DFS stack.
	 * Each node maintains a count of the number of children it possesses <num_children>, a boolean to indicate whether it is
	 * finished processing <done_processing>, as well as a count of its children which finished processing <completed_children>.
	 * 
	 * When a node is being processed by a worker:
	 * 		Each child it possesses is placed onto the DFS stack and <num_children> is incremented.
	 * After processing:
	 * 		The worker checks if either of two conditions are true for the node:
	 * 			- If the node is a leaf, then it has no dependencies in the topological sort, and so it can be freed.
	 * 			- If the node's <completed_children> count equals the <num_children>, then this means that the children
	 * 			  all finished processing (possible due to concurrency of the algorithm). Then topological order is guaranteed,
	 * 			  and so the node can be freed.
	 * 		Otherwise:
	 * 			- The node cannot yet be freed, otherwise topological order will be violated. Thus, the worker marks the node
	 * 			  as <done_processing>, and leaves the freeing of the node as a duty to the LAST CHILD of the node which finishes
	 * 			  processing.
	 */

	while(true) {
		/* Pop a new node off the DFS stack */
		write_lock_worker_stack(worker_num);
		if (stack_empty(worker_stacks[worker_num])) {
			unlock_worker_stack(worker_num);
			break;
		}
		struct scanner *scanner = stack_pop(worker_num);
		unlock_worker_stack(worker_num);

		/* Process the node */
		bool is_leaf = process_node(worker_num, scanner);

		/* Run the free algorithm on the node */
		if (is_leaf ||
			!scanner->done_processing &&
			scanner->completed_children == scanner->num_children) {
			free_nodes(scanner);
		} else {
			scanner->done_processing = true;
		}
		return;
	}
}


/*
 * This function polls all workers for work to add to <worker_num>'s stack. 
 * If there is work available on another worker's stack and the stack size exceeds
 * the cutoff depth, then work is taken from the other worker.
 *
 * Returns false if the worker should fully terminate, i.e. if all other
 * workers also have no work left, and are looking for work.
 */

bool get_work_for(unsigned int worker_num)
{
	unsigned int target = 0;
	++num_workers_terminating;
	while (num_workers_terminating != WORKER_COUNT) {
		/* 'Target' is the target processor that we are taking tasks from */
		target = (target + 1) % WORKER_COUNT;
		if (target == worker_num) continue;
		write_lock_worker_stack(target);

		/* If this target processor has too many tasks then take one of its tasks */
		if (stack_len(worker_stacks[target]) > CUTOFF_DEPTH) {
			--num_workers_terminating;
			struct scanner* target_task = stack_pop(target);
			unlock_worker_stack(target);
			write_lock_worker_stack(worker_num);
			stack_push(worker_num, target_task);
			unlock_worker_stack(worker_num);
			return true;
		}

		unlock_worker_stack(target);
	}

	return false;
}


void *worker_main(void *worker_num_v)
{
	unsigned int worker_num = (uintptr_t) worker_num_v;

	while (true) {
		read_lock_worker_stack(worker_num);

		if (stack_empty(worker_stacks[worker_num])) {
			unlock_worker_stack(worker_num);

			/*
			 * If there is no work for this worker, then poll other workers for work.
			 * If the function returns false, it is an indicator to terminate.
			 */
			if (!get_work_for(worker_num)) break;
		} else {
			unlock_worker_stack(worker_num);
		}

		dfs_worker(worker_num);
	}

	return NULL;
}


void initialize_workers(void)
{
	for (unsigned int i = 0; i < WORKER_COUNT; i++) {
		/* Initialize the worker's stack */
		worker_stacks[i] = NULL;

		/* Initialize the mutex for the worker's stack */
		pthread_rwlock_init(&worker_rw_locks[i], NULL);
	}
}


void create_worker_pool(struct scanner *scanner)
{
	for (uintptr_t worker_num = 0; worker_num < WORKER_COUNT; worker_num++) {
		assert(
			pthread_create(
				&worker_threads[worker_num],
				NULL,
				&worker_main,
				(void *) worker_num
			)
		== 0);
	}
}


void join_workers(void)
{
	for (unsigned int i = 0; i < WORKER_COUNT; i++) {
		pthread_join(worker_threads[i], NULL);
	}
}


struct duc_index_report *duc_index(duc_index_req *req, const char *path, duc_index_flags flags)
{
	duc *duc = req->duc;

	req->flags = flags;

	/* Canonicalize index path */
	char *path_canon = duc_canonicalize_path(path);
	if(path_canon == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error converting path %s: %s", path, strerror(errno));
		duc->err = DUC_E_UNKNOWN;
		if(errno == EACCES) duc->err = DUC_E_PERMISSION_DENIED;
		if(errno == ENOENT) duc->err = DUC_E_PATH_NOT_FOUND;
		return NULL;
	}

	/* Create report */
	struct duc_index_report *report = duc_malloc0(sizeof(struct duc_index_report));
	gettimeofday(&report->time_start, NULL);
	snprintf(report->path, sizeof(report->path), "%s", path_canon);

	/* Read mounted file systems to find fs types */
	if(req->fstypes_include || req->fstypes_exclude) {
		read_mounts(req);
	}

	/* Index subdirectories using a worker pool with a parallel topological sort */
	struct scanner *scanner = scanner_new(duc, NULL, path_canon, path_canon, NULL);
	if(! scanner) return NULL;
	
	scanner->req = req;
	scanner->rep = report;
	req->dev = scanner->ent.devino.dev;
	report->devino = scanner->ent.devino;

	/* Initialize, create, and join the worker pool */
	pthread_mutex_init(&database_write_lock, NULL);
	initialize_workers();
	stack_push(0, scanner); /* Push first task onto worker 0's stack */
	create_worker_pool(scanner);
	join_workers();
	
	/* Free resources */
	pthread_mutex_destroy(&database_write_lock);
	duc_free(worker_threads);
	duc_free(worker_stacks);
	duc_free(worker_rw_locks);

	/* Store report */
	if(!(req->flags & DUC_INDEX_DRY_RUN)) {
		gettimeofday(&report->time_stop, NULL);
		db_write_report(duc, report);
	}

	return report;
}


int duc_index_report_free(struct duc_index_report *rep)
{
	free(rep);
	return 0;
}


/*
 * End
 */
