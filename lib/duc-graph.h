
#ifndef duc_graph_h
#define duc_graph_h

#include <cairo.h>
#include <duc.h>

/* 
 * Graph drawing
 */

typedef struct duc_graph duc_graph;

duc_graph *duc_graph_new(duc *duc);
void duc_graph_free(duc_graph *g);

void duc_graph_set_max_level(duc_graph *g, int max_level);
void duc_graph_set_size(duc_graph *g, int size);

int duc_graph_draw_file(duc_graph *g, duc_dir *dir, FILE *fout);
int duc_graph_draw_cairo(duc_graph *g, duc_dir *dir, cairo_t *cr);
duc_dir *duc_graph_find_spot(duc_graph *g, duc_dir *dir, int x, int y);

#endif
