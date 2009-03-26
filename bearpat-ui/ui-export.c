// ISOC99 is used for isfinite() function
#define _ISOC99_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <locale.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "ui-freesurfer.h"
#include "ui-export.h"

struct CLIP_LIMIT clip_limit = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

static SIDE side = LEFT;
static CLIP clip = CLIP_NONE;
static COOR_SYS coor = COOR_FREESURFER;
static gboolean take_out = FALSE;
static gboolean mod_param = TRUE;

char *surf_edit = NULL;
char *meshlab_cmd = NULL;
char *meshlab_ico = NULL;
char *k3b_cmd = NULL;
char *k3b_ico = NULL;

gboolean event_set_side (GtkWidget *widget, gpointer data ) {
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		int s = (int) data;
	
		if ((s == LEFT) || (s == RIGHT) || (s == BOTH)) {
			mod_param = TRUE;
			side = s;
		}
		g_message("side %d", side);
	}
	return FALSE;
}

gboolean event_set_clip (GtkWidget *widget, gpointer data ) {
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		int c = (int) data;
	
		if ((c == CLIP_NONE) || (c == CLIP_INSIDE) || (c == CLIP_OUTSIDE)) {
			mod_param = TRUE;
			clip = c;
		}
		g_message("clip %d", clip);
	}
	return FALSE;
}

gboolean event_set_coor (GtkWidget *widget, gpointer data) {
	double *d = (double *) data;

	*d = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	mod_param = TRUE;

	return FALSE;
}

gboolean event_set_coorsys (GtkWidget *widget, gpointer data ) {
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		int c = (int) data;
		
		if ((c == COOR_FREESURFER) || (c == COOR_SCANNER) || (c == COOR_MIMICS)) {
			mod_param = TRUE;
			coor = c;
		}
		g_message("coordinate system %d", coor);
	}
	return FALSE;
}

gboolean event_set_takeout(GtkWidget *widget, gpointer data) {
	take_out = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	g_message("with%s white matter", take_out ? "" : "out");
	mod_param = TRUE;

	return FALSE;
}


#define TAG_PROGRESS	"progress_script"
#define TAG_END	"end"

static double save_status_max = 1.0;
static double save_last = 0.0;

static gboolean save_update_status (GIOChannel *source, GIOCondition condition, gpointer data) {
	GError *error = NULL;
	gchar *line = NULL;
	gsize l = 0, t = 0;
	GIOStatus s;

	switch ((s = g_io_channel_read_line (source, &line, &l, &t, &error))) {
		case G_IO_STATUS_EOF:
			g_message ("end of save surface status pipe");
			if ((line == NULL) || (l == 0)) {
				if (line) g_free(line);
				if (error) g_free(error);
				error = NULL;
				g_io_channel_shutdown(source, FALSE, &error); // kill stream if we are done with it
				if (error) g_free(error);
				gtk_widget_destroy (GTK_WIDGET(gtk_widget_get_parent(GTK_WIDGET(gtk_widget_get_parent(GTK_WIDGET(GTK_PROGRESS_BAR(data))))))); // kill window
				return FALSE; // don't restart after end
			}
			break;

		case G_IO_STATUS_ERROR:
			g_printerr ("save surface status pipe error : %s", error->message);
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
	GtkLabel *label = GTK_LABEL(gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(GTK_WIDGET(bar)))) -> data);
	// vbox -> pbar, then vbox -> first

	if (strcmp(col[0], TAG_PROGRESS) == 0) {
		if ((strcmp(col[1], TAG_END) == 0) || (strstr(line, "assert") > 0)) {
			g_message("end of save surface script");
			// close dialog
			gtk_widget_destroy (GTK_WIDGET(dialog));	//	children are auto-destroyed

			g_strfreev(col);
			g_free(line);
			g_io_channel_shutdown(source, FALSE, &error);  // kill stream - we are done with it
			if (error) g_free(error);
			return FALSE; // dont retry
		}  else {
			int i = atoi(col[1]);
			char *t = g_strdup_printf ("%d / %.0f", i+1, save_status_max);
			gtk_progress_bar_set_fraction(bar, (save_last = (double) i) / save_status_max);
			gtk_progress_bar_set_text(bar,t);
			g_free(t);

			if (col [2]) {
				t = g_strdup_printf ("%s %s", col[2], col[3] ?: "");
//				gtk_window_set_title (dialog, t );
				gtk_label_set_text (label, t );
				g_free(t);
			}
		}
	} else if (strcmp(col[0], "CSG:") == 0) {
		char *t = g_strdup_printf ("%s %s", col[1], col[2] ?: "");
		gtk_progress_bar_set_fraction(bar, (save_last += 1.0 / 6.0) / save_status_max);
		gtk_progress_bar_set_text(bar,t);
		g_free(t);
	} else  if (t > 0)
		g_message("from %s : <%s>", surf_edit, line);

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


static char * TAGS [] = {
"\n" TAG_PROGRESS " 0 ",
"\n" TAG_PROGRESS " 1 ",
"\n" TAG_PROGRESS " 2 ",
"\n" TAG_PROGRESS " 3 ",
"\n" TAG_PROGRESS " 4 ",
"\n" TAG_PROGRESS " 5 ",
"\n" TAG_PROGRESS " 6 ",
"\n" TAG_PROGRESS " 7 ",
"\n" TAG_PROGRESS " 8 ",
"\n" TAG_PROGRESS " 9 ",
"\n" TAG_PROGRESS " 10 ",
"\n" TAG_PROGRESS " 11 ",
"\n" TAG_PROGRESS " 12 ",
NULL };

#define MAX_ARG 128

gboolean event_savesurf(GtkWidget *widget, gpointer data) {
	GtkTreeIter iter;
	GValue value;
	char *argv[MAX_ARG] = { NULL };
	char *fname1 = NULL;
	char *fname2 = NULL;
	char *fname3 = NULL;
	char *fname4 = NULL;
	char *matname1 = NULL;
	char *matname2 = NULL;
	char *matname3 = NULL;
	char *box = NULL;
	char *subj = NULL;
	char *target = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	char *subjdir = freesurfer_subjects;
	int i = 0, tags = 0;
	GError *error;
	
//	char *subjdir = freesurfer_subjects;

	if ((target == NULL) || (strlen(target) == 0)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"No target file selected\n"
			"Please select a target file");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	} else {
		if (name_error(target, "target file", TRUE))
			return FALSE;
	}


	if (! gtk_tree_selection_get_selected (fssurf_sel, (GtkTreeModel **)NULL, &iter)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			(gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fssubj_store), &iter)) 
			? 	"No FreeSurfer subject selected.\n"
				"Please select a FreeSurfer subject first."
			:	"No FreeSurfer subject found.\n"
				"Please create FreeSurfer subject first,\n"
				"and import a DICOM serie into it.");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	if (fssubj_num_running > 0) {
		memset(&value, 0, sizeof(GValue));
		gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_IS_RUNNING, &value);
		// is the selected one running ?
		if (g_value_get_boolean(&value)) {

			// we are going to crash !
			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK_CANCEL,
				"This subject is still segmented by FreeSurfer.\n"
				"Some surface may not be ready yet.");
	
			if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
				g_value_unset(&value);
				gtk_widget_destroy (dialog);
				return FALSE;
			}
	
			gtk_widget_destroy (dialog);
		}
		g_value_unset(&value);
	}

	// set locale
	if (!setlocale (LC_ALL, "POSIX"))
		g_warning ("cannot set locale to POSIX");

	// subject name
	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
	subj = g_strdup((char *) g_value_get_string(&value));
	g_value_unset(&value);

	
	// get matrices
	if ((coor == COOR_SCANNER) || (coor == COOR_MIMICS)) {
		matname1 = mktemp(strdup("/tmp/vox2ras.XXXXXX"));
		matname2 = mktemp(strdup("/tmp/vox2ras-tkr.XXXXXX"));
		if (coor == COOR_MIMICS)
			matname3 = mktemp(strdup("/tmp/orig.XXXXXX"));			
		dump_fsmat(subj, matname1, matname2, matname3);
	}
	
#define add_arg(a)	\
	if (i == MAX_ARG-1) {	\
		g_message("out of args");	\
	\
		if (matname1) { unlink(matname1); free(matname1); } \
		if (matname2) { unlink(matname2); free(matname2); } \
		if (matname3) { unlink(matname3); free(matname3); } \
	\
		if (subj)	g_free(subj);	\
		if (fname1)	g_free(fname1);	\
		if (fname2)	g_free(fname2);	\
		if (fname3)	g_free(fname3);	\
		if (fname4)	g_free(fname4);	\
		if (box)	g_free(box);	\
	\
		return FALSE;	\
	}	\
	argv[i] = a;	\
	i++;	\
	argv[i] = NULL;

#define add_tag(str)	\
	add_arg("print");	\
	add_arg(TAGS[tags++]);	\
	add_arg("print");	\
	add_arg(str);

	add_arg(surf_edit);
	switch(side) {
		case LEFT:
			add_tag("Loading left-grey-matter\n");
			add_arg("load");
			add_arg(fname1 = g_strdup_printf("%s/%s/surf/lh.pial", subjdir, subj));
			break;
		case RIGHT:
			add_tag("Loading right-grey-matter\n");
			add_arg("load");
			add_arg(fname1 = g_strdup_printf("%s/%s/surf/rh.pial", subjdir, subj));
			break;
		case BOTH:
			add_tag("Loading left-grey-matter\n");
			add_arg("load");
			add_arg(fname1 = g_strdup_printf("%s/%s/surf/lh.pial", subjdir, subj));
			add_tag("Loading right-grey-matter");
			add_arg("load");
			add_arg(fname2 = g_strdup_printf("%s/%s/surf/rh.pial", subjdir, subj));
			add_tag("Combining grey-matter\n");
			add_arg("union");
			add_arg("freeall");
			add_arg("1");
			break;
	}
	if (clip != CLIP_NONE) {
		add_tag("Cliping grey-matter\n");
		if (clip == CLIP_INSIDE) {
			add_arg("box");
		} else {
			add_arg("invbox");
		}
		add_arg(fname1 = g_strdup_printf("%f,%f,%f,%f,%f,%f", clip_limit.left, clip_limit.back, clip_limit.bottom, clip_limit.right, clip_limit.front, clip_limit.top));
		add_arg("inter");
		add_arg("freeall");
		add_arg("1");
	}
	if ((clip != CLIP_NONE) && take_out) {
		if ((side==LEFT) || (side==BOTH)) {
			add_tag("Loading left-white-matter\n");
			add_arg("load");
			add_arg(fname3 = g_strdup_printf("%s/%s/surf/lh.white", subjdir, subj));
			add_tag("Removing left-white-matter\n");
			add_arg("diff");
			add_arg("freeall");
			add_arg("1");
		}
		if ((side==RIGHT) || (side==BOTH)) {
			add_tag("Loading right-white-matter\n");
			add_arg("load");
			add_arg(fname4 = g_strdup_printf("%s/%s/surf/rh.white", subjdir, subj));
			add_tag("Removing left-white-matter\n");
			add_arg("diff");
			add_arg("freeall");
			add_arg("1");
		}
	}
	
	// transform into native coordinates
	if ((coor == COOR_SCANNER) || (coor == COOR_MIMICS)) {
		add_tag("Transforming coordinates\n");
		add_arg("fsmat");
		add_arg(matname1);
		add_arg(matname2);
		add_arg("transform");
		if (coor == COOR_MIMICS) {
			add_arg("loadmat");
			add_arg(matname3);
			add_arg("transform");
		}
	}
	
	add_tag("Saving output\n");
	add_arg("save");
	add_arg(target);
	add_arg("print");
	add_arg(TAG_PROGRESS " " TAG_END);
	add_arg("end");

	g_message("run surface export script ( %s )", argv[0]);

	int fdes;
	if (!g_spawn_async_with_pipes(NULL,	// cwd
		argv,
		NULL, // inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL, // no GSpawnChildSetupFunc child_setup,
		NULL,
		NULL,	// no pid
		NULL,	// no stdin
                NULL,	// no stdout
		&fdes,	// sotre descriptor to build stream later
		&error)
	) {
		g_printerr ("Failed to launch %s\n error : %s\nscript :\n%s\n", argv[0], error->message, argv[1]);
		g_free (error);
		if (matname1) { unlink(matname1); free(matname1); }
		if (matname2) { unlink(matname2); free(matname2); }
		if (matname3) { unlink(matname3); free(matname3); }
		if (fname1)	g_free(fname1);
		if (fname2)	g_free(fname2);
		if (fname3)	g_free(fname3);
		if (fname4)	g_free(fname4);
		if (subj)	g_free(subj);
		if (box)	g_free(box);
		return FALSE;
	}

	mod_param = FALSE;

	if (matname1) { free(matname1); }
	if (matname2) { free(matname2); }
	if (matname3) { free(matname3); }
	if (fname1)	g_free(fname1);
	if (fname2)	g_free(fname2);
	if (fname3)	g_free(fname3);
	if (fname4)	g_free(fname4);
	if (subj)	g_free(subj);
	if (box)	g_free(box);

	GtkWidget *dialog = gtk_dialog_new_with_buttons (
		"Exporting surface",
		GTK_WINDOW(main_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), "Exporting surface");
//	gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);

	GtkWidget *label = gtk_label_new("Exporting surface");
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, FALSE, 0);
	gtk_widget_show (label);

	GtkWidget *bar = gtk_progress_bar_new();
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), bar, TRUE, FALSE, 0);
	gtk_widget_show (bar);
	gtk_widget_show (dialog);

	save_status_max = (double) tags;
	GIOChannel *input_chan = g_io_channel_unix_new (fdes);	
	g_io_add_watch(input_chan, G_IO_IN | G_IO_PRI, save_update_status, bar);
	// pid is not destroyed !!! (important for Windows)
	// not important for Unix
	// important for Windows
	// must change to G_IO_HUP signal handler


	return FALSE;
}








gboolean event_meshlab(GtkWidget *widget, gpointer data) {
	GtkTreeIter iter;
	char *target = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	int i = 0;
	GError *error;
	
	if ((target == NULL) || (strlen(target) == 0)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"No target file selected\n"
			"Please select a target file");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	} else {
		if (name_error(target, "target file", TRUE))
			return FALSE;
	}

// check file exists
	if (mod_param) {
		// we are going to crash !
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
			"File not saved using this parameters\n"
			"Do you want to export it again ?\n"
			"(otherwise file will be read from disc)");

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_NO) {
			event_savesurf(widget, data);
		}

		gtk_widget_destroy (dialog);
	} 

	char *argv[] = { meshlab_cmd, target, NULL };

	g_message("run %s ( %s )", argv[0], argv[0]);
	if (!g_spawn_async(NULL,	// cwd
		argv,
		NULL, // inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL, NULL, // no GSpawnChildSetupFunc child_setup,
		NULL,  // no pid
		&error)
	) {
		g_printerr ("Failed to launch %s (%s) : error : %s\n", argv[0], argv[1], error->message);
		g_free (error);
		return FALSE;
	}

	return FALSE;
}


gboolean event_burnk3b(GtkWidget *widget, gpointer data) {
	GtkTreeIter iter;
	char *target = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	int i = 0;
	GError *error;
	
	if ((target == NULL) || (strlen(target) == 0)) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"No target file selected\n"
			"Please select a target file");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	} else {
		if (name_error(target, "target file", TRUE))
			return FALSE;
	}

// check file exists
	if (mod_param) {
		// we are going to crash !
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
			"File not saved using this parameters\n"
			"Do you want to export it again ?\n"
			"(otherwise file will be read from disc)");

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_NO) {
			event_savesurf(widget, data);
		}

		gtk_widget_destroy (dialog);
	} 

	char *argv[] = { k3b_cmd, "--datacd", target, NULL };

	g_message("run %s ( %s )", argv[0], argv[0]);
	if (!g_spawn_async(NULL,	// cwd
		argv,
		NULL, // inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL, NULL, // no GSpawnChildSetupFunc child_setup,
		NULL,  // no pid
		&error)
	) {
		g_printerr ("Failed to launch %s ( --datacd %s) : error : %s\n", argv[0], argv[2], error->message);
		g_free (error);
		return FALSE;
	}

	return FALSE;
}



