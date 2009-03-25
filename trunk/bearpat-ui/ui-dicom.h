extern char *aeskulap_cmd;
extern char *aeskulap_ico;

extern GtkListStore *dicom_store;
extern GtkTreeSelection *dicom_sel;

extern volatile GPtrArray *gthread_search;
extern GMutex *search_mutex;


enum {
	DICOM_SELECTOR_PREVIEW,
	DICOM_SELECTOR_NAME,
	DICOM_SELECTOR_DATE,
	DICOM_SELECTOR_ID,
	DICOM_SELECTOR_VOXSIZE,
	DICOM_SELECTOR_SEQNAME,
	DICOM_SELECTOR_SEQTYPE,
	DICOM_SELECTOR_SEQID, // hidden : ID of sequence
	DICOM_SELECTOR_NUMSLICES, // ascii representation of g_list_length()
	DICOM_SELECTOR_SLICES, // hidden : pointer to GList of slices files
	DICOM_SELECTOR_PREVIEW_SLICE, // slice currently that is currently displayed
	DICOM_SELECTOR_NUMCOLS
} DICOM_SELECTOR_COLUMN;

typedef struct {
	double location;
	char *fname;
} DICOM_SLICE_DATA;


void init_search_mutex ();
void build_dicom_store ();

gboolean event_scan (GtkWidget *widget, gpointer data);
gboolean event_preview_aeskulap (GtkWidget *widget, gpointer data);
