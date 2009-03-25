// ISOC99 is used for isfinite() function
#define _ISOC99_SOURCE

#include <stdlib.h>
#include <string.h>
#include <math.h>

//#include <endian.h>#include <sys/param.h>

#include "gts.h"

#ifndef __FLOAT_WORD_ORDER
#ifndef BYTE_ORDER
#warning Missing BYTE_ORDER !!!
#endif
#warning Missing __FLOAT_WORD_ORDER !!!
#define __FLOAT_WORD_ORDER 0xdeadbeef
#endif


// modified from GTS tools "stl2gts" and "gts2stl"

// sources for STL formats :
// * http://en.wikipedia.org/wiki/STL_%28file_format%29
// * http://www.sdsc.edu/tmf/Stl-specs/stl.html
// * http://www.csit.fsu.edu/~burkardt/data/stla/stla.html
// * http://rpdrc.ic.polyu.edu.hk/old_files/stl_ascii_format.htm
// * http://rpdrc.ic.polyu.edu.hk/old_files/stl_binary_format.htm

// STL checking :
// * http://www.sdsc.edu/tmf/Whitepaper/whitepaper.html

// Source of IEEE-754 loader
// * http://en.wikipedia.org/wiki/IEEE_floating-point_standard
// it's an ugly overdesigned solution, but, it does the job on whatever architecture it is thrown at
// and GCC manage to optimize it quite well. Even with it, the vertex merging is still the slowest part.

void udpate_load_stat(double x, double y, double z);

#define STLB_HEAD_SIZE 80

static enum STL_DEFAULT {
	STL_ASCII = 1,
	STL_BINARY = 0,
} stl_default = STL_BINARY;
static int stl_fmt_precision = 10;

/*********************
*                    *
*   LOAD STL ASCII   *
*                    *
*********************/


static inline GPtrArray *read_stl_a(GtsFile *f, const char *fname) {
	GPtrArray * a = g_ptr_array_new ();

	while (f->type == GTS_STRING) {
		// helper macro...
		inline gdouble read_coordinate(char name) {
			gts_file_next_token (f);
			if (f->type != GTS_INT && f->type != GTS_FLOAT) {
				fprintf	(stderr, 
					"Input file is not a valid STL file\n"
					"%s:%d:%d: expecting a number (%c-coordinate)\n",
					fname, f->line, f->pos, name);
				exit (1);
			}	
			return atof (f->token->str);
		}

		if (!strcmp (f->token->str, "vertex")) { // keyword = "vertex" - tip of triangle
			gdouble	x = read_coordinate('x'),
				y = read_coordinate('y'),
				z = read_coordinate('z');

			g_ptr_array_add (a, gts_vertex_new (gts_vertex_class (), x, y, z));
			udpate_load_stat(x, y, z);
		} else if (!strcmp (f->token->str, "endsolid")) // keyword = "endsolid" - end of file
		break;
		// all other keywords "facet", "normal", "outer loop", "endloop", "endfacet" and extensions like "color" are ignored
		// we can do so because, because STL format defines facets strictly as triangle 
		// and therefor we can treat STL as a list of groups of 3 vertices
		gts_file_first_token_after (f, '\n');  // jump to next line
	}

	return a;
}


/**********************
*                     *
*   LOAD STL BINARY   *
*                     *
**********************/


static inline GPtrArray *read_stl_b(FILE *fp, const char *fname) {
	GPtrArray * a = g_ptr_array_new ();
	guint nf, i;
	gchar header[STLB_HEAD_SIZE];

	if (fread (header, sizeof (gchar), STLB_HEAD_SIZE, fp) != STLB_HEAD_SIZE) {
		fprintf	(stderr, "Input file is not a valid STL file\n"
			"%s: incomplete header\n", fname);
		exit (1);
	}

	inline guint get_int_littleendian() {
#if	BYTE_ORDER == LITTLE_ENDIAN
		guint32 r;
		return (fread (&r, sizeof (guint32), 1, fp) == 1) ? r : EOF;
#else
		guint r = 0, nb = 4, s = 0, c;
		while (nb--) { 
			if ((c = getc(fp)) == EOF) return c;
			r |= c << s;
			s += 8;
		}
		return r;
#endif
	}

	if ((nf = get_int_littleendian()) == EOF) {
		fprintf	(stderr, "Input file is not a valid STL file\n"
			"%s: incomplete header\n", fname);
		exit (1);
	}

	for (i = nf; i > 0; i--) {
		gfloat x, y, z;

		// helper inline func
		inline void read_vertex(char *name) {
			inline void read_vertex_float (gfloat *n, char axis) {

#if	(__FLOAT_WORD_ORDER == LITTLE_ENDIAN) || (BYTE_ORDER == __FLOAT_WORD_ORDER)
# if	__FLOAT_WORD_ORDER == LITTLE_ENDIAN
				if (fread (n, sizeof (gfloat), 1, fp) != 1) {
					fprintf	(stderr, "Input file is not a valid STL file\n"
						"%s: missing face %d %s %c-coordinate\n", fname, i, name, axis);
					exit (1);
				}
# elif	BYTE_ORDER == __FLOAT_WORD_ORDER
				union {
					gfloat f;
					guint32 i;
				} t = { i: get_int_littleendian() };
				if (t.i == EOF) {
					fprintf	(stderr, "Input file is not a valid STL file\n"
						"%s: missing face %d %s %c-coordinate\n", fname, i, name, axis);
					exit (1);
				}
				*n = t.f;
# endif
				if (! isfinite(*n)) {
					fprintf	(stderr, "\nInput file is not a valid STL file\n"
						"%s: face %d %s %c-coordinate is not a finite number\n", fname, i, name, axis);
					exit (1);
				}
#else
#warning using software float conversion
				int s, e;
				unsigned long src;
				long f;
				
				src = get_int_littleendian();
				
				s = (src & 0x80000000UL) >> 31;
				e = (src & 0x7F800000UL) >> 23;
				f = (src & 0x007FFFFFUL);
				
				if (e > 0 && e < 255) {
					// Normal number 
					f += 0x00800000UL;
					if (s) f = -f;
					*n = ldexp(f, e - 127 - 23);
				} else if (e == 0 && f != 0) {
					// Denormal number 
					if (s) f = -f;
					*n = ldexp(f, -126 - 23);
				} else if (e == 0 && f == 0 && s == 1)
					// Negative zero 
					*n = -0.0;
				else if (e == 0 && f == 0 && s == 0)
					// Positive zero 
					*n = +0.0;
				else {
					//	e == 255 && f != 0		NaN - Not a number 
					//	e == 255 && f == 0 && s == 1	Negative infinity 
					//	e == 255 && f == 0 && s == 0	Positive infinity 
					fprintf	(stderr, "\nInput file is not a valid FreeSurfer Binary Triangle file\n"
						"%s: face %d %s %c-coordinate is not a finite number\n", fname, i, name, axis);
					exit (1);
				}	
#endif
			}

		
			read_vertex_float (&x, 'x');
			read_vertex_float (&y, 'y');
			read_vertex_float (&z, 'z');
		}

		guint j;
		guint16 attbytecount;

		read_vertex("normal"); // skip normal

		for (j = 0; j < 3; j++) {
			read_vertex("vertex"); // use triangle tip
			g_ptr_array_add (a, gts_vertex_new (gts_vertex_class (), x, y, z));
			udpate_load_stat(x, y, z);
		}

		// skip attribute
		if (fread (&attbytecount, sizeof (guint16), 1, fp) != 1) {
			fprintf	(stderr, "Input file is not a valid STL file\n"
			"%s: incomplete poly info\n", fname);
			exit (1);
		}
	}
	return a;
}


/*********************
*                    *
*   CONVERT TO GTS   *
*                    *
*********************/

static inline void vertices_merge (GPtrArray * stl, gdouble epsilon) {
	GPtrArray * array;
	GNode * kdtree;
	guint i;

	// load vertex in a kdtree
	array = g_ptr_array_new ();
	for (i = 0; i < stl->len; i++)
		g_ptr_array_add (array, stl->pdata[i]);
	kdtree = gts_kdtree_new (array, NULL);
	g_ptr_array_free (array, TRUE);


	// for each still active vertex,
	// a bounding box will be made,
	// and all vertices within the box (ie.: sharing same coordinates within 'espilon' limit)
	// will be merged 
	// (first pass marks additionnal shared vertices by putting reference to first occurence in 'reserved' attribute)
	// (second pass removes additionnal shared vertices and maps them to first occurence)

	for (i = 0; i < stl->len; i++) {
		GtsVertex * v = stl->pdata[i];

		if (!GTS_OBJECT (v)->reserved) { // Do something only if v is active 
			GtsBBox * bbox;
			GSList * selected, * j;

			// build bounding box 
			bbox = gts_bbox_new (gts_bbox_class (),
				v, 
				GTS_POINT (v)->x - epsilon,
				GTS_POINT (v)->y - epsilon,
				GTS_POINT (v)->z - epsilon,
				GTS_POINT (v)->x + epsilon,
				GTS_POINT (v)->y + epsilon,
				GTS_POINT (v)->z + epsilon);

			// select vertices which are inside bbox using kdtree 
			for (j = selected = gts_kdtree_range (kdtree, bbox, NULL); j; j = j->next) {
				GtsVertex * sv = j->data;

				// additionnal occurence (sv other than v) will be marked
				// (their 'reserved' will point to v.
				if (sv != v && !GTS_OBJECT (sv)->reserved)
					GTS_OBJECT (sv)->reserved = v; // mark sv as inactive 
				
			}
			g_slist_free (selected);
			gts_object_destroy (GTS_OBJECT (bbox));
		}
	}

	gts_kdtree_destroy (kdtree);

	// destroy inactive vertices 

	// we want to control vertex destruction 
	gts_allow_floating_vertices = TRUE;

	for (i = 0; i < stl->len; i++) {
		GtsVertex * v = stl->pdata[i];

		// 'reserved' set : not first occurence.
		// destroy vertex and replace with first occurence
		if (GTS_OBJECT (v)->reserved) { // v is inactive 
			stl->pdata[i] = GTS_OBJECT (v)->reserved;
			gts_object_destroy (GTS_OBJECT (v));
		}
	}

	gts_allow_floating_vertices = FALSE; 
}

static inline void add_stl (GtsSurface * s, GPtrArray * stl) {
	guint i;
	// because of the STL, format we treat the vertex list as series of 3 vertices.
	for (i = 0; i < stl->len/3; i++) {
		GtsEdge *e1, *e2, *e3;
		GtsFace *f;
		inline GtsEdge *find_or_create_edge(guint j, guint k) {
			// same vertex serves for 2 points ?
			return	 (stl->pdata[j] == stl->pdata[k]) ? ({
					// the file has been destroyed by rounding during save
					// and vertices have been accidentaly merged
					// the file cannot be repared, but the surface is still usable
					fprintf(stderr, "\rrounding error : face %d has two tips in the same point <%g,%g,%g> (file is b0rked but usable)\n",
						i, GTS_POINT (stl->pdata[j])->x, GTS_POINT (stl->pdata[j])->y, GTS_POINT (stl->pdata[j])->z);
					// pass a null, that will be picked by CHECK_FOR_NULL
					// to avoid attempting to create degenerate face  with two duplicate and one zero-lenght edges
					NULL;
					// note that the STL ASCII format is the only one susceptible to this kind of damage
				}) : (
					// search for edge between vertex
					GTS_EDGE (gts_vertices_are_connected (stl->pdata[j], stl->pdata[k]))
						// if first occurence, create a new edge
						?:	gts_edge_new (s->edge_class,  stl->pdata[j], stl->pdata[k])
				);
		}

// This NULL trick is necessary because we can't 'continue' out of a nested inline
// And because I prefer using nested inline instead of #define macros for multi-line segment
#define CHECK_FOR_NULL(e) ((e) ?: ({ continue; NULL; }))
		gts_surface_add_face (s, gts_face_new (s->face_class, 
			CHECK_FOR_NULL(find_or_create_edge(3*i,   3*i+1)),
			CHECK_FOR_NULL(find_or_create_edge(3*i+1, 3*i+2)),
			CHECK_FOR_NULL(find_or_create_edge(3*i+2, 3*i))));
#undef CHECK_FOR_NULL
	}
}

static GtsSurface *read_stl_detect (FILE *fp, const char *fname) {
	GPtrArray *a = NULL;
	GtsSurface *s = NULL;
	char tag[6]; // used for format detection

	fprintf(stderr, "\r%s : Loading", fname);

	// ASCII begins with keyword "solid", 
	// binary begins with 80 char comment field
	fgets (tag, 6, fp);
	rewind (fp);
	if (!strcmp (tag, "solid")) { /* ASCII file */
		fprintf(stderr, "\r%s : Loading ASCII", fname);
		GtsFile *f = gts_file_new (fp);
		a = read_stl_a(f, fname);
		gts_file_destroy (f);
	} else { /* binary file */
		fprintf(stderr, "\r%s : Loading Binary", fname);
		a = read_stl_b(fp, fname);
	}

	// STL format definition requires triangle as facets, 
	// so number of vertex MUST be multiple of 3
	if ((a->len % 3) != 0) { 
		fprintf	(stderr, "Input file is not a valid STL file\n"
		"%s: number of vertices (%d) does not divide by 3 - non triangle facets ?\n", fname, a->len);
		exit (1);
	}

	// merge all vertices that share exact same coordinates (diff = 0.0)
	fprintf(stderr, "\r%s : Merging vertices", fname);
	vertices_merge (a, (gdouble) 0.);

	// create triangles in surface from vertex list
	fprintf(stderr, "\r%s : Importing surface", fname);
	s = gts_surface_new (gts_surface_class (),
		gts_face_class (),
		gts_edge_class (),
		gts_vertex_class ());
	add_stl (s, a);

	g_ptr_array_free(a, FALSE); // free array but keep vertexes

	fprintf(stderr, "\r%s : Done             \n", fname);
//	gts_surface_print_stats (s, stderr);

	return s;
}

GtsSurface * read_stl (FILE *fp) {
	return read_stl_detect(fp, "(pipe)");
}

GtsSurface *load_stl (const char *fname) {
	FILE *fp = fopen(fname, "r");
	GtsSurface *s = (fp != NULL) 
		? read_stl_detect(fp, fname)
		: ({ 	fprintf(stderr, "Could not open input file %s\n", fname);
			perror("");
			exit(1); NULL; });
	fclose(fp);
	return s;
}




/*********************
*                    *
*   SAVE STL ASCII   *
*                    *
*********************/

static void _write_stla (const char *fname, FILE *fp, GtsSurface *s) {
	void write_face (GtsTriangle * t) {
		GtsVertex * v1, * v2, * v3;
		GtsVector n;
		
		gts_triangle_vertices (t, &v1, &v2, &v3);
		gts_triangle_normal (t, &n[0], &n[1], &n[2]);
		gts_vector_normalize (n);
		fprintf (fp, " facet normal %.*g %.*1$g %.*1$g\n  outer loop\n", stl_fmt_precision, n[0], n[1], n[2]);
		fprintf (fp, "   vertex %.*g %.*1$g %.*1$g\n", stl_fmt_precision, 
			GTS_POINT (v1)->x, GTS_POINT (v1)->y, GTS_POINT (v1)->z);
		fprintf (fp, "   vertex %.*g %.*1$g %.*1$g\n", stl_fmt_precision, 
			GTS_POINT (v2)->x, GTS_POINT (v2)->y, GTS_POINT (v2)->z);
		fprintf (fp, "   vertex %.*g %.*1$g %.*1$g\n", stl_fmt_precision, 
			GTS_POINT (v3)->x, GTS_POINT (v3)->y, GTS_POINT (v3)->z);
		fputs ("  endloop\n endfacet\n", fp);
	}

	fprintf (fp, "solid %s\n", ((fname == NULL) || (! strcmp(fname, "(pipe)"))) ? "gts_surface" : fname);
	gts_surface_foreach_face (s, (GtsFunc) write_face, NULL);
	fputs ("endsolid\n", fp);
}


void write_stla (FILE *fp, GtsSurface *s) {
	_write_stla("(pipe)", fp, s);
}


void save_stla (const char *fname, GtsSurface *s) {
	FILE *fp=fopen(fname, "w");
	if (! fp) {
		fprintf(stderr, "Could not open output file %s\n", fname);
		perror("");
		exit (1);
	}

	_write_stla (fname, fp, s);
	fclose(fp);
}


/**********************
*                     *
*   SAVE STL BINARY   *
*                     *
**********************/

static void _write_stlb (const char *fname, FILE *fp, GtsSurface *s) {
	char header[STLB_HEAD_SIZE];
	const char *fmt = "BEAR-PAT - %-*s";

	memset(header, 0, STLB_HEAD_SIZE);
	sprintf(header, fmt, STLB_HEAD_SIZE - strlen(fmt), ((fname == NULL) || (! strcmp(fname, "(pipe)"))) ? "gts_surface" : fname);
	fwrite(header, sizeof(char), STLB_HEAD_SIZE, fp);

#if	BYTE_ORDER == LITTLE_ENDIAN
	inline void put_int_littleendian(guint32 i) {
		fwrite(&i, sizeof(guint32), 1, fp);
	}
#else
#warning using integer endian conversion
	inline void put_int_littleendian(guint i) {
		guint nb;
		for (nb = 4; nb--; i >>= 8)
			putc(i & 0xff, fp);
	}
#endif

	// number of faces in a 32bit int
	put_int_littleendian(gts_surface_face_number(s));



	void write_face (GtsTriangle * t) {
		GtsVertex * v1, * v2, * v3;
		GtsVector n;
		const guint16 xtra = 0;

		inline void write_vertex (gfloat x, gfloat y, gfloat z) {
			inline void write_vertex_float (gfloat n) {
#if	(__FLOAT_WORD_ORDER == LITTLE_ENDIAN) || (BYTE_ORDER == __FLOAT_WORD_ORDER)
# if	__FLOAT_WORD_ORDER == LITTLE_ENDIAN
				fwrite(&n, sizeof (gfloat), 1, fp);
# elif	BYTE_ORDER == FLOAT_WORD_ORDER
				union {
					gfloat f;
					guint32 i;
				} t = { f: n };
				put_int_littleendian(t.i);
# endif
#else
#warning using software float conversion
				signed int e = 0;
				double f;
				unsigned long i, s = 0;
				if (! isfinite(n)) {
					fprintf	(stderr, "\nInvalid coordinate found (is not a finite number)\n");
					exit (1);
				}
				s = signbit (n);
			
				if (n == 0.0) {
					put_int_littleendian (s << 31UL);
					return;
				}
	
				s = signbit (n);
				f = frexp(fabs(n), &e);
	
				if (e > 254 - 127 + 1 ) {
					fprintf	(stderr, "\nInvalid coordinate found (exp overflow)\n");
					exit (1);
				} else	if (e == -127 + 1) {
					// fractionnal part, denormalized, exp is exactly the minimum
					i = ((unsigned int) ldexp(f, 23)) & 0x007FFFFFUL; // if ldexp is a built-in
					// exp part is 0
				} else if  (e < -127 + 1) {
					// fractionnal part, denormalized
					i = ((unsigned int) ldexp(f, 23 + (e + 127 - 1))) & 0x007FFFFFUL;
					// exp part is 0
				} else {
					// fractionnal part, normalized to 1,xxx (hence shift 24 instead of shift 23)
					i = ((unsigned int) ldexp(f, 24)) & 0x007FFFFFUL; // if ldexp is a built-in
//				i = ((unsigned int) ((double) f * 0x1.0p24)) & 0x007FFFFFUL;
//				i = ((unsigned int) (f * 0x01000000UL) )& 0x007FFFFFUL;
				
					// exp part
					i |= ((unsigned int) (e + 127 - 1) << 23UL) & 0x7F800000UL;
				}
				if (s) i |= 1 << 31L;
				put_int_littleendian(i);
#endif
			}

			write_vertex_float (x);
			write_vertex_float (y);
			write_vertex_float (z);
		}
		
		gts_triangle_vertices (t, &v1, &v2, &v3);
		gts_triangle_normal (t, &n[0], &n[1], &n[2]);
		gts_vector_normalize (n);

		// normal vector
		write_vertex (n[0], n[1], n[2]);
		// triangle tip
		write_vertex (GTS_POINT (v1)->x, GTS_POINT (v1)->y, GTS_POINT (v1)->z);
		write_vertex (GTS_POINT (v2)->x, GTS_POINT (v2)->y, GTS_POINT (v2)->z);
		write_vertex (GTS_POINT (v3)->x, GTS_POINT (v3)->y, GTS_POINT (v3)->z);

		// extra 16 bit :
		// - pad in older versions
		// - color in newer versions
		fwrite(&xtra, sizeof(guint16), 1, fp);
 
	}

	gts_surface_foreach_face (s, (GtsFunc) write_face, NULL);
}

void write_stlb (FILE *fp, GtsSurface *s) {
	_write_stlb("(pipe)", fp, s);
}

void save_stlb (const char *fname, GtsSurface *s) {
	FILE *fp=fopen(fname, "w");
	if (! fp) {
		fprintf(stderr, "Could not open output file %s\n", fname);
		perror("");
		exit (1);
	}

	_write_stlb (fname, fp, s);
	fclose(fp);
}



void write_stl (FILE *fp, GtsSurface *s) {
	if (stl_default == STL_BINARY)
		_write_stlb ("(pipe)", fp, s);
	else
		_write_stla ("(pipe)", fp, s);
}

void save_stl (const char *fname, GtsSurface *s) {
	if (stl_default == STL_BINARY)
		save_stlb (fname, s);
	else
		save_stla (fname, s);
}


/*********************************************
* Functions to set parameters for STL files *
*********************************************/

int stl_param_get_precision() {
	return stl_fmt_precision;
}

int stl_param_set_precision(int precision) {
	stl_fmt_precision = precision;
	return stl_fmt_precision;
}

int stl_param_get_default () {
	return stl_default;
}

int stl_param_set_default (enum STL_DEFAULT new_default) {
	switch (new_default) {
		case STL_ASCII:
		case STL_BINARY:
			stl_default = new_default;
			break;
		default:
			fprintf(stderr, "stl_param_set_default (%d) - bad parameter\n", new_default);
			break;
	}
			
	return stl_default;
}

int stl_param_is_default_ascii () {
	return stl_default == STL_ASCII;
}

int stl_param_is_default_binary () {
	return stl_default == STL_BINARY;
}

int stl_param_set_default_ascii () {
	return stl_default= STL_ASCII;
}

int stl_param_set_default_binary () {
	return stl_default = STL_BINARY;
}

