
enum {
	DICOM_PARAM_SERUID,
	DICOM_PARAM_NAME,
	DICOM_PARAM_ID, 
	DICOM_PARAM_DATE,
	DICOM_PARAM_TYPE,
	DICOM_PARAM_DESC,
	DICOM_PARAM_RES,
	DICOM_PARAM_LOC,

	DICOM_PARAM_NUM
};

#define DICOM_FOUND_SERUID	(1<<DICOM_PARAM_SERUID)
#define	DICOM_FOUND_NAME	(1<<DICOM_PARAM_NAME)
#define	DICOM_FOUND_ID	(1<<DICOM_PARAM_ID)
#define	DICOM_FOUND_DATE	(1<<DICOM_PARAM_DATE)
#define	DICOM_FOUND_TYPE	(1<<DICOM_PARAM_TYPE)
#define	DICOM_FOUND_DESC	(1<<DICOM_PARAM_DESC)
#define	DICOM_FOUND_RES	(1<<DICOM_PARAM_RES)
#define	DICOM_FOUND_LOC	(1<<DICOM_PARAM_LOC)
#define	DICOM_FOUND_ALL	((1<<DICOM_PARAM_NUM)-1)


int dicom_getinfo_overview (char *fname, char **seruid, char **name, char **id, char **date, char **type, char **desc, char **res, double *loc);
