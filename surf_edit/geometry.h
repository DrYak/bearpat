enum BOOL_OP { BOOL_UNION, BOOL_INTER, BOOL_DIFF };
GtsSurface *bool_op (enum BOOL_OP op, GtsSurface *outside, GtsSurface *inside, const char *n1, const char *n2);

GtsSurface *bool_union (GtsSurface *s1, GtsSurface *s2, const char *n1, const char *n2);
GtsSurface *bool_inter (GtsSurface *s1, GtsSurface *s2, const char *n1, const char *n2);
GtsSurface *bool_diff (GtsSurface *s1, GtsSurface *s2, const char *n1, const char *n2);

GtsSurface* make_box (gdouble x1, gdouble y1, gdouble z1, gdouble x2, gdouble y2, gdouble z2);

void gts_surface_transform (GtsSurface *s, GtsMatrix *m);
void polylines(char *fname, GtsSurface* s, int n,  double ox, double oy, double oz,   double cx, double cy, double cz, double rx, double ry, double rz, double sx, double sy, double sz);
