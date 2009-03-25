typedef enum {
	LEFT,
	RIGHT,
	BOTH
} SIDE;

typedef enum {
	CLIP_NONE,
	CLIP_INSIDE,
	CLIP_OUTSIDE
} CLIP;

extern struct CLIP_LIMIT {
	double top;
	double bottom;
	double front;
	double back;
	double left;
	double right;
} clip_limit;

extern char *surf_edit;
extern char *meshlab_cmd;
extern char *meshlab_ico;
extern char *k3b_cmd;
extern char *k3b_ico;

gboolean event_set_side (GtkWidget *widget, gpointer data );
gboolean event_set_clip (GtkWidget *widget, gpointer data );
gboolean event_set_coor (GtkWidget *widget, gpointer data );
gboolean event_set_takeout(GtkWidget *widget, gpointer data);
gboolean event_savesurf(GtkWidget *widget, gpointer data);
gboolean event_meshlab(GtkWidget *widget, gpointer data);
gboolean event_burnk3b(GtkWidget *widget, gpointer data);

