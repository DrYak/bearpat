// ISOC99 is used for isfinite() function
#define _ISOC99_SOURCE

#include <stdlib.h>
#include <string.h>
#include <math.h>

//#include <endian.h>#include <sys/param.h>

#ifndef __FLOAT_WORD_ORDER
#ifndef BYTE_ORDER
#warning Missing BYTE_ORDER !!!
#endif
#warning Missing __FLOAT_WORD_ORDER !!!
#define __FLOAT_WORD_ORDER 0xdeadbeef
#endif

#include "gts.h"

#define BUF_SIZE 1024
#define BIN_SIG (-2 & 0x00ffffff)

// Source about freesurfer formats :
// * freesurfer wiki
// * http://wideman-one.com/gw/brain/fs/surfacefileformats.htm

// Source of IEEE-754 loader
// * http://en.wikipedia.org/wiki/IEEE_floating-point_standard
// it's an ugly overdesigned solution, but, it does the job on whatever architecture it is thrown at
// and GCC manage to optimize it quite well

void udpate_load_stat(double x, double y, double z);
static inline scanlinef(const char *, int, const char *, ...) __attribute__ ((format(printf,1,4),format(scanf,3,4)));

static GtsSurface *_read_asc(FILE *fp, const char *fname) {
	char sname[BUF_SIZE];
	guint nvertices = 0, nfaces = 0, i;
	GtsSurface *s = NULL;
	GtsVertex **v = NULL;

	fprintf(stderr, "\r%s : Loading", fname);

	inline scanlinef(const char *name, int minimum, const char *fmt, ...) {
		char buffer[BUF_SIZE];
		va_list ap;
		int i;

		va_start(ap, fmt);
		if (fgets (buffer, BUF_SIZE, fp) == NULL) {
			vsprintf (buffer, name, ap);
			fprintf	(stderr, "Input file is not a valid FreeSurfer ASCII file\n"
				"Reached end of file file trying to read %s\n", buffer);
			exit (1);
		}

		if (i = vsscanf(buffer, fmt, ap) < minimum) { 
			vsprintf (buffer, name, ap);
			fprintf	(stderr, "\nInput file is not a valid FreeSurfer ASCII file\n"
				"cannot read %s\n", buffer);
			exit (1);
		}
		va_end(ap);
		return i;
	}

	scanlinef("header", 1, "#!ascii version of %1024s", sname);
	fprintf(stderr, "\r%s : Loading surface %s", fname, sname);

	scanlinef("number of vertices/faces", 2, "%d %d", &nvertices, &nfaces);

	if (! (v = malloc(nvertices * sizeof(GtsVertex *)))) { 
		fprintf	(stderr, "\nout of memory\n");
		exit (1);
	}

	for (i = 0; i < nvertices; i++) {
		double x, y, z;
		scanlinef("coordinates of vertex %4$d", 3, "%lf %lf %lf", &x, &y, &z, i);
		v[i] = gts_vertex_new (gts_vertex_class (), x, y, z);
		udpate_load_stat(x, y, z);
	}

	s = gts_surface_new (gts_surface_class (),
		gts_face_class (),
		gts_edge_class (),
		gts_vertex_class ());


	for (i = 0; i < nfaces; i++) {
		int v1, v2, v3;
		scanlinef("vertices index of face %4$d", 3, "%d %d %d", &v1, &v2, &v3, i);

		inline void checkindex (guint vn, char vert) {
			if (vn >= nvertices) {
				fprintf	(stderr, "\nInput file is not a valid FreeSurfer Binary Triangle file\n"
					"%s: triangle %d vertex %c index is out of range\n", fname, i, vert);
				exit (1);
			}	
		}
		checkindex (v1, '1');
		checkindex (v2, '2');
		checkindex (v3, '3');

		inline GtsEdge *find_or_create_edge(guint j, guint k) {
			// search for edge between vertex
			return	GTS_EDGE (gts_vertices_are_connected (GTS_VERTEX(v[j]), GTS_VERTEX(v[k])))
				// if first occurence, create a new edge
				?:	gts_edge_new (s->edge_class,  GTS_VERTEX(v[j]), GTS_VERTEX(v[k]));
		}

		GtsFace *t;
		gts_surface_add_face (s, t = gts_face_new (s->face_class,
			find_or_create_edge(v1,v2),
			find_or_create_edge(v2,v3),
			find_or_create_edge(v3,v1)));

		GtsVector n;	
		gts_triangle_normal (GTS_TRIANGLE(t), &n[0], &n[1], &n[2]);

		GtsVertex *vx1 = GTS_VERTEX(v[v1]), *vx2 = GTS_VERTEX(v[v2]), *vx3 = GTS_VERTEX(v[v3]);
	}
	fprintf(stderr, "\r%s : Done surface %s   \n", fname, sname);

	free(v);
	return s;
}

GtsSurface *read_asc(FILE *fp) {
	return _read_asc(fp, "(pipe)");
}

GtsSurface *load_asc(const char *fname) {
	FILE *fp = fopen(fname, "r");
	GtsSurface *s = (fp != NULL) 
		? _read_asc (fp, fname)
		: ({ 	fprintf(stderr, "Could not open input file %s\n", fname);
			perror("");
			exit(1); NULL; });

	fclose(fp);

	return s;
}





static GtsSurface *_read_fsbin(FILE *fp, const char *fname) {
	char cname[BUF_SIZE];
	guint nvertices = 0, nfaces = 0, i;
	GtsSurface *s = NULL;
	GtsVertex **v = NULL;

	// int are in Big-endian format.
	guint read_bigendian(int nb) {
		guint32 r = 0;
#if	BYTE_ORDER == BIG_ENDIAN
		fread(&n, nb, 1, fp);
#else
		while (nb--) {
			r <<= 8;
			r |= fgetc(fp);
		}
#endif
		return r;
	}

	fprintf(stderr, "\r%s : Loading", fname);

	// check signature (3 byte int)
	if ((i = read_bigendian(3)) != BIN_SIG) {
		fprintf	(stderr, "\nInput file is not a valid FreeSurfer Binary Triangle file\n"
			"signature 0x%x instead of 0x%06x not found\n", i, BIN_SIG);
		exit (1);
	}

	// 'created by' string
	{
	int c, oc;
	for (i = 0, c = fgetc(fp), oc = 0;	// in loop i = position counter, c = curcar, oc = oldcar
		c != '\n' || oc != '\n'; 	// comment is terminated by 2 consecutive \n (0x0a0a) and is NOT nul terminated, see sources on top of this file
		i++, oc = c, c = fgetc(fp)) 	// advance pointer, rotate car's and get next.
		if (c == EOF) {
			perror	("\nInput file is not a valid FreeSurfer Binary Triangle file\n"
				"cannot read comment");
			exit (1);
		} else if (i < BUF_SIZE)
			cname[i] = c;
		else if  (i == BUF_SIZE) 
			fprintf	(stderr, "- warning : comment seems abnormally wrong\n");
	}
	// appends (nul) to string, killing the second-to-last '\n'
	cname[(i > BUF_SIZE) ? BUF_SIZE-1 : i-1] = 0;

	fprintf(stderr, "\r%s : Loading surface %s", fname, cname);

	nvertices = read_bigendian(4); // EOF will be checked later
	nfaces = read_bigendian(4);

	if (! (v = malloc(nvertices * sizeof(GtsVertex *)))) { 
		fprintf	(stderr, "\nout of memory\n");
		exit (1);
	}


	for (i = 0; i < nvertices; i++) {
#if	(__FLOAT_WORD_ORDER == BIG_ENDIAN) || (BYTE_ORDER == __FLOAT_WORD_ORDER)
		gfloat x, y, z;
		inline void read_vertex_float_bigendian (gfloat *n, char axis) {
			// work if float32 is bigendian
# if	__FLOAT_WORD_ORDER == BIG_ENDIAN
			if (fread (n, sizeof (gfloat), 1, fp) != 1) {
				fprintf	(stderr, "\nInput file is not a valid FreeSurfer Binary Triangle file\n"
					"%s: missing vertex %d %c-coordinate\n", fname, i, axis);
				exit (1);
			}
# elif BYTE_ORDER == __FLOAT_WORD_ORDER 
			// only work when bytesex of float32 is same as bytesex of int32
			// SSE float and x86 int32 are both little endian			
			union {
				gfloat f;
				guint32 i;
			} t = { i: read_bigendian(4) };
			*n = t.f;
# endif 
			if (! isfinite(*n)) {
				fprintf	(stderr, "\nInput file is not a valid FreeSurfer Binary Triangle file\n"
					"%s: vertex %d %c-coordinate is not a finite number\n", fname, i, axis);
				exit (1);
			}
		}
#else
#warning Using slow float reader
		gdouble x,y,z;

		// IEEE float loader - bytesex- and IEEE compliance- independant.
		// (works whatever the subjacent floating point is system is.)

		inline void read_vertex_float_bigendian (gdouble *n, char axis) {
			int s, e;
			unsigned long src;
			long f;
			
			src = read_bigendian(4);
			
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
					"%s: vertex %d %c-coordinate is not a finite number\n", fname, i, axis);
				exit (1);
			}
		}
#endif

		read_vertex_float_bigendian (&x, 'x');
		read_vertex_float_bigendian (&y, 'y');
		read_vertex_float_bigendian (&z, 'z');
//		fprintf(stderr, "\rv[%d]%d: %f %f %f\n", i,sizeof (gfloat), x, y, z);
		v[i] = gts_vertex_new (gts_vertex_class (), x, y, z);
		udpate_load_stat(x, y, z);
	}

	s = gts_surface_new (gts_surface_class (),
		gts_face_class (),
		gts_edge_class (),
		gts_vertex_class ());


	for (i = 0; i < nfaces; i++) {
		guint v1, v2, v3;

		inline void getindex (guint *vn, char vert) {
			if ((*vn = read_bigendian(4)) >= nvertices) {
				fprintf	(stderr, "\nInput file is not a valid FreeSurfer Binary Triangle file\n"
					"%s: triangle %d vertex %c index is out of range\n", fname, i, vert);
				exit (1);
			}
		}
		getindex(&v1, '1');
		getindex(&v2, '2');
		getindex(&v3, '3');		

		inline GtsEdge *find_or_create_edge(guint j, guint k) {
			// search for edge between vertex
			return	GTS_EDGE (gts_vertices_are_connected (GTS_VERTEX(v[j]), GTS_VERTEX(v[k])))
				// if first occurence, create a new edge
				?:	gts_edge_new (s->edge_class,  GTS_VERTEX(v[j]), GTS_VERTEX(v[k]));
		}

		GtsFace *t;
		gts_surface_add_face (s, t = gts_face_new (s->face_class,
			find_or_create_edge(v1,v2),
			find_or_create_edge(v2,v3),
			find_or_create_edge(v3,v1)));

		GtsVector n;	
		gts_triangle_normal (GTS_TRIANGLE(t), &n[0], &n[1], &n[2]);

		GtsVertex *vx1 = GTS_VERTEX(v[v1]), *vx2 = GTS_VERTEX(v[v2]), *vx3 = GTS_VERTEX(v[v3]);
	}
	fprintf(stderr, "\r%s : Done surface %s   \n", fname, cname);

	free(v);
//	gts_surface_print_stats (s, stderr);
	return s;
}



GtsSurface *read_fsbin(FILE *fp) {
	return _read_fsbin(fp, "(pipe)");
}

GtsSurface *load_fsbin(const char *fname) {
	FILE *fp = fopen(fname, "r");
	GtsSurface *s = (fp != NULL) 
		? _read_fsbin (fp, fname)
		: ({ 	fprintf(stderr, "Could not open input file %s\n", fname);
			perror("");
			exit(1); NULL; });

	fclose(fp);

	return s;
}















/*

		fprintf (stdout, " facet normal %f %f %f\n  outer loop\n", n[0], n[1], n[2]);
		fprintf (stdout, "   vertex %f %f %f\n", 
			GTS_POINT (vx1)->x, GTS_POINT (vx1)->y, GTS_POINT (vx1)->z, v1);
		fprintf (stdout, "   vertex %f %f %f\n", 
			GTS_POINT (vx2)->x, GTS_POINT (vx2)->y, GTS_POINT (vx2)->z, v2);
		fprintf (stdout, "   vertex %f %f %f\n", 
			GTS_POINT (vx3)->x, GTS_POINT (vx3)->y, GTS_POINT (vx3)->z, v3);
		fputs ("  endloop\n endfacet\n", stdout);
	}
	fputs ("solid gts_surface\n", stdout);
	fputs ("endsolid\n", stdout);


	if (! (v = calloc(nvertices, sizeof(GtsVertex *)))) { 
		fprintf	(stderr, "\nout of memory\n");
		exit (1);
	}

	GtsFile *f = gts_file_new (fp);
	for (i = 0; i < nvertices; i++) {
		gts_file_first_token_after (f, '\n');  // jump to next line
		inline gdouble read_coordinate(char name) {
			gdouble r;

			if (f->type != GTS_INT && f->type != GTS_FLOAT) {
				fprintf	(stderr, 
					"\nInput file is not a valid FreeSurfer ASCII file\n"
					"%s:%d:%d: expecting a number (%c-coordinate)\n",
					fname, f->line, f->pos, name);
				exit (1);
			}	
			r = atof(f->token->str);
			gts_file_next_token (f);
			return r;
		}
	
		v[i] = gts_vertex_new (gts_vertex_class (), 
			read_coordinate('x'),
			read_coordinate('y'),
			read_coordinate('z'));
	}



		inline GtsEdge *find_or_create_edge(guint j, guint k) {
			// search for edge between vertex
			return	GTS_EDGE (gts_vertices_are_connected (v[j], v[k]))
				// if first occurence, create a new edge
				?:	gts_edge_new (s->edge_class,  v[j], v[k]);
		}

*/
