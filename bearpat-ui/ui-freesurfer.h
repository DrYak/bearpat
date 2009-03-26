extern char *freesurfer_home;	// FREESURFER_HOME
extern char *freesurfer_subjects;	// SUBJECTS_DIR

extern GtkListStore *fssubj_store;
extern GtkTreeSelection *fssubj_sel;
extern GtkTreeSelection *fssurf_sel;
extern int fssubj_num_running;

GString *freesurfer_script();

enum {
	FSSUBJ_SELECTOR_NAME,
	FSSUBJ_SELECTOR_NUM_ORIGS,	// number of MRI in ORIGS
	FSSUBJ_SELECTOR_CALCULATED,	// how far has RECON-ALL reached ?
	FSSUBJ_SELECTOR_IS_RUNNING,	// true if is currently running in a screen session
	FSSUBJ_SELECTOR_PID,	// PID
	FSSUBJ_SELECTOR_NUMCOLS
} FDSUBJ_SELECTOR_COLUMN;

void scan_fssubj();
void build_fssubj_store();
void dump_fsmat(const char *subj, const char *vox2ras, const char *vox2ras_tkr , const char *orig = NULL, double *det=NULL, double *cres=NULL, double *rres=NULL, double *sres=NULL);
gboolean event_launch_recon (GtkWidget *widget, gpointer data );
gboolean event_restart_recon_bm (GtkWidget *widget, gpointer data );
gboolean event_restart_recon_wm (GtkWidget *widget, gpointer data );
gboolean event_restart_recon_pial (GtkWidget *widget, gpointer data );
gboolean event_check_tkmedit (GtkWidget *widget, gpointer data );
gboolean event_check_tksurfer (GtkWidget *widget, gpointer data );
gboolean event_abort_recon (GtkWidget *widget, gpointer data );
