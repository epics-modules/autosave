/*
 * 10/29/96  tmm  v2.0 conversion to EPICS 3.13
 * 01/03/97  tmm  v2.1 use backup save file if save file can't be opened
 * 04/26/99  tmm  v2.2 check first character of restore file.  If not '#',
 *                then file is not trusted.
 * 11/24/99  tmm  v2.3 file-ok marker is now <END> and is placed at end of file.
 *                allow caller to choose whether boot-backup file is written,
 *                provide option of dated boot-backup files.
 */
#define VERSION "2.3"

#include	<stdioLib.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<ctype.h>
#include	<usrLib.h>
#include	<time.h>

#include	"dbDefs.h"
#include	"dbStaticLib.h"
#include	"initHooks.h"

extern	DBBASE		*pdbbase;

extern unsigned int	sr_restore_incomplete_sets_ok;

#ifdef NODEBUG
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V) {  if(l <= reboot_restoreDebug) \
			{ printf("%s(%d):",__FILE__,__LINE__); printf(FMT,V); } }
#define Debug2(l,FMT,V,W) {  if(l <= reboot_restoreDebug) \
			{ printf("%s(%d):",__FILE__,__LINE__); printf(FMT,V,W); } }
#endif
volatile int    reboot_restoreDebug = 0;
volatile int    reboot_restoreDatedBU = 0;


/*
 * file_restore
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database values before ioc_init is invoked.
 * Must use static database access routines.
 *
 */
int reboot_restore(char *filename, initHookState init_state, int noWrite)
{
	char		channel[80];
	char		bu_filename[120];
	char		fname[120];
	char		buffer[120], *bp;
	char		input_line[120];
	char		*s;
	FILE		*inp_fd;
	char		c;
	unsigned short	i,j;
	DBENTRY		dbentry;
	DBENTRY		*pdbentry= &dbentry;
	long		status;
	char		*endp, tmpstr[30];
	int			n, try_backup=0;
	time_t		sec;

	logMsg("reboot_restore (v%s): entry\n", VERSION);
	/* initialize database access routines */
	if (!pdbbase) {
		printf("No Database Loaded\n");
		return(0);
	}

	dbInitEntry(pdbbase,pdbentry);

	/* open file */
	strncpy(fname, filename, 118);
	if ((inp_fd = fopen(fname, "r")) == NULL) {
		logMsg("reboot_restore - unable to open file '%s'\n",fname);
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
		strncat(fname, "B", 1);
		logMsg("reboot_restore - trying backup file '%s'\n",fname);
		if ((inp_fd = fopen(fname, "r")) == NULL) {
			logMsg("reboot_restore - unable to open file '%s'\n",fname);
			dbFinishEntry(pdbentry);
			return(-1);
		} else {
			if ((fseek(inp_fd, -6, SEEK_END)) ||
				(fgets(tmpstr, 6, inp_fd) == 0) ||
				(strncmp(tmpstr, "<END>", 5) != 0)) {
				logMsg("reboot_restore - file '%s' was not successfully written.\n",fname);
				fclose(inp_fd);
				return(-1);
			} else {
				fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
			}
		}
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
				logMsg("dbFindRecord for '%s' failed\n", channel);
				errMessage(status,"");
			} else {
				if (!dbFoundField(pdbentry)) {
					logMsg("reboot_restore: dbFindRecord did not find field '%s'\n",
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
						logMsg("reboot_restore: for %s, dbVerify() says %s\n", channel, s);
					}
					break;

				case DBF_INLINK:
				case DBF_OUTLINK:
				case DBF_FWDLINK:
					/* Can't restore links after InitDatabase */
					if (init_state < INITHOOKafterInitDatabase) {
						status = dbPutString(pdbentry,input_line);
						Debug(5,"dbPutString() returns %d:",status);
						if (reboot_restoreDebug >= 5) errMessage(status,"");
						if ((s = dbVerify(pdbentry,input_line))) {
							logMsg("reboot_restore: for %s, dbVerify() says %s\n", channel, s);
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
					logMsg("dbPutString/dbPutMenuIndex of '%s' for '%s' failed\n",
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
			logMsg("%s channel(s) not connected / fetch failed\n",channel);
			if (!sr_restore_incomplete_sets_ok) {
				logMsg("aborting restore\n");
				fclose(inp_fd);
				dbFinishEntry(pdbentry);
				return(-1);
			}
		} else if (channel[0] == '<') {
			/* end of file */
			break;
		}
	}
	fclose(inp_fd);
	dbFinishEntry(pdbentry);

	if (noWrite) return(0);

	/* make  backup */
	strcpy(bu_filename,filename);
	if (reboot_restoreDatedBU) {
		sec = time (NULL);
		s = ctime (&sec);
		strncpy(tmpstr, s, 29);
		for (s=tmpstr; *s; s++) {
			if (*s == '\n') *s = 0;
			if isspace(*s) *s = '_';
		}
		strncat(bu_filename, &tmpstr[3], 29);
	} else {
		strcat(bu_filename, ".bu");
	}
	copy(fname,bu_filename);

	return(0);
}
