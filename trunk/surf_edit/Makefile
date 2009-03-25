all: surf_edit

surf_edit: surf_edit.c file.c file-stl.c file-asc.c geometry.c
	$(CC) $(CFLAGS) $(shell pkg-config --libs --cflags gts) -lm -o $@ $^
