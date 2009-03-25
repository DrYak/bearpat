all: bearpat-ui

bearpat-ui: bearpat-ui.c dicom.c ui.c ui-dicom.c ui-freesurfer.c ui-import.c ui-export.c
	$(CC) $(CFLAGS) $(shell pkg-config --libs --cflags gtk+-2.0 gthread-2.0) -o $@ $^


