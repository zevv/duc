
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "duc.h"
#include "db.h"
#include "cmd.h"
#include "ducrc.h"


struct cmd cmd_help;
struct cmd cmd_info;
struct cmd cmd_index;
struct cmd cmd_manual;
struct cmd cmd_ls;
struct cmd cmd_gui;
struct cmd cmd_guigl;
struct cmd cmd_graph;
struct cmd cmd_xml;
struct cmd cmd_cgi;
struct cmd cmd_ui;


struct cmd *cmd_list[] = {
	&cmd_help,
	&cmd_index,
	&cmd_info,
	&cmd_manual,
	&cmd_ls,
	&cmd_xml,
#ifdef ENABLE_CAIRO
	&cmd_cgi,
	&cmd_graph,
#endif
#ifdef ENABLE_X11
	&cmd_gui,
#endif
#ifdef ENABLE_OPENGL
	&cmd_guigl,
#endif
#ifdef ENABLE_UI
	&cmd_ui,
#endif

};

#define SUBCOMMAND_COUNT (sizeof(cmd_list) / sizeof(cmd_list[0]))

static struct cmd *find_cmd_by_name(const char *name);
static void help_cmd(struct cmd *cmd);
static void show_version(void);


static int opt_debug = 0;
static int opt_verbose = 0;
static int opt_quiet = 0;
static int opt_help = 0;
static int opt_version = 0;


static struct ducrc_option global_options[] = {
	{ &opt_debug,    "debug",      0, DUCRC_TYPE_BOOL,   "increase verbosity to debug level" },
	{ &opt_help,     "help",     'h', DUCRC_TYPE_BOOL,   "show help" },
	{ &opt_quiet,    "quiet",    'q', DUCRC_TYPE_BOOL,   "quiet mode, do not print any warning" },
	{ &opt_verbose,  "verbose",  'v', DUCRC_TYPE_BOOL,   "increase verbosity" },
	{ &opt_version,  "version",    0, DUCRC_TYPE_BOOL,   "output version information and exit" },
	{ NULL }
};


int main(int argc, char **argv)
{
	int r;

	/* Open duc context */
	
	duc *duc = duc_new();
	if(duc == NULL) {
		duc_log(duc, DUC_LOG_FTL, "Error creating duc context");
		return -1;
	}

	/* Find subcommand */

	struct cmd *cmd = NULL;

	if(argc >= 2) {
		cmd = find_cmd_by_name(argv[1]);
		if(strcmp(argv[1], "--version") == 0) opt_version = 1;
	}

	if(cmd == NULL) {
		if(getenv("QUERY_STRING")) {
			cmd = &cmd_cgi;
		} else {
			cmd = &cmd_help;
		}
	}


	/* Register options */

	struct ducrc *ducrc = ducrc_new(cmd->name);
	ducrc_add_options(ducrc, global_options);
	ducrc_add_options(ducrc, cmd->options);

	/* Call init function */

	if(cmd->init) {
		int r = cmd->init(duc, argc, argv);
		if(r != 0) exit(r);
	}

	/* Read configuration files from /etc/ducrc, ~/.ducrc and .ducrc and
	 * finally from the command line. Newer options will override older
	 * options */

	ducrc_read(ducrc, "/etc/ducrc");
	char *home = getenv("HOME");
	if(home) {
		char tmp[DUC_PATH_MAX];
		snprintf(tmp, sizeof(tmp), "%s/.ducrc", home);
		ducrc_read(ducrc, tmp);
	}
	ducrc_read(ducrc, "./.ducrc");
	r = ducrc_getopt(ducrc, &argc, &argv);

	/* Error detected on option parsing? */

	if(r == -1) {
		fprintf(stderr, "Try 'duc --help' for more information.\n");
		exit(1);
	}

	/* Version requested? */

	if(opt_version) {
		show_version();
		exit(0);
	}

	/* Help requested ? */

	if(opt_help) {
		help_cmd(cmd);
		return(EXIT_SUCCESS);
	}


	/* Set log level */

	duc_log_level log_level = DUC_LOG_WRN;
	if(opt_quiet) log_level = DUC_LOG_FTL;
	if(opt_verbose) log_level = DUC_LOG_INF;
	if(opt_debug) log_level = DUC_LOG_DMP;
	duc_set_log_level(duc, log_level);


	/* Handle command */

	r = cmd->main(duc, argc, argv);
	if(r == -2) help_cmd(cmd);

	
	/* Cleanup */

	duc_del(duc);
	ducrc_free(ducrc);
	return (r == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}


static struct cmd *find_cmd_by_name(const char *name)
{
	int i;

	for(i=0; i<SUBCOMMAND_COUNT; i++) {
		struct cmd *cmd = cmd_list[i];
		if(strcmp(cmd->name, name) == 0) {
			return cmd;
		}
	}
	return NULL;
}


static void printi(const char *s, int pos, int indent, int max)
{
	const char *p1 = s;
	const char *p2 = s;
	int i;

	while(*p1) {
		while(*p2 && !isspace(*p2)) p2++;
		int l = p2 - p1;

		if(pos + l >= max) {
			putchar('\n');
			for(i=0; i<indent; i++) putchar(' ');
			pos = indent;
		}

		fwrite(p1, l, 1, stdout);
		while(*p2 && isspace(*p2)) p2++;
		pos += l;
		if(pos < max) {
			putchar(' ');
			pos ++;
		}
		p1 = p2;
	}
}

		

static void show_options(struct ducrc_option *o, int show_long)
{
	int width = 80;

#ifdef TIOCGWINSZ
	if(isatty(STDOUT_FILENO)) {
		struct winsize w;
		int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
		if(r == 0) width = w.ws_col;
	}
#endif

	while(o && o->longopt) {
		char s[4] = "";
		char l[20] = "";

		if(o->shortopt) snprintf(s, sizeof(s), "-%c,", o->shortopt); 

		if(o->type != DUCRC_TYPE_BOOL) {
			snprintf(l, sizeof(l), "%s=VAL", o->longopt);
		} else {
			snprintf(l, sizeof(l), "%s", o->longopt);
		}

		printf("  %-4.4s --%-20.20s", s, l);
		if(o->descr_short) printf("%s", o->descr_short); 
		if(show_long && o->descr_long) {
			printf(". ");
			printi(o->descr_long, 29 + strlen(o->descr_short), 29, width-2);
		}
		printf("\n");

		o++;
	}
}


static void help_cmd(struct cmd *cmd)
{
	if(cmd->usage) {
		printf("usage: duc %s %s\n", cmd->name, cmd->usage);
		printf("\n");
	}

	if(cmd->descr_short) {
		printf("%s.\n", cmd->descr_short);
		printf("\n");
	}
	
	printf("Options for the command '%s':\n", cmd->name);
	show_options(cmd->options, 1);

	printf("\n");
	printf("Global options:\n");
	show_options(global_options, 1);

	if(cmd->descr_long) {
		printf("\n%s", cmd->descr_long); 
	}
}


int opt_all = 0;


static int help_main(duc *duc, int argc, char **argv)
{
	struct cmd *cmd = NULL;

	if(argc > 0) cmd = find_cmd_by_name(argv[0]);

	if(cmd) {
		help_cmd(cmd);
	} else {
		printf("usage: duc <cmd> [options] [args]\n"
			"\n"
			"Available subcommands:\n"
			"\n"
		);

		int i;
		for(i=0; i<SUBCOMMAND_COUNT; i++) {
			struct cmd *c = cmd_list[i];
			if(c->hidden) continue;

			if(opt_all) {
				printf("duc %s %s: %s\n", c->name, c->usage, c->descr_short);
				printf("\n");
				show_options(c->options, 0);
				printf("\n");
			} else {
				printf("  %-10.10s: %s\n", c->name, c->descr_short);
			}
		}

		if(!opt_all) {

			printf("\n");
			printf("Global options:\n");
			show_options(global_options, 1);

			printf(
				"\n"
				"Use 'duc help <subcommand>' or 'duc <subcommand> -h' for a complete list of all\n"
				"options and detailed description of the subcommand.\n"
				"\n"
				"Use 'duc help --all' for a complete list of all options for all subcommands.\n"
			);
		}
	}

	return 0;
}


static void show_options_manual(struct ducrc_option *o)
{
	while(o && o->longopt) {

		printf("  * ");

		if(o->shortopt) printf("`-%c`, ", o->shortopt); 

		if(o->type != DUCRC_TYPE_BOOL) {
			printf("`--%s=VAL`:", o->longopt);
		} else {
			printf("`--%s`:", o->longopt);
		}
		printf("\n");
		printf("    %s", o->descr_short);
		if(o->descr_long) {
			printf(". %s\n", o->descr_long);
		}
		printf("\n");
		printf("\n");
		o++;
	}
}


static int manual_main(duc *duc, int argc, char **argv)
{
	int i;

	printf("### Global options\n");
	printf("\n");
	printf("These options apply to all Duc subcommands:\n");
	printf("\n");
	show_options_manual(global_options);

	for(i=0; i<SUBCOMMAND_COUNT; i++) {
		struct cmd *c = cmd_list[i];
		if(c->hidden) continue;
		printf("### duc %s\n", c->name);
		printf("\n");
		if(c->descr_long) {
			printf("%s\n", c->descr_long);
			printf("\n");
		}
		printf("Options for command `duc %s %s`:\n", c->name, c->usage);
		printf("\n");
		show_options_manual(c->options);
	}


	return 0;
}


static void show_version(void)
{
	printf("duc version: %s\n", PACKAGE_VERSION);
	printf("options: ");
#ifdef ENABLE_CAIRO
	printf("cairo ");
#endif
#ifdef ENABLE_OPENGL
	printf("opengl ");
#endif
#ifdef ENABLE_X11
	printf("x11 ");
#endif
#ifdef ENABLE_UI
	printf("ui ");
#endif
	printf(DB_BACKEND "\n");

	exit(0);
}

static struct ducrc_option help_options[] = {
	{ &opt_all,     "all",     'a', DUCRC_TYPE_BOOL,   "show complete help for all commands" },
	{ NULL }
};

struct cmd cmd_help = {
	.name = "help",
	.descr_short = "Show help",
	.usage = "[options]",
	.main = help_main,
	.options = help_options,
};


struct cmd cmd_manual = {
       .name = "manual",
       .descr_short = "Show manual",
       .usage = "",
       .main = manual_main,
       .hidden = 1,
};

/*
 * End
 */
