/* save_restore.h */
#define STATIC_VARS 0
#define DEBUG 1

#if STATIC_VARS
#define STATIC static
#else
#define STATIC
#endif

#define     TATTLE(CA_ERROR_CODE, FMT, ARG) \
{ \
    int err_code = (CA_ERROR_CODE); \
    if (!(err_code & CA_M_SUCCESS)) \
        printf(FMT, (ARG), ca_message(err_code)); \
}

#define CONNECTED(CHID) ((CHID) && (ca_state(CHID) == cs_conn))

/* do a ca_put, if the channel is connected */
#define TRY_TO_PUT(TYPE, CHID, POINTER) \
{if (CONNECTED(CHID)) ca_put(TYPE, CHID, POINTER);}

#define TRY_TO_PUT_AND_FLUSH(TYPE, CHID, POINTER) \
{if (CONNECTED(CHID)) {ca_put(TYPE, CHID, POINTER); ca_flush_io();}}


#define         MAX(a,b)   ((a)>(b)?(a):(b))
#define         MIN(a,b)   ((a)<(b)?(a):(b))
#define         MAXRESTOREFILES 8

#define SR_STATUS_OK		4
#define SR_STATUS_SEQ_WARN	3
#define SR_STATUS_WARN		2
#define SR_STATUS_FAIL		1
#define SR_STATUS_INIT		0

/* Make sure to leave room for trailing null */
static char SR_STATUS_STR[5][10] =
	{"No Status", " Failure ", " Warning ", " Warning ", "    Ok   "};

#define FLOAT_FMT "%.7g"
#define DOUBLE_FMT "%.14g"

#define BUF_SIZE 120
#define ARRAY_BEGIN '{'
#define ARRAY_END '}'
#define ELEMENT_BEGIN '\"'
#define ELEMENT_END '\"'
#define ESCAPE '\\'
#define ARRAY_MARKER "@array@"
#define ARRAY_MARKER_LEN 7

#define FN_LEN 80 /* filename length */
#define STRING_LEN MAX_STRING_SIZE	/* EPICS max length for string PV */
#define PV_NAME_LEN 80 /* string containing a PV name */
#define NFS_PATH_LEN 128                /* string length for NFS related path */

struct restoreList {
        int pass0cnt;
        int pass1cnt;
        char *pass0files[MAXRESTOREFILES];
		long pass0Status[MAXRESTOREFILES];
		char *pass0StatusStr[MAXRESTOREFILES];
        char *pass1files[MAXRESTOREFILES];
		long pass1Status[MAXRESTOREFILES];
		char *pass1StatusStr[MAXRESTOREFILES];
};

extern void myPrintErrno(char *s, char *file, int line);
extern FILE *fopen_and_check(const char *file, long *status);

extern long SR_get_array_info(char *name, long *num_elements, long *field_size, long *field_type);
extern long SR_get_array(char *name, void *pArray, long *num_elements);
extern long SR_write_array_data(FILE *out_fd, char *name, void *pArray, long num_elements);
extern long SR_array_restore(int pass, FILE *inp_fd, char *PVname, char *value_string);
extern long SR_put_array_values(char *PVname, void *p_data, long num_values);

#define PATH_SIZE 255		/* max size of the complete path to one file */

extern volatile int save_restoreIncompleteSetsOk;
extern char saveRestoreFilePath[];              /* path to save files */
extern volatile int save_restoreNumSeqFiles;
extern volatile int save_restoreDebug;
extern volatile int save_restoreDatedBackupFiles;
extern struct restoreList restoreFileList;
extern int myFileCopy(const char *source, const char *dest);
extern void dbrestoreShow(void);

extern int	save_restoreNFSOK;
extern int	save_restoreIoErrors;
extern volatile int	save_restoreRemountThreshold;


/* strncpy sucks (may copy extra characters, may not null-terminate) */
#define strNcpy(dest, src, N) {			\
	int ii;								\
	char *dd=dest, *ss=src;				\
	for (ii=0; *ss && ii < N-1; ii++)	\
		*dd++ = *ss++;					\
	*dd = '\0';							\
}
