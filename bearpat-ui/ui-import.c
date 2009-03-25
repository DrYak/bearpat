                                              // ISOC99 is used for isfinite() function
#define _ISOC99_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "ui-freesurfer.h"
#include "ui-dicom.h"
#include "ui-import.h"



static gint compare_slices (gconstpointer a, gconstpointer b) {
	inline double sloc (gconstpointer p) {
		return ((const DICOM_SLICE_DATA *) (*((gconstpointer *) p)))->location;
	}

	// non-finite elements got rejected earlier
	return (sloc(a) > sloc(b)) - (sloc(a) < sloc(b));	
}



/*********************
***                ***
***   PARAMETERS   ***
***                ***
*********************/

static char *dircopy_cmd = "cp";
static gboolean hint_names = TRUE;
static gboolean hint_lastonly = TRUE;


gboolean event_set_hint_names (GtkWidget *widget, gpointer data ) {
	hint_names = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	return FALSE;
}

gboolean event_set_hint_lastonly (GtkWidget *widget, gpointer data ) {
	hint_lastonly = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	return FALSE;
}

gboolean event_set_dircopy_cmd (GtkWidget *widget, gpointer data ) {
	if (data && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		char *cmd = (char *) data;
		dircopy_cmd = cmd;
		printf("import cmd : %s\n", dircopy_cmd);
	}
	return FALSE;
}

static inline gboolean check_char_error (char c, gboolean slash) {
	return ((! g_ascii_isalnum(c)) &&
		(c != '-') &&
		(c != '_') &&
		(c != '.') && 
		((! slash) || (c != '/')));
}

inline gboolean name_error(char *name, char *target_name, gboolean slash) {
	char *p;

	for (p = name; *p; p++)
		if (check_char_error(*p, slash)) {

			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"Invalid caracter in %s : %c\n"
				"%s should only contain \n"
				"a-z, A-Z, 0-9, %s-, _ and .\n",
				target_name, *p,
				target_name,
				slash ? "/, " : "");
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			return TRUE; // error
		}
	return FALSE; // no error
}

gboolean event_hint_names (GtkWidget *widget, gpointer data ) {
	static GtkTreeIter lastiter;
	static char *hintname = NULL;

	GtkTreeIter iter;
	GValue value;
	

	if (! hint_names) return FALSE;


	if (dicom_sel != GTK_TREE_SELECTION(data)) 
		g_warning ("selection oopses while hinting name");

	if (! gtk_tree_selection_get_selected (dicom_sel, (GtkTreeModel **)NULL, &iter)) 
		// no selection ? out.
		return FALSE;

	if ((hintname == NULL) || (memcmp(&iter, &lastiter, sizeof(GtkTreeIter))) != 0) {
		if (hintname) g_free(hintname);

		inline char *build_hint() {
			char *r, *p;

			gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_NAME, &value);

			inline const char *fromback(const char *v) {
				const char *p;
				if (hint_lastonly) 
					for (p = v + strlen(v) - 1; p > v; p--)
						if (*p == '^')
							return p + 1;
				return v;
			}

			r = g_ascii_strdown(fromback(g_strstrip((char *) g_value_get_string(&value))), -1);

			for (p = r; *p; p++)
				if (check_char_error(*p, FALSE)) // no slashes
					*p = '_';

			return r;
		}

		memset(&value, 0, sizeof(GValue));
		if (gthread_search->len) {
			g_message("hint : thread safe");
	
			g_mutex_lock (search_mutex);
			hintname = build_hint();
			g_mutex_unlock (search_mutex);
		} else {
			hintname = build_hint();
		}
		g_value_unset(&value);

		lastiter = iter;
	}

	if (GTK_IS_FILE_CHOOSER (widget)) {
		char *tmp = g_strdup_printf("%s/%s/", 
			gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(widget)),
			hintname);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(widget), tmp);
		g_free(tmp);
	}

	if (GTK_IS_COMBO_BOX_ENTRY (widget)) {
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widget))), hintname);
	}

	return FALSE;
}



/**************************
***                     ***
***   RUNNING SCRIPTS   ***
***                     ***
**************************/

#define TAG_PROGRESS	"progress_script"
#define TAG_RESCAN	"rescan"
#define TAG_END	"end"

static double import_status_max = 1.0;

static gboolean import_update_status (GIOChannel *source, GIOCondition condition, gpointer data) {
	GError *error = NULL;
	gchar *line = NULL;
	gsize l = 0, t = 0;
	GIOStatus s;

	switch ((s = g_io_channel_read_line (source, &line, &l, &t, &error))) {
		case G_IO_STATUS_EOF:
			g_message ("end of import status pipe");
			if ((line == NULL) || (l == 0)) {
				if (line) g_free(line);
				if (error) g_free(error);
				error = NULL;
				g_io_channel_shutdown(source, FALSE, &error); // kill stream if we are done with it
				if (error) g_free(error);
				return FALSE; // don't restart after end
			}
			break;

		case G_IO_STATUS_ERROR:
			g_printerr ("import status pipe error : %s", error->message);
		case G_IO_STATUS_AGAIN:
			// test if there is at least somehting to read
			if ((line == NULL) || (l == 0)) {
				if (line) g_free(line);
				if (error) g_free(error);
				return TRUE; // retry - see if next time we have more luck
			}
			break;
		case G_IO_STATUS_NORMAL:
			break;
	}

	if (error) g_free(error);
	error = NULL;

	if (line == NULL) return FALSE;
	if (l == 0) {
		g_free(line);
		return FALSE;
	}
	if (t) line[t] = '\0';

	gchar **col = g_strsplit(line, " ", 6);

	GtkProgressBar	*bar = GTK_PROGRESS_BAR(data);
	GtkWindow	*dialog = GTK_WINDOW(gtk_widget_get_parent(GTK_WIDGET(gtk_widget_get_parent(GTK_WIDGET(bar)))));
	// dialog -> vbox -> pbar

	if (strcmp(col[0], TAG_PROGRESS) == 0) {
		if (strcmp(col[1], TAG_END) == 0) {
			g_message("end of import script");
			// close dialog
			gtk_widget_destroy (GTK_WIDGET(dialog));	//	children are auto-destroyed

			g_strfreev(col);
			g_free(line);
			g_io_channel_shutdown(source, FALSE, &error);  // kill stream - we are done with it
			if (error) g_free(error);
			return FALSE; // dont retry
		} else if (strcmp(col[1], TAG_RESCAN) == 0) {
			GtkTreeIter iter;
			GtkLabel *label = GTK_LABEL(gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(bar)))) -> data);

			gtk_window_set_title (dialog, "Rescan subjects" );
			gtk_label_set_text (label, "Rescan subjects" );
			gdk_flush();

			scan_fssubj ();
			if (col[2] && gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fssubj_store), &iter)) {
				// preselect subject in next pane
				// we must search anew, because we rebuilt the fssubj store, because subject may be new
				do {
					GValue value;
					char *search_name;
		
					memset(&value, 0, sizeof(GValue));
					gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
					search_name = (char *) g_value_get_string(&value);
					if (strcmp(search_name, col[2]) == 0) {
						g_value_unset(&value);
						gtk_tree_selection_select_iter (fssubj_sel, &iter);
						break;
					}
					g_value_unset(&value);
				} while (gtk_tree_model_iter_next(GTK_TREE_MODEL (fssubj_store), &iter));
			}

			gtk_window_set_title (dialog, "Import DICOMs" );
			gtk_label_set_text (label, "Import DICOMs" );
			gdk_flush();
		} else {
			int i = atoi(col[1]);
			char *t = g_strdup_printf ("%d / %.0f", i, import_status_max);
			gtk_progress_bar_set_fraction(bar, ((double) i) / import_status_max);
			gtk_progress_bar_set_text(bar,t);
			g_free(t);
		}
	} else if (strcmp(col[0], "reading") == 0) {
		GtkLabel *label = GTK_LABEL(gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(bar)))) -> data);
		// vbox -> pbar, then vbox -> first
		gtk_window_set_title (dialog, "FreeSurfer conversion" );
		gtk_label_set_text (label, "FreeSurfer conversion" );
		gtk_progress_bar_set_fraction(bar, 0.0);
		gtk_progress_bar_set_text(bar,col[0]);
	} else if (strcmp(col[0], "writing") == 0) {
		gtk_progress_bar_set_fraction(bar, 0.5);
		gtk_progress_bar_set_text(bar,col[0]);
	}

	g_strfreev(col);
	g_free(line);
	if (s ==  G_IO_STATUS_EOF) {
		gtk_widget_destroy (GTK_WIDGET(dialog));	//	children are auto-destroyed

		g_io_channel_shutdown(source, FALSE, &error);  // kill stream - we are done with it
		if (error) g_free(error);
		return FALSE; // dont retry
	}
	return TRUE;	// keep the source if not the end yet
}


static gboolean import_run_script(char *script, int num) {
	GError *error = NULL;
	gchar *argv[] = { "sh", "-c", script, NULL };
	int fdes;

	g_message("run import script ( shell : %s ) :\n", argv[0], script);


	if (!g_spawn_async_with_pipes(NULL,	// cwd
		argv,
		NULL, // inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL, // no GSpawnChildSetupFunc child_setup,
		NULL,
		NULL,	// no pid
		NULL,	// no stdin
		&fdes,	// sotre descriptor to build stream later
                NULL,	// no stderr
		&error)
	) {
		g_printerr ("Failed to launch %s\n error : %s\nscript :\n%s\n", argv[0], error->message, argv[2]);
		g_free (error);
		return FALSE;
	} 

	GtkWidget *dialog = gtk_dialog_new_with_buttons (
		"Importing DICOMs",
		GTK_WINDOW(main_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), "Importing DICOMs");
//	gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);

	GtkWidget *label = gtk_label_new("Importing DICOMs");
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, FALSE, 0);
	gtk_widget_show (label);

	GtkWidget *bar = gtk_progress_bar_new();
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), bar, TRUE, FALSE, 0);
	gtk_widget_show (bar);
	gtk_widget_show (dialog);

	import_status_max = (double) num;
	GIOChannel *input_chan = g_io_channel_unix_new (fdes);	
	g_io_add_watch(input_chan, G_IO_IN | G_IO_PRI, import_update_status, bar);
	// pid is not destroyed !!! (important for Windows)
	// not important for Unix
	// important for Windows
	// must change to G_IO_HUP signal handler
	return TRUE;
}



/***************************
***                      ***
***   IMPORTING DICOMS   ***
***                      ***
***************************/

gboolean event_dir_copy (GtkWidget *widget, gpointer data ) {
	char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	DICOM_SLICE_DATA *slices_data = NULL;
	GtkTreeIter iter;
	GPtrArray *slices = NULL;
	GString *cmd = NULL;
	gchar *cmd_final = NULL;
	int must_free_copies = 0;
	int num_slices = 0;
	int i;

	if ((path == NULL) || (strlen(path) == 0)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"No target directory selected\n"
			"Please select a directory first");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	} else {
		if (name_error(path, "target path", TRUE))
			return FALSE;
	}

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
		GValue value;
		GPtrArray *orig_slices = NULL;

		g_message("args : thread safe");

		must_free_copies = 1;
		memset(&value, 0, sizeof(GValue));

		g_mutex_lock (search_mutex);

		gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_SLICES, &value);
		orig_slices = (GPtrArray *) g_value_get_pointer(&value);
		num_slices = orig_slices->len;

		slices = g_ptr_array_sized_new(num_slices);
		DICOM_SLICE_DATA *slices_data = g_malloc(num_slices * sizeof(DICOM_SLICE_DATA));
		for (i = 0; i < num_slices; i++) {
			slices_data[i] = (DICOM_SLICE_DATA) { 
				location: ((DICOM_SLICE_DATA *) orig_slices->pdata[i])->location,
				fname: g_strdup(((DICOM_SLICE_DATA *) orig_slices->pdata[i])->fname)
			};
			g_ptr_array_add(slices, slices_data + i);
		}

		// ok now we have a working copy, we can let the scan continue and modifies watever it wants with the GPtrArray, or the fname strings.
		g_mutex_unlock (search_mutex);
		
		g_value_unset(&value);
	} else {
		GValue value;

		g_message("args : mono task");

		must_free_copies = 0;
		// note that the opposite is true :
		// the thread could be destroyed between the check and the locking
		// but the worst effect is only that we waste a little bit more memory

		memset(&value, 0, sizeof(GValue));
		gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &iter, DICOM_SELECTOR_SLICES, &value);
		slices = (GPtrArray *) g_value_get_pointer(&value);
		num_slices = slices->len;

		g_value_unset(&value);
	}

	g_message("copy to %s", path);
	
	g_ptr_array_sort (slices, compare_slices);
	cmd = g_string_new("");
	g_string_printf (cmd, "mkdir -p -m ug+wx -- %s ;\n", path);
	for (i = 0; i < num_slices; i++)
		g_string_append_printf(cmd,
			"%s '%s' \"%s/%08d.dcm\" ; \n"
			"echo " TAG_PROGRESS " %d ; \n",
			dircopy_cmd,
			((DICOM_SLICE_DATA *) slices->pdata[i])->fname,
			path, i, i+1);
	g_string_append(cmd, "echo " TAG_PROGRESS " " TAG_END " ; \n");

	// free data...
	cmd_final = g_string_free(cmd, FALSE);	 // FALSE - ...except for the actual command string
	if (must_free_copies) {
		for (i = 0; i < num_slices; i++)
			g_free (((DICOM_SLICE_DATA *) slices->pdata[i])->fname); // fname copies
		g_free(slices_data);	// copy holder
		g_ptr_array_free(slices, TRUE);	// array copy
	}

	// do it !
	import_run_script(cmd_final, num_slices);

	g_free(cmd_final);
	return FALSE;
}




gboolean event_import_fs (GtkWidget *widget, gpointer data ) {
	char *subject_name = gtk_combo_box_get_active_text (GTK_COMBO_BOX(widget));

	GtkTreeIter fs_iter, dicom_iter;
	GValue value;
	GString *cmd = NULL;
	gchar *cmd_final = NULL;
	int num_slices = 0;
	int num_mri = 1;
	int i;

	if ((subject_name == NULL) || (strlen(subject_name) == 0)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"No target FreeSurfer subject select\n"
			"Please type a new subject name\n"
			"or select an existing one\n");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	} else {
		if (name_error(subject_name, "subject name", FALSE))
			return FALSE;
	}

	if (! gtk_tree_selection_get_selected (dicom_sel, (GtkTreeModel **)NULL, &dicom_iter)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			(gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dicom_store), &dicom_iter)) 
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


	// begin FreeSurfer enviro
	cmd = freesurfer_script();

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &fs_iter) 
		?: ({
			gboolean r = FALSE;
			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fssubj_store), &fs_iter)) {
				do {
					char *search_name;

					memset(&value, 0, sizeof(GValue));
					gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &fs_iter, FSSUBJ_SELECTOR_NAME, &value);
					search_name = (char *) g_value_get_string(&value);
					if (strcmp(search_name, subject_name) == 0) {
						g_value_unset(&value);
						r = TRUE; // return TRUE
						break;
					}
					g_value_unset(&value);
				} while (gtk_tree_model_iter_next(GTK_TREE_MODEL (fssubj_store), &fs_iter));
			}
			r;
		})
	) {
		memset(&value, 0, sizeof(GValue));
		gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &fs_iter, FSSUBJ_SELECTOR_NUM_ORIGS, &value);
		num_mri = g_value_get_int(&value);
		g_value_unset(&value);


		// one is selected
		GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
		GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_OK_CANCEL,
			"Warning, subject %s already exists in FreeSurfer\n"
			"it contains has %d MRI",
			subject_name, num_mri);

		
		GtkWidget *button_add = gtk_radio_button_new_with_label (NULL, "continue  adding the MRI to the subject");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_add), TRUE);
		gtk_box_pack_start (GTK_BOX(vbox), button_add, TRUE, FALSE, 0);
		gtk_widget_show (button_add);

		GtkWidget *button_rep = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_add), "replace last MRI in subject");
		gtk_box_pack_start (GTK_BOX(vbox), button_rep, TRUE, FALSE, 0);
		gtk_widget_show (button_rep);

		GtkWidget *button_del = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_add), "erase subject and overwrite as a new");
		gtk_box_pack_start (GTK_BOX(vbox), button_del, TRUE, FALSE, 0);
		gtk_widget_show (button_del);


		gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, FALSE, 0);
		gtk_widget_show (vbox);

		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, FALSE, 0);
		gtk_widget_show (hbox);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			g_string_free(cmd, TRUE);
			gtk_widget_destroy (dialog);
			return FALSE;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_add))) {
			// import as next available MRI
			num_mri++;
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_del))) {
			// kill all previous MRIs and restart from 1
			g_string_append_printf(cmd, "rm -rf $SUBJECTS_DIR/%s/mri/orig/[0-9][0-9][0-9]* ;\n", subject_name);
			num_mri = 1;
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_rep))) {
			// noting special
		}

		gtk_widget_destroy (dialog); // and it's children...
	} else {
		// typed free
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK_CANCEL,
			"%s is a new subject in FreeSurfer\n"
			"click on OK to continue creating it\n",
			subject_name);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			g_string_free(cmd, TRUE);
			gtk_widget_destroy (dialog);
			return FALSE;
		}
		gtk_widget_destroy (dialog);

		g_string_append_printf(cmd, "mksubjdirs \"$SUBJECTS_DIR/%s\" ;\n", subject_name);
		num_mri = 1;
	}


	g_string_append_printf(cmd, 
		"mkdir -p \"$SUBJECTS_DIR/%s/mri/orig/%03d/\" ;\n"
		"echo " TAG_PROGRESS " " TAG_RESCAN " %s ; \n", 
		subject_name, num_mri,
		subject_name);

	// temp to
	char *dcmname = NULL;

	inline build_script() {
		GPtrArray *slices = NULL;

		gtk_tree_model_get_value (GTK_TREE_MODEL (dicom_store), &dicom_iter, DICOM_SELECTOR_SLICES, &value);
		slices = (GPtrArray *) g_value_get_pointer(&value);
		num_slices = slices->len;
		for (i = 0; i < num_slices; i++) {
			const char * dcmfile = ((DICOM_SLICE_DATA *) slices->pdata[i])->fname;
			// add line to script
			g_string_append_printf(cmd,
				"%s '%s' \"$SUBJECTS_DIR/%s/mri/orig/%03d/\" ;\n"
				"echo " TAG_PROGRESS " %d ; \n",
				dircopy_cmd,
				dcmfile,
				subject_name, num_mri, i+1);
			// remember at least 1 name of dicom file
			if (! dcmname) {
				dcmname = strdup(strrchr(dcmfile, '/'));
			}
		}
	}

	memset(&value, 0, sizeof(GValue));
	if (gthread_search->len) {
		g_message("args : thread safe");

		g_mutex_lock (search_mutex);
		build_script();
		// the CMD script is a copy anyway, so once it's ready there's no need to take into account threads.
		g_mutex_unlock (search_mutex);
	} else {
		g_message("args : mono task");
		build_script();
	}
	g_value_unset(&value);

	// launch convert
	g_string_append_printf(cmd, 
		"mri_convert -it dicom -ot mgz"
			" \"$SUBJECTS_DIR/%s/mri/orig/%03d/%s\""
			" \"$SUBJECTS_DIR/%s/mri/orig/%03d.mgz\" ;\n", 
		subject_name, num_mri, (dcmname ? dcmname : ""),
		subject_name, num_mri);
	g_string_append(cmd, "echo " TAG_PROGRESS " " TAG_END " ; \n");

	// free up name
	if (dcmname) { 
		free(dcmname);
		dcmname = NULL;
	}

	g_message("import into FreeSurfer as %s", subject_name);
	
	// free data...
	cmd_final = g_string_free(cmd, FALSE);	 // FALSE - ...except for the actual command string

	// do it !
	import_run_script(cmd_final, num_slices);
	g_free(cmd_final);
	return FALSE;
}
