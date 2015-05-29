// http://stackoverflow.com/questions/6377934/drawing-a-circle-with-a-sector-cut-out-in-opengl-es-1-1

 #include "config.h"

#ifdef ENABLE_GRAPH

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

#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>

#include "private.h"
#include "duc.h"
#include "duc-graph.h"
#include "graph-private.h"
#include "utlist.h"

struct opengl_backend_data {
	GLuint sp;
};


static const GLchar *vshader = 
	"#version 330 core\n"
	"layout (location = 0) in vec4 pos;\n"
	"uniform mat4 mat;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = mat * pos;\n"
	"}";

static const GLchar *fshader = 
	"#version 330 core\n"
	"uniform vec4 color = { 0.1, 0.5, 0.5, 1.0 } ;\n"
	"out vec4 outV;\n"
	"void main()\n"
	"{\n"
	"	outV = color;\n"
	"}\n";


void br_opengl_start(duc_graph *g)
{
	struct opengl_backend_data *bd = g->backend_data;
	GLuint sp = bd->sp;

	glUseProgram(sp);

	GLint mat = glGetUniformLocation(sp, "mat");

	printf("%f %f\n", g->width, g->height);

	GLfloat matrix[] = { 
		1 / g->width*2, 0, 0, 0,
		0, 1/g->height*2, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1 
	};

	glUniformMatrix4fv(mat, 1, 0, matrix);

	glClearColor(1, 1, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


static void br_opengl_draw_text(duc_graph *g, int x, int y, int size, char *text)
{
}


static void br_opengl_draw_tooltip(duc_graph *g, int x, int y, char *text)
{
}


static void br_opengl_draw_section(duc_graph *g, double a1, double a2, double r1, double r2, double H, double S, double V, double line)
{
	struct opengl_backend_data *bd = g->backend_data;
	GLuint sp = bd->sp;
	int i;

	double R, G, B;
	hsv2rgb(H, S, V, &R, &G, &B);

	GLint c = glGetUniformLocation(sp, "color");

	a1 *= M_PI * 2;
	a2 *= M_PI * 2;

	int ss = (a2 - a1) * 10 + 2;

	GLfloat vs_fill[ss * 2][2];
	GLfloat vs_line[ss * 2][2];

	double da = (a2-a1) / (ss-1);

	for(i=0; i<ss; i++) {
		double x1, y1, x2, y2;
		x1 = r1 * sin(a1 + da * i); y1 = r1 * cos(a1 + da * i);
		x2 = r2 * sin(a1 + da * i); y2 = r2 * cos(a1 + da * i);

		vs_fill[i*2 + 0][0] = x1; vs_fill[i*2 + 0][1] = y1;
		vs_fill[i*2 + 1][0] = x2; vs_fill[i*2 + 1][1] = y2;
		
		vs_line[i][0] = x1; vs_line[i][1] = y1;
		vs_line[2*ss-i-1][0] = x2; vs_line[2*ss-i-1][1] = y2;
	}

	glLineWidth(1);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vs_line);
	glUniform4f(c, 0, 0, 0, 1.0);
	glDrawArrays(GL_LINE_LOOP, 0, ss*2);
	
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vs_fill);
	glUniform4f(c, R, G, B, 1.0);
	glDrawArrays(GL_QUAD_STRIP, 0, ss*2);
}


static GLuint load_shader(const GLchar * const *txt, GLenum type)
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


duc_graph *duc_graph_new_opengl(duc *duc)
{
	duc_graph *g = duc_graph_new(duc);
	g->backend = &duc_graph_backend_opengl;

	struct opengl_backend_data *bd;
	bd = duc_malloc(sizeof *bd);
	g->backend_data = bd;
	
	bd->sp = shaders();

	return g;
}

#endif

/*
 * End
 */

