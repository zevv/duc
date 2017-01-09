
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <libgen.h>

#include "private.h"
#include "duc.h"
#include "duc-graph.h"
#include "graph-private.h"
#include "utlist.h"

struct svg_backend_data {
	FILE *fout;
	int gid;
};


void br_svg_start(duc_graph *g)
{
	struct svg_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;

	fprintf(f, "<?xml version='1.0' standalone='no'?>\n");
	fprintf(f, "<!DOCTYPE svg PUBLIC '-//W3C//DTD SVG 1.1//EN' \n");
	fprintf(f, " 'http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd'>\n");
	fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink= 'http://www.w3.org/1999/xlink'>\n");

	fprintf(f, " <style><![CDATA[\n");
	fprintf(f, "    text {\n");
	fprintf(f, "      font-family: 'Arial';\n");
	fprintf(f, "      text-anchor: middle;\n");
	fprintf(f, "      dominant-baseline: middle;\n");
	fprintf(f, "    }\n");
	fprintf(f, "    radialGradient {\n");
	fprintf(f, "      gradientUnits: userSpaceOnUse;\n");
	fprintf(f, "    }\n");
	fprintf(f, "  ]]>\n");
	fprintf(f, "</style>\n");

}

static void print_html(const char *s, FILE *f)
{
	while(*s) {
		switch(*s) {
			case '<': fprintf(f, "&lt;"); break;
			case '>': fprintf(f, "&gt;"); break;
			case '&': fprintf(f, "&amp;"); break;
			case '"': fprintf(f, "&quot;"); break;
			default: fputc(*s, f); break;
		}
		s++;
	}
}

static void br_svg_draw_text(duc_graph *g, double x, double y, double size, char *text)
{
	struct svg_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;

	fprintf(f, "<text x='%.0f' y='%.0f' font-size='%.0fpt' stroke='white' stroke-width='2'>", x, y, size);
	print_html(text, f);
	fprintf(f, "</text>\n");
	
	fprintf(f, "<text x='%.0f' y='%.0f' font-size='%.0fpt' fill='black' stroke-width='2'>", x, y, size);
	print_html(text, f);
	fprintf(f, "</text>\n");
}


static void br_svg_draw_tooltip(duc_graph *g, double x, double y, char *text)
{
}


static void br_svg_draw_section(duc_graph *g, double a1, double a2, double r1, double r2, double R, double G, double B, double L)
{
	struct svg_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;

	if(g->gradient) {
		fprintf(f, "<defs>\n");
		fprintf(f, " <radialGradient gradientUnits='userSpaceOnUse' id='g%d' cx='%.0f' cy='%.0f' r='%.0f'>\n", bd->gid, g->cx, g->cy, r2);
		fprintf(f, "  <stop offset='%.1f%%' style='stop-color:#%02x%02x%02x;'/>\n", 100*r1/r2, (int)(R*156), (int)(G*156), (int)(B*156));
		fprintf(f, "  <stop offset='100%%' style='stop-color:#%02x%02x%02x;'/>\n", (int)(R*255), (int)(G*255), (int)(B*255));
		fprintf(f, " </radialGradient>\n");
		fprintf(f, "</defs>\n");
		fprintf(f, "<path fill='url(#g%d)' ", bd->gid);
		bd->gid++;
	} else {
		fprintf(f, "<path fill='#%02x%02x%02x' ", (int)(R*255), (int)(G*255), (int)(B*255));
	}

	int large = (a2 - a1) > 0.5;

	fprintf(f, "d='");

	fprintf(f, "M%.0f,%.0f ",
			g->cx + r1 * sin(a1 * M_PI*2),
			g->cy - r1 * cos(a1 * M_PI*2));

	fprintf(f, "L%.0f,%.0f ",
			g->cx + r2 * sin(a1 * M_PI*2),
			g->cy - r2 * cos(a1 * M_PI*2));

	fprintf(f, "A %.0f %.0f 0 %d 1 %.0f %.0f ", r2, r2, large,
			g->cx + r2 * sin(a2 * M_PI*2),
			g->cy - r2 * cos(a2 * M_PI*2));

	fprintf(f, "L%.0f,%.0f ",
			g->cx + r1 * sin(a2 * M_PI*2),
			g->cy - r1 * cos(a2 * M_PI*2));

	fprintf(f, "A %.0f %.0f 0 %d 0 %.0f %.0f ", r1, r1, large,
			g->cx + r1 * sin(a1 * M_PI*2),
			g->cy - r1 * cos(a1 * M_PI*2));

	fprintf(f, "'/>\n");

}


void br_svg_done(duc_graph *g)
{
	struct svg_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;
	fprintf(f, "</svg>\n");
}


static void br_svg_free(duc_graph *g)
{
}



struct duc_graph_backend duc_graph_backend_svg = {
	.start = br_svg_start,
	.draw_text = br_svg_draw_text,
	.draw_tooltip = br_svg_draw_tooltip,
	.draw_section = br_svg_draw_section,
	.done = br_svg_done,
	.free = br_svg_free,
};



duc_graph *duc_graph_new_svg(duc *duc, FILE *fout)
{
	duc_graph *g = duc_graph_new(duc);
	g->backend = &duc_graph_backend_svg;

	struct svg_backend_data *bd;
	bd = duc_malloc(sizeof *bd);
	g->backend_data = bd;

	bd->fout = fout;
	bd->gid = 0;

	return g;
}

/*
 * End
 */

