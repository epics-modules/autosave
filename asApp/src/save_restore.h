/* save_restore.h */
#define STATIC_VARS 1
#define DEBUG 1

#if STATIC_VARS
#define STATIC
#else
#define STATIC static
#endif

#if DEBUG
#define Debug(l,FMT,V...) {if (l <= save_restoreDebug) \
              { errlogPrintf("%s(%d):",__FILE__,__LINE__); \
                errlogPrintf(FMT,##V); epicsThreadSleep(0.5); }}
#else
#define Debug(l,FMT,V...) ;
#endif

#define myPrintErrno(s) {errlogPrintf("%s(%d): [0x%x]=",__FILE__,__LINE__,errno); perror(s);}

#define         MAX(a,b)   ((a)>(b)?(a):(b))
#define         MIN(a,b)   ((a)<(b)?(a):(b))
#define         MAXRESTOREFILES 8

#define SR_STATUS_OK		4
#define SR_STATUS_SEQ_WARN	3
#define SR_STATUS_WARN		2
#define SR_STATUS_FAIL		1
#define SR_STATUS_INIT		0

/* Make sure to leave room for trailing null */
static char SR_STATUS_STR[4][10] =
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
#define PV_NAME_LEN 40 /* string containing a PV name */

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


