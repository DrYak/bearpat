#include <stdlib.h>
#include <strings.h>
#include <gts.h>

#ifdef __APPLE__
char *strndup(const char *orig, size_t len) {
    const size_t orig_len = (orig == NULL) ? 0: strlen(orig);
    const size_t new_len = MIN(orig_len, len);
    char *out = malloc(new_len+1);
	
    if (out == NULL)
        return NULL;
	
    out[new_len] = '\0';
    return memcpy(out, orig, new_len);
}
#endif

// SHOULD MOVE TO :
// - g_spawn_async_with_pipes ()  - better at chaining pipes.

static double load_stat_min_x = 200.0;
static double load_stat_max_x = -200.0;
static double load_stat_min_y = 200.0;
static double load_stat_max_y = -200.0;
static double load_stat_min_z = 200.0;
static double load_stat_max_z = -200.0;

void udpate_load_stat(double x, double y, double z) {
	if (load_stat_min_x > x) load_stat_min_x = x;
	if (load_stat_max_x < x) load_stat_max_x = x;
	if (load_stat_min_y > y) load_stat_min_y = y;
	if (load_stat_max_y < y) load_stat_max_y = y;
	if (load_stat_min_z > z) load_stat_min_z = z;
	if (load_stat_max_z < z) load_stat_max_z = z;
}

void get_load_stat(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2) {
	*x1 = load_stat_min_x;
	*y1 = load_stat_min_y;
	*z1 = load_stat_min_z;
	*x2 = load_stat_max_x;
	*y2 = load_stat_max_y;
	*z2 = load_stat_max_z;
}


/****************************
***                       ***
***   FILE FORMAT TOOLS   ***
***                       ***
****************************/

/*******************************************
*                                          *
*   filters implemented somewhere else   *
*                                          *
*******************************************/

GtsSurface *load_fsbin (const char *);
GtsSurface *read_fsbin (FILE *);
GtsSurface *load_asc (const char *);
GtsSurface *read_asc (FILE *);
GtsSurface *load_stl (const char *);
GtsSurface *read_stl (FILE *);
void save_stl(const char *, GtsSurface *);
void write_stl(FILE *, GtsSurface *);
void save_stla(const char *, GtsSurface *);
void write_stla(FILE *, GtsSurface *);
void save_stlb(const char *, GtsSurface *);
void write_stlb(FILE *, GtsSurface *);


/********************************************
*                                           *
*   trivial filters (implemented by GTS)   *
*                                           *
********************************************/

// oogl file format
// source :
// * http://www.martinreddy.net/gfx/3d/OOGL.spec
void write_off (FILE *fp, GtsSurface *s) {
	gts_surface_write_oogl(s, fp);
}


void save_off (const char *fname, GtsSurface *s) {
	FILE *fp=fopen(fname, "w");
	if (! fp) {
		fprintf(stderr, "Could not open output file %s\n", fname);
		perror("");
		exit (1);
	}

	write_off(fp, s);
	fclose(fp);
}

// gts file format

static GtsSurface *_read_gts(FILE *fp, const char *fname) {
	GtsFile *f = gts_file_new (fp);
	GtsSurface *s = gts_surface_new (gts_surface_class (),
		gts_face_class (),
		gts_edge_class (),
		gts_vertex_class ());

	if (gts_surface_read (s, f)) {
		fputs ("load_gts: file is not a valid GTS file\n", 
			stderr);
		fprintf (stderr, "%s:%d:%d: %s\n", fname, f->line, f->pos, f->error);
		exit (1); /* failure */
	}

	gts_file_destroy (f);

	return s;
}

GtsSurface *read_gts(FILE *fp) {
	return _read_gts(fp, "(pipe)");
}

GtsSurface *load_gts(const char *fname) {
	FILE *fp = fopen(fname, "r");
	GtsSurface *s = (fp != NULL) 
		? _read_gts (fp, fname)
		: ({ 	fprintf(stderr, "Could not open input file %s\n", fname);
			perror("");
			exit(1); NULL; });

	fclose(fp);

	return s;
}

void write_gts (FILE *fp, GtsSurface *s) {
	gts_surface_write(s, fp);
}

void save_gts (const char *fname, GtsSurface *s) {
	FILE *fp=fopen(fname, "w");
	if (! fp) {
		fprintf(stderr, "Could not open output file %s\n", fname);
		perror("");
		exit (1);
	}

	write_gts (fp, s);
	fclose(fp);
}


/*******************************************************
*                                                      *
*   piped filters (used for on-the-fly compression)   *
*                                                      *
*******************************************************/

// general type-independant pipe functions
GtsSurface *load_pipe (const char *pipe, const char *fname);
void save_pipe (const char *pipe, const char *fname, GtsSurface *);

#define MAKE_PIPE_LOAD(EXT,CMD)	\
GtsSurface *load_##EXT(const char *fname) {	\
	return load_pipe(CMD, fname);	\
}
#define MAKE_PIPE_SAVE(EXT,CMD)	\
void save_##EXT (const char *fname, GtsSurface *s) {	\
	save_pipe (CMD, fname, s);	\
}

// use macros to build filters for common on-the-fly compressions 
// that are realy designed to work on a pipe 

// with a prefix table, network protocols could be implemented easily, too.
// WARNING : AS OF TODAY, THE PIPES CANNOT BE STACKED (due to limitation of the POPEN function which is only ONE-way)
// so network pipe + compression pipe + format reader won't work. sorry.

static int compression_level = 9;

static int use_7z = 0;
static const char *gz_orig = "gzip -%2$01d --stdout > %1$s";
static const char *bz2_orig = "bzip2 --compress -%2$01d --stdout > %1$s";
static const char *zip_orig = "zip -j -%2$01d %1$s -";

static const char *gz_7z = "7z a -si -tgzip -mx=%2$01d %1$s";
static const char *bz2_7z = "7z a -si -tbzip2 -mx=%2$01d %1$s";
static const char *zip_7z = "7z a -si%3$s -t%4$s -mx=%2$01d %1$s";

MAKE_PIPE_SAVE(bz2, (use_7z ? bz2_7z : bz2_orig))
MAKE_PIPE_SAVE(gz,  (use_7z ? gz_7z : gz_orig))
MAKE_PIPE_SAVE(lzo, "lzop -%2$01d --output=%1$s")

MAKE_PIPE_LOAD(bz2, "bzcat %1$s")
MAKE_PIPE_LOAD(gz,  "zcat %1$s")
MAKE_PIPE_LOAD(lzo, "lzop --decompress --stdout --keep %1$s")

#undef MAKE_PIPE_LOAD
#undef MAKE_PIPE_SAVE

// archives are a special case
GtsSurface *load_7z (const char *fname);
void save_7z (const char *fname, GtsSurface *);



/*********************************************
* Functions to set parameters for STL files *
*********************************************/

int compress_param_get_level()	{	return compression_level;	}
int compress_param_set_level(int lvl)	{	return (compression_level = lvl);	}

int compress_param_get_default()	{	return use_7z;	}
int compress_param_set_default(int def)	{	return (use_7z = def);	}

int compress_param_is_default_7z ()	{	return (use_7z != 0);	}
int compress_param_is_default_orig ()	{	return (use_7z == 0);	}

int compress_param_set_default_7z ()	{	return (use_7z = 1);	}
int compress_param_set_default_orig ()	{	return (use_7z = 0);	}



/*********************************************************
*                                                        *
*   Automatic type & pipe detection and manipulation   *
*                                                        *
*********************************************************/

// all known pipe and format
// with their probable extension

static const struct FMT_STR {
	GtsSurface * (*load) (const char *);
	GtsSurface * (*read) (FILE *);
	void (*write) (FILE *, GtsSurface *);
	void (*save) (const char *, GtsSurface *);
	const char *ext;
} surf_formats [] =  {
	{ load: &load_fsbin,read:&read_fsbin,save:NULL,      write: NULL,       ext: "pial" },  // as in "rh.pial", no a true extension
	{ load: &load_fsbin,read:&read_fsbin,save:NULL,      write: NULL,       ext: "white" }, // as in "lh.white", no a true extension
	{ load: &load_asc, read: &read_asc, save: NULL,      write: NULL,       ext: "asc" },
	{ load: &load_gts, read: &read_gts, save: &save_gts, write: &write_gts, ext: "gts" },
	{ load: &load_stl, read: &read_stl, save: &save_stl, write: &write_stl, ext: "stl" },
	{ load: &load_stl, read: &read_stl, save: &save_stla,write: &write_stla,ext: "stla" },
	{ load: &load_stl, read: &read_stl, save: &save_stlb,write: &write_stlb,ext: "stlb" },
	{ load: NULL,      read: NULL,      save: &save_off, write: &write_off, ext: "off" },
	{ load: &load_7z,  read: NULL,      save: &save_7z,  write: NULL,       ext: "zip" },
	{ load: &load_7z,  read: NULL,      save: &save_7z,  write: NULL,       ext: "7z" },
	{ load: &load_gz,  read: NULL,      save: &save_gz,  write: NULL,       ext: "gz" },
	{ load: &load_bz2, read: NULL,      save: &save_bz2, write: NULL,       ext: "bz2" },
	{ load: &load_lzo, read: NULL,      save: &save_lzo, write: NULL,       ext: "lzo" },
	{ NULL, NULL, NULL } };


// marco to search in table, based on extension
// could be hacked into prefix finding sollution, too.
static inline struct FMT_STR const * find_surf(const char *fname) {
	struct FMT_STR const * ptr = surf_formats;
	char *ext = strrchr(fname, '.');

	if (ext == NULL) {
		fprintf(stderr, "unrecognizable format name %s\n", fname);
		exit (1);
	} else ext++;

	for(; ptr->ext; ptr++) 
		if (! strcasecmp(ext, ptr->ext))
			return ptr;

	fprintf(stderr, "unknown type %s\n", fname);
	exit (1);
}

// builds command line to run a pipe
static inline const char *pipe_build_cmd (const char *pipe, const char *fname) {
	char *cmd = malloc(strlen(pipe) + strlen(fname));

	if (! cmd) {
		fprintf(stderr, "memory b0rk !\n", fname);
		exit (1);
	}

	sprintf(cmd, pipe, fname, compression_level);
	return cmd;
}

// for normal basic piped filters, tries to find the packed name, based on name
// ie: .gts.gz is a .gts file (packed with gzip)
// doesn't work for archives (zip files...)
static inline const char *pipe_subname (const char *fname) {
	char *c = strrchr(fname, '.');
	size_t n = (c) ? (c - fname) 
		: ({ fprintf(stderr, "unrecognizable sub-format name %s\n", fname);
		exit(1); 0; });

	return (const char *) strndup(fname,  n);
}

// implements the pipe operations
static void pipe_open (struct FMT_STR const * *ptr, FILE * *p, const char *pipe, const char *fname, const char *mode) {
	const char *cmd = pipe_build_cmd(pipe, fname);
	const char *sub = pipe_subname(fname);
	*ptr = find_surf(sub);
	free ((void *) sub);

	if (! (*p = popen(cmd, mode))) {
		fprintf(stderr, "cannot open pipe for %s\n", fname);
		perror(cmd);
		exit (1);
	}
	free ((void *) cmd);
}

// generic function to open a read pipe. Used by the above on-the-fly compressors/decompressors
GtsSurface *load_pipe (const char *pipe, const char *fname) {
	struct FMT_STR const *ptr;
	FILE *p;

	fprintf(stderr, "\r%s: Reading from pipe\n", fname);
	pipe_open (&ptr, &p, pipe, fname, "r");
	if (! ptr->read) {
		fprintf(stderr, "read function not implemented for sub-type  of %s\n", fname);
		exit (1);
	}

	GtsSurface *s = ptr->read(p);
	pclose(p);
	return s;
}

// generic function to open a write pipe. Used by the above on-the-fly compressors/decompressors
void save_pipe (const char *pipe, const char *fname, GtsSurface *s) {
	struct FMT_STR const *ptr;
	FILE *p;

	fprintf(stderr, "\r%s: Writing to pipe\n", fname);
	pipe_open (&ptr, &p, pipe, fname, "w");
	if (! ptr->write) {
		fprintf(stderr, "write function not implemented for sub-type of %s\n", fname);
		exit (1);
	}

	ptr->write(p, s);
	pclose(p);
}

// LAST BUT NOT LEAST :
// the open- and save- file functions themselves

GtsSurface *load_surf(const char *fname) {
	struct FMT_STR const *ptr = find_surf(fname);

	if (ptr->load != NULL)
		return ptr->load(fname);
	else {
		fprintf(stderr, "load function not implemented for %s\n", fname);
		exit (1);
	}
}

void save_surf(const char *fname, GtsSurface *s) {
	struct FMT_STR const *ptr = find_surf(fname);

	if (ptr->save != NULL) {
		ptr->save(fname, s);
		return;
	} else {
		fprintf(stderr, "save function not implemented for %s\n", fname);
		exit (1);
	}
}


/*********************************
*                                *
*   Special case for archives   *
*                                *
*********************************/

// Archives are a special case.
// - filename should be provided when writing from a pipe (for the archive table of file to contain coherent informations)
// - filename cannot be infered from archive name (listing should be implemented instead)

// 7z :  (modules)       7z, ZIP, CAB, ARJ, GZIP, BZIP2, TAR, CPIO, RPM and DEB
// 7za : (standalone)    7z, zip, gzip, bzip2, Z and tar
// 7zr : (single)        7z
// -t...
// -siFilename
// 7z a -sifull-inside.gts -t7z -mx=9 full-inside.gts.7z

GtsSurface *load_7z (const char *fname) {
// listing is NOT yet implemented !!!
}


void save_7z (const char *fname, GtsSurface *s) {
	const char *pipe = (use_7z ) ? zip_7z : zip_orig;
	const char *ext = strrchr(fname, '.');
	size_t n = (ext) ? (ext - fname) 
		: ({ fprintf(stderr, "unrecognizable sub-format name %s\n", fname);
		exit(1); 0; });
	const char *sub = (const char *)strndup(fname, n);
	char *cmd = malloc(strlen(pipe) + 2 * strlen(fname));
	struct FMT_STR const *ptr = find_surf(sub) ?: ({
		fprintf(stderr, "write function not implemented for %s\n", fname);
		exit (1); NULL;
	});
	FILE *p;

	sprintf(cmd, pipe, fname, compression_level, sub, ext+1);
	fprintf(stderr, "\r%s: Writing to pipe %s\n", fname, cmd);
	free((void *) sub);

	if (! (p = popen(cmd, "w"))) {
		fprintf(stderr, "cannot open pipe for %s\n", fname);
		perror(cmd);
		exit (1);
	}
	free ((void *) cmd);

	ptr->write(p, s);
	pclose(p);
}




/************************************
 *                                  *
 *   Load transformation matrices   *
 *                                  *
 ***********************************/

GtsMatrix* load_matrix(const char *fname) {
	GtsMatrix* r = NULL;
	
	double	m00=0, m01=0, m02=0, m03=0,
		m10=0, m11=0, m12=0, m13=0,
		m20=0, m21=0, m22=0, m23=0;

	FILE *fp=fopen(fname, "r");
	if (! fp) {
		fprintf(stderr, "Could not open output file %s\n", fname);
		perror("");
		exit (1);
	}
	
	
	fscanf(fp,
		" %lg %lg %lg %lg\n"
		" %lg %lg %lg %lg\n"
		" %lg %lg %lg %lg\n",
		&m00, &m01, &m02, &m03,
		&m10, &m11, &m12, &m13,
		&m20, &m21, &m22, &m23);
	fclose(fp);
	
	r = gts_matrix_new(
		m00, m01, m02, m03,
		m10, m11, m12, m13,
		m20, m21, m22, m23,
		0,0,0,1);

	return r;
}



