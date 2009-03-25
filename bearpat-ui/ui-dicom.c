// ISOC99 is used for isfinite() function
#define _ISOC99_SOURCE

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "ui-dicom.h"
#include "dicom.h"


GtkListStore *dicom_store = NULL;
GtkTreeSelection *dicom_sel = NULL; 

volatile GPtrArray *gthread_search = NULL;
GMutex *search_mutex = NULL;
static GMutex *search_array_mutex = NULL;
// GPtrArray->len are atomic ( 1 int load ) 
// and thread that reads it is the same that creats threads (the main threads),
// so it cannot be increased in background, only decreased.
// therefor we don't care each time we do check for amount of threads
//
// but g_ptr_array_* function ARE NOT atomic, so when adding/removing threads to list, 
// we need mutex

void init_search_mutex ()  {
	g_assert (search_mutex == NULL);
	search_mutex = g_mutex_new ();
	search_array_mutex = g_mutex_new ();

	// also initialise the thread pointer
	gthread_search = g_ptr_array_new();
}

void build_dicom_store() {
	dicom_store = gtk_list_store_new (DICOM_SELECTOR_NUMCOLS,
		GDK_TYPE_PIXBUF,	// preview is a picture
		G_TYPE_STRING,	// name is a string
		G_TYPE_STRING,	// date is a string
		G_TYPE_STRING,	// SubjectID is a string
		G_TYPE_STRING,	// Voxel size is a string
		G_TYPE_STRING,	// SEQNAME is a string
		G_TYPE_STRING,	// SEQTYPE is a string
		G_TYPE_STRING,	// SeqID is a string
		G_TYPE_INT,	// NumSlices is a string
		G_TYPE_POINTER,	// Slices  is a Double-Linked list
		G_TYPE_INT);	// currently preview slice is an INT
}

static void *search_thread(void *args) {
	gchar *target = (gchar *) args;
 	gchar *argv[] = { "find", target, NULL };
	gint fdes, i;
	FILE *p;
	GError *error = NULL;
	GPid child_pid;
	GtkTreeIter iter;

	g_message("scan-thread (%p) : %s", g_thread_self(), target);

	// launch find to scan directory
	g_message("search pipe (%p) : %s(%s)", g_thread_self(), argv[0], argv[1]);
	if (!g_spawn_async_with_pipes(NULL,	// cwd
		argv,
		NULL,	// inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL,	// no GSpawnChildSetupFunc child_setup,
		NULL,
		&child_pid,
		NULL,	// no stdin
		&fdes,	// sotre descriptor to build stream later
                NULL,	// no stderr
 		&error)
	) {
		g_printerr ("Failed to launch %s(%s): %s\n", argv[0], argv[1], error->message);
		g_free (error);

		// remove self from the list of active threads
		g_mutex_lock(search_array_mutex); 
		g_ptr_array_remove_fast ((GPtrArray*)gthread_search, g_thread_self());
		g_mutex_unlock(search_array_mutex); 
		return;
	} 

	// build stream from descriptor
	p = fdopen(fdes, "r");
#define MAX_PATH 32768
	do {
		char fname[MAX_PATH];
		int l, r;

		char *seruid	= NULL;
		char *name	= NULL;
		char *subjid	= NULL;
		char *date	= NULL;
		char *type	= NULL;
		char *desc	= NULL;
		char *res	= NULL;
		double loc	= NAN;

		int found = 0;

		// input 1 line from child process
		if (fgets (fname, MAX_PATH, p) == NULL) {
			g_message("search pipe (%p) ended", g_thread_self());
			break;
		}
		if ((l = strlen(fname)) <= 1) continue; // empty lines aren't interesting...
		fname[--l] = '\0'; // remove trailing '\n'


		if (! ((r = dicom_getinfo_overview (fname, &seruid, &name, &subjid, &date, &type, &desc, &res, &loc)) &
			(DICOM_FOUND_SERUID | DICOM_FOUND_LOC))) continue; // need a SERIE_UID and a LOCATION to be inserted

		if (! isfinite(loc)) continue; // need a real Location !!!

		found = 0;
		gdk_threads_enter();
		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dicom_store), &iter)) {
			do {
				GValue value;
				gchar *search_seruid;

				memset(&value, 0, sizeof(GValue));
				gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_SEQID, &value);
				search_seruid = (gchar *)g_value_get_string(&value);
				if (strcmp(search_seruid, seruid) == 0) {
					g_value_unset(&value);
					found = 1;
					break;
				}
				g_value_unset(&value);
			} while (gtk_tree_model_iter_next(GTK_TREE_MODEL (dicom_store), &iter));
		}
		gdk_threads_leave();

		if (found) {
			gint i = 0, num = 1; 
			GValue value;
			GPtrArray *slices = NULL;

			DICOM_SLICE_DATA *slice = g_malloc(sizeof(DICOM_SLICE_DATA)); 
			slice->fname = g_strdup(fname);
			slice->location = loc;

			memset(&value, 0, sizeof(GValue));
			gdk_threads_enter();
			gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_SLICES, &value);
			gdk_threads_leave();
			slices = (GPtrArray *) g_value_get_pointer(&value);

			// walk the array and remove copies of the same slice
			i = 0;
			while (i < slices->len) 
				if (((DICOM_SLICE_DATA *) slices->pdata[i])->location == loc) {
					g_mutex_lock(search_mutex); // lock before modifing data
					g_free(((DICOM_SLICE_DATA *) slices->pdata[i])->fname);
					g_free(slices->pdata[i]);
					slices->pdata[i] = NULL;
					g_ptr_array_remove_index(slices, i);
					g_mutex_unlock(search_mutex); // unlock after modifing data
				} else
					i++;
			g_mutex_lock(search_mutex); // lock before modifing data
			g_ptr_array_add(slices, slice);
			g_mutex_unlock(search_mutex); // unlock after modifing data

			num = slices->len;
			g_value_unset(&value);

			// get GTK thread lock 
			gdk_threads_enter();
			gtk_list_store_set (GTK_LIST_STORE (dicom_store), 
					&iter,
					DICOM_SELECTOR_NUMSLICES,	num,
					-1);
			//	gdk_flush ();	// don't force refresh, for some speed up

			gdk_threads_leave();
		} else {
			GPtrArray *slices = g_ptr_array_sized_new(50);
			DICOM_SLICE_DATA *slice = g_malloc(sizeof(DICOM_SLICE_DATA)); 
			slice->fname = g_strdup(fname);
			slice->location = loc;
			g_ptr_array_add(slices, slice);

			gdk_threads_enter();
			gtk_list_store_append (GTK_LIST_STORE (dicom_store), &iter);
			gtk_list_store_set (GTK_LIST_STORE (dicom_store), 
					&iter,
					DICOM_SELECTOR_SEQID,	seruid	?: "",
					DICOM_SELECTOR_SEQTYPE,	type	?: "",
					DICOM_SELECTOR_NAME,	name	?: "",
					DICOM_SELECTOR_DATE,	date	?: "",
					DICOM_SELECTOR_ID,	subjid	?: "",
					DICOM_SELECTOR_VOXSIZE,	res	?: "",
					DICOM_SELECTOR_SEQNAME,	desc	?: "",
					DICOM_SELECTOR_NUMSLICES,	1,
					DICOM_SELECTOR_SLICES,	slices,
//					DICOM_SELECTOR_PREVIEW_SLICE, -1,
					-1);
//	DICOM_SELECTOR_PREVIEW,
//	
			gdk_flush ();
			gdk_threads_leave();

		}
		if (seruid)	g_free(seruid);
		if (name) 	g_free(name);
		if (subjid)	g_free(subjid);
		if (date) 	g_free(date);
		if (type) 	g_free(type);
		if (desc) 	g_free(desc);
		if (res) 	g_free(res);
	} while(1);
	g_spawn_close_pid(child_pid);	// close pid
	close(fdes);	// close pipe 

	// remove self from the list of active threads
	g_mutex_lock(search_array_mutex); 
	g_ptr_array_remove_fast ((GPtrArray*)gthread_search, g_thread_self());
	g_mutex_unlock(search_array_mutex); 
	return;
}

gboolean event_scan (GtkWidget *widget, gpointer data ) {
	GThread *gthread_new;
	GError *error = NULL;

	if (gthread_search->len) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK_CANCEL,
			"Warning, another scan is still in progress\n"
			"Two scan from the same drive are very slow\n");

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			gtk_widget_destroy (dialog);
			return FALSE;
		}

		gtk_widget_destroy (dialog);
	}


	gchar *target = (gchar *) data ?: ({
			static char *path = NULL;
			if (path == NULL) path = g_strdup((char *) getenv("HOME") ?: "~");
			GtkWidget *dialog = gtk_file_chooser_dialog_new ("Choose directory to scan",
				GTK_WINDOW(main_window), // NULL, // parent_window,
				GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				NULL);
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), path);

//tree-depth limit wiht : 
// - gtk_file_chooser_set_extra_widget()
//gnome_vfs :
// - gtk_file_chooser_set_local_only ()

			if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
				gtk_widget_destroy (dialog);
				return FALSE;
			}

			g_free(path); // free previous content
			path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	
			gtk_widget_destroy (dialog);
			path;
		});

	// TODO:  selector for /media can be added based on "mount|gawk '$3~/media/{print $3}'"

	g_mutex_lock(search_array_mutex); 
	if (!(gthread_new = g_thread_create(search_thread, target, FALSE, &error))) {
		g_mutex_unlock(search_array_mutex); 
		g_printerr ("Failed to create search_thread (%s): %s\n", target, error->message);
		g_free(error);
	}
	// add new thread to list
	g_ptr_array_add((GPtrArray*)gthread_search, gthread_new);
	g_mutex_unlock(search_array_mutex); 

	return FALSE;
}




char *aeskulap_cmd = NULL;
char *aeskulap_ico = NULL;

gboolean event_preview_aeskulap (GtkWidget *widget, gpointer data ) {
	GtkTreeIter iter;
	GError *error = NULL;
	int must_free_copies = 0;
	int num_slices = 0;
	char **argv = NULL;

	if (! gtk_tree_selection_get_selected (dicom_sel, (GtkTreeModel **)NULL, &iter)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			(gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dicom_store), &iter)) 
			? 	"No DICOM series selected.\n"
				"Please select a DICOM serie first."
			:	"No DICOM series found.\n"
				"Please scan for DICOM first.");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	if (gthread_search->len) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK_CANCEL,
			"Warning, the scan is still in progress");

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			gtk_widget_destroy (dialog);
			return FALSE;
		}

		gtk_widget_destroy (dialog);
	}

	if (gthread_search->len) {
		// the gthread test it-self is thread-safe because the search_thread is 
		// created by the same thread (the main one) as this event handler
		// therefor, no race condition may happen that _creates_ the thread until it's locked and temporary copy of the data is obtained.
		gint i; 
		GValue value;
		GPtrArray *slices = NULL;

		g_message("args : thread safe");

		must_free_copies = 1;
		memset(&value, 0, sizeof(GValue));

		g_mutex_lock (search_mutex);

		gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_SLICES, &value);
		slices = (GPtrArray *) g_value_get_pointer(&value);
		num_slices = slices->len;

		argv = (char **) g_malloc(sizeof(char *) * (num_slices + 2)); // counst also the null pointer and the program name		
		for (i = 0; i < num_slices; i++)
			argv [i + 1] = g_strdup(((DICOM_SLICE_DATA *) slices->pdata[i])->fname);

		// ok now we have a working copy, we can let the scan continue and modifies watever it wants with the GPtrArray, or the fname strings.
		g_mutex_unlock (search_mutex);
		
		g_value_unset(&value);
	} else {
		gint i; 
		GValue value;
		GPtrArray *slices = NULL;

		g_message("args : mono task");

		must_free_copies = 0;
		// note that the opposite is true :
		// the thread could be destroyed between the check and the locking
		// but the worst effect is only that we waste a little bit more memory

		memset(&value, 0, sizeof(GValue));
		gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_SLICES, &value);
		slices = (GPtrArray *) g_value_get_pointer(&value);
		i = num_slices = slices->len;

		argv = (char **) g_malloc(sizeof(char *) * (num_slices + 2)); // counst also the null pointer and the program name		
		for (i = 0; i < num_slices; i++)
			// it's faster and more memory efficient, but we can do this only when no background thread
			// could free the fname before we finish, or even change the pointer during the 'args' building
			argv [i + 1] = ((DICOM_SLICE_DATA *) slices->pdata[i])->fname;
	
		g_value_unset(&value);
	}

	// ok now vargs is safe. either no back ground, or working copy.
	argv[num_slices+1] = NULL; // null terminator		
	argv[0] = aeskulap_cmd;

	g_message("run : %s(%s, %s, ... )", argv[0], argv[1], argv[2]);
	if (!g_spawn_async(NULL,	// cwd
		argv,
		NULL, // inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL, // no GSpawnChildSetupFunc child_setup,
		NULL,
		NULL, // no pid
		&error)
	) {
		g_printerr ("Failed to launch %s(%s): %s\n", argv[0], argv[1], error->message);
		g_free (error);
	}


	// free memory
	if (must_free_copies) {
		// if we were using working copies, free them
		int i = num_slices;
		do 
			g_free(argv[i]);
		while (--i);
	}
	g_free(argv);
}

