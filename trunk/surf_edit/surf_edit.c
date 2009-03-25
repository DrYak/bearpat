/*	surf_edit 
		useful surface edition tool

	License : GPL v2 or later 
	(see http://www.fsf.org )

	Author :
		Ivan Topolsky
	Contact :
		blackkitty@bigfoot.com

	Version :
		0.3 - 2008/01/13
		0.2 - 2006/11/07
		0.1 - 2006/08/30

	Based on :
	-	library GTS (Gnu Triandulated mesh Surface) and examples
	-	asc2stl	(previous work)
	-	additionnal bibliography for ASC and STL in corresponding 'file-???.c' source files.

	Depends on :
	-	GTS	( http://gts.sf.net )	- library needed at compile time
	-	7z	( http://p7zip.sf.net )	- must be in $PATH at run-time
	-	lzop	( http://www.lzop.org )	- must be in $PATH at run-time
	-	bzip2	( http://www.bzip.org )	- must be in $PATH at run-time
	-	gzip	( http://www.gnu.org/software/gzip/gzip.html )	- must be in $PATH at run-time

	Bugs :
	-	no interpolation implemented
	-	no self-intersecting triangles removal implemented
	-	may crash sometimes when intersecting
	-	beside "print" command, no coherent status signaling
*/

#include <math.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "gts.h"

#include "geometry.h"

static char *this_cmd;

// file operations
GtsSurface *load_surf(const char *fname);
void save_surf(const char *fname, GtsSurface *s);

GtsMatrix* load_matrix(const char *fname);

// compression parameters
int compress_param_get_level();
int compress_param_set_level(int);
int compress_param_get_default();
int compress_param_set_default(int);
int compress_param_is_default_7z ();
int compress_param_is_default_orig ();
int compress_param_set_default_7z ();
int compress_param_set_default_orig ();

// STL parameters
int stl_param_get_precision ();
int stl_param_set_precision(int);
int stl_param_get_default ();
int stl_param_set_default (int);
int stl_param_is_default_ascii ();
int stl_param_is_default_binary ();
int stl_param_set_default_ascii ();
int stl_param_set_default_binary ();

// min/max stats
void get_load_stat(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2);


static void print_usage (FILE *out) {
	fprintf(out, 
		"Usage :\n"
		"\t%s [options] {commands ...}\n"
		"\n"
		"Options :\n"
		"\t-h\tthis help\n"
		"\t-b\tSet default STL format as Binary\n"
		"\t-a\tSet default STL format as ASCII\n"
		"\t-p #\tSet decimal precision in ASCCII #\tdefault %d\n"
		"\t-7\tUse 7z optimized deflate (slower but better compression)\n"
		"\t-z\tUse original gzip and bzip2 (faster but less optimized compression)\n"
		"\t-l #\tSet compression level to #\tdefault %d\n"
		"\t\tSee man gzip, man bzip2, man lzop and man 7z for details on levels\n"
		"\t--\tforce end of options\n"
		"\n"
		"Supported surface format :\n"
		"\t*.asc\tFreeSurfer ASCII export format\n"
		"\t*.pial\tFreeSurfer internal binary format (cortical surface)\n"
		"\t*.white\tFreeSurfer internal binary format (white matter surface)\n"
		"\t\tNote: FreeSurfer are read only\n"
		"\t*.stla\tSTL ASCII format (decimal precision : %d)\n"
		"\t*.stlb\tSTL Binary format\n"
		"\t*.stl\tDefault STL is %s\n"
		"\t*.gts\tGTS ascii format\n"
		"\n"
		"Supported on-the-fly compression / decompression filters :\n"
		"\t*.zip\tpkZIP archive (default using %s)\n"
		"\t*.7z\t7-ZIP archive (using 7z)\n"
		"\t\tNote: archives are compress-only\n"
		"\t*.gz\tgzip compressed (default using %s)\n"
		"\t*.bz2\tbzip2 compressed (default using %s)\n"
		"\t*.lzo\tlzop compressed\n"
		"\t\tNote: lzop is the fastest of all when using \"-l 3 \" and still had good compression\n"
		"\t\t(recommanded default level based on lzop manual)\n"
		"\n"
		"commands :\n"
		"commands are stack based. top is position 0\n"
		"	load	$	load surface $ and place it on top of the stack\n"
		"	save	$	save the top of the stack into file $\n"
		"	box	#,#,#,#,#,#	create regular box (inside is full)\n"
		"	invbox	#,#,#,#,#,#	create inverted box (outside is full)\n"
		"	union		calculate union between top and previous\n"
		"	inter		calculate intersection between top and previous\n"
		"	diff		remove top from previous\n"
		"	freeall	#	free from position # to the end of the stack\n"
		"	free	#	remove position # from stack\n"
		"	top	#	move position # to top of the stack\n"
		"	end		quit\n"
		"	setmat #,#,#,#,#,#,#,#,#,#,#,# creates a transformation matrix\n"
		"	loadmat	$   loads a transformation matrix\n"
		"	fsmat $ $	loads pair of FreeSurfer's vox2ras (Scanner Native) and vox2ras-tkr (Volume RAS) matrices and builds a transformation matrix based on them\n"
		"	transform	applies current transformation matrix to top\n"
		"	polyline #,#,#,#,#,#,#,#,#,#,#,#,# $	creates a poly line\n"
		,
		this_cmd,
		stl_param_get_precision(),
		compress_param_get_level(),

		stl_param_get_precision (),
		stl_param_is_default_binary () ? "binary" : "ascii",

		compress_param_is_default_7z() ? "optimized 7z DEFLATE" : "original zip",
		compress_param_is_default_7z() ? "optimized 7z DEFLATE" : "original gzip",
		compress_param_is_default_7z() ? "optimized 7z DEFLATE" : "original bzip2");
}

int main (int argc, char *argv[]) {
	GPtrArray* surfaces;
	GtsMatrix* m = NULL;
	double cuty = 0.0;

	this_cmd = argv[0];

	if (!setlocale (LC_ALL, "POSIX"))
		g_warning ("cannot set locale to POSIX");

	if (argc < 3) {
		print_usage (stderr);
		exit (2);
	}

	inline void getnext_arg () {
		if ((++argv)[1] == NULL) {
			fprintf(stderr, "not enough args.\nfor help, use : \"%s -h\"\n", this_cmd);
			exit (2);
		}
	}


	do {
		if (strcmp(argv[1], "-h") == 0) {
			print_usage (stdout);
			exit(0);
		} else if (strcmp(argv[1], "-l") == 0) {
			int l = compress_param_get_level();
			getnext_arg ();
			sscanf(argv[1], "%d", &l);
			if (l < 0) l = 0;
			if (l > 9) l = 9;
			compress_param_set_level(l);
		} else if (strcmp(argv[1], "-p") == 0) {
			int p = stl_param_get_precision ();
			getnext_arg ();
			sscanf(argv[1], "%d", &p);
			if (p < 0) p = 0;
			if (p > 20) p = 20;
			stl_param_set_precision (p);
		} else if (strcmp(argv[1], "-7") == 0) {
			compress_param_set_default_7z();
		} else if (strcmp(argv[1], "-z") == 0) {
			compress_param_set_default_orig();
		} else if (strcmp(argv[1], "-a") == 0) {
			stl_param_set_default_ascii();
		} else if (strcmp(argv[1], "-b") == 0) {
			stl_param_set_default_binary();
		} else if (strcmp(argv[1], "--") == 0) {
			++argv;
			break;
		} else break;
	} while((++argv)[1] != NULL);

	if (argv[1] == NULL) {
		fprintf(stderr, "not enough args.\nfor help, use : \"%s -h\"\n", this_cmd);
		exit (2);
	}

	surfaces = g_ptr_array_new();
	inline GtsSurface *GET_SURF(int P) {
		if (P >= surfaces->len) {
			fprintf(stderr, "surface stack is empty\n");
			exit (2);
		}
		return GTS_SURFACE(surfaces->pdata[surfaces->len-P-1]);
	}
#define LAST_SURF GET_SURF(0)
#define PREV_SURF GET_SURF(1)
	do {
		if (strcmp(argv[1], "print") == 0) {
			int l  = strlen(argv[1]);
			getnext_arg ();
			if (l) { 
				fputs (argv[1], stderr);
				if (argv[1][l-1] == '\n')
					fflush(stderr); // send new lines to pipe
			}
		} else if (strcmp(argv[1], "load") == 0) {
			getnext_arg ();
			GtsSurface *s = load_surf(argv[1]);
			if (s == NULL) exit(1);
			g_ptr_array_add(surfaces, s);
		} else if (strcmp(argv[1], "save") == 0) {
			getnext_arg ();
			save_surf(argv[1], LAST_SURF);
		} else if ((strcmp(argv[1], "box") == 0) || (strcmp(argv[1], "invbox") == 0)) {
			int normal = (strcmp(argv[1], "box") == 0);
			double x1, y1, z1, x2, y2, z2, dx1, dy1, dz1, dx2, dy2, dz2;
			getnext_arg ();
			if (sscanf(argv[1], "%lf,%lf,%lf,%lf,%lf,%lf",
				&dx1, &dy1, &dz1, &dx2, &dy2, &dz2) < 6) {
				fprintf(stderr, "boxes need 6 coordinates\n");
				exit (2);
			}

			get_load_stat(&x1, &y1, &z1, &x2, &y2, &z2);
			x1 += dx1 ?: -1.0;
			y1 += dy1 ?: -1.0;
			z1 += dz1 ?: -1.0;
			x2 -= dx2 ?: -1.0;
			y2 -= dy2 ?: -1.0;
			z2 -= dz2 ?: -1.0;

			GtsSurface *box = (normal) ? 
				make_box (x1,y1,z1, x2,y2,z2) :
				make_box (x2,y2,z2, x1,y1,z1);

			if (box == NULL) exit(1);
			g_ptr_array_add(surfaces, box);
		} else if (strcmp(argv[1], "union") == 0) {
			GtsSurface *b = bool_union(PREV_SURF, LAST_SURF, "previous", "top");
			if (b == NULL) exit(1);
			g_ptr_array_add(surfaces, b);
		} else if (strcmp(argv[1], "inter") == 0) {
			GtsSurface *b = bool_inter(PREV_SURF, LAST_SURF, "previous", "top");
			if (b == NULL) exit(1);
			g_ptr_array_add(surfaces, b);
		} else if (strcmp(argv[1], "diff") == 0) {
			GtsSurface *b = bool_diff(PREV_SURF, LAST_SURF, "previous", "top");
			if (b == NULL) exit(1);
			g_ptr_array_add(surfaces, b);
		} else if (strcmp(argv[1], "freeall") == 0) {
			getnext_arg ();
			int num = atoi(argv[1]), i;
			if (num >= surfaces->len) {
				fprintf(stderr, "freeall exceeded stack\n");
				exit (2);
			}
//			for (i = 0; i < surfaces->len-num; i++)
//				gts_object_destroy (GTS_OBJECT(GET_SURF(i)));
			g_ptr_array_remove_range(surfaces,0,surfaces->len-num-1);
		} else if (strcmp(argv[1], "free") == 0) {
			getnext_arg ();
			int num = atoi(argv[1]);
			if (num >= surfaces->len) {
				fprintf(stderr, "freeall exceeded stack\n");
				exit (2);
			}
//			gts_object_destroy (GTS_OBJECT(GET_SURF(num)));
			g_ptr_array_remove_index(surfaces,surfaces->len-num-1);
		} else if (strcmp(argv[1], "top") == 0) {
			getnext_arg ();
			int num = atoi(argv[1]);
			if (num >= surfaces->len) {
				fprintf(stderr, "freeall exceeded stack\n");
				exit (2);
			}
			GtsSurface *s = GET_SURF(num);
			g_ptr_array_remove(surfaces,s);	// ..from middle of stack
			g_ptr_array_add(surfaces,s);	// ..on top of stack
		} else if (strcmp(argv[1], "loadmat") == 0) {
			if (m) gts_matrix_destroy(m);
			getnext_arg ();
			m = load_matrix(argv[1]);
		} else if (strcmp(argv[1], "setmat") == 0) {
			double	m00=0, m01=0, m02=0, m03=0,
			m10=0, m11=0, m12=0, m13=0,
			m20=0, m21=0, m22=0, m23=0;
			
			if (m) gts_matrix_destroy(m);
			
			getnext_arg ();
			if (sscanf(argv[1], "%lf,%lf,%lf,%lf,"
				"%lf,%lf,%lf,%lf,"
				"%lf,%lf,%lf,%lf",
				&m00, &m01, &m02, &m03,
				&m10, &m11, &m12, &m13,
				&m20, &m21, &m22, &m23) < 12) {
				fprintf(stderr, "matrices need 12 elements\n");
				exit (2);
			}
			
			m = gts_matrix_new(
				m00, m01, m02, m03,
				m10, m11, m12, m13,
				m20, m21, m22, m23,
				0,0,0,1);
		} else if (strcmp(argv[1], "polylines") == 0) {
			double ox=0, oy=0, oz=0, // origin
				cx=0, cy=0, cz=0, // column vector
				rx=0, ry=0, rz=0, // row vector
				sx=0, sy=0, sz=0; // slice vector
			int n = 0;
			
			if (m) gts_matrix_destroy(m);
			
			getnext_arg ();
			if (sscanf(argv[1], 
					   "%d,"
					   "%lf,%lf,%lf,"
					   "%lf,%lf,%lf,"
					   "%lf,%lf,%lf,"
					   "%lf,%lf,%lf",
					   &n, // number
					   &ox, &oy, &oz, // origin
					   // row/column/slice vector					   
					   &cx, &cy, &cz,
					   &rx, &ry, &rz,
					   &sx, &sy, &sz) < 13) {
				fprintf(stderr, "polylines need 13 elements : number, origin, column/row/slice vector\n");
				exit (2);
			}
			
			getnext_arg ();
			polylines(argv[1], LAST_SURF, n, ox,oy,oz,  cx,cy,cz,rx,ry,rz,sx,sy,sz);
		} if (strcmp(argv[1], "fsmat") == 0) {
			GtsMatrix *vtr, *tkr, *inv; 
			if (m) gts_matrix_destroy(m);
			
			getnext_arg ();
			vtr = load_matrix(argv[1]); // voxel to ras (scanner native) 
			getnext_arg ();
			tkr = load_matrix(argv[1]); // tkregister matrix (volume ras)
			inv = gts_matrix_inverse(tkr); // volume ras to volume index
			m = gts_matrix_product(vtr, inv); // volume ras to scanner native
			gts_matrix_destroy(vtr);
			gts_matrix_destroy(tkr);
			gts_matrix_destroy(inv);
		} else if (strcmp(argv[1], "transform") == 0) {
			if (! m) {
				fprintf(stderr, "no transform matrix loaded\n");
				exit (2);				
			}
				
			gts_surface_transform(LAST_SURF, m);
		} else if (strcmp(argv[1], "ed") == 0) {
			break;
		}
	} while ((++argv)[1] != NULL);


	exit (0); // and free wads of memory
}
