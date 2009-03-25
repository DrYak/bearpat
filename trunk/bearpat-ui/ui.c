#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "ui-freesurfer.h"
#include "ui-dicom.h"
#include "ui-import.h"
#include "ui-export.h"


// GTK :
// - http://www.gtk.org
//
// Documentation :
//	tutorial
// - http://www.gtk.org/tutorial/
//	api
// - http://developer.gnome.org/doc/API/2.0/gtk/index.html	- GTK
// - http://developer.gnome.org/doc/API/2.0/gobject/index.html	- GObject (also used by GTS)
// - http://developer.gnome.org/doc/API/2.0/glib/index.html	- GLib

static gboolean event_eject (GtkWidget *widget, gpointer   data ) {
	GError *error = NULL;
	gchar *target = (gchar *) data;

	gchar *argv[] = { "eject", target, NULL };

	g_message("run : %s(%s)", argv[0], argv[1]);
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

	return FALSE;
}


static gboolean event_bye (GtkWidget *widget, GdkEvent  *event, gpointer   data ) {
	gtk_main_quit ();
	return FALSE;
}

GtkWidget *main_window = NULL;

// all the common stuff needed to append a page to the notebook
static void notebook_texticon_vertical(GtkNotebook *notebook, GtkWidget *page, const gchar *stock_id, const gchar *label_text) {
	GtkWidget *tab_box;
	GtkWidget *icon;
	GtkWidget *label;
	
	tab_box = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (tab_box), 2);
	icon = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new (label_text);
	gtk_label_set_angle(GTK_LABEL(label), 90);
	gtk_box_pack_start (GTK_BOX (tab_box), label, FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (tab_box), icon,  FALSE, FALSE, 3);
	gtk_widget_show (icon);
	gtk_widget_show (label);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, tab_box);
	gtk_widget_show (tab_box);
	gtk_widget_show (page);
}

static void notebook_texticon_horizontal (GtkNotebook *notebook, GtkWidget *page, const gchar *stock_id, const gchar *label_text) {
	GtkWidget *tab_box;
	GtkWidget *icon;
	GtkWidget *label;
	
	tab_box = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (tab_box), 2);
	icon = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new (label_text);
	gtk_box_pack_start (GTK_BOX (tab_box), icon,  FALSE, FALSE, 3);
	gtk_box_pack_start (GTK_BOX (tab_box), label, FALSE, FALSE, 3);
	gtk_widget_show (icon);
	gtk_widget_show (label);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, tab_box);
	gtk_widget_show (tab_box);
	gtk_widget_show (page);
}



// tool to build a button from a stock icon and a text label
static GtkWidget *button_stock_label (const gchar *stock_id, const gchar *label_text) {
	GtkWidget *result;
	GtkWidget *button_box;
	GtkWidget *icon;
	GtkWidget *label;
	
	button_box = gtk_vbox_new (TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (button_box), 2);
	icon = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new (label_text);
	gtk_box_pack_start (GTK_BOX (button_box), icon,  TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (button_box), label, TRUE, TRUE, 3);
	gtk_widget_show (icon);
	gtk_widget_show (label);

	result = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (result), button_box);
	gtk_widget_show (button_box);

	return result;
}


// tool to build a button from a stock icon and a text label
static GtkWidget *button_png_label (const gchar *fname, const gchar *label_text) {
	GtkWidget *result;
	GtkWidget *button_box;
	GtkWidget *icon;
	GtkWidget *label;
	
	button_box = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (button_box), 2);
	icon = gtk_image_new_from_file (fname);

	label = gtk_label_new (label_text);
	gtk_box_pack_start (GTK_BOX (button_box), icon,  TRUE, TRUE, 3);
	gtk_box_pack_start (GTK_BOX (button_box), label, TRUE, TRUE, 3);
	gtk_widget_show (icon);
	gtk_widget_show (label);

	result = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (result), button_box);
	gtk_widget_show (button_box);

	return result;
}



#define MAKE_TEXT_COLUMN(TITLE,DATA)	\
gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),	\
	GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new_with_attributes (	\
		TITLE, gtk_cell_renderer_text_new(),	\
		"text", DATA,	\
		NULL)));

#define MAKE_PROG_COLUMN(TITLE,DATA)	\
gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),	\
	GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new_with_attributes (	\
		TITLE, gtk_cell_renderer_progress_new(),	\
		"value", DATA,	\
		NULL)));

#define MAKE_CHECK_COLUMN(TITLE,DATA)	\
gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),	\
	GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new_with_attributes (	\
		TITLE, gtk_cell_renderer_toggle_new(),	\
		"active", DATA,	\
		NULL)));



void build_interface() {
	GtkWidget *window;
	GtkWidget *notebook;

	// Main Window
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "Bear-pat");
	g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (event_bye), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 4);

	// tabs holder
	notebook = gtk_notebook_new ();

	
	GtkWidget *tab_select () {
		GtkWidget *vpaned;
		GtkWidget *hpaned;

		GtkWidget *pane_button() {
			GtkWidget *button;
			GtkWidget *box;
	
			// Holding box
			box = gtk_hbutton_box_new();
			gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_SPREAD );
	
			// Button - Directory scan
			button = button_stock_label (GTK_STOCK_DIRECTORY, "Directory scan");
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (event_scan), (gpointer) NULL);
			gtk_container_add (GTK_CONTAINER (box), button);
			gtk_widget_show (button);
		
			// Button - Directory scan
			button = button_stock_label (GTK_STOCK_HOME, "Home scan");
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (event_scan), (gpointer) getenv("HOME") ?: "~");
			gtk_container_add (GTK_CONTAINER (box), button);
			gtk_widget_show (button);
		
			
			// Button - CD scan
			button = button_stock_label (GTK_STOCK_CDROM, "Scan Media");
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (event_scan), (gpointer) "/media");
			gtk_container_add (GTK_CONTAINER (box), button);
			gtk_widget_show (button);
		
			// Button - CD scan
			button = button_stock_label (GTK_STOCK_GOTO_TOP,"CD Eject");
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (event_eject), (gpointer) "/dev/cdrom");
			gtk_container_add (GTK_CONTAINER (box), button);
			gtk_widget_show (button);
		
			gtk_widget_show (box);
			return box;
		}	

		GtkWidget *pane_list () {
			GtkWidget *tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (dicom_store));

			// !!! add pixbuf at number 1 !!!
		
			MAKE_TEXT_COLUMN("Type", DICOM_SELECTOR_SEQTYPE)
			MAKE_TEXT_COLUMN("Name", DICOM_SELECTOR_NAME)
			MAKE_TEXT_COLUMN("ID", DICOM_SELECTOR_ID)
			MAKE_TEXT_COLUMN("Date", DICOM_SELECTOR_DATE)
			MAKE_TEXT_COLUMN("Resolution", DICOM_SELECTOR_VOXSIZE)
			MAKE_TEXT_COLUMN("Sequence Name", DICOM_SELECTOR_SEQNAME)
			MAKE_TEXT_COLUMN("Images", DICOM_SELECTOR_NUMSLICES)

			dicom_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));

			GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
			gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				GTK_POLICY_AUTOMATIC,
				GTK_POLICY_AUTOMATIC);
			gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);
			gtk_widget_show (tree_view);

			gtk_widget_show (scrolled_window);
			return scrolled_window;
		}

		GtkWidget *pane_preview () {
			GtkWidget *box;
			GtkWidget *button;


			box = gtk_hbutton_box_new ();
			gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_END );


			button = button_png_label(aeskulap_ico, "Preview with\nAeskulap");
			g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (event_preview_aeskulap), NULL);

			gtk_widget_show (button);
			gtk_container_add (GTK_CONTAINER (box), button);

			gtk_widget_show (box);
			return box;
		}

		GtkWidget *pane_import_target () {
			GtkWidget *vert_division;
			GtkWidget *tabs;
			GtkWidget *target_chooser;
			GtkWidget *box;
			GtkWidget *button;
			GtkWidget *label;
			GtkWidget *buttonlist;

			GtkWidget *frame;

			vert_division = gtk_vbox_new (FALSE, 0);


			// box for options
			box = gtk_hbox_new (TRUE, 0);

			// General options : naming options
			buttonlist =  gtk_vbox_new (FALSE, 0);

			button = gtk_check_button_new_with_label ("Automatically propose names");
			g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_hint_names), NULL);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
			gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button));
			gtk_box_pack_start (GTK_BOX (buttonlist), button, FALSE, FALSE, 0);
			gtk_widget_show (button);

			button = gtk_check_button_new_with_label ("Use only last part of names");
			g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_hint_lastonly), NULL);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
			gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button));
			gtk_box_pack_start (GTK_BOX (buttonlist), button, FALSE, FALSE, 0);
			gtk_widget_show (button);


//			frame = gtk_frame_new ("Naming options");
//			gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
			frame = gtk_expander_new("Naming options");
			gtk_container_add (GTK_CONTAINER (frame), buttonlist);
			gtk_widget_show (buttonlist);

			gtk_box_pack_start (GTK_BOX (box), frame,  TRUE, TRUE, 3);
			gtk_widget_show (frame);

			// General options : copy / hardlink / symlink
			buttonlist =  gtk_vbox_new (FALSE, 0);

			button = gtk_radio_button_new_with_label (NULL, "Copy files");
			g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_dircopy_cmd), "cp");
			gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), TRUE);
			gtk_box_pack_start (GTK_BOX (buttonlist), button, FALSE, FALSE, 0);
			gtk_widget_show (button);

			button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button), "Hardlink files");
			g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_dircopy_cmd), "ln");
			gtk_box_pack_start (GTK_BOX (buttonlist), button, FALSE, FALSE, 0);
			gtk_widget_show (button);

			button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button), "Symlink files");
			g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_dircopy_cmd), "ln -s");
			gtk_box_pack_start (GTK_BOX (buttonlist), button, FALSE, FALSE, 0);
			gtk_widget_show (button);

//			frame = gtk_frame_new ("Saving options");
//			gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
			frame = gtk_expander_new("Saving options");

			gtk_container_add (GTK_CONTAINER (frame), buttonlist);
			gtk_widget_show (buttonlist);

			gtk_box_pack_start (GTK_BOX (box), frame,  TRUE, TRUE, 3);
			gtk_widget_show (frame);


			// (pack options)
			gtk_box_pack_start (GTK_BOX (vert_division), box,  TRUE, TRUE, 3);
			gtk_widget_show (box);



			// tabs holder
			tabs = gtk_notebook_new ();
			gtk_notebook_set_tab_pos (GTK_NOTEBOOK (tabs), GTK_POS_TOP);


			// Import : into free surfer
			box = gtk_vbox_new (FALSE, 0);
			buttonlist = gtk_hbox_new (FALSE, 0);

			label = gtk_label_new ("FreeSurfer subject name : ");
			gtk_box_pack_start (GTK_BOX (buttonlist), label,  FALSE, FALSE, 3);
			gtk_widget_show (label);

//			target_chooser = gtk_combo_box_new_with_model(GTK_TREE_MODEL (fssubj_store));
			target_chooser = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (fssubj_store), FSSUBJ_SELECTOR_NAME);
			g_signal_connect_swapped (G_OBJECT (dicom_sel), "changed", G_CALLBACK (event_hint_names), target_chooser);


			gtk_box_pack_start (GTK_BOX (buttonlist), target_chooser,  TRUE, TRUE, 3);
			gtk_widget_show (target_chooser);
			gtk_box_pack_start (GTK_BOX (box), buttonlist,  TRUE, FALSE, 3);
			gtk_widget_show (buttonlist);

			buttonlist = gtk_hbutton_box_new ();
			gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonlist), GTK_BUTTONBOX_END );
//			button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
			button = button_stock_label (GTK_STOCK_SAVE, "Import into FreeSurfer");
			g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK ( event_import_fs ), target_chooser);
			gtk_container_add (GTK_CONTAINER (buttonlist), button);
			gtk_widget_show (button);

			gtk_box_pack_start (GTK_BOX (box), buttonlist,  FALSE, FALSE, 3);
			gtk_widget_show (buttonlist);

			notebook_texticon_horizontal (GTK_NOTEBOOK (tabs), box, GTK_STOCK_CUT, "Import serie into FreeSurfer subject");

			// Import : into directory
			target_chooser = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER);

			box = gtk_hbutton_box_new ();
			gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_END );
//			button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
			button = button_stock_label (GTK_STOCK_SAVE, "Import into Directory");
			g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK ( event_dir_copy ), target_chooser);
			gtk_container_add (GTK_CONTAINER (box), button);
			gtk_widget_show (button);
			gtk_widget_show (box);

			gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (target_chooser ), box);
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (target_chooser), g_get_current_dir());
#if (GTK_MAJOR_VERSION > 2)||(GTK_MINOR_VERSION>=8)
			gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (target_chooser), TRUE);
#endif
			g_signal_connect_swapped (G_OBJECT (dicom_sel), "changed", G_CALLBACK (event_hint_names), target_chooser);

			notebook_texticon_horizontal (GTK_NOTEBOOK (tabs), target_chooser, GTK_STOCK_DIRECTORY, "Save sorted serie into directory");

			// (pack tabs)
			gtk_box_pack_start (GTK_BOX (vert_division), tabs,  TRUE, TRUE, 3);
			gtk_widget_show (tabs);
			gtk_widget_show (vert_division);
			return vert_division;
		}

		// panes between button and lis
		vpaned = gtk_vbox_new (FALSE, 4);
		gtk_box_pack_start (GTK_BOX (vpaned), pane_button (), FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vpaned), pane_list (), TRUE, TRUE, 0);
		gtk_widget_show (vpaned);

		// panes between selector and preview
		hpaned = gtk_hpaned_new ();
		gtk_paned_pack1(GTK_PANED (hpaned), vpaned, TRUE, FALSE);
		gtk_paned_pack2(GTK_PANED (hpaned), pane_preview (), FALSE, TRUE);
		gtk_widget_show (hpaned);

		// panes between everythin before, and target
		vpaned = gtk_vpaned_new ();
		gtk_paned_pack1(GTK_PANED (vpaned), hpaned, TRUE, FALSE);
		gtk_paned_pack2(GTK_PANED (vpaned), pane_import_target (), FALSE, FALSE);
	
		return vpaned;
	}

	GtkWidget *tab_segment () {
		GtkWidget *box = gtk_vbox_new (FALSE, 4);
		GtkWidget *buttonlist =  gtk_vbutton_box_new ();

		GtkWidget *tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (fssubj_store));

		MAKE_TEXT_COLUMN("Subject Name",	FSSUBJ_SELECTOR_NAME)
		MAKE_TEXT_COLUMN("MRI",	FSSUBJ_SELECTOR_NUM_ORIGS)
		MAKE_CHECK_COLUMN("Running",		FSSUBJ_SELECTOR_IS_RUNNING)
		MAKE_PROG_COLUMN("Accomplished",	FSSUBJ_SELECTOR_CALCULATED)

		fssubj_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));

		GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);
		gtk_widget_show (tree_view);

		gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonlist), GTK_BUTTONBOX_END );

		GtkWidget *button = button_stock_label (GTK_STOCK_APPLY, "Check with tkmedit");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_check_tkmedit ), NULL );
		gtk_container_add (GTK_CONTAINER (buttonlist), button);
		gtk_widget_show (button);

		button = button_stock_label (GTK_STOCK_APPLY, "Check with tksurfer");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_check_tksurfer ), NULL );
		gtk_container_add (GTK_CONTAINER (buttonlist), button);
		gtk_widget_show (button);

		GtkWidget *hpaned = gtk_hpaned_new ();
		gtk_paned_pack1(GTK_PANED (hpaned), scrolled_window, TRUE, FALSE);
		gtk_paned_pack2(GTK_PANED (hpaned), buttonlist, FALSE, TRUE);
		gtk_widget_show (hpaned);
		gtk_widget_show (buttonlist);

		gtk_box_pack_start (GTK_BOX (box), hpaned,  TRUE, TRUE, 0);
		gtk_widget_show (scrolled_window);
		gtk_widget_show (hpaned);

		GtkWidget *hbox = gtk_hbox_new (FALSE, 0);

		GtkWidget *label = gtk_label_new("Send finish message to email :");
		gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
		gtk_widget_show (label);


		GtkWidget *mail = gtk_entry_new();
		gtk_box_pack_start (GTK_BOX(hbox), mail, TRUE, TRUE, 0);
		gtk_widget_show (mail);

		gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);
		gtk_widget_show (hbox);

		hbox = gtk_hbutton_box_new ();
		gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END );
		gtk_box_set_spacing (GTK_BOX (hbox), 4);

		button = button_stock_label (GTK_STOCK_STOP, "Abort segmentation");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_abort_recon ), NULL );
		gtk_container_add (GTK_CONTAINER (hbox), button);
		gtk_widget_show (button);

		button = button_stock_label (GTK_STOCK_REFRESH, "Restart from Brainmask");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_restart_recon_bm ), mail );
		gtk_container_add (GTK_CONTAINER (hbox), button);
		gtk_widget_show (button);

		button = button_stock_label (GTK_STOCK_REFRESH, "Restart from White Matter");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_restart_recon_wm ), mail );
		gtk_container_add (GTK_CONTAINER (hbox), button);
		gtk_widget_show (button);

		button = button_stock_label (GTK_STOCK_REFRESH, "Restart from Pial surface");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_restart_recon_pial ), mail);
		gtk_container_add (GTK_CONTAINER (hbox), button);
		gtk_widget_show (button);

		button = button_stock_label (GTK_STOCK_CUT, "Launch segmentation");
		g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK ( event_launch_recon ), mail);
		gtk_container_add (GTK_CONTAINER (hbox), button);
		gtk_widget_show (button);

		gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);
		gtk_widget_show (hbox);
		gtk_widget_show (box);


		return box;
	}

	GtkWidget *tab_export3d () {
		GtkWidget *paned = gtk_vpaned_new ();

		GtkWidget *tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (fssubj_store));

		MAKE_TEXT_COLUMN("Subject Name",	FSSUBJ_SELECTOR_NAME)
		MAKE_TEXT_COLUMN("MRI",	FSSUBJ_SELECTOR_NUM_ORIGS)
		MAKE_CHECK_COLUMN("Running",		FSSUBJ_SELECTOR_IS_RUNNING)
		MAKE_PROG_COLUMN("Accomplished",	FSSUBJ_SELECTOR_CALCULATED)

		fssurf_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));

		GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
			GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), tree_view);
		gtk_widget_show (tree_view);

		gtk_paned_pack1(GTK_PANED (paned), scrolled_window, TRUE, FALSE);
		gtk_widget_show (scrolled_window);

		GtkWidget *box = gtk_vbox_new (FALSE, 4);


		// export options
		GtkWidget *vbox = gtk_vbox_new (FALSE, 4);

		// side
		GtkWidget *group_box = gtk_vbox_new (FALSE, 4);

		GtkWidget *button = gtk_radio_button_new_with_label (NULL, "Left");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_side), (gpointer) LEFT);
		gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), TRUE);
		gtk_box_pack_start (GTK_BOX (group_box), button, FALSE, FALSE, 0);
		gtk_widget_show (button);

		button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button), "Right");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_side), (gpointer) RIGHT);
		gtk_box_pack_start (GTK_BOX (group_box), button, FALSE, FALSE, 0);
		gtk_widget_show (button);

		button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button), "Both");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_side), (gpointer) BOTH );
		gtk_box_pack_start (GTK_BOX (group_box), button, FALSE, FALSE, 0);
		gtk_widget_show (button);

//		GtkWidget *group_frame = gtk_frame_new ("Hemisphere");
//		gtk_frame_set_shadow_type (GTK_FRAME (group_frame), GTK_SHADOW_ETCHED_IN);
		GtkWidget *group_frame = gtk_expander_new("Naming options");
		gtk_expander_set_expanded(GTK_EXPANDER(group_frame), TRUE);
		gtk_container_add (GTK_CONTAINER (group_frame), group_box);
		gtk_widget_show (group_box);
		gtk_box_pack_start (GTK_BOX (vbox), group_frame,  FALSE, FALSE, 0);
		gtk_widget_show (group_frame);


		// clip
		group_box = gtk_table_new (5, 5, TRUE);

		inline table_input(int x, int y, const char *title, double *target) {
			GtkWidget *input_box = gtk_hbox_new (FALSE, 0);

			GtkWidget *label = gtk_label_new(title);
			gtk_box_pack_start (GTK_BOX (input_box), label,  FALSE, FALSE, 4);
			gtk_widget_show (label);

			GtkWidget *spin = gtk_spin_button_new_with_range(0.0, 200.0, 0.5);
			g_signal_connect (G_OBJECT(spin), "value-changed", G_CALLBACK (event_set_coor), target);
			gtk_box_pack_start (GTK_BOX (input_box), spin,  FALSE, FALSE, 0);
			gtk_widget_show (spin);
		
			gtk_table_attach_defaults(GTK_TABLE(group_box), input_box, x, x+1, y+2, y+3);
			gtk_widget_show (input_box);
		}

		table_input(0, 0, "Superior",	&clip_limit.top);
		table_input(0, 2, "Inferior",	&clip_limit.bottom);
		table_input(3, 0, "Anterior",	&clip_limit.front);
		table_input(3, 2, "Posterior",	&clip_limit.back);
		table_input(2, 1, "Right",	&clip_limit.right);
		table_input(4, 1, "Left",	&clip_limit.left);

		button = gtk_radio_button_new_with_label (NULL, "No clip");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_clip), (gpointer) CLIP_NONE);
		gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), TRUE);
		gtk_table_attach_defaults(GTK_TABLE(group_box), button, 0, 1, 0, 1);
		gtk_widget_show (button);

		button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button), "Inside (slice, part)");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_clip), (gpointer) CLIP_INSIDE);
		gtk_table_attach_defaults(GTK_TABLE(group_box), button, 2, 3, 0, 1);
		gtk_widget_show (button);

		button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button), "Outside (pole, side)");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_clip), (gpointer) CLIP_OUTSIDE);
		gtk_table_attach_defaults(GTK_TABLE(group_box), button, 4, 5, 0, 1);
		gtk_widget_show (button);

//		group_frame = gtk_frame_new ("Cut (milimeters, anatomic orientation)");
//		gtk_frame_set_shadow_type (GTK_FRAME (group_frame), GTK_SHADOW_ETCHED_IN);
		group_frame = gtk_expander_new("Cut (milimeters, anatomic orientation)");
		gtk_expander_set_expanded(GTK_EXPANDER(group_frame), TRUE);
		gtk_container_add (GTK_CONTAINER (group_frame), group_box);
		gtk_widget_show (group_box);
		gtk_box_pack_start (GTK_BOX (vbox), group_frame,  FALSE, FALSE, 0);
		gtk_widget_show (group_frame);


		// removing part
		button = gtk_check_button_new_with_label ("Take out white matter");
		g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (event_set_takeout), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
		gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(button));
		
//		group_frame = gtk_frame_new ("Removing parts");
//		gtk_frame_set_shadow_type (GTK_FRAME (group_frame), GTK_SHADOW_ETCHED_IN);
		group_frame = gtk_expander_new("Removing parts");
		gtk_expander_set_expanded(GTK_EXPANDER(group_frame), TRUE);
		gtk_container_add (GTK_CONTAINER (group_frame), button);
		gtk_widget_show (button);
		gtk_box_pack_start (GTK_BOX (vbox), group_frame,  FALSE, FALSE, 0);
		gtk_widget_show (group_frame);

			// pack it
		GtkWidget *frame = gtk_frame_new ("Export to STL options");
		gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
		gtk_container_add (GTK_CONTAINER (frame), vbox);
		gtk_widget_show (vbox);

			// end of main frame
		gtk_box_pack_start (GTK_BOX (box), frame,  FALSE, FALSE, 0);
		gtk_widget_show (frame);

		// target file
		GtkWidget *target_chooser = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (target_chooser), g_get_current_dir());
#if (GTK_MAJOR_VERSION > 2)||(GTK_MINOR_VERSION>=8)
		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER (target_chooser), TRUE);
#endif
		gtk_box_pack_start (GTK_BOX (box), target_chooser,  TRUE, TRUE, 0);
		gtk_widget_show (target_chooser);



		// buttons
		frame = gtk_hbutton_box_new ();
		gtk_button_box_set_layout (GTK_BUTTON_BOX (frame), GTK_BUTTONBOX_END );
		gtk_box_set_spacing (GTK_BOX (frame), 4);

		button = button_png_label(k3b_ico, "Burn to CD-R\nusing K3B");
		g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK ( event_burnk3b ), target_chooser );
		gtk_container_add (GTK_CONTAINER (frame), button);
		gtk_widget_show (button);

		button = button_png_label(meshlab_ico, "Preview with\nMeshlab");
		g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK ( event_meshlab ), target_chooser );
		gtk_container_add (GTK_CONTAINER (frame), button);
		gtk_widget_show (button);

		button = button_stock_label (GTK_STOCK_CONVERT, "Export 3D surface");
		g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK ( event_savesurf ), target_chooser );
		gtk_container_add (GTK_CONTAINER (frame), button);
		gtk_widget_show (button);

		gtk_box_pack_start (GTK_BOX (box), frame,  FALSE, FALSE, 0);
		gtk_widget_show (frame);
		
		gtk_paned_pack2(GTK_PANED (paned), box, FALSE, FALSE);
		gtk_widget_show (box);
		gtk_widget_show (paned);

		return paned;
	}

	GtkWidget *tab_about () {
		GtkWidget *holder = gtk_label_new ("Andra moi ennepe, mousa, polytropon...");
//		gtk_hbox_new (FALSE, 0);
		return holder;
	}

	// Append to tabs
	notebook_texticon_vertical(GTK_NOTEBOOK (notebook), tab_select(), GTK_STOCK_FIND, "Find DICOMs");
	notebook_texticon_vertical(GTK_NOTEBOOK (notebook), tab_segment(), GTK_STOCK_CUT, "Segment");
	notebook_texticon_vertical(GTK_NOTEBOOK (notebook), tab_export3d(), GTK_STOCK_CONVERT, "Export 3D");
	notebook_texticon_vertical(GTK_NOTEBOOK (notebook), tab_about(), GTK_STOCK_ABOUT, "About");
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);

	// done
	gtk_container_add (GTK_CONTAINER (window), notebook);
	gtk_widget_show (notebook);
	gtk_widget_show (window);

	main_window = window;
}

