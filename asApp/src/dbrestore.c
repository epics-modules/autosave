/*
 * 10/29/96  tmm  v2.0 conversion to EPICS 3.13
 * 01/03/97  tmm  v2.1 use backup save file if save file can't be opened
 * 04/26/99  tmm  v2.2 check first character of restore file.  If not '#',
 *                then file is not trusted.
 * 11/24/99  tmm  v2.3 file-ok marker is now <END> and is placed at end of file.
 *                allow caller to choose whether boot-backup file is written,
 *                provide option of dated boot-backup files.
 * 02/27/02  tmm  v2.4 Added some features from Frank Lenkszus's (FRL) code:
 *                added path to request files
 *                added set_pass0_restoreFile( char *filename)
 *                added set_pass1_restoreFile( char *filename)
 *                a few more tweaks: 
 *                changed date-time stamp suffix to use FRL's fGetDateStr()
 *                don't write redundant backup files
 * 03/15/02  tmm  v2.5 check saveRestoreFilePath before using it.
 * 03/19/02  tmm  v2.6 initialize fname before using it.
 * 04/05/02  tmm  v2.7 Don't use copy for backup file.  It uses mode 640.
 * 08/01/03  mlr  v3.0 Convert to R3.14.2
 */
#define VERSION "3.0"

#include	<stdio.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<ctype.h>
#include	<time.h>

#include	<dbDefs.h>
#include	<dbStaticLib.h>
#include	<initHooks.h>
#include 	"fGetDateStr.h"
#include	"save_restore.h"

#ifndef vxWorks
#define OK 0
#define ERROR -1
#endif

extern	DBBASE *pdbbase;

extern unsigned int	sr_restore_incomplete_sets_ok;
extern char *saveRestoreFilePath;              /* path to save files */

#ifdef NODEBUG
#define Debug(l,FMT,V) ;
#else
#define Debug0(l,FMT) {  if(l <= reboot_restoreDebug) \
			{ epicsPrintf("%s(%d):",__FILE__,__LINE__); \
			  epicsPrintf(FMT); } }
#define Debug(l,FMT,V) {  if(l <= reboot_restoreDebug) \
			{ epicsPrintf("%s(%d):",__FILE__,__LINE__); \
			  epicsPrintf(FMT,V); } }
#define Debug2(l,FMT,V,W) {  if(l <= reboot_restoreDebug) \
			{ epicsPrintf("%s(%d):",__FILE__,__LINE__); \
			  epicsPrintf(FMT,V,W); } }
#endif
volatile int    reboot_restoreDebug = 0;
volatile int    reboot_restoreDatedBU = 0;

struct restoreList restoreFileList = {0, 0, 
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}};


static int myCopy(char *source, char *dest)
{
	FILE 	*source_fd, *dest_fd;
	char	buffer[120], *bp;

	if ((source_fd = fopen(source,"r")) == NULL) {
		printf("reboot_restore: Can't open file %s\n", source);
		return(-1);
	}
	if ((dest_fd = fopen(dest,"w")) == NULL) {
		printf("reboot_restore: Can't open file %s\n", dest);
		return(-1);
	}
	while ((bp=fgets(buffer, 120, source_fd))) {
		fprintf(dest_fd, "%s", bp);
	}
	fclose(source_fd);
	fclose(dest_fd);
	return(0);
}

/*
 * file_restore
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database values before ioc_init is invoked.
 * Must use static database access routines.
 *
 */
int reboot_restore(char *filename, initHookState init_state)
{
	char		channel[80];
	char		bu_filename[258];
	char		fname[256] = "";
	char		buffer[120], *bp;
	char		input_line[120];
	char		datetime[32];
	char		*s;
	FILE		*inp_fd;
	char		c;
	unsigned short	i,j;
	DBENTRY		dbentry;
	DBENTRY		*pdbentry= &dbentry;
	long		status;
	char		*endp;
	int			n;

	epicsPrintf("reboot_restore (v%s): entry\n", VERSION);
	/* initialize database access routines */
	if (!pdbbase) {
		printf("No Database Loaded\n");
		return(OK);
	}

	dbInitEntry(pdbbase,pdbentry);

	/* open file */
	if (saveRestoreFilePath) {
		strncpy(fname, saveRestoreFilePath, sizeof(fname) -1);
	}
	strncat(fname, filename, MAX(sizeof(fname) -1 - strlen(fname),0));
	epicsPrintf("*** restoring from '%s' at initHookState %d ***\n",
		fname, (int)init_state);
	if ((inp_fd = fopen_and_check(fname, "r")) == NULL) {
		epicsPrintf("reboot_restore: Can't open save file.");
		return(ERROR);
	}

	(void)fgets(buffer, 120, inp_fd); /* discard header line */
	Debug(1, "reboot_restore: header line '%s'\n", buffer);
	/* restore from data file */
	while ((bp=fgets(buffer, 120, inp_fd))) {
		/* get PV_name, one space character, value */
		/* (value may be a string with leading whitespace; it may be */
		/* entirely whitespace; the number of spaces may be crucial; */
		/* it might also consist of zero characters) */
		n = sscanf(bp,"%s%c%[^\n]", channel, &c, input_line);
		if (n<3) *input_line = 0;
		if (isalpha(channel[0]) || isdigit(channel[0])) {
			/* add default field name */
			if (strchr(channel,'.') == 0)
				strcat(channel,".VAL");

			Debug2(5,"attempting to put '%s' to '%s'\n", input_line, channel);
			status = dbFindRecord(pdbentry,channel);
			if (status < 0) {
				epicsPrintf("dbFindRecord for '%s' failed\n", channel);
				errMessage(status,"");
			} else {
				if (!dbFoundField(pdbentry)) {
					epicsPrintf("reboot_restore: dbFindRecord did not find field '%s'\n",
						channel);
				}
				Debug(5,"field type %s\n",
					pamapdbfType[pdbentry->pflddes->field_type].strvalue);
				switch (pdbentry->pflddes->field_type) {
				case DBF_STRING:
				case DBF_CHAR:
				case DBF_UCHAR:
				case DBF_SHORT:
				case DBF_USHORT:
				case DBF_LONG:
				case DBF_ULONG:
				case DBF_FLOAT:
				case DBF_DOUBLE:
				case DBF_ENUM:
					status = dbPutString(pdbentry, input_line);
					Debug(5,"dbPutString() returns %d:", status);
					if (reboot_restoreDebug >= 5) errMessage(status, "");
					if ((s = dbVerify(pdbentry, input_line))) {
						epicsPrintf("reboot_restore: for %s, dbVerify() says %s\n", channel, s);
					}
					break;

				case DBF_INLINK:
				case DBF_OUTLINK:
				case DBF_FWDLINK:
					/* Can't restore links after InitDatabase */
					if (init_state < initHookAfterInitDatabase) {
						status = dbPutString(pdbentry,input_line);
						Debug(5,"dbPutString() returns %d:",status);
						if (reboot_restoreDebug >= 5) errMessage(status,"");
						if ((s = dbVerify(pdbentry,input_line))) {
							epicsPrintf("reboot_restore: for %s, dbVerify() says %s\n", channel, s);
						}
					}
					break;

				case DBF_MENU:
					n = (int)strtol(input_line,&endp,0);
					status = dbPutMenuIndex(pdbentry, n);
					Debug(5,"dbPutMenuIndex() returns %d:",status);
					if (reboot_restoreDebug >= 5) errMessage(status,"");
					break;

				default:
					status = -1;
					Debug(5,"field type not handled\n", 0);
					break;
				}
				if (status < 0) {
					epicsPrintf("dbPutString/dbPutMenuIndex of '%s' for '%s' failed\n",
					  input_line,channel);
					errMessage(status,"");
				}
				Debug(5,"dbGetString() returns '%s'\n",dbGetString(pdbentry));
			}
		} else if (channel[0] == '!') {
			for (i = 0; input_line[i] == ' '; i++);	/*skip blanks */
			for (j = 0; (input_line[i] != ' ') && (input_line[i] != 0); i++,j++)
				channel[j] = input_line[i];
			channel[j] = 0;
			epicsPrintf("%s channel(s) not connected / fetch failed\n",channel);
			if (!sr_restore_incomplete_sets_ok) {
				epicsPrintf("aborting restore\n");
				fclose(inp_fd);
				dbFinishEntry(pdbentry);
				return(ERROR);
			}
		} else if (channel[0] == '<') {
			/* end of file */
			break;
		}
	}
	fclose(inp_fd);
	dbFinishEntry(pdbentry);

	/* If this is the second pass for a restore file, don't write backup file again.*/
	if (init_state >= initHookAfterInitDatabase) {
		for(i = 0; i < restoreFileList.pass0cnt; i++) {
			if (strcmp(filename, restoreFileList.pass0files[i]) == 0)
				return(OK);
		}
	}

	/* write  backup file*/
	Debug0(1, "reboot_restore: writing BU file.\n");
	strcpy(bu_filename,fname);
	if (reboot_restoreDatedBU && (fGetDateStr(datetime) == 0)) {
		strcat(bu_filename, datetime);
	} else {
		strcat(bu_filename, ".bu");
	}
	status = (long)myCopy(fname,bu_filename);
	if (status) printf("reboot_restore: Can't write backup file.\n");

	return(OK);
}

int set_pass0_restoreFile( char *filename)
{
	
	if(restoreFileList.pass0cnt >= MAXRESTOREFILES) {
		epicsPrintf("set_pass0_restoreFile: MAXFILE count exceeded\n");
		return(ERROR);
	}
        if ((restoreFileList.pass0files[restoreFileList.pass0cnt] =
		 (char *)calloc(strlen(filename) + 4,sizeof(char))) == NULL){
                epicsPrintf("set_pass0_restoreFile: calloc failed\n");
                return(ERROR);
        }
        strcpy(restoreFileList.pass0files[restoreFileList.pass0cnt++],
		 filename);
        return(OK);
}

int set_pass1_restoreFile( char *filename)
{
	
	if(restoreFileList.pass1cnt >= MAXRESTOREFILES) {
		epicsPrintf("set_pass1_restoreFile: MAXFILE count exceeded\n");
		return(ERROR);
	}
        if ((restoreFileList.pass1files[restoreFileList.pass1cnt] =
		 (char *)calloc(strlen(filename) + 4,sizeof(char))) == NULL){
                epicsPrintf("set_pass1_restoreFile: calloc failed\n");
                return(ERROR);
        }
        strcpy(restoreFileList.pass1files[restoreFileList.pass1cnt++],
		 filename);
        return(OK);
}

FILE *fopen_and_check(const char *fname, const char *mode)
{
	FILE *inp_fd = NULL;
	int try_backup = 0;
	char tmpstr[30];
	char file[256];

	strcpy(file, fname);
	if ((inp_fd = fopen(file, "r")) == NULL) {
		epicsPrintf("fopen_and_check: Can't open file '%s'.\n", file);
		try_backup = 1;
	} else {
		/* check out "successfully written" marker */
		if ((fseek(inp_fd, -6, SEEK_END)) ||
				(fgets(tmpstr, 6, inp_fd) == 0) ||
				(strncmp(tmpstr, "<END>", 5) != 0)) {
			fclose(inp_fd);
			try_backup = 1;
		} else {
			fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
		}
	}

	if (try_backup) {
		/* try the backup file */
		strncat(file, "B", 1);
		epicsPrintf("fopen_and_check: Trying backup file '%s'\n", file);
		if ((inp_fd = fopen(file, "r")) == NULL) {
			epicsPrintf("fopen_and_check: Can't open file '%s'\n", file);
			return(NULL);
		} else {
			if ((fseek(inp_fd, -6, SEEK_END)) ||
					(fgets(tmpstr, 6, inp_fd) == 0) ||
					(strncmp(tmpstr, "<END>", 5) != 0)) {
				epicsPrintf("fopen_and_check: File '%s' is not trusted.\n",
					file);
				fclose(inp_fd);
				return(NULL);
			} else {
				fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
			}
		}
	}
	return(inp_fd);
}
