#include "config.h"

#ifdef ENABLE_CAIRO

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
static char *opt_css_url = NULL;
static char *opt_database = NULL;
static int opt_bytes = 0;
static int opt_list = 0;
static int opt_size = 800;
static double opt_fuzz = 0.7;
static int opt_levels = 4;
static char *opt_palette = NULL;
static int opt_tooltip = 0;
static int opt_ring_gap = 4;

static struct param *param_list = NULL;

static void print_html(const char *s)
{
	while(*s) {
		switch(*s) {
			case '<': printf("&lt;"); break;
			case '>': printf("&gt;"); break;
			case '&': printf("&amp;"); break;
			case '"': printf("&quot;"); break;
			default: putchar(*s); break;
		}
		s++;
	}
}

static void print_cgi(const char *s)
{
	while(*s) {
		if(*s == '/' || isalnum(*s)) {
			putchar(*s);
		} else {
			printf("%%%02x", *(uint8_t *)s);
		}
		s++;
	}
}


static int decode_uri(char *src, char *dst) 
{
	int len;
	for (len = 0; *src; len++) {
		if (*src == '%' && src[1] && src[2] && isxdigit(src[1]) && isxdigit(src[2])) {
			src[1] -= src[1] <= '9' ? '0' : (src[1] <= 'F' ? 'A' : 'a')-10;
			src[2] -= src[2] <= '9' ? '0' : (src[2] <= 'F' ? 'A' : 'a')-10;
			dst[len] = 16 * src[1] + src[2];
			src += 3;
			continue;
		}
		dst[len] = *src++;
	}
	dst[len] = '\0';
	return len;
}


static int cgi_parse(void)
{
	char *qs = getenv("QUERY_STRING");
	if(qs == NULL) return -1;

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
		decode_uri(param->key, param->key);

		param->val = malloc(vallen+1);
		assert(param->val);
		strncpy(param->val, val, vallen);
		param->val[vallen] = '\0';
		decode_uri(param->val, param->val);
		
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

	
	char *path = cgi_get("path");
	char *script = getenv("SCRIPT_NAME");
	if(!script) return;
		
	char url[DUC_PATH_MAX];
	snprintf(url, sizeof url, "%s?cmd=index", script);

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
		duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y, NULL);
		if(dir2) {
			dir = dir2;
			path = duc_dir_get_path(dir);
		}
	}

	struct duc_index_report *report;
	int i = 0;

	printf(
		"Content-Type: text/html\n"
		"\n"
		"<!DOCTYPE html>\n"
		"<head>\n"
		"  <meta charset=\"utf-8\" />\n"
	);

	if(opt_css_url) {
		printf("<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\">\n", opt_css_url);
	} else {
		printf(
			"<style>\n"
			"body { font-family: \"arial\", \"sans-serif\"; font-size: 11px; }\n"
			"table, thead, tbody, tr, td, th { font-size: inherit; font-family: inherit; }\n"
			"#main { display:table-cell; }\n"
			"#index { border-bottom: solid 1px #777777; }\n"
			"#index table td { padding-left: 5px; }\n"
			"#graph { float: left; }\n"
			"#list { float: left; }\n"
			"#list table { margin-left: auto; margin-right: auto; }\n"
			"#list table td { padding-left: 5px; }\n"
			"#list table td.name, th.name { text-align: left; }\n"
			"#list table td.size, th.size { text-align: right; }\n"
			"#tooltip { display: none; position: absolute; background-color: white; border: solid 1px black; padding: 3px; }\n"
			"</style>\n"
		);
	}

	if(opt_tooltip) {
		printf(
			"<script>\n"
			"  window.onload = function() {\n"
			"    var img = document.getElementById('img');\n"
			"    var rect = img.getBoundingClientRect(img);\n"
			"    var tt = document.getElementById('tooltip');\n"
			"    var timer;\n"
			"    img.onmouseout = function() { tt.style.display = \"none\"; };\n"
			"    img.onmousemove = function(e) {\n"
			"      if(timer) clearTimeout(timer);\n"
			"      timer = setTimeout(function() {\n"
			"        var x = e.clientX - rect.left;\n"
			"        var y = e.clientY - rect.top;\n"
			"        var req = new XMLHttpRequest();\n"
			"        req.onreadystatechange = function() {\n"
			"          if(req.readyState == 4 && req.status == 200) {\n"
			"            tt.innerHTML = req.responseText;\n"
			"            tt.style.display = tt.innerHTML.length > 0 ? \"block\" : \"none\";\n"
			"            tt.style.left = (e.clientX - tt.offsetWidth / 2) + \"px\";\n"
			"            tt.style.top = (e.clientY - tt.offsetHeight - 5) + \"px\";\n"
			"          }\n"
			"        };\n"
			"        req.open(\"GET\", \"?cmd=lookup&path=%s&x=\"+x+\"&y=\"+y , true);\n"
			"        req.send()\n"
			"      }, 100);\n"
			"    };\n"
			"  };\n"
			"</script>\n", path
			);
	}

	printf(
		"</head>\n"
		"<div id=main>\n"
	);

	printf("<div id=index>");
	printf(" <table>\n");
	printf("  <tr>\n");
	printf("   <th>Path</th>\n");
	printf("   <th>Size</th>\n");
	printf("   <th>Files</th>\n");
	printf("   <th>Directories</th>\n");
	printf("   <th>Date</th>\n");
	printf("   <th>Time</th>\n");
	printf("  </tr>\n");

	while( (report = duc_get_report(duc, i)) != NULL) {

		char ts_date[32];
		char ts_time[32];
		struct tm *tm = localtime(&report->time_start.tv_sec);
		strftime(ts_date, sizeof ts_date, "%Y-%m-%d",tm);
		strftime(ts_time, sizeof ts_time, "%H:%M:%S",tm);

		duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;
		char siz[16];
		duc_human_size(&report->size, st, 0, siz, sizeof siz);

		printf("  <tr>\n");
		printf("   <td><a href=\"%s&path=", url);
		print_cgi(report->path);
		printf("\">");
		print_html(report->path);
		printf("</a></td>\n");
		printf("   <td>%s</td>\n", siz);
		printf("   <td>%zu</td>\n", report->file_count);
		printf("   <td>%zu</td>\n", report->dir_count);
		printf("   <td>%s</td>\n", ts_date);
		printf("   <td>%s</td>\n", ts_time);
		printf("  </tr>\n");

		if(path == NULL && report->path) {
			//path = duc_strdup(report->path);
		}

		duc_index_report_free(report);
		i++;
	}
	printf(" </table>\n");

	if(path) {
		printf("<div id=graph>\n");
		printf(" <a href=\"%s?cmd=index&path=", script);
		print_cgi(path);
		printf("&\">\n");
		printf("  <img id=\"img\" src=\"%s?cmd=image&path=", script);
		print_cgi(path);
		printf("\" ismap=\"ismap\">\n");
		printf(" </a>\n");
		printf("</div>\n");
	}

	if(path && dir && opt_list) {

		duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

		printf("<div id=list>\n");
		printf(" <table>\n");
		printf("  <tr>\n");
		printf("   <th class=name>Filename</th>\n");
		printf("   <th class=size>Size</th>\n");
		printf("  </tr>\n");

		struct duc_dirent *e;
		int n = 0;
		while((n++ < 40) && (e = duc_dir_read(dir, st)) != NULL) {
			char siz[16];
			duc_human_size(&e->size, st, opt_bytes, siz, sizeof siz);
			printf("  <tr><td class=name>");

			if(e->type == DUC_FILE_TYPE_DIR) {
				printf("<a href=\"%s&path=", url);
				print_cgi(path);
				printf("/");
				print_cgi(e->name);
				printf("\">");
			}

			print_html(e->name);

			if(e->type == DUC_FILE_TYPE_DIR) 
				printf("</a>\n");

			printf("   <td class=size>%s</td>\n", siz);
			printf("  </tr>\n");
		}

		printf(" </table>\n");
		printf("</div>\n");
	}

	printf("</div>\n");

	if(opt_tooltip) {
		printf("<div id=\"tooltip\"></div>\n");
	}

	fflush(stdout);
}


void do_image(duc *duc, duc_graph *graph, duc_dir *dir)
{
	printf("Content-Type: image/png\n");
	printf("\n");

	if(dir) {
		duc_graph_draw(graph, dir);
	}
}


void do_lookup(duc *duc, duc_graph *graph, duc_dir *dir)
{
	duc_size_type st = opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL;

	printf("Content-Type: text/html\n");
	printf("\n");

	char *xs = cgi_get("x");
	char *ys = cgi_get("y");

	if(dir && xs && ys) {

		int x = atoi(xs);
		int y = atoi(ys);

		struct duc_dirent *ent = NULL;
		duc_dir *dir2 = duc_graph_find_spot(graph, dir, x, y, &ent);
		if(dir2) duc_dir_close(dir2);

		if(ent) {

			char siz[16];
			duc_human_size(&ent->size, st, opt_bytes, siz, sizeof siz);
			char *typ = duc_file_type_name(ent->type);
			if(typ == NULL) typ = "unknown";

			printf( "name: %s<br>\n"
				"type: %s<br>\n"
				"size: %s<br>\n",
				ent->name,
				typ,
				siz);

			free(ent->name);
			free(ent);
		}
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
			printf("Content-Type: text/plain\n\n");
			printf("%s\n", duc_strerror(duc));
			print_html(path);
			return -1;
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

	duc_graph *graph = duc_graph_new_cairo_file(duc, DUC_GRAPH_FORMAT_PNG, stdout);
	duc_graph_set_size(graph, opt_size, opt_size);
	duc_graph_set_max_level(graph, opt_levels);
	duc_graph_set_fuzz(graph, opt_fuzz);
	duc_graph_set_palette(graph, palette);
	duc_graph_set_exact_bytes(graph, opt_bytes);
	duc_graph_set_size_type(graph, opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL);
	duc_graph_set_ring_gap(graph, opt_ring_gap);

	if(strcmp(cmd, "index") == 0) do_index(duc, graph, dir);
	if(strcmp(cmd, "image") == 0) do_image(duc, graph, dir);
	if(strcmp(cmd, "lookup") == 0) do_lookup(duc, graph, dir);

	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "Show apparent instead of actual file size" },
	{ &opt_bytes,     "bytes",     'b', DUCRC_TYPE_BOOL,   "show file size in exact number of bytes" },
	{ &opt_css_url,   "css-url",     0, DUCRC_TYPE_STRING, "url of CSS style sheet to use instead of default CSS" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_fuzz,      "fuzz",       0,  DUCRC_TYPE_DOUBLE, "use radius fuzz factor when drawing graph [0.7]" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "draw up to ARG levels deep [4]" },
	{ &opt_list,      "list",        0, DUCRC_TYPE_BOOL,   "generate table with file list" },
	{ &opt_palette,   "palette",     0, DUCRC_TYPE_STRING, "select palette <size|rainbow|greyscale|monochrome>" },
	{ &opt_ring_gap,  "ring-gap",   0,  DUCRC_TYPE_INT,    "leave a gap of VAL pixels between rings" },
	{ &opt_size,      "size",      's', DUCRC_TYPE_INT,    "image size [800]" },
	{ &opt_tooltip,   "tooltip",     0, DUCRC_TYPE_BOOL,   "enable tooltip when hovering over the graph",
		"enabling the tooltip will cause an asynchronous HTTP request every time the mouse is moved and "
		"can greatly increas the HTTP traffic to the web server" },
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

