#include "config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>

#include "cmd.h"
#include "duc.h"
#include "duc-graph.h"

/* John */

/*
 * Simple parser for CGI parameter line. Does not escape CGI strings.
 */

struct param {
	char *key;
	char *val;
	struct param *next;
};

struct param *param_list = NULL;

int decodeURIComponent (char *sSource, char *sDest) {
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

/* silly little helper */
static void print_html_header(const char *title) {
	printf(
		"Content-Type: text/html\n"
		"\n"
		"<!DOCTYPE html>\n"
		"<HEAD>\n"
		"<STYLE>\n"
		"body { font-family: 'arial', 'sans-serif'; font-size: 11px; }\n"
		"table, thead, tbody, tr, td, th { font-size: inherit; font-family: inherit; }\n"
		"#list { 100%%; }\n"
		"#list td { padding-left: 5px; }\n"
		"</STYLE>\n"
		"<TITLE>%s</TITLE>\n"
		"</HEAD>\n",
		title
	);
}	

/* 
   Parses the CGI input parameters, does NO sanity checking.  Needs
   work to make safer.  
*/
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

		/* Do we filter out bogus commands here?  I think so... FIXME */

		param->key = malloc(keylen+1);
		assert(param->key);
		strncpy(param->key, key, keylen);

		/* Now to filter parameters, to make sure we don't have an XSS
		   vulns either.  FIXME */

		param->val = malloc(vallen+1);
		assert(param->val);
		strncpy(param->val, val, vallen);
		
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
    print_html_header("Index");

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
			duc_dir_close(dir);
			dir = dir2;
			path = duc_dir_get_path(dir);
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

		char *siz = duc_human_size(report->size_total);

		printf("<tr>");
		printf("<td><a href='%s'>%s</a></td>", url, report->path);
		printf("<td>%s</td>", siz);
		printf("<td>%lu</td>", (unsigned long)report->file_count);
		printf("<td>%lu</td>", (unsigned long)report->dir_count);
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

/* return a string of the time the file was last modified.  I hate C
   programming... */

char *last_updated(char * path) {

  struct stat stbuf;
  int r = stat(path, &stbuf);

  if (r == -1) {
	return NULL;
  }
  else {
	return(ctime(&stbuf.st_mtime));
  }
}

/* Output the PNG image to stdout (i.e. the web browser...) */
void do_image(duc *duc, duc_graph *graph, duc_dir *dir)
{
	printf("Content-Type: image/png\n");
	printf("\n");

	if(dir) {
		duc_graph_draw_file(graph, dir, DUC_GRAPH_FORMAT_PNG, stdout);
	}
}



static int cgi_main(int argc, char **argv)
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

	char *path_db = NULL;
	char *db_dir = NULL;

	struct option longopts[] = {
		{ "database",       required_argument, NULL, 'd' },
		{ "dbdir",          required_argument, NULL, 'D' },
		{ NULL }
	};

	int c;
	while( ( c = getopt_long(argc, argv, "d:D:q", longopts, NULL)) != EOF) {

		switch(c) {
			case 'd':
				path_db = optarg;
				break;
			case 'D':
				db_dir = optarg;
				break;
			default:
				return -2;
		}
	}

	char *cmd = cgi_get("cmd");
	char *path = cgi_get("path");
	char *db = cgi_get("db");
	char *script = getenv("SCRIPT_NAME");

	if(cmd == NULL) { cmd = "index"; }
	
	duc *duc = duc_new();
	if(duc == NULL) {
	    
	    print_html_header("Error creating duc context\n");
	    printf("<BODY>Sorry, we had a problem with the CGI script.\n</BODY></HTML>\n");
	    return -1;
        }

	if (db_dir) {
	  if (db == NULL) {
	    glob_t bunch_of_dbs;
	    char **db_file;
		
		char url[PATH_MAX];
		char *db_name;


	    size_t n = duc_find_dbs(db_dir, &bunch_of_dbs);
	    int i = 0;

	    print_html_header("DUC db_dir list");
	    
	    printf("<BODY>\n<H1>DUC db_dir list: %s</H1>\n<UL>\n",script);
	    printf("<br>Found %zu (%zu) DBs to look at.<br>\n", n, bunch_of_dbs.gl_pathc);
	    for (db_file = bunch_of_dbs.gl_pathv; i < n; db_file++, i++) {
		  db_name = basename(*db_file);
		  snprintf(url, sizeof url, "%s?db=%s&cmd=index", script, db_name);
		  printf("  <LI> <A HREF=\"%s\">%s</A> updated %s\n", url, db_name, last_updated(*db_file));
	    }
	    printf("</UL>\n</BODY>\n</HTML>\n");
	    exit(1);
	  } else {
		r = duc_open(duc, db, DUC_OPEN_RO);
		if (r != DUC_OK) {
		  print_html_header("Error Opening DB");
		  printf("<BODY>%s\n</BODY></HTML>", duc_strerror(duc));
		  return -1;
        }
	  }
	}

        r = duc_open(duc, path_db, DUC_OPEN_RO);
        if(r != DUC_OK) {
	    print_html_header("Error Opening DB");
	    printf("<BODY>%s\n</BODY></HTML>", duc_strerror(duc));
		return -1;
        }

	duc_dir *dir = NULL;

	if(path) {
		dir = duc_dir_open(duc, path);
		if(dir == NULL) {
			fprintf(stderr, "%s\n", duc_strerror(duc));
			return 0;
		}
	}

	duc_graph *graph = duc_graph_new(duc);
	duc_graph_set_size(graph, 800);
	duc_graph_set_max_level(graph, 4);

	if(strcmp(cmd, "index") == 0) do_index(duc, graph, dir);
	if(strcmp(cmd, "image") == 0) do_image(duc, graph, dir);

	if(dir) duc_dir_close(dir);
	duc_close(duc);
	duc_del(duc);

	return 0;
}


struct cmd cmd_cgi = {
	.name = "cgi",
	.description = "CGI interface",
	.usage = "[options] [PATH]",
	.help = "\
  -d, --database=ARG      use database file ARG [~/.duc.db]\n\
  -D, --datadir=ARG       use directory of database file(s) ARG\n",
	.main = cgi_main,
		
};


/*
 * End
 */

