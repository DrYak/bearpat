#include <stdlib.h>
#include "gts.h"

#include "geometry.h"

/*

0----1
|\    \
| 2----3
| |    |
4 |  5 |
 \|    |
  6----7

*/

GtsSurface* make_box (gdouble x1, gdouble y1, gdouble z1, gdouble x2, gdouble y2, gdouble z2) {
	gts_allow_floating_vertices = TRUE; 
	GtsSurface *s = NULL;
#define VERTEX_LIST_LEFT_RIGHT(x1, y1, z1)	\
	gts_vertex_new (gts_vertex_class (), x1, y1, z1)
#define VERTEX_LIST_FRONT_BACK(x1, y1, z1, x2)	\
	VERTEX_LIST_LEFT_RIGHT(x1, y1, z1),	\
	VERTEX_LIST_LEFT_RIGHT(x2, y1, z1)	
#define VERTEX_LIST_TOP_BOTTOM(x1, y1, z1, x2, y2)	\
	VERTEX_LIST_FRONT_BACK(x1, y1, z1, x2),	\
	VERTEX_LIST_FRONT_BACK(x1, y2, z1, x2) 
#define VERTEX_LIST(x1, y1, z1, x2, y2, z2)	\
	VERTEX_LIST_TOP_BOTTOM(x1, y1, z1, x2, y2),	\
	VERTEX_LIST_TOP_BOTTOM(x1, y1, z2, x2, y2)
	GtsVertex *v[] = { VERTEX_LIST(x1, y1, z1, x2, y2, z2) };
#undef VERTEX_LIST
#undef VERTEX_LIST_TOP_BOTTOM
#undef VERTEX_LIST_FRONT_BACK
#undef VERTEX_LIST_LEFT_RIGHT

	inline GtsEdge *find_or_create_edge(guint j, guint k) {
		// search for edge between vertex
		return	GTS_EDGE (gts_vertices_are_connected (v[j], v[k]))
			// if first occurence, create a new edge
			?:	gts_edge_new (s->edge_class,  v[j], v[k]);
	}

	s = gts_surface_new (gts_surface_class (),
		gts_face_class (),
		gts_edge_class (),
		gts_vertex_class ());

#define ADD_TRIANGLE(v1, v2, v3)	\
	gts_surface_add_face (s, gts_face_new (s->face_class,	\
			find_or_create_edge(v1,v2),	\
			find_or_create_edge(v2,v3),	\
			find_or_create_edge(v3,v1)));	
#define ADD_QUAD(v1, v2, v3, v4)	\
	ADD_TRIANGLE(v1, v2, v3)	\
	ADD_TRIANGLE(v1, v3, v4)
				
	ADD_QUAD(0,2,3,1) // TOP face on drawing
	ADD_QUAD(1,3,7,5) // RIGHT face on drawing
	ADD_QUAD(0,4,6,2) // LEFT face on drawing
	ADD_QUAD(0,1,5,4) // BACK face on drawing
	ADD_QUAD(2,6,7,3) // FRONT face on drawing
	ADD_QUAD(4,5,7,6) // BOTTOM face on drawing
	gts_allow_floating_vertices = FALSE; 
#undef ADD_QUAD
#undef ADD_TRIANGLE

//	gts_surface_print_stats (s, stderr);
	return s;
}




GtsSurface* make_quad (gdouble x, gdouble y, gdouble z, gdouble sz, gdouble vx1, gdouble vy1, gdouble vz1, gdouble vx2, gdouble vy2, gdouble vz2) {
	gts_allow_floating_vertices = TRUE; 
	GtsSurface *s = NULL;

	GtsVertex *v[] = {
		gts_vertex_new (gts_vertex_class (), x, y, z),
		gts_vertex_new (gts_vertex_class (), x+sz*vx1, y+sz*vy1, z+sz*vz1),
		gts_vertex_new (gts_vertex_class (), x+sz*vx1+sz*vx2, y+sz*vy1+sz*vy2, z+sz*vz1+sz*vz2),
		gts_vertex_new (gts_vertex_class (), x+sz*vx1, y+sz*vy1, z+sz*vz2) };
	
	inline GtsEdge *find_or_create_edge(guint j, guint k) {
		// search for edge between vertex
		return	GTS_EDGE (gts_vertices_are_connected (v[j], v[k]))
		// if first occurence, create a new edge
		?:	gts_edge_new (s->edge_class,  v[j], v[k]);
	}
	
	s = gts_surface_new (gts_surface_class (),
						 gts_face_class (),
						 gts_edge_class (),
						 gts_vertex_class ());
	
#define ADD_TRIANGLE(v1, v2, v3)	\
gts_surface_add_face (s, gts_face_new (s->face_class,	\
find_or_create_edge(v1,v2),	\
find_or_create_edge(v2,v3),	\
find_or_create_edge(v3,v1)));	
#define ADD_QUAD(v1, v2, v3, v4)	\
ADD_TRIANGLE(v1, v2, v3)	\
ADD_TRIANGLE(v1, v3, v4)
	
	ADD_QUAD(0,1,2,3) 
	gts_allow_floating_vertices = FALSE; 
#undef ADD_QUAD
#undef ADD_TRIANGLE
	
	//	gts_surface_print_stats (s, stderr);
	return s;
}





GtsSurface *bool_op (enum BOOL_OP op, GtsSurface *outside, GtsSurface *inside, const char *n1, const char *n2) {
	GtsSurface *result;
	GtsSurfaceInter *inter;
	GNode *outside_tree, *inside_tree;

	gboolean outside_is_open, inside_is_open, inter_is_closed = TRUE;
	fprintf(stderr, "\rCSG: prepare inside");
	/* build bounding box tree for first surface */
	inside_tree = gts_bb_tree_surface (inside);
	inside_is_open = gts_surface_volume (inside) < 0. ? TRUE : FALSE;

	fprintf(stderr, "\rCSG: prepare outside");
	/* build bounding box tree for second surface */
	outside_tree = gts_bb_tree_surface (outside);
	outside_is_open = gts_surface_volume (outside) < 0. ? TRUE : FALSE;

	fprintf(stderr, "\rCSG: intersecting   ");
	inter =	gts_surface_inter_new (gts_surface_inter_class (),
		outside, inside, outside_tree, inside_tree, outside_is_open, inside_is_open);
	gts_surface_inter_check (inter, &inter_is_closed);

	if (! inter_is_closed) {
		fprintf	(stderr,
			"the intersection of `%s' and `%s' is not a closed curve\n",
			n1, n2);
		exit(1);
	}

	result = gts_surface_new (gts_surface_class (),
			gts_face_class (),
			gts_edge_class (),
			gts_vertex_class ());

	switch (op) {
		case BOOL_UNION:
			fprintf(stderr, "\rCSG: union          ");
			gts_surface_inter_boolean (inter, result, GTS_1_OUT_2);
			gts_surface_inter_boolean (inter, result, GTS_2_OUT_1);
		case BOOL_INTER:
			fprintf(stderr, "\rCSG: intersection   ");
			gts_surface_inter_boolean (inter, result, GTS_1_IN_2);
			gts_surface_inter_boolean (inter, result, GTS_2_IN_1);
			break;
		case BOOL_DIFF:
			fprintf(stderr, "\rCSG: difference     ");
			gts_surface_inter_boolean (inter, result, GTS_1_OUT_2);
			gts_surface_inter_boolean (inter, result, GTS_2_IN_1);
			gts_surface_foreach_face (inter->s2, (GtsFunc) gts_triangle_revert, NULL);
			gts_surface_foreach_face (inside, (GtsFunc) gts_triangle_revert, NULL);
			break;
		default:
			fprintf(stderr, "\rBool(%d,%s,%s) : wrong op ", op, n1, n2);
			exit(1);
			break;
	}

	fprintf(stderr, "\rCSG: check          ");
	if (gts_surface_is_self_intersecting (result)) 
		fprintf (stderr, "\rthe resulting surface is self-intersecting\n");

	fprintf(stderr, "\rCSG: done           \n");
	return result;
}

GtsSurface *bool_union (GtsSurface *s1, GtsSurface *s2, const char *n1, const char *n2) {
	return bool_op(BOOL_UNION, s1, s2, n1, n2);
}

GtsSurface *bool_inter (GtsSurface *s1, GtsSurface *s2, const char *n1, const char *n2) {
	return bool_op(BOOL_INTER, s1, s2, n1, n2);
}

GtsSurface *bool_diff (GtsSurface *s1, GtsSurface *s2, const char *n1, const char *n2) {
	return bool_op(BOOL_DIFF, s1, s2, n1, n2);
}







void gts_surface_transform (GtsSurface *s, GtsMatrix *m) {
	void transform_vertex (GtsVertex *v) {
		gts_point_transform(GTS_POINT(v), m);
	}

	gts_surface_foreach_vertex (s, (GtsFunc) transform_vertex,  NULL);
}





void polylines(char *fname, GtsSurface* s, int n,  double ox, double oy, double oz,   double cx, double cy, double cz, double rx, double ry, double rz, double sx, double sy, double sz) {
	int i;
	fprintf(stderr,"polyline : tree\n");
	GNode* s_tree = gts_bb_tree_surface(s);
	GNode* p_tree;
	GtsSurface* plane;
	GSList* curve;
	
	for (i = 0; i < n / 2; i++) {
		// moves to next slice
		ox += sx;
		oy += sy;
		oz += sz;
	}
	
	fprintf(stderr,"polyline : slice %d\n", i);
	plane = make_quad(ox,oy,oz, 1024, cx,cy,cz, rx,ry,rz);
	p_tree = gts_bb_tree_surface(plane);
	curve = gts_surface_intersection(s,plane,s_tree,p_tree);

	void display(GtsEdge* e) {
		fprintf(stdout, "%f,%f,%f - %f,%f,%f\n",
			GTS_POINT(GTS_SEGMENT(e)->v1)->x,
			GTS_POINT(GTS_SEGMENT(e)->v1)->y,
			GTS_POINT(GTS_SEGMENT(e)->v1)->z,

			GTS_POINT(GTS_SEGMENT(e)->v2)->x,
			GTS_POINT(GTS_SEGMENT(e)->v2)->y,
			GTS_POINT(GTS_SEGMENT(e)->v2)->z);
	}
	
	g_slist_foreach(curve, (GFunc) display, NULL);
}
