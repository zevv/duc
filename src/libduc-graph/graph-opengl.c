// http://nothings.org/stb/font/latin_ext/

#include "config.h"

#ifdef ENABLE_OPENGL

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

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "private.h"
#include "duc.h"
#include "duc-graph.h"
#include "graph-private.h"
#include "utlist.h"
#include "font.c"

struct opengl_backend_data {
	GLuint sp;
	stb_fontchar fontdata[STB_SOMEFONT_NUM_CHARS];
	GLuint font_texid;
	double font_scale;

	GLuint loc_pos;
	GLuint loc_texture;
	GLuint loc_sampler;
	GLuint loc_matrix;
	GLuint loc_color;
};

static const GLchar *vshader = 
	"uniform mat4 matrix;\n"
	"attribute vec4 pos_in;\n"
	"attribute vec2 tex_in;\n"
	"attribute vec4 color_in;\n"
	"varying vec2 tex_out;\n"
	"varying vec4 color;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = matrix * pos_in;\n"
	"       tex_out = tex_in;\n"
	"       color = color_in;\n"
	"}\n";

static const GLchar *fshader = 
	"uniform sampler2D Texture;\n"
	"varying vec4 color;\n"
	"varying vec2 tex_out;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = color + texture2D(Texture, tex_out);\n"
	"}\n";


void br_opengl_start(duc_graph *g)
{
	struct opengl_backend_data *bd = g->backend_data;
	GLuint sp = bd->sp;

	glUseProgram(sp);

	GLfloat mv[] = { 
		1 / g->width*2, 0,             0, 0,
		0,              -1/g->height*2, 0, 0,
		0,              0,             1, 0,
		0,              0,             0, 1 
	};

	glUniformMatrix4fv(bd->loc_matrix, 1, 0, mv);

	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}


static double draw_char(duc_graph *g, double x, double y, double size, int c)
{
	struct opengl_backend_data *bd = g->backend_data;

	if(c < STB_SOMEFONT_FIRST_CHAR ||
	   c >= STB_SOMEFONT_FIRST_CHAR + STB_SOMEFONT_NUM_CHARS)
		return 0;

	stb_fontchar *cd = &bd->fontdata[c - STB_SOMEFONT_FIRST_CHAR];

	double f = size * bd->font_scale;

	y += f * STB_SOMEFONT_LINE_SPACING * 0.2;

	GLfloat vVertices[] = {
		x + cd->x0f * f, y + cd->y0f * f,   cd->s0f, cd->t0f,
		x + cd->x1f * f, y + cd->y0f * f,   cd->s1f, cd->t0f,
		x + cd->x1f * f, y + cd->y1f * f,   cd->s1f, cd->t1f,
		x + cd->x0f * f, y + cd->y1f * f,   cd->s0f, cd->t1f,
	};

	GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

	glVertexAttribPointer(bd->loc_pos,     2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), &vVertices[0]);
	glVertexAttribPointer(bd->loc_texture, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), &vVertices[2]);

	glEnableVertexAttribArray(bd->loc_pos);
	glEnableVertexAttribArray(bd->loc_texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, bd->font_texid);

	glUniform1i(bd->loc_texture, 0);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

	return cd->advance * size * bd->font_scale;
}


static void text_size(duc_graph *g, char *text, double *w, double *h, double size)
{
	struct opengl_backend_data *bd = g->backend_data;
	char *p = text;
	double wmax = 0;
	*w = *h = 0;

	while(*p) {
		int c = *p++;
		if(c == '\r' || c == '\n') {
			*h += (int)(size * bd->font_scale * STB_SOMEFONT_LINE_SPACING * 1.5);
			wmax = 0;
		} else {
			stb_fontchar *cd = &bd->fontdata[c - STB_SOMEFONT_FIRST_CHAR];
			wmax += size * bd->font_scale * cd->advance;
			if(wmax > *w) *w = wmax;
		}
	}
}


static void draw_text_line(duc_graph *g, double x, double y, double size, char *text, int l)
{
	int i;
	
	for(i=0; i<l; i++) {
		x += draw_char(g, x, y, size, text[i]);
	}
}


static void draw_text(duc_graph *g, double _x, double _y, double size, char *text)
{
	struct opengl_backend_data *bd = g->backend_data;

	double x = _x - g->cx;
	double y = _y - g->cy - size * bd->font_scale * STB_SOMEFONT_LINE_SPACING;
		
	glDisableVertexAttribArray(bd->loc_color);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	char *p1 = text;
	char *p2 = text;

	while(*p2) {
		while(*p2 && *p2 != '\n' && *p2 != '\r') {
			p2++;
		}

		glVertexAttrib4f(bd->loc_color, 1, 1, 1, 0);
		draw_text_line(g, x-1, y+0, size, p1, p2-p1);
		draw_text_line(g, x+1, y-0, size, p1, p2-p1);
		draw_text_line(g, x-0, y+1, size, p1, p2-p1);
		draw_text_line(g, x+0, y-1, size, p1, p2-p1);
		glVertexAttrib4f(bd->loc_color, 0, 0, 0, 0);
		draw_text_line(g, x+0, y, size, p1, p2-p1);

		if(!*p2) break;

		y += size * bd->font_scale * STB_SOMEFONT_LINE_SPACING * 1.5;
		x = _x - g->cx;

		p2 ++;
		p1 = p2;
	}

	glBlendFunc(GL_ONE, GL_ZERO);
}


static void br_opengl_draw_text(duc_graph *g, double x, double y, double size, char *text)
{
	double w, h;
	text_size(g, text, &w, &h, size);
	draw_text(g, x - w/2, y - h/2, size, text);
}


static void br_opengl_draw_tooltip(duc_graph *g, double x, double y, char *text)
{
	struct opengl_backend_data *bd = g->backend_data;
	double w, h;

	text_size(g, text, &w, &h, FONT_SIZE_TOOLTIP);

	GLfloat vVertices[] = {
		x - g->cx - w - 10, y - g->cy - h - 15,
		x - g->cx         , y - g->cy - h - 15,
		x - g->cx         , y - g->cy         ,
		x - g->cx - w - 10, y - g->cy         ,
	};


	glVertexAttribPointer(bd->loc_pos, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), &vVertices[0]);
	glDisableVertexAttribArray(bd->loc_color);
	glDisableVertexAttribArray(bd->loc_texture);

	glVertexAttrib4f(bd->loc_color, 1, 1, 1, 0);
	GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
	
	glVertexAttrib4f(bd->loc_color, 0, 0, 0, 0);
	glDrawArrays(GL_LINE_LOOP, 0, 4);
	
	draw_text(g, x - w - 5, y - h - 5, FONT_SIZE_TOOLTIP, text);
}


static void br_opengl_draw_section(duc_graph *g, double a1, double a2, double r1, double r2, double R, double G, double B, double L)
{
	struct opengl_backend_data *bd = g->backend_data;
	int i;

	a1 *= M_PI * 2;
	a2 *= M_PI * 2;

	int ss = (int)((a2 - a1) * r2 / 10 + 2);

	GLfloat vs_fill[ss * 2][6];
	GLfloat vs_line[ss * 2][2];

	double da = (a2-a1) / (ss-1);

	for(i=0; i<ss; i++) {
		double x1, y1, x2, y2;
		x1 = r1 * sin(a1 + da * i); y1 = -r1 * cos(a1 + da * i);
		x2 = r2 * sin(a1 + da * i); y2 = -r2 * cos(a1 + da * i);

		vs_fill[i*2 + 0][0] = x1; 
		vs_fill[i*2 + 0][1] = y1;
		vs_fill[i*2 + 0][2] = R * (g->gradient ? 0.7 : 1.0);
		vs_fill[i*2 + 0][3] = G * (g->gradient ? 0.7 : 1.0);
		vs_fill[i*2 + 0][4] = B * (g->gradient ? 0.7 : 1.0);
		vs_fill[i*2 + 0][5] = 0;
		vs_fill[i*2 + 1][0] = x2; 
		vs_fill[i*2 + 1][1] = y2;
		vs_fill[i*2 + 1][2] = R;
		vs_fill[i*2 + 1][3] = G;
		vs_fill[i*2 + 1][4] = B;
		vs_fill[i*2 + 1][5] = 0;
		
		vs_line[i][0] = x1;
		vs_line[i][1] = y1;
		vs_line[2*ss-i-1][0] = x2; 
		vs_line[2*ss-i-1][1] = y2;
	}

	glDisable(GL_TEXTURE_2D);


	if(R != 1.0 || G != 1.0 || B != 1.0) {
		glEnableVertexAttribArray(bd->loc_pos);
		glEnableVertexAttribArray(bd->loc_color);
		glVertexAttribPointer(bd->loc_pos,   2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), &vs_fill[0][0]);
		glVertexAttribPointer(bd->loc_color, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), &vs_fill[0][2]);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, ss*2);
	}

	if(L != 0.0) {
		glLineWidth(0.8);
		glDisableVertexAttribArray(bd->loc_color);
		glVertexAttrib4f(bd->loc_color, L, L, L, 0);
		glVertexAttribPointer(bd->loc_pos, 2, GL_FLOAT, GL_FALSE, 0, vs_line);
		glDrawArrays(GL_LINE_LOOP, 0, ss*2);
	}
}


static GLuint load_shader(const GLchar **txt, GLenum type)
{
	GLuint s;
	s = glCreateShader(type);
	glShaderSource(s, 1, txt, NULL);
	glCompileShader(s);

	GLint ok;
	GLchar err[512];
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

	if(!ok)
	{
		glGetShaderInfoLog(s, sizeof(err), NULL, err);
		printf("shader compile error: %s\n", err);
	}

	return s;
}


static GLuint shaders(void)
{
	/* Load and link shaders */

	GLuint vs = load_shader(&vshader, GL_VERTEX_SHADER);
	GLuint fs = load_shader(&fshader, GL_FRAGMENT_SHADER);

	GLuint sp = glCreateProgram();
	glAttachShader(sp, vs);
	glAttachShader(sp, fs);
	glLinkProgram(sp);

	GLint ok;
	GLchar err[512];
	glGetProgramiv(sp, GL_LINK_STATUS, &ok);
	if(!ok)
	{
		glGetProgramInfoLog(sp, sizeof(err), NULL, err);
		glGetShaderInfoLog(sp, 512, NULL, err);
		printf("shader link error: %s\n", err);
	}

	glDeleteShader(vs);
	glDeleteShader(fs);

	return sp;
}


void br_opengl_done(duc_graph *g)
{
}


static void br_opengl_free(duc_graph *g)
{
}



struct duc_graph_backend duc_graph_backend_opengl = {
	.start = br_opengl_start,
	.draw_text = br_opengl_draw_text,
	.draw_tooltip = br_opengl_draw_tooltip,
	.draw_section = br_opengl_draw_section,
	.done = br_opengl_done,
	.free = br_opengl_free,
};



duc_graph *duc_graph_new_opengl(duc *duc, double font_scale)
{
	duc_graph *g = duc_graph_new(duc);
	g->backend = &duc_graph_backend_opengl;

	struct opengl_backend_data *bd;
	bd = duc_malloc(sizeof *bd);
	g->backend_data = bd;
	
	bd->sp = shaders();
	bd->loc_pos     = glGetAttribLocation(bd->sp, "pos_in");
	bd->loc_texture = glGetAttribLocation(bd->sp, "tex_in");
	bd->loc_color   = glGetAttribLocation(bd->sp, "color_in");
	bd->loc_sampler = glGetUniformLocation(bd->sp, "Texture");
	bd->loc_matrix  = glGetUniformLocation(bd->sp, "matrix");
	bd->font_scale  = font_scale / STB_SOMEFONT_LINE_SPACING;

	unsigned char fontpixels[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];
	STB_SOMEFONT_CREATE(bd->fontdata, fontpixels, STB_SOMEFONT_BITMAP_HEIGHT);

	glGenTextures(1, &bd->font_texid);
	glBindTexture(GL_TEXTURE_2D, bd->font_texid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, STB_SOMEFONT_BITMAP_WIDTH, STB_SOMEFONT_BITMAP_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, fontpixels );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);

	return g;
}

#endif

/*
 * End
 */

