
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

struct html_backend_data {
	FILE *fout;
	int write_body;
};


void br_html_start(duc_graph *g)
{
	struct html_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;

	if(bd->write_body) {
		fprintf(f, "<?xml version='1.0' standalone='no'?>\n");
		fprintf(f, "<!DOCTYPE html PUBLIC '-//W3C//DTD SVG 1.1//EN' \n");
		fprintf(f, " 'http://www.w3.org/Graphics/SVG/1.1/DTD/html11.dtd'>\n");
		fprintf(f, "<html xmlns='http://www.w3.org/2000/html' xmlns:xlink= 'http://www.w3.org/1999/xlink'>\n");
	}

	fprintf(f, "<canvas id='duc_canvas' width='%.0f' height='%.0f'>\n", g->width, g->height);
	fprintf(f, "Your browser does not support the HTML5 canvas tag.\n");
	fprintf(f, "</canvas>\n");


	fprintf(f, "<script type='text/javascript'>\n");
	fprintf(f, "var canvas = document.getElementById('duc_canvas');\n");
	fprintf(f, "var f = Math.floor;\n");
	fprintf(f, "var pi = Math.PI;\n");
	fprintf(f, "var c = canvas.getContext('2d');\n");
	fprintf(f, "c.textAlign = 'center'\n");
	fprintf(f, "c.textBaseline = 'middle'\n");
	fprintf(f, "c.lineJoin = 'round'\n");

	fprintf(f, "function g(a1, a2, r1, r2, r, g, b) {\n");
	fprintf(f, "  a1 = a1*1e-3 * pi * 2 - pi / 2;\n");
	fprintf(f, "  a2 = a2*1e-3 * pi * 2 - pi / 2;\n");
	fprintf(f, "  var c1 = 'rgb(' + f(r*0.6) + ',' + f(g*0.6) + ',' + f(b*0.6) + ')';\n");
	fprintf(f, "  var c2 = 'rgb(' + f(r*1.0) + ',' + f(g*1.0) + ',' + f(b*1.0) + ')';\n");
	if(g->gradient) {
		fprintf(f, "  var g = c.createRadialGradient(%.0f, %.0f, r1, %.0f, %.0f, r2);\n", g->cx, g->cy, g->cx, g->cy);
		fprintf(f, "  g.addColorStop(0, c1);\n");
		fprintf(f, "  g.addColorStop(1, c2);\n");
		fprintf(f, "  c.fillStyle = g;\n");
	} else {
		fprintf(f, "  c.fillStyle = c2;\n");
	}
	fprintf(f, "  c.beginPath();\n");
	fprintf(f, "  c.arc(%.0f, %.0f, r1, a1, a2, false);\n",  g->cx,  g->cy);
	fprintf(f, "  c.arc(%.0f, %.0f, r2, a2, a1, true);\n",  g->cx,  g->cy);
	fprintf(f, "  c.closePath();\n");
	fprintf(f, "  c.fill();\n");
	fprintf(f, "}\n");

	fprintf(f, "function t(text, s, x, y) {\n");
	fprintf(f, "  c.font = s + 'pt Arial'\n");
	fprintf(f, "  c.lineWidth = 2;\n");
	fprintf(f, "  c.strokeStyle = '#ffffff';\n");
	fprintf(f, "  c.fillStyle = '#000000';\n");
	fprintf(f, "  var h = Math.floor(c.measureText('M').width * 1.6);\n");
	fprintf(f, "  var y = Math.floor(y-h/2);\n");
	fprintf(f, "  var ls = text.split('\\n');\n");
	fprintf(f, "  for(var i=0; i<ls.length; i++) {\n");
	fprintf(f, "    c.strokeText(ls[i], x, y+i*h);\n");
	fprintf(f, "    c.fillText(ls[i], x, y+i*h);\n");
	fprintf(f, "  }\n");
	fprintf(f, "}\n");

}

static void print_html(const char *s, FILE *f)
{
	while(*s) {
		switch(*s) {
			case '\'': fprintf(f, "\\'"); break;
			case '\n': fprintf(f, "\\n"); break;
			case '\r': fprintf(f, "\\r"); break;
			default: fputc(*s, f); break;
		}
		s++;
	}
}

static void br_html_draw_text(duc_graph *g, double x, double y, double size, char *text)
{
	struct html_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;

	fprintf(f, "t('"); 
	print_html(text, f); 
	fprintf(f, "',%.0f,%.0f,%.0f);\n", size, x, y);
}


static void br_html_draw_tooltip(duc_graph *g, double x, double y, char *text)
{
}


static void br_html_draw_section(duc_graph *g, double a1, double a2, double r1, double r2, double H, double S, double V, double line)
{
	struct html_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;

	double R, G, B;
	hsv2rgb(H, S, V, &R, &G, &B);

	fprintf(f, "g(%.0f,%.0f,%.0f,%.0f,%d,%d,%d);\n", 
			a1 * 1000, a2 * 1000,
			r1, r2,
			(int)(R*255), (int)(G*255), (int)(B*255));
}


void br_html_done(duc_graph *g)
{
	struct html_backend_data *bd = g->backend_data;
	FILE *f = bd->fout;
	fprintf(f, "</script>\n");
	if(bd->write_body) {
		fprintf(f, "</html>\n");
	}
}


static void br_html_free(duc_graph *g)
{
}



struct duc_graph_backend duc_graph_backend_html = {
	.start = br_html_start,
	.draw_text = br_html_draw_text,
	.draw_tooltip = br_html_draw_tooltip,
	.draw_section = br_html_draw_section,
	.done = br_html_done,
	.free = br_html_free,
};



duc_graph *duc_graph_new_html(duc *duc, FILE *fout, int write_body)
{
	duc_graph *g = duc_graph_new(duc);
	g->backend = &duc_graph_backend_html;

	struct html_backend_data *bd;
	bd = duc_malloc(sizeof *bd);
	g->backend_data = bd;

	bd->fout = fout;
	bd->write_body = write_body;

	return g;
}

/*
 * End
 */

