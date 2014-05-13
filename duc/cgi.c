
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cmd.h"
#include "duc.h"
#include "cgi.h"

int size = 600;
int depth = 4;


static char *find_xy(x, y)
{
	static char path_out[PATH_MAX];

	char *path = cgi_param("p");
	if(!path) return NULL;
        
	duc_errno err;
        duc *duc = duc_open("/home/ico/.duc.db", DUC_OPEN_RO, &err);
        if(duc == NULL) {
                fprintf(stderr, "%s\n", duc_strerror(err));
                return NULL;
        }

        ducdir *dir = duc_opendir(duc, path);
        if(dir == NULL) {
                fprintf(stderr, "%s\n", duc_strerror(duc_error(duc)));
                return NULL;
        }

	char path_found[PATH_MAX];
	int found = duc_graph_xy_to_path(dir, size, depth, x, y, path_found, sizeof(path_found));
	if(found) {
		snprintf(path_out, sizeof(path_out), "%s%s", path, path_found);
		fprintf(stderr, "found %s", path_out);
		return path_out;
	}
	
	duc_closedir(dir);
	duc_close(duc);

	return NULL;
}



static void do_index(void)
{
	cgi_init_headers();

	char *path = cgi_param("p");
	char *script = getenv("SCRIPT_NAME");
	if(!script) return;

	char *qs = getenv("QUERY_STRING");
	int x, y;
	if(qs) {
		char *p1 = strchr(qs, '?');
		if(p1) {
			char *p2 = strchr(p1, ',');
			if(p2) {
				x = atoi(p1+1);
				y = atoi(p2+1);
				path = find_xy(x, y);
			}
		}
	}

	if(path == NULL) path = "/home/ico";
	char *path2 = cgi_escape_special_chars(path);

	printf("<center>");
	printf("<b>%s</b><br>", path);
	printf("<a href='%s?c=p&p=%s&'>", script, path);
	printf("<img src='%s?p=%s&c=i' ismap='ismap'>\n", script, path);
	printf("</a>");
	fflush(stdout);

	free(path2);
}


int do_img(void)
{
	char *path = cgi_param("p");
	if(!path) path = "/home/ico/sandbox/prjs";

        duc_errno err;
        duc *duc = duc_open("/home/ico/.duc.db", DUC_OPEN_RO, &err);
        if(duc == NULL) {
                fprintf(stderr, "%s\n", duc_strerror(err));
                return -1;
        }

        ducdir *dir = duc_opendir(duc, path);
        if(dir == NULL) {
                fprintf(stderr, "%s\n", duc_strerror(duc_error(duc)));
                return -1;
        }

	printf("Content-Type: image/png\n");
	printf("\n");
	duc_graph(dir, size, depth, stdout);

	duc_closedir(dir);
	duc_close(duc);

	return 0;
}


static int cgi_main(int argc, char **argv)
{
	if(0) {
		printf("Content-type: text/plain\n");
		printf("\n");
		fflush(stdout);
		int r = system("printenv");
		if(r) r = 0;
	}

	cgi_init();
	cgi_process_form();

	char *cmd = cgi_param("c");
	if(cmd == NULL) cmd = "p";

	if(cmd[0] == 'p') do_index();
	if(cmd[0] == 'i') do_img();

	cgi_end();

	return 0;
}


struct cmd cmd_cgi = {
	.name = "cgi",
	.description = "CGI interface",
	.usage = "",
	.help = "",
	.main = cgi_main
};


/*
 * End
 */

