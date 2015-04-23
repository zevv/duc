
#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>

#include "duc.h"
#include "duc-graph.h"
#include "cmd.h"

static char *opt_database = NULL;
static int opt_apparent = 0;
static char *opt_format = "png";
static int opt_size = 800;
static double opt_fuzz = 0.7;
static int opt_levels = 4;
static char *opt_output = NULL;
static char *opt_palette = NULL;
static enum duc_graph_palette palette = 0;

static int graph_main(duc *duc, int argc, char **argv)
{
	char *path_out = opt_output;
	char *path_out_default = "duc.png";

	enum duc_graph_file_format format = DUC_GRAPH_FORMAT_PNG;

	if(strcasecmp(opt_format, "svg") == 0) {
		format = DUC_GRAPH_FORMAT_SVG;
		path_out_default = "duc.svg";
	}

	if(strcasecmp(opt_format, "pdf") == 0) {
		format = DUC_GRAPH_FORMAT_PDF;
		path_out_default = "duc.pdf";
	}
	
	if(opt_palette) {
		char c = tolower(opt_palette[0]);
		if(c == 's') palette = DUC_GRAPH_PALETTE_SIZE;
		if(c == 'r') palette = DUC_GRAPH_PALETTE_RAINBOW;
		if(c == 'g') palette = DUC_GRAPH_PALETTE_GREYSCALE;
		if(c == 'm') palette = DUC_GRAPH_PALETTE_MONOCHROME;
	}

	if(path_out == NULL) path_out = path_out_default;

	char *path = ".";
	if(argc > 0) path = argv[0];

	int r = duc_open(duc, opt_database, DUC_OPEN_RO);
	if(r != DUC_OK) {
		duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
		return -1;
	}

	duc_dir *dir = duc_dir_open(duc, path);
	if(dir == NULL) {
		duc_log(duc, DUC_LOG_WRN, "%s", duc_strerror(duc));
		return -1;
	}

	duc_graph *graph = duc_graph_new(duc);
	duc_graph_set_size(graph, opt_size);
	duc_graph_set_fuzz(graph, opt_fuzz);
	duc_graph_set_max_level(graph, opt_levels);
	duc_graph_set_palette(graph, palette);
	duc_graph_set_size_type(graph, opt_apparent ? DUC_SIZE_TYPE_APPARENT : DUC_SIZE_TYPE_ACTUAL);

	FILE *f = NULL;
	if(strcmp(path_out, "-") == 0) {
		f = stdout;
	} else {
		f = fopen(path_out, "w");
	}

	if(f == NULL) {
		duc_log(duc, DUC_LOG_WRN, "Error opening output file: %s", strerror(errno));
		return -1;
	}

	duc_graph_draw_file(graph, dir, format, f);

	duc_graph_free(graph);
	duc_dir_close(dir);
	duc_close(duc);

	return 0;
}


static struct ducrc_option options[] = {
	{ &opt_apparent,  "apparent",  'a', DUCRC_TYPE_BOOL,   "Show apparent instead of actual file size" },
	{ &opt_database,  "database",  'd', DUCRC_TYPE_STRING, "select database file to use [~/.duc.db]" },
	{ &opt_format,    "format",    'f', DUCRC_TYPE_STRING, "select output format <png|svg|pdf> [png]" },
	{ &opt_fuzz,      "fuzz",       0,  DUCRC_TYPE_DOUBLE, "use radius fuzz factor when drawing graph [0.7]" },
	{ &opt_levels,    "levels",    'l', DUCRC_TYPE_INT,    "draw up to ARG levels deep [4]" },
	{ &opt_output,    "output",    'o', DUCRC_TYPE_STRING, "output file name [duc.png]" },
	{ &opt_palette,   "palette",    0,  DUCRC_TYPE_STRING, "select palette <size|rainbow|greyscale|monochrome>" },
	{ &opt_size,      "size",      's', DUCRC_TYPE_INT,    "image size [800]" },
	{ NULL }
};
	
struct cmd cmd_graph = {
	.name = "graph",
	.descr_short = "Generate a sunburst graph for a given path",
	.usage = "[options] [PATH]",
	.main = graph_main,
	.options = options,
	.descr_long = 
		"The 'graph' subcommand queries the duc database and generates a sunburst graph\n"
		"showing the disk usage of the given path. If no path is given a graph is created\n"
		"for the current working directory.\n"
		"\n"
		"By default the graph is written to the file 'duc.png', which can be overridden by\n"
		"using the -o/--output option. The output can be sent to stdout by using the special\n"
		"file name '-'.\n"
};

/*
 * End
 */

