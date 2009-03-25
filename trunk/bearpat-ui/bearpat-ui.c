/*	BEAR-PAT
		Brain Extraction and Rapid-Prototyping for Anatomy Teaching

	License : GPL v2 or later 
	(see http://www.fsf.org )

	Author :
		Ivan Topolsky
	Contact :
		blackkitty@bigfoot.com

	Version :
		0.3.2 - 2009/01/13
		0.3.1 - 2008/01/21
		0.3 - 2007/12/01
		0.2 - 2006/11/07
		0.1 - 2006/08/30

	Based on :
	-	GTK / GOBJECT / GTHREADS / GLIB 
	-	DCM_TK sourcecode
	-	dicomtable	(previous work)
	-	additionnal bibliography for GTK+, DICOM and FreeSurfer in corresponding source files.

	Depends on :
	compile time system components
	-	GTK+ 2.0	( http://www.gtk.org)	- interface

	runtime
	-	FreeSurfer	( http://surfer.nmr.mgh.harvard.edu/ )	- Segmentation suite used at run-time
	-	Aeskulap	( http://aeskulap.nongnu.org/ )	- DICOM viewer used at run-time

	runtime system components
	-	findutils	find		used in most scripts
	-	bash				used in all scripts
	-	fileutils	mkdir, ln, cp	used in importing scripts
	-	grep				used in subject manipulations
	-	screen				used in backgrounding task
	-	date				used in backgrounding task
	-	mail				used in backgrounding task

	BUGS :
	-	no configurable paths
	-	no translations
	-	no help system

//
// *** IMPROVEMENTS THAT NEED TO BE MADE : ***
//
// IMPORTANT IMPROVEMENTS :
// - build tool-tip and/or help system
//
// SMALL IMPROVEMENT :
// - specify with data is missing in export pane
// - advpng to further compress mgz
// - GStringChunk for small string in huge quantities (like fnames in ui-dicom)
// - rewrite ui-dicom using GArray instead of GPtrArray
// - import_status_max  not re-entrant
// - DICOM TransferFormat negociations
// - DICOM T2 and PD separation criterions
// - change NICE priority of background tasks
// - runing status icon (now is check box)
//
//
// *** SKIPPED DESIGN ELEMENTS (to be done later) : ***
//
// language
// - tooltips
// - help pane
// - translations
//
// dicom list :
// - Iconic preview
// - Info packed on two lines
//
// preview pane :
// - Browseable preview image
// - Serie data
//
// freesurfer list :
// - runing status icon (now is check box)
// - auto-get steps list from recon-all source code
//
// export pane :
// - specify with data is missing
// - SDL/openGL preview
// - surfedit options (precision, ...)

*/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "ui.h"
#include "ui-freesurfer.h"
#include "ui-dicom.h"
#include "ui-export.h"



// OpenGL in GTK
// - http://www.gtk-fr.org/wakka.php?wiki=OpenGlEtGtk

// TODO:
//	check for file in export
//	change detection in STL preview, etc...
//	meshlab detect ".STL"
//	filter surface type
//	tkmedit %%subj%% norm.mgz -segmentation mri/aseg.mgz $FREESURFER_HOME/FreeSurferColorLUT.txt -surface lh.white -aux-surface rh.white
//	tksurf %%subj%% %%side%% pial
//	timeouts in scan_fssubj
//	subj destroyer


void init_strings () {
/*
	freesurfer_home = "~/freesurfer";	// FREESURFER_HOME
//	freesurfer_subjects = "$FREESURFER_HOME/subjects";	// SUBJECTS_DIR
*/
/*
	freesurfer_home = "/usr/local/freesurfer";	// FREESURFER_HOME
	freesurfer_subjects = g_strdup_printf("%s/subjects", getenv("HOME") ?: "~");	// SUBJECTS_DIR
*/

	freesurfer_home = "/Applications/freesurfer";	// FREESURFER_HOME
	freesurfer_subjects = "/Applications/freesurfer/subjects";	// SUBJECTS_DIR

	aeskulap_cmd = "aeskulap";
	aeskulap_ico = "/usr/local/share/aeskulap/images/aeskulap.png";

	surf_edit = "surf_edit";

/*	meshlab_cmd = "meshlab";
	meshlab_ico = "/usr/local/meshlab/images/eye64.png";
*/
	meshlab_cmd = "/Applications/meshlab.app/Contents/MacOS/meshlab";
	meshlab_ico = "/Applications/meshlab.app/Contents/Resources/meshlab.icns";


	k3b_cmd = "k3b";
	k3b_ico = "/opt/kde3/share/icons/hicolor/64x64/apps/k3b.png";
}


int main (int argc, char *argv[]) {
	// init threads
	g_thread_init(NULL);
	gdk_threads_init();

	// arguments and inits
	gtk_init (&argc, &argv);
	init_strings ();
	init_search_mutex ();
	
	// build everything
	build_dicom_store();
	build_fssubj_store();
	build_interface();

	// run it
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	return 0;
}
