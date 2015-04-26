#include "config.h"

#ifdef ENABLE_GRAPH

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>

#include "cmd.h"
#include "duc.h"
#include "duc-graph.h"


struct param {
	char *key;
	char *val;
	struct param *next;
};


static int opt_apparent = 0;
static char *opt_database = NULL;
static int opt_size = 800;
static double opt_fuzz = 0.7;
static int opt_levels = 4;
static char *opt_output = NULL;
static char *opt_palette = NULL;

static struct param *param_list = NULL;


int decodeURIComponent (char *sSource, char *sDest) 
{
	int nLength;
	for (nLength = 0; *sSource; nLength++) {
		if (*sSource == '%' && sSource[1] && sSource[2] && isxdigit(sSource[1]) && isxdigit(sSource[2])) {
			sSource[1] -= sSource[1] <= '9' ? '0' : (sSource[1] <= 'F' ? 'A' : 'a')-10;
			sSource[2] -= sSource[2] <= '9' ? '0' : (sSource[2] <= 'F' ? 'A' : 'a')-10;
			sDest[nLength] = 16 * sSource[1] + sSource[2];
			sSource += 3;
			continue;
		}
		sDest[nLength] = *sSource++;
	}
	sDest[nLength] = '\0';
	return nLength;
}


static int cgi_parse(void)
{
	char *qs = getenv("QUERY_STRING");
	if(qs == NULL) return -1;
	decodeURIComponent(qs,qs);

	char *p = qs;

	for(;;) {

		char *pe = strchr(p, '=');
		if(!pe) break;
		char *pn = strchr(pe, '&');
		if(!pn) pn = pe + strlen(pe);

		char *key = p;
		int keylen = pe-p;
		char *val = pe+1;
		int vallen = pn-pe-1;

		struct param *param = malloc(sizeof(struct param));
		assert(param);

		param->key = malloc(keylen+1);
		assert(param->key);
		strncpy(param->key, key, keylen);
		param->key[keylen] = '\0';

		param->val = malloc(vallen+1);
		assert(param->val);
		strncpy(param->val, val, vallen);
		param->val[vallen] = '\0';
		
		param->next = param_list;
		param_list = param;

		if(*pn == 0) break;
		p = pn+1;
	}

	return 0;
}


static char *cgi_get(const char *key)
{
	struct param *param = param_list;

	while(param) {
		if(strcmp(param->key, key) == 0) {
			return param->val;
		}
		param = param->next;
	}

	return NULL;
}


static void do_index(duc *duc, duc_graph *graph, duc_dir *dir)
{

	printf(
		"Content-Type: text/html\n"
		"\n"
		"<!DOCTYPE html>\n"
		"<head>\n"
		"<style>\n"
		"body { font-family: 'arial', 'sans-serif'; font-size: 11px; }\n"
		"table, thead, tbody, tr, td, th { font-size: inherit; font-family: inherit; }\n"
		"#list { 100%%; }\n"
		"#list td { padding-left: 5px; }\n"
		"</style>\n"
		"</head>\n"
	);
	
	char *path = cgi_get("path");
	char *script = getenv("SCRIPT_NAME");
	if(!script) return;

	char *qs = getenv("QUERY_STRING");
	int x = 0, y = 0;
	if(qs) {
		char *p1 = strchr(qs, '?');
		if(p1) {
			char *p2 = strchr(p1, ',');
			if(p2) {
				x = atoi(p1+1);
				y = atoi(p2+1);
			}
		}
	}

	if(x || y) {
		duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y);
		if(dir2) {
			path = duc_dir_get_path(dir2);
			duc_dir_close(dir2);
		}
	}

	struct duc_index_report *report;
	int i = 0;

	printf("<center>");

	printf("<table id=list>");
	printf("<tr>");
	printf("<th>Path</th>");
	printf("<th>Size</th>");
	printf("<th>Files</th>");
	printf("<th>Directories</th>");
	printf("<th>Date</th>");
	printf("<th>Time</th>");
	printf("</tr>");

	while( (report = duc_get_report(duc, i)) != NULL) {

		char ts_date[32];
		char ts_time[32];
		struct tm *tm = localtime(&report->time_start.tv_sec);
		strftime(ts_date, sizeof ts_date, "%Y-%m-%d",tm);
		strftime(ts_time, sizeof ts_time, "%H:%M:%S",tm);

		char url[PATH_MAX];
		snprintf(url, sizeof url, "%s?cmd=index&path=%s", script, report->path);

		duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;
		char *siz = duc_human_size(&report->size, st, 0);

		printf("<tr>");
		printf("<td><a href='%s'>%s</a></td>", url, report->path);
		printf("<td>%s</td>", siz);
		printf("<td>%zu</td>", report->file_count);
		printf("<td>%zu</td>", report->dir_count);
		printf("<td>%s</td>", ts_date);
		printf("<td>%s</td>", ts_time);
		printf("</tr>\n");

		free(siz);

		duc_index_report_free(report);
		i++;
	}
	printf("</table>");

	if(path) {
		printf("<a href='%s?cmd=index&path=%s&'>", script, path);
		printf("<img src='%s?cmd=image&path=%s' ismap='ismap'>\n", script, path);
		printf("</a><br>");
	}
	fflush(stdout);
}


void do_image(duc *duc, duc_graph *graph, duc_dir *dir)
{
	printf("Content-Type: image/png\n");
	printf("\n");

	if(dir) {
		duc_graph_draw_file(graph, dir, DUC_GRAPH_FORMAT_PNG, stdout);
	}
}



static int cgi_main(duc *duc, int argc, char **argv)
{
	int r;
	
	r = cgi_parse();
	if(r != 0) {
		fprintf(stderr, 
			"The 'cgi' subcommand is used for integrating Duc into a web server.\n"
			"Please refer to the documentation for instructions how to install and configure.\n"
		);
		return(-1);
	}


	char *cmd = cgi_get("cmd");
	if(cmd == NULL) cmd = "index";

        r = duc_open(duc, opt_database, DUC_OPEN_RO);
        if(r != DUC_OK) {
		printf("Content-Type: text/plain\n\n");
                printf("%s\n", duc_strerror(duc));
		return -1;
        }

	duc_dir *dir = NULL;
	char *path = cgi_get("path");
	if(path) {
		dir = duc_dir_open(duc, path);
		if(dir == NULL) {
			duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
			return 0;
		}
	}

	static enum duc_graph_palette palette = 0;
	
	if(opt_palette) {
		char c = tolower(opt_palette[0]);
		if(c == 's') palette = DUC_GRAPH_PALETTE_SIZE;
		if(c == 'r') palette = DUC_GRAPH_PALETTE_RAINBOW;
		if(c == 'g') palette = DUC_GRAPH_PALETTE_GREYSCALE;
		if(c == 'm') palette = DUC_GRAPH_PALETTE_MONOCHROME;
	}

	duc_graph *graph = duc_graph_new(duc);
	duc_graph_set_size(graph, opt_size);
	duc_graph_set_max_level(graph, opt_levels);
	duc_graph_set_fuzz(graph, opt_fuzz);
	duc_graph_set_palette(graph, palette);
	duc_graph_set_size_type(graph, opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL);

	if(strcmp(cmd, "index") == 0) do_index(duc, graph, dir);
	if(strcmp(cmd, "image") == 0) do_image(duc, graph, dir);

	if(dir) duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "Show apparent instead of actual file size" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_fuzz,      "fuzz",       0,  DUCRC_TYPE_DOUBLE, "use radius fuzz factor when drawing graph [0.7]" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "draw up to ARG levels deep [4]" },
	{ &opt_output,    "output",    'o', DUCRC_TYPE_STRING, "output file name [duc.png]" },
	{ &opt_palette,   "palette",    0,  DUCRC_TYPE_STRING, "select palette <size|rainbow|greyscale|monochrome>" },
	{ &opt_size,      "size",      's', DUCRC_TYPE_INT,    "image size [800]" },
	{ NULL }
};

struct cmd cmd_cgi = {
	.name = "cgi",
	.descr_short = "CGI interface wrapper",
	.usage = "[options] [PATH]",
	.main = cgi_main,
	.options = options,
		
};


#endif

/*
 * End
 */

