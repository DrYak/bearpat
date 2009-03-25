// ISOC99 is used for isfinite() function
#define _ISOC99_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <locale.h>

#include <glib.h>

#include "dicom.h"

#define MAX_BUFF 32768
#define SIG_LEN 4
#define SIG "DICM"

// BUGS: 
//	-	negociation de TransferFormat
//	-	critère pour distinguer T2/PD

// Bibliography :
//	-	http://www.dicomwiki.com/wiki/index.php?title=DICOM_Standard
//		Part 5	Data Structures and Encoding
//		Part 10	Media Storage and File Format for Media Interchange 

int dicom_getinfo_overview (char *fname, char **seruid, char **name, char **subjid, char **datetime, char **fulltype, char **fulldesc, char **res, double *loc) {
	FILE *f;
	char signature[SIG_LEN];

	int always_tags = 0;
	int checked_vr = 0;
	int r = 0;

	char *date = NULL, *time = NULL, *type = NULL, *mrtype = NULL, *seqname = NULL, *serdesc = NULL, *proto = NULL;
	int scanw = 0, scanh = 0, picw = 0, pich = 0;
	double thick = NAN, pixw = NAN, pixh = NAN;

	// set locale
	setlocale (LC_ALL, "POSIX");

	
	if ((f = fopen(fname, "rb")) == NULL) {
		perror(fname);
		return 0;
	}
	if ((fseek(f, 0x80, SEEK_SET) < 0) ||	// jump to header
		(fread(signature, 1, SIG_LEN, f) != SIG_LEN) ||	// try reading header
		(strncmp(signature, SIG, SIG_LEN) != 0)) {	// cheack header
		fclose(f);
		return 0;
	}


	while (!feof(f)) {
		unsigned short int l1 = 0, l2 = 0, unknown;
		unsigned long int len = 0;
		char t[3];
		t[2] = '\0';
		
		if (fread(&l1, 2, 1, f) < 1) break;
		if (fread(&l2, 2, 1, f) < 1) break; 
		if (always_tags || l1 == 2) {
			// MetaData always with explicit VR because the format is negotiated THERE.
			unsigned short int l = 0;	

			if (fread(t, 2, 1, f) < 1) break;
			if (fread(&l, 2, 1, f) < 1) break;
			if ((strncmp(t, "SQ", 2) == 0) ||
				(strncmp(t, "OB", 2) == 0) || (strncmp(t, "OW", 2) == 0)|| (strncmp(t, "OF", 2) == 0) ||
				(strncmp(t, "UT", 2) == 0) || (strncmp(t, "UN", 2) == 0)) {
				// the 16bit field is undefined
				// the next 32bits are the length
				if (fread(&len, 4, 1, f) < 1) break;
			} else 
				len = l;

		} else {
			if (fread(&len, 4, 1, f) < 1) break;
			// BAD hack : instead of decoding the negotiation of format, we try to guess the explicitness of the VR
			// explicit VR have always 2 uppercases, followed by lenght
			// first data set has a short lenght, so it is unlikely

			// BUG: NO BYTESEX negotiations !!!
			if (! checked_vr) {
				checked_vr = 1;
				if (isupper(t[0] = (char) len) &&  isupper(t[1] = (char) (len >> 8LU)) && (len >> 16LU)) {
					len >>= 16UL;
					always_tags = 1;
				}
			}
		}
		if (len & 1) len++; // SHOULD NOT HAPPEN ! Data should be PRE-padded
/*			
		inline void get_from_tag(char *buffer, int l) {
			if (fread(buffer, l, 1, f) < 1) break;
			buffer[l] = '\0';
			len -= l;
		}
*/	
#define get_from_tag(b,l) { if (fread((b), l, 1, f) < 1) break; (b)[l] = '\0'; len -= l; }
#define u16_from_tag()   ({ int b = 0; if (fread(&b, 2, 1, f) < 1) break; len -= 2; b; })
		switch (l1) {
			case 0x0008:
				switch (l2) {
					case 0x0060: { // Type
							type = (char *) alloca(len+1);
							get_from_tag(type, len);
						} break;
					case 0x0022:{ // Acquisition Date [yyyymmdd]
							date = (char *) alloca(len+1);
							get_from_tag(date, len);
						} break;
						break;
					case 0x0032:{ // Acquisition Time
							time = (char *) alloca(len+1);
							get_from_tag(time, len);
						} break;
					case 0x103e: { // SeriesDescription
							serdesc = (char *) alloca(len+1);
							get_from_tag(serdesc, len);
						} break;
				}
				break;
			case 0x0010:
				switch (l2) {
					case 0x0010: { // Name
							*name = (char *) g_malloc(len+1);
							get_from_tag(*name, len);
							r |= DICOM_FOUND_NAME;
						} break;
					case 0x0020: { // ID
							*subjid = (char *) g_malloc(len+1);
							get_from_tag(*subjid, len);
							r |= DICOM_FOUND_ID;
						} break;
				}
				break;
			case 0x0018:
				switch (l2) {
					case 0x0023: { // MRAcquisitionType
							mrtype = (char *) alloca(len+1);
							get_from_tag(mrtype, len);
						} break;
					case 0x0024: { // SequenceName
							seqname = (char *) alloca(len+1);
							get_from_tag(seqname, len);
						} break;
					case 0x0050: { // Slice Thickness
							char tmp[len+1];
							get_from_tag(tmp, len);
							thick = atof(tmp);
						} break;
					case 0x1030: { // ProtocolName
							proto = (char *) alloca(len+1);
							get_from_tag(proto, len);
						} break;
					case 0x1310: { // AcquisitionMatrix
							u16_from_tag();
							scanw = u16_from_tag();
							scanh = u16_from_tag();
							// u16_from_tag();
						} break;
				}
				break;
			case 0x0020:
				switch (l2) {
					case 0x000e: { // Serie_UID
							*seruid = (char *) g_malloc(len+1);
							get_from_tag(*seruid, len);
							r |= DICOM_FOUND_SERUID;
						} break;
					case 0x1041: { // Slice Location
							char tmp[len+1];
							get_from_tag(tmp, len);
							*loc = atof(tmp);
							r |= DICOM_FOUND_LOC;
						} break;
				}
				break;
			case 0x0028:
				switch (l2) {
					case 0x0010: { // Rows
							picw = u16_from_tag();
						} break;
					case 0x0011: { // Columns
							pich = u16_from_tag();
						} break;
					case 0x0030: { // PixelSpacing
							char tmp[len+1];
							get_from_tag(tmp, len);
							sscanf(tmp, "%lf\\%lf", &pixw, &pixh);
						} break;
				}
				break;
		}
		
		if (len && (fseek(f, len, SEEK_CUR) < 0)) break;
	}

	if (date || time) {
		int l = 6;	
		if (date) l += strlen(date);
		if (time) l += strlen(time);
		*datetime = (char *) g_malloc(l);
		**datetime = '\0';

		if (date) {
			strncat(*datetime, date, 4);
			strcat(*datetime, "/");
			strncat(*datetime, date + 4 , 2);
			strcat(*datetime, "/");
			strncat(*datetime, date + 6 , 2);

			strcat(*datetime, " ");
//			free (date);
		}

		if (time) {
			strncat(*datetime, time, 2);
			strcat(*datetime, ":");
			strncat(*datetime, time + 2 , 2);
			strcat(*datetime, ":");
			strcat(*datetime, time + 4);

//			free (time);
		}
		r |= DICOM_FOUND_DATE;
	} else *datetime = NULL;

	if (type || mrtype) {
		int l = 2; // one car + null terminator !
		if (type) l += strlen(type);
		if (mrtype) l += strlen(mrtype);
		*fulltype = (char *) g_malloc(l);
		**fulltype = '\0';

		if (type) {
			strcat(*fulltype, type);
//			free (type);
		}

		if (mrtype) {
			strcat(*fulltype, "-");
			strcat(*fulltype, mrtype);
//			free (mrtype);
		}
		r |= DICOM_FOUND_TYPE;
	} else *fulltype = NULL;

	if (serdesc) {
		if (proto && (strcmp(serdesc, proto) == 0)) {
//			free(proto);
			proto = serdesc;
			serdesc = NULL;
		}
		if (! proto) {
			proto = serdesc;
			serdesc = NULL;
		}
	}

	if (seqname || proto || serdesc) {
		int l = 7;
		if (seqname)	l += strlen(seqname);
		if (proto)	l += strlen(proto);
		if (serdesc)	l += strlen(serdesc);

		*fulldesc = (char *) g_malloc(l);
		**fulldesc = '\0';

		if (seqname) {
			strcat(*fulldesc, seqname);
//			free (seqname);
		}

		if (proto) {
			if (**fulldesc) strcat(*fulldesc, " \\ ");
			strcat(*fulldesc, proto);
//			free (proto);
		}

		if (serdesc) {
			if (**fulldesc) strcat(*fulldesc, " \\ ");
			strcat(*fulldesc, serdesc);
//			free (serdesc);
		}
		r |= DICOM_FOUND_DESC;
	} else *fulldesc = NULL;

	if (isfinite(thick) && isfinite(pixw) && isfinite(pixh)) {
		// convert pixel resolution to scanned resolution based on :
		// - acquisition dimensions (= propotionnal to scanned dimensions)
		// - picture dimensions ( = scanned dimensions X interpolation factor )
		// - picture resolution ( = scanned resolution / interpolation factor )
		if (picw && scanw) {
			pixw *= (double) picw;
			pixw /= (double) scanw;
		}
		if (pich && scanh) {
			pixh *= (double) pich;
			pixh /= (double) scanh;
		}

//		*res = (char *) g_malloc(15);
//		sprintf(*res, "%1.2f %1.2f %1.2f", pixw, pixh, thick);
		*res = g_strdup_printf("%.2f x %.2f x %.2f", pixw, pixh, thick);
		r |= DICOM_FOUND_RES;
	}

	fclose(f);

	return r;
}
