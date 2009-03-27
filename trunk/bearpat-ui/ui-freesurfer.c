#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "ui-freesurfer.h"
//#include "dicom.h"	// for building Mimics' matrix

// FreeSurfer home page :
// -	http://surfer.nmr.mgh.harvard.edu/	
//
// Obtaining :
// -	http://surfer.nmr.mgh.harvard.edu/registration.html	-	Registration
// -	https://surfer.nmr.mgh.harvard.edu/fswiki/Download	-	Download
//
// Using :
// -	https://surfer.nmr.mgh.harvard.edu/fswiki/SetupConfiguration	-	Starting
// -	https://surfer.nmr.mgh.harvard.edu/fswiki/TestingFreeSurfer	-	Starting
// -	https://surfer.nmr.mgh.harvard.edu/fswiki/ReconAllDevTable	-	List of steps
// -	https://surfer.nmr.mgh.harvard.edu/fswiki/FsTutorial	-	Tutorial for controlling pipe-line



char *freesurfer_home = NULL;	// FREESURFER_HOME
char *freesurfer_subjects = NULL;	// SUBJECTS_DIR




/* correspondance between STEPS and Touch files :
gawk '$1~/#(----)+/||/\$touchdir/' /usr/local/freesurfer/bin/recon-all
#----------- Motion Correct and Average ------------------#
      echo $cmd > $touchdir/motion_correct.touch
    echo $cmd > $touchdir/conform.touch
#----------- Nu Intensity Correction ------------------#
  touch $touchdir/nu.touch
#----------- Talairach ------------------#
  echo $cmd > $touchdir/talairach.touch
#----------- Defacing ------------------#
  touch $touchdir/deface.touch
#----------- Intensity Normalization1 ------------------#
  echo $cmd > $touchdir/inorm1.touch
#----------- Skull Stripping ------------------#
  echo $cmd > $touchdir/skull_strip.touch
#-------------- GCA Registration  -------------------------#
  echo $cmd > $touchdir/em_register.touch
#------------ Canonical Normalization --------------#
  echo $cmd > $touchdir/ca_normalize.touch
#------------ Canonical Registration --------------#
  echo $cmd > $touchdir/ca_register.touch
#------------ Inverse of Canonical Registration --------------#
  echo $cmd > $touchdir/ca_register_inv.touch
#------------ Removes neck and part of the face  --------------#
  echo $cmd > $touchdir/mri_remove_neck.touch
#------------ Recompute lta with skull but no neck  --------------#
  echo $cmd > $touchdir/skull.lta.touch
#-------------- SubCort Segmentation --------------#
  echo $cmd > $touchdir/ca_label.touch
#-------------- ASeg Stats --------------#
  echo $cmd > $touchdir/segstats.touch
  echo $cmd > $touchdir/inorm2.touch
#---------------- WM Segmentation --------------------------#
  echo $cmd > $touchdir/wmsegment.touch
#---------------- Fill --------------------------#
  echo $cmd > $touchdir/fill.touch
  #---------------- Tessellate --------------------------#
    echo $cmd > $touchdir/$hemi.tessellate.touch
  #---------------- Smooth1 --------------------------#
    echo $cmd > $touchdir/$hemi.smoothwm1.touch
  #---------------- Inflate1 --------------------------#
    echo $cmd > $touchdir/$hemi.inflate1.touch
  #---------------- QSphere --------------------------#
    echo $cmd > $touchdir/$hemi.qsphere.touch
  #---------------- Fix Topology --------------------------#
    echo $cmd > $touchdir/$hemi.topofix.touch
  #---------------- Make Final Surfaces --------------------------#
    echo $cmd > $touchdir/$hemi.final_surfaces.touch
  #---------------- Smooth2 --------------------------#
    echo $cmd > $touchdir/$hemi.smoothwm2.touch
  #---------------- Inflate2 --------------------------#
    echo $cmd > $touchdir/$hemi.inflate2.touch
  #---------------- Cortical Ribbon -------------------------#
    echo $cmd > $touchdir/$hemi.cortical_ribbon.touch
  #---------------Begin Morph --------------------------------#
  #---------------- Sphere --------------------------#
    echo $cmd > $touchdir/$hemi.sphmorph.touch
  #---------------- Surface Registration --------------------------#
    echo $cmd > $touchdir/$hemi.sphreg.touch
  #---------------- Surface Registration --------------------------#
    echo $cmd > $touchdir/$hemi.jacobian.touch
  #-------- Contra Surface Registration ----------------#
    echo $cmd > $touchdir/$hemi.sphreg.contra.touch
  #---------------- Average Curv for Display----------------------#
    echo $cmd > $touchdir/$hemi.avgcurv.touch
  #---------------- Cortical Parcellation------------------------#
    echo $cmd > $touchdir/$hemi.aparc.touch
    echo $cmd > $touchdir/$hemi.aparcstats.touch
  #---------------- Cortical Parcellation 2------------------------#
    echo $cmd > $touchdir/$hemi.aparc2.touch
    echo $cmd > $touchdir/$hemi.aparcstats2.touch
  echo $cmd > $touchdir/aparc2aseg.touch

*/

static const char *steps_list[] = {
	"motion_correct",
	"conform",
	"nu",
	"talairach",
	"deface",
	"inorm1",
	"skull_strip",
	"em_register",
	"ca_normalize",
	"ca_register",
	"ca_register_inv",
	"mri_remove_neck",
	"skull.lta",
	"ca_label",
	"segstats",
	"inorm2",
	"wmsegment",
	"fill",
	"lh.tessellate",
	"lh.smoothwm1",
	"lh.inflate1",
	"lh.qsphere",
	"lh.topofix",
	"lh.final_surfaces",
	"lh.smoothwm2",
	"lh.inflate2",
	"lh.cortical_ribbon",
	"lh.sphmorph",
	"lh.sphreg",
	"lh.jacobian",
	"lh.sphreg.contra",
	"lh.avgcurv",
	"lh.aparc",
	"lh.aparcstats",
	"lh.aparc2",
	"lh.aparcstats2",
	"rh.tessellate",
	"rh.smoothwm1",
	"rh.inflate1",
	"rh.qsphere",
	"rh.topofix",
	"rh.final_surfaces",
	"rh.smoothwm2",
	"rh.inflate2",
	"rh.cortical_ribbon",
	"rh.sphmorph",
	"rh.sphreg",
	"rh.jacobian",
	"rh.sphreg.contra",
	"rh.avgcurv",
	"rh.aparc",
	"rh.aparcstats",
	"rh.aparc2",
	"rh.aparcstats2",
	"aparc2aseg",
	NULL };

GtkListStore *fssubj_store = NULL;
GtkTreeSelection *fssubj_sel = NULL; 
GtkTreeSelection *fssurf_sel = NULL; 
int fssubj_num_running = 0;	// total of "fssubj" that have "_IS_RUNNING" TRUE
static guint fssubj_timeout = 0; // auto-rescan timeout (0 when absent)
// seconds
#define RESCAN_INTERVAL 10 

#define TAG_SCAN_FSSUBJ_SCRIPT	"scan_fssubj_script"
#define TAG_SCREEN_SCRIPT	"screen_script"
#define TAG_SCREEN	"BEARPAT"
#define TAG_START	"start"
#define TAG_FOUND	"found"
#define TAG_END	"end"


GString *freesurfer_script() {
	GString *r = g_string_new("");
	g_string_append_printf(r, "export FREESURFER_HOME=%s ;\n", freesurfer_home);
	if (freesurfer_subjects && strlen(freesurfer_subjects))
		g_string_append_printf(r, "export SUBJECTS_DIR=%s ;\n", freesurfer_subjects);
	g_string_append_printf(r, ". \"$FREESURFER_HOME/SetUpFreeSurfer.sh\" ;\n");

	return r;
}

static gboolean scan_timeout(gpointer data) {
	scan_fssubj();

	// Handle re-scan timer
	if (fssubj_num_running > 0)
		return TRUE;
	else  {
		fssubj_timeout = 0; // timer is destroyed when returning FALSE
		return FALSE;
	}
}

void scan_fssubj() {
	GString *script = freesurfer_script();
	GError *error = NULL;
	gchar *cmd = NULL, *output = NULL, *err = NULL;
	gchar **ptr, **output_split;
	int data_is_comming = 0, i, max_touch = 0;
	
	g_string_append(script,
		"echo " TAG_SCAN_FSSUBJ_SCRIPT " " TAG_START " ;\n"
		"SCREENS=`screen -list|grep '" TAG_SCREEN "'` ; \n"
//		"for SUBJ in `find $SUBJECTS_DIR/* -maxdepth 0 -type d -printf '%f\\n'` ; do\n"
		"for SUBJ in `find $SUBJECTS_DIR/* -maxdepth 0 -type d -print | grep -oE '[^/]*$'` ; do\n"
		"	MRIS=0 ; \n"
		"	(( i=1 )) ; \n"
		"	while (( i < 999 )); do\n"
		"		ORIG_PATH=$SUBJECTS_DIR/$SUBJ/mri/orig/`printf '%03d' $i` ; \n"
		"		if [ -e $ORIG_PATH/ ] || [ -e $ORIG_PATH.mgz ]; then \n"
		"			MRIS=$i ;\n"
		"			(( i++ )) ;\n"
		"		else \n"		
		"			break ;\n"
		"		fi ; \n"
		"	done ;\n");

	// steps are static const
	// if compiler doesn't I should move this part out of the function
	for (max_touch = 0; steps_list[max_touch]; max_touch++)
		;

	for (i = 0; i < max_touch; i++) 
		g_string_append_printf (script,
			"	%s [ -e $SUBJECTS_DIR/$SUBJ/touch/%s.touch ]; then \n"
			"		STEPS=\"%d\"; \n",
			i ? "elif" : "if",
			steps_list[max_touch - 1 - i],
			max_touch - i);

	g_string_append(script,
		"	else \n"
		"		STEPS=\"0\" ; \n"
		"	fi ; \n"
		"	BACK=`echo $SCREENS|grep $SUBJ`; \n"
		"	if [[ -z \"$BACK\" ]]; then BACK=\"no\"; fi ; \n"
		"	echo " TAG_SCAN_FSSUBJ_SCRIPT " found $SUBJ $MRIS $STEPS $BACK; \n"
		"done ;\n"
		"echo " TAG_SCAN_FSSUBJ_SCRIPT " " TAG_END " ;\n");
	cmd = g_string_free (script, FALSE);

	gchar *argv[] = { "sh", "-c", cmd, NULL };

	g_message("run freesurfer scan script ( shell : %s )", argv[0]);
	if (!g_spawn_sync(NULL,	// cwd
		argv,
		NULL, // inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL, NULL, // no GSpawnChildSetupFunc child_setup,
		&output,	// get stdout
		NULL,	// ignore stderr
		NULL,	// ignore exit status
		&error)
	) {
		g_printerr ("Failed to launch %s\n error : %s\nscript :\n%s\n", argv[0], error->message, argv[2]);
		g_free (error);
		g_free(cmd);
		return;
	}
	output_split = g_strsplit(output, "\n", 0);
	g_free(cmd);

	fssubj_num_running = 0;

	for (ptr = output_split; *ptr !=NULL; ptr++)  {
		if (strlen(*ptr) == 0) continue;
//		0			1	2	3	4	5
//"	echo	scan_fssubj_script	found	$SUBJ	$MRIS	$STEPS	$BACK ; \n"

		gchar **col = g_strsplit(*ptr, " ", 6);

		if (strcmp(col[0], TAG_SCAN_FSSUBJ_SCRIPT) == 0) {
			if (strcmp(col[1], TAG_START) == 0) {
				data_is_comming = 1;
			} else if (strcmp(col[1], TAG_END) == 0) {
				data_is_comming = 0;
			} else if (data_is_comming) {
				if (strcmp(col[1], TAG_FOUND) == 0) {
					GtkTreeIter iter;
					char *name = col[2];
					int 	pid = atoi(col[5]),
						numorigs = atoi(col[3]),
						calculated = atoi(col[4]),
						found = 0;
					if (pid > 0) fssubj_num_running++;

					if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fssubj_store), &iter)) {
						GValue value;
						memset(&value, 0, sizeof(GValue));

						do {
							char *search_name;

							gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
							search_name = (char *) g_value_get_string(&value);
							if (strcmp(search_name, name) == 0) {
								g_value_unset(&value);
								found = 1;
								break;
							}
							g_value_unset(&value);
						} while (gtk_tree_model_iter_next(GTK_TREE_MODEL (fssubj_store), &iter));
					}

					if (! found)
						gtk_list_store_append (GTK_LIST_STORE (fssubj_store), &iter);

					gtk_list_store_set (GTK_LIST_STORE (fssubj_store), 
							&iter,
							FSSUBJ_SELECTOR_NAME,	name,
							FSSUBJ_SELECTOR_NUM_ORIGS,	numorigs,
							FSSUBJ_SELECTOR_CALCULATED,	100 * calculated / max_touch,
							FSSUBJ_SELECTOR_IS_RUNNING,	(pid > 0),
							FSSUBJ_SELECTOR_PID,	pid,
							-1);
				}
			}
		}

		g_strfreev(col);
	}

	g_free(output);
	g_strfreev(output_split);

	// Start rescanning 
	if ((fssubj_num_running > 0) && (fssubj_timeout == 0))
		fssubj_timeout = g_timeout_add (RESCAN_INTERVAL * 1000, scan_timeout, NULL);
//		fssubj_timeout = g_timeout_add_seconds (RESCAN_INTERVAL, scan_timeout, NULL);
}

void build_fssubj_store() {
	fssubj_store = gtk_list_store_new (FSSUBJ_SELECTOR_NUMCOLS,
		G_TYPE_STRING,	// name is a string
		G_TYPE_INT,	// NUM of ORIGS is a INT
		G_TYPE_INT,	// CALCULATED state is expressed with an INT
		G_TYPE_BOOLEAN,	// IS RUNNING is as boolean
		G_TYPE_INT);	// PID

	scan_fssubj();	// fill it !
}


void dump_fsmat(const char *subj, const char *vox2ras, const char *vox2ras_tkr, const char *orig, double *det, double *cres, double *rres, double *sres, double *offset) {
	GString *script = freesurfer_script();
	GError *error = NULL;
	char *cmd;
	
	g_string_append_printf(script,
		"mri_info --vox2ras $SUBJECTS_DIR/%s/mri/T1.mgz > %s ;\n"
		"mri_info --vox2ras-tkr $SUBJECTS_DIR/%s/mri/T1.mgz > %s ;\n"
		"mri_info --ras2vox $SUBJECTS_DIR/%s/mri/orig/001.mgz > %s ;\n"
		,
		subj, vox2ras,
		subj, vox2ras_tkr,
		subj, orig);

	if (orig) {
		g_string_append_printf(script,
			"mri_info --ras2vox $SUBJECTS_DIR/%s/mri/orig/001.mgz > %s ;\n"
			"mri_info --det $SUBJECTS_DIR/%s/mri/orig/001.mgz ;\n"
			"mri_info --cres $SUBJECTS_DIR/%s/mri/orig/001.mgz ;\n"
			"mri_info --rres $SUBJECTS_DIR/%s/mri/orig/001.mgz ;\n"
			"mri_info --sres $SUBJECTS_DIR/%s/mri/orig/001.mgz ;\n"
			"mri_info --slicedirection $SUBJECTS_DIR/%s/mri/orig/001.mgz ;\n"
		    "mri_info --vox2ras $SUBJECTS_DIR/%s/mri/orig/001.mgz ;\n"
			,
			subj, orig,
			subj,
			subj,
			subj,
			subj,
			subj,
			subj);
	}
	
	cmd = g_string_free (script, FALSE);
	gchar *argv[] = { "sh", "-c", cmd, NULL };
	gchar *output = NULL;
	g_message("get freesurfer matrices ( shell : %s )", argv[0]);
	if (! g_spawn_sync(NULL,	// cwd
	 argv,
	 NULL, // inherit ENV
	 G_SPAWN_SEARCH_PATH,
	 NULL, NULL, // no GSpawnChildSetupFunc child_setup,
	 &output,	// stdout - data for mimics
	 NULL,	// ignore stderr
	 NULL,	// ignore exit status
	 &error)
	) {
		g_printerr ("Failed to launch %s\n error : %s\nscript :\n%s\n", argv[0], error->message, argv[2]);
		g_free (error);
		g_free(cmd);
		return;
	}	
	g_free(cmd);
	
	
	if (orig) {
		char buffer[32];
		double x = 0.,y =0.,z=0.;
		sscanf(output, "%lf\n%lf\n%lf\n%lf\n"
			"%32s\n"
			" %*lf %*lf %*lf %lf\n"
			" %*lf %*lf %*lf %lf\n"
			" %*lf %*lf %*lf %lf\n"
			, 
			det, cres, rres, sres,
			buffer,
			&x, &y, &z);
		if (0 == strcasecmp(buffer, "sagittal")) {
			*offset = -x;
		} else if (0 == strcasecmp(buffer, "coronal")) {
			*offset = -y;
		} else if (0 == strcasecmp(buffer, "axial")) {
			*offset = z;
		}
		
/*		
		gchar *dirname = g_strdup_printf("%s/%s/mri/orig/001/", freesurfer_subjects, subj);
		GDir *d = g_dir_open(dirname, 0, NULL);
		
		// find the offset of the first DICOM frame
		if (d) {
			const gchar *fname;
			double min = +10000.;

			while (fname = g_dir_read_name(d)) {
				gchar *fullpath = g_strdup_printf("%s/%s", dirname, fname);
				double loc = +10000.;
				char *seruid = NULL, *name = NULL, *subjid = NULL, *datetime = NULL, *fulltype = NULL, *fulldesc = NULL, *res = NULL;
				if (DICOM_FOUND_LOC & dicom_getinfo_overview (fullpath, &seruid, &name, &subjid, &datetime, &fulltype, &fulldesc, &res, &loc))
					if (loc < min) min = loc;
				g_free(fullpath);				
				if (seruid)	free(seruid);
				if (name)	free(name);
				if (subjid)	free(subjid);
				if (datetime)	free(datetime);
				if (fulltype)	free(fulltype);
				if (fulldesc)	free(fulldesc);
				if (res)	free(res);
			}
			g_dir_close(d);
			
			if (min < +10000.) *offset = min;
		}
		g_free(dirname);
*/
		g_message("results %f\t%f\t%f\t%f\t%f", *det, *cres, *rres, *sres, *offset);
	}
	g_free(output);
	return;
}




gboolean event_check_tkmedit (GtkWidget *widget, gpointer data ) {
	GtkTreeIter iter;
	GError *error = NULL;

	GString *cmd = NULL;
	GValue value;
	char *cmd_final;
	char *subj;

	char *main_vol = NULL;
	char *aux_vol = NULL;
	char *main_surf = NULL;
	char *aux_surf = NULL;


	if (! gtk_tree_selection_get_selected (fssubj_sel, (GtkTreeModel **)NULL, &iter)) {
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

	// subject name
	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
	subj = g_strdup((char *) g_value_get_string(&value));
	g_value_unset(&value);


	{
		// one is selected
		GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
		GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_OK_CANCEL,
			"Which volume of subject %s should be displayed\n"
			"as MAIN volume ?",
			subj);

		
		GtkWidget *button_T1 = gtk_radio_button_new_with_label (NULL, "T1 image");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_T1), TRUE);
		gtk_box_pack_start (GTK_BOX(vbox), button_T1, TRUE, FALSE, 0);
		gtk_widget_show (button_T1);

		GtkWidget *button_mask = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_T1), "Brain mask (use to remove horns from the surfaces)");
		gtk_box_pack_start (GTK_BOX(vbox), button_mask, TRUE, FALSE, 0);
		gtk_widget_show (button_mask);

		GtkWidget *button_wm = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_mask), "White Matter (to correct small holes)");
		gtk_box_pack_start (GTK_BOX(vbox), button_wm, TRUE, FALSE, 0);
		gtk_widget_show (button_wm);

		gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, FALSE, 0);
		gtk_widget_show (vbox);

		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, FALSE, 0);
		gtk_widget_show (hbox);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			gtk_widget_destroy (dialog);
			g_free(subj);
			return FALSE;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_T1))) {
			main_vol = "T1.mgz";
			aux_vol = "orig.mgz";
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_mask))) {
			main_vol = "brainmask.mgz";
			aux_vol = "wm.mgz";
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_wm))) {
			main_vol = "wm.mgz";
			aux_vol = "brainmask.mgz";
		}

		gtk_widget_destroy (dialog); // and it's children...
	}


	main_surf = "lh.white";
	aux_surf = "rh.white";


	// begin FreeSurfer enviro
	cmd = freesurfer_script();

	// base command
	g_string_append_printf(cmd,
		"tkmedit %s %s",
		subj,
		main_vol);

	if (main_surf)	g_string_append_printf(cmd, " %s", main_surf);
	if (aux_vol)	g_string_append_printf(cmd, " -aux %s", aux_vol);
	if (aux_surf)	g_string_append_printf(cmd, " -aux-surface %s", aux_surf);

	g_string_append_printf(cmd, "& \n"); // run in background

	// free data...
	cmd_final = g_string_free(cmd, FALSE);	 // FALSE - ...except for the actual command string

	// do it !
	char *argv [] = { "sh", "-c", cmd_final };

	if (!g_spawn_async(NULL,	// cwd
		argv,
		NULL,	// inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL,	// no GSpawnChildSetupFunc child_setup,
		NULL,
		NULL,
 		&error)
	) {
		g_printerr ("Failed to launch %s(%s): %s\n", argv[0], argv[1], error->message);
		g_free (error);
		g_free(cmd_final);
		g_free(subj);

		return FALSE;
	} 

	g_free(cmd_final);
	g_free(subj);

	scan_fssubj();
	return FALSE;
}




gboolean event_check_tksurfer (GtkWidget *widget, gpointer data ) {
	GtkTreeIter iter;
	GError *error = NULL;

	GString *cmd = NULL;
	GValue value;
	char *cmd_final;
	char *subj;

	char *surf = NULL;
	char *hemi = NULL;


	if (! gtk_tree_selection_get_selected (fssubj_sel, (GtkTreeModel **)NULL, &iter)) {
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

	// subject name
	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
	subj = g_strdup((char *) g_value_get_string(&value));
	g_value_unset(&value);


	{
		// one is selected
		GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
		GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_OK_CANCEL,
			"Which surface of subject %s should be displayed ?\n",
			subj);

		
		GtkWidget *label = gtk_label_new ("LEFT hemisphere :");
		gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, FALSE, 0);
		gtk_widget_show (label);

		GtkWidget *button_l_w = gtk_radio_button_new_with_label (NULL, "WHITE surface");
		gtk_box_pack_start (GTK_BOX(vbox), button_l_w, TRUE, FALSE, 0);
		gtk_widget_show (button_l_w);

		GtkWidget *button_l_p = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_l_w), "PIAL surface");
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button_l_p), TRUE);
		gtk_box_pack_start (GTK_BOX(vbox), button_l_p, TRUE, FALSE, 0);
		gtk_widget_show (button_l_p);

		GtkWidget *button_l_i = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_l_p), "INFLATED surface");
		gtk_box_pack_start (GTK_BOX(vbox), button_l_i, TRUE, FALSE, 0);
		gtk_widget_show (button_l_i);

		label = gtk_label_new ("RIGHT hemisphere :");
		gtk_box_pack_start (GTK_BOX(vbox), label, TRUE, FALSE, 0);
		gtk_widget_show (label);

		GtkWidget *button_r_w = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_l_i), "WHITE surface");
		gtk_box_pack_start (GTK_BOX(vbox), button_r_w, TRUE, FALSE, 0);
		gtk_widget_show (button_r_w);

		GtkWidget *button_r_p = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_r_w), "PIAL surface");
		gtk_box_pack_start (GTK_BOX(vbox), button_r_p, TRUE, FALSE, 0);
		gtk_widget_show (button_r_p);

		GtkWidget *button_r_i = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(button_r_p), "INFLATED surface");
		gtk_box_pack_start (GTK_BOX(vbox), button_r_i, TRUE, FALSE, 0);
		gtk_widget_show (button_r_i);

		gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, FALSE, 0);
		gtk_widget_show (vbox);

		gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, FALSE, 0);
		gtk_widget_show (hbox);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			gtk_widget_destroy (dialog);
			g_free(subj);
			return FALSE;
		}

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_l_w))) {
			surf = "white";
			hemi = "lh";
		} else 	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_l_p))) {
			surf = "pial";
			hemi = "lh";
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_l_i))) {
			surf = "inflated";
			hemi = "lh";
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_r_w))) {
			surf = "white";
			hemi = "rh";
		} else 	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_r_p))) {
			surf = "pial";
			hemi = "rh";
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button_r_i))) {
			surf = "inflated";
			hemi = "rh";
		}

		gtk_widget_destroy (dialog); // and it's children...
	}

	// begin FreeSurfer enviro
	cmd = freesurfer_script();

	// base command
	g_string_append_printf(cmd,
		"tksurfer %s %s %s &\n",
		subj,
		hemi, 
		surf);

	// free data...
	cmd_final = g_string_free(cmd, FALSE);	 // FALSE - ...except for the actual command string

	// do it !
	char *argv [] = { "sh", "-c", cmd_final };

	if (!g_spawn_async(NULL,	// cwd
		argv,
		NULL,	// inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL,	// no GSpawnChildSetupFunc child_setup,
		NULL,
		NULL,
 		&error)
	) {
		g_printerr ("Failed to launch %s(%s): %s\n", argv[0], argv[1], error->message);
		g_free (error);
		g_free(cmd_final);
		g_free(subj);

		return FALSE;
	} 

	g_free(cmd_final);
	g_free(subj);

	scan_fssubj();
	return FALSE;
}







gboolean recon_launch (GtkWidget *widget, gpointer data, char *phase ) {
	GError *error = NULL;
	GtkTreeIter iter;
	GString *cmd = NULL;
	GValue value;
	char *cmd_final = NULL;
	char *subj = NULL;
	char *warn_mails = (char *) gtk_entry_get_text(GTK_ENTRY(data));;


	if (! gtk_tree_selection_get_selected (fssubj_sel, (GtkTreeModel **)NULL, &iter)) {
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

	if (warn_mails && strlen(warn_mails)) {
		char *p;

		for (p = warn_mails; *p; p++)
			if ((! g_ascii_isalnum(*p)) &&
				(*p != '-') &&
				(*p != '_') &&
				(*p != ' ') &&
				(*p != '@') &&
				(*p != '.')) {

				GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					"Invalid caracter in email : %c\n"
					"E-Mail should only contain \n"
					"a-z, A-Z, 0-9, @, -, _ and .\n",
					*p);
				gtk_dialog_run (GTK_DIALOG (dialog));
				gtk_widget_destroy (dialog);
				return FALSE;
			}
	} else {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK_CANCEL,
			"No e-Mail address given,\n"
			"you'll have to check manually when FreeSurfer finishes.\n"
			"Click on OK to continue.\n");

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
			gtk_widget_destroy (dialog);
			return FALSE;
		}
		gtk_widget_destroy (dialog);

		warn_mails = NULL;
	}


	// no more than 1 session
	if (fssubj_num_running > 0) {
		memset(&value, 0, sizeof(GValue));
		gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_IS_RUNNING, &value);
		// is the selected one running ?
		if (g_value_get_boolean(&value)) {
			g_value_unset(&value);
			// we are already doing it !
			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"This subject is already currently being segmented.\n"
				"Either wait until segmentation finishes\n"
				"Or abort current segmentation.");

			gtk_dialog_run (GTK_DIALOG (dialog));	
			gtk_widget_destroy (dialog);
			return FALSE;
		} else {
			g_value_unset(&value);

			// we are going to crash !
			GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_OK_CANCEL,
				"Another FreeSurfer segmentation is still in progress.\n"
				"Unless there are more than 1 CPU Core and more than 1 GO RAM,\n"
				"two concurrent segmentations will run slow\n"
				"and/or exhaust all avaible ressource and crash.");
	
			if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
				gtk_widget_destroy (dialog);
				return FALSE;
			}
	
			gtk_widget_destroy (dialog);
		}
	}


	// subject name
	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
	subj = g_strdup((char *) g_value_get_string(&value));
	g_value_unset(&value);

	// check mris
	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NUM_ORIGS, &value);
	if (g_value_get_int(&value) == 0) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"No DICOM series found in FreeSurfer subject %s.\n"
			"Please import a DICOM serie into this subject first."
			,
			subj);
		g_value_unset(&value);

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free(subj);
		return FALSE;
	}
	g_value_unset(&value);


	g_message("segment %s with freesurfer", subj);

	// begin FreeSurfer enviro
	cmd = freesurfer_script();
	g_string_append(cmd,
		"echo " TAG_SCREEN_SCRIPT " " TAG_START " ; \n"
		"START_DATE=`date --rfc-2822` ; \n");

//	nice goes here

	g_string_append_printf(cmd,
		"recon-all -subjid %s %s%s%s ; \n"
		"END_DATE=`date --rfc-2822` ; \n"
		,
		subj,
		phase,
		(warn_mails ? " -mail " : ""),
		(warn_mails ? warn_mails : ""));

	if (warn_mails)
		g_string_append_printf(cmd,
			"echo -e '\\n"
				"FreeSurfer has finished segmenting\\n"
				"\\n"
				"Subject :\t%s\\n"
				"Started on :\t' $START_DATE '\\n"
				"Finished on :\t' $END_DATE '\\n"
				"Host :\t' $HOST '\\n"
			"\\n' | "
				"mail -s 'FreeSurfer %s finished' -r no-reply@$HOST %s ; \n"
			,
				subj,
			subj, warn_mails);

	g_string_append(cmd, "echo " TAG_SCREEN_SCRIPT " " TAG_END " ; \n");

//	-autorecon-all : same as -all
//
// Manual-Intervention Workflow Directives:
//	-autorecon1    : process stages 1-5 (see below)
//	-autorecon2    : process stages 6-24



	// free data...
	cmd_final = g_string_free(cmd, FALSE);	 // FALSE - ...except for the actual command string
	g_message("segment script :\n%s", cmd_final);

	// do it !
	char *tag = g_strdup_printf(TAG_SCREEN "-%s", subj);
	char *argv [] = { "screen", 
				"-d", "-m",	// detached session
				"-h", "4096",	// scroll back buffer
				"-S", tag,	// use a special Socket Name
				"-t", subj,	// title
				"-U",	// UTF8
					"sh",
						"-c", cmd_final };

	if (!g_spawn_async(NULL,	// cwd
		argv,
		NULL,	// inherit ENV
		G_SPAWN_SEARCH_PATH,
		NULL,	// no GSpawnChildSetupFunc child_setup,
		NULL,
		NULL,
 		&error)
	) {
		g_printerr ("Failed to launch %s(%s): %s\n", argv[0], argv[1], error->message);
		g_free (error);
		g_free(cmd_final);
		g_free(tag);
		g_free(subj);

		return FALSE;
	} 

//	recon_start_screened(cmd_final);
	g_free(cmd_final);
	g_free(tag);
	g_free(subj);

	scan_fssubj();
	return FALSE;
}


gboolean event_launch_recon (GtkWidget *widget, gpointer data ) {
	return recon_launch (widget, data, "-all");
}

gboolean event_restart_recon_bm (GtkWidget *widget, gpointer data ) {
	return recon_launch (widget, data, "-autorecon2");
}

gboolean event_restart_recon_wm (GtkWidget *widget, gpointer data ) {
	return recon_launch (widget, data, "-autorecon2-wm -autorecon3");
}

gboolean event_restart_recon_pial (GtkWidget *widget, gpointer data ) {
	return recon_launch (widget, data, "-autorecon2-pial -autorecon3");
}

gboolean event_abort_recon (GtkWidget *widget, gpointer data ) {
	GtkTreeIter iter;
	GValue value;

	if (! gtk_tree_selection_get_selected (fssubj_sel, (GtkTreeModel **)NULL, &iter)) {
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

	if (fssubj_num_running == 0) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"There are no running segmentation to stop.");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_IS_RUNNING, &value);
	// is the selected one running ?
	if (g_value_get_boolean(&value)) {
		g_value_unset(&value);

		memset(&value, 0, sizeof(GValue));
		gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_NAME, &value);
		char *name = g_strdup((char *) g_value_get_string(&value));

		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO,
			"Are you sure you want to stop subject %s ?", 
			name);
		g_value_unset(&value);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_YES) {
			gtk_widget_destroy (dialog);
			return FALSE;
		}
	
		gtk_widget_destroy (dialog);
	} else {
		g_value_unset(&value);

		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(main_window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE,
			"This subject is not currently being segmented.\n"
			"Obviously it cannot be stoped.");

		gtk_dialog_run (GTK_DIALOG (dialog));	
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	memset(&value, 0, sizeof(GValue));
	gtk_tree_model_get_value (GTK_TREE_MODEL (fssubj_store), &iter, FSSUBJ_SELECTOR_PID, &value);
	int pid = g_value_get_int(&value);
	g_value_unset(&value);

	if (pid > 0) {
		g_message("sending TERM to background pid %d", pid);
		kill (pid, SIGTERM);
	} else 
		g_message("borked pid %d", pid);

	scan_fssubj();

	return FALSE;
}

