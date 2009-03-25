gboolean name_error(char *name, char *target_name, gboolean slash);

gboolean event_set_hint_names (GtkWidget *widget, gpointer data );
gboolean event_set_hint_lastonly (GtkWidget *widget, gpointer data );
gboolean event_hint_names (GtkWidget *widget, gpointer data );

gboolean event_set_dircopy_cmd (GtkWidget *widget, gpointer data );

gboolean event_dir_copy (GtkWidget *widget, gpointer data );
gboolean event_import_fs (GtkWidget *widget, gpointer data );
