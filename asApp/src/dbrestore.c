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
 * 08/01/03  mlr  v3.0 Convert to R3.14.2.  Fix a couple of bugs where failure
 *                 was assumed to be status<0, while it should be status!=0.
 * 11/18/03  tmm  v3.01 Save files with versions earlier than 1.8 don't have to
 *                pass the <END> test.
 * 04/20/04  tmm  v4.0 3.13-compatible v3.5, with array support, converted to
 *                EPICS 3.14, though NFS stuff is #ifdef vxWorks
 *                In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.
 *                Attempt to restore scalars to DBF_NOACCESS fields by calling
 *                dbNameToAddr() and then a dbFastPutConvert[][] routine.  (It's
 *                not an error to attempt this in pass 0, though it must fail.)
 *                Previously, restoreFileList.pass<n>Status wasn't initialized. */
#define VERSION "4.0"

#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<ctype.h>
#include	<time.h>
/* added for 3.14 port */
#include	<math.h>	/* for safeDoubleToFloat() */
#include	<float.h>	/* for safeDoubleToFloat() */

#include	<dbStaticLib.h>
#include	<dbAccess.h>	/* includes dbDefs.h, dbBase.h, dbAddr.h, dbFldTypes.h */
#include	<recSup.h>		/* rset */
#include	<dbConvert.h> 	/* dbPutConvertRoutine */
#include	<dbConvertFast.h>	/* dbFastPutConvertRoutine */
#include	<initHooks.h>
#include	<epicsThread.h>
#include	<epicsExport.h>
#include	<iocsh.h>
#include 	"fGetDateStr.h"
#include	"save_restore.h"

#ifndef vxWorks
#define OK 0
#define ERROR -1
#endif

extern	DBBASE *pdbbase;

struct restoreList restoreFileList = {0, 0, 
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{0,0,0,0,0,0,0,0},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
			{0,0,0,0,0,0,0,0},
			{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

STATIC float mySafeDoubleToFloat(double d)
{
	float f;
    double abs = fabs(d);
    if (d==0.0) {
        f = 0.0;
    } else if (abs>=FLT_MAX) {
        if (d>0.0) f = FLT_MAX; else f = -FLT_MAX;
    } else if (abs<=FLT_MIN) {
        if (d>0.0) f = FLT_MIN; else f = -FLT_MIN;
    } else {
        f = d;
    }
	return(f);
}

void dbrestoreShow(void)
{
	int i;
	printf("  '     filename     ' -  status  - 'message'\n");
	printf("  pass 0:\n");
	for (i=0; i<MAXRESTOREFILES; i++) {
		if (restoreFileList.pass0files[i]) {
			printf("  '%s' - %s - '%s'\n", restoreFileList.pass0files[i],
				SR_STATUS_STR[restoreFileList.pass0Status[i]],
				restoreFileList.pass0StatusStr[i]);
		}
	}
	printf("  pass 1:\n");
	for (i=0; i<MAXRESTOREFILES; i++) {
		if (restoreFileList.pass1files[i]) {
			printf("  '%s' - %s - '%s'\n", restoreFileList.pass1files[i],
				SR_STATUS_STR[restoreFileList.pass1Status[i]],
				restoreFileList.pass1StatusStr[i]);
		}
	}
}

STATIC int myFileCopy(char *source, char *dest)
{
	FILE 	*source_fd, *dest_fd;
	char	buffer[BUF_SIZE], *bp;
	struct stat fileStat;
	int		chars_printed, size=0;

	Debug(5, "myFileCopy: copying '%s' to '%s'\n", source, dest);

	if (stat(source, &fileStat) == 0) size = (int)fileStat.st_size;
	errno = 0;
	if ((source_fd = fopen(source,"r")) == NULL) {
		errlogPrintf("save_restore:myFileCopy: Can't open file '%s'\n", source);
		if (errno) myPrintErrno("myCopy");
		if (++save_restoreIoErrors > save_restoreRemountThreshold) 
			save_restoreNFSOK = 0;
		return(ERROR);
	}
	errno = 0;
	/* Note: under vxWorks, the following fopen() frequently will set errno
	 * to S_nfsLib_NFSERR_NOENT even though it succeeds.  Probably this means
	 * a failed attempt was retried. (System calls never set errno to zero.)
	 */
	if ((dest_fd = fopen(dest,"w")) == NULL) {
		errlogPrintf("save_restore:myFileCopy: Can't open file '%s'\n", dest);
		if (errno) myPrintErrno("myFileCopy");
		fclose(source_fd);
		return(ERROR);
	}
	chars_printed = 0;
	while ((bp=fgets(buffer, BUF_SIZE, source_fd))) {
		errno = 0;
		chars_printed += fprintf(dest_fd, "%s", bp);
		if (errno) {myPrintErrno("myFileCopy"); errno = 0;}
	}
	errno = 0;
	fclose(source_fd);
	if (errno) {myPrintErrno("myFileCopy"); errno = 0;}
	fclose(dest_fd);
	if (errno) myPrintErrno("myFileCopy");
	if (size && (chars_printed != size)) {
		errlogPrintf("myFileCopy: size=%d, chars_printed=%d\n",
			size, chars_printed);
		return(ERROR);
	}
	return(OK);
}


STATIC long scalar_restore(int pass, DBENTRY *pdbentry, char *PVname, char *value_string)
{
	long 	n, status = 0;
	char 	*s;
	DBADDR	dbaddr;
	DBADDR	*paddr = &dbaddr;
	dbfType field_type = pdbentry->pflddes->field_type;
	
	Debug(15,"scalar_restore:entry:field type '%s'\n", pamapdbfType[field_type].strvalue);
	switch (field_type) {
	case DBF_STRING: case DBF_ENUM:
	case DBF_CHAR:   case DBF_UCHAR:
	case DBF_SHORT:  case DBF_USHORT:
	case DBF_LONG:   case DBF_ULONG:
	case DBF_FLOAT:  case DBF_DOUBLE:
		status = dbPutString(pdbentry, value_string);
		Debug(15,"dbPutString() returns %ld:", status);
		if (save_restoreDebug >= 15) errMessage(status, " ");
		if ((s = dbVerify(pdbentry, value_string))) {
			errlogPrintf("save_restore: for '%s', dbVerify() says '%s'\n", PVname, s);
			status = -1;
		}
		break;

	case DBF_INLINK: case DBF_OUTLINK: case DBF_FWDLINK:
		/* Can't restore links in pass 1 */
		if (pass == 0) {
			status = dbPutString(pdbentry, value_string);
			Debug(15,"dbPutString() returns %ld:",status);
			if (save_restoreDebug >= 15) errMessage(status," ");
			if ((s = dbVerify(pdbentry, value_string))) {
				errlogPrintf("save_restore: for '%s', dbVerify() says '%s'\n", PVname, s);
				status = -1;
			}
		} else {
			Debug(1,"Can't restore link field (%s) in pass 1.\n", PVname);
		}
		break;

	case DBF_MENU:
		n = (int)atol(value_string);
		status = dbPutMenuIndex(pdbentry, n);
		Debug(15,"dbPutMenuIndex() returns %ld:",status);
		if (save_restoreDebug >= 15) errMessage(status," ");
		break;

	case DBF_NOACCESS:
		if (pass == 1) {
			status = dbNameToAddr(PVname, paddr);
			if (!status) {
				/* record initilization may have changed the field type */
				field_type = paddr->field_type;
				if (field_type <= DBF_MENU) {
					status = (*dbFastPutConvertRoutine[DBR_STRING][field_type])
						(value_string, paddr->pfield, paddr);
					if (status) {
						errlogPrintf("dbFastPutConvert failed (status=%ld) for field '%s'.\n",
							status, PVname);
					}
				}
			}
		} else {
			Debug(1,"Can't restore DBF_NOACCESS field (%s) in pass 0.\n", PVname);
		}
		break;

	default:
		status = -1;
		Debug(1,"field_type '%d' not handled\n", field_type);
		break;
	}
	if (status) {
		errlogPrintf("save_restore: dbPutString/dbPutMenuIndex of '%s' for '%s' failed\n",
			value_string, PVname);
		errMessage(status," ");
	}
	Debug(15,"dbGetString() returns '%s'\n",dbGetString(pdbentry));
	return(status);
}

STATIC void *p_data = NULL;
STATIC long p_data_size = 0;

long SR_put_array_values(char *PVname, void *p_data, long num_values)
{
	DBADDR dbaddr;
	DBADDR *paddr = &dbaddr;
	long status, max_elements=0;
	STATIC long curr_no_elements=0, offset=0;
	struct rset	*prset;
	dbfType field_type;
						
	if ((status = dbNameToAddr(PVname, paddr)) != 0) {
		errlogPrintf("save_restore: dbNameToAddr can't find PV '%s'\n", PVname);
		return(status);
	}
	/* restore array values */
	max_elements = paddr->no_elements;
	field_type = paddr->field_type;
	prset = dbGetRset(paddr);
	if (prset && (prset->get_array_info) ) {
		status = (*prset->get_array_info)(paddr, &curr_no_elements, &offset);
	} else {
		offset = 0;
	}
	Debug(1, "restoring %ld values to %s (max_elements=%ld)\n", num_values, PVname, max_elements);
	if (VALID_DB_REQ(field_type)) {
		status = (*dbPutConvertRoutine[field_type][field_type])(paddr,p_data,num_values,max_elements,offset);
	} else {
		errlogPrintf("save_restore:SR_put_array_values: PV %s: bad field type '%d'\n",
			PVname, field_type);
		status = -1;
	}
	/* update array info */
	if (prset && (prset->put_array_info) && !status) {
		status = (*prset->put_array_info)(paddr, num_values);
	}
	return(status);
}


/* SR_array_restore()
 *
 * Parse file *inp_fd, starting with value_string, to extract array data into *p_data
 * ((re)allocate if existing space is insufficient).  If init_state permits, write array
 * to PV named *PVname.
 *
 * Expect the following syntax:
 * <white>[...]<begin>[<white>...<element>]...<white>...<end>[<anything>]
 * where
 *    <white> is whitespace
 *    [] surround an optional syntax element
 *    ... means zero or more repetitions of preceding syntax element
 *    <begin> is the character ARRAY_BEGIN
 *    <end> is the character ARRAY_END (must be different from ARRAY_BEGIN)
 *    <element> is
 *       <e_begin><character><e_end>
 *       where
 *          <e_begin> is the character ELEMENT_BEGIN (must be different from ARRAY_*)
 *          <e_end> is the character ELEMENT_END (may be the same as ELEMENT_BEGIN)
 *          <character> is any ascii character, or <escape><e_begin> or <escape><e_end>
 *          where <escape> is the character ESCAPE.
 *
 * Examples:
 *    { "1.23" " 2.34" " 3.45" }
 *    { "abc" "de\"f" "g{hi\"" "jkl mno} pqr" }
 */
long SR_array_restore(int pass, FILE *inp_fd, char *PVname, char *value_string)
{
	int				j, end_mark_found=0, begin_mark_found=0, end_of_file=0, found=0, in_element=0;
	long			status, max_elements=0, num_read=0;
	char			buffer[BUF_SIZE], *bp = NULL;
	char			string[MAX_STRING_SIZE];
	DBADDR			dbaddr;
	DBADDR			*paddr = &dbaddr;
	dbfType			field_type = DBF_NOACCESS;
	int				field_size = 0;
	char			*p_char = NULL;
	short			*p_short = NULL;
	long			*p_long = NULL;
	unsigned char	*p_uchar = NULL;
	unsigned short	*p_ushort = NULL;
	unsigned long	*p_ulong = NULL;
	float			*p_float = NULL;
	double			*p_double = NULL;

	Debug(1,"array_restore:entry: PV = '%s'\n", PVname);
	if ((status = dbNameToAddr(PVname, paddr)) != 0) {
		errlogPrintf("save_restore: dbNameToAddr can't find PV '%s'\n", PVname);
	} else {
		/*** collect array elements from file into local array ***/
		max_elements = paddr->no_elements;
		field_type = paddr->field_type;
		field_size = paddr->field_size;
		/* if we've already allocated a big enough memory block, use it */
		if ((p_data == NULL) || ((max_elements * field_size) > p_data_size)) {
			Debug(1,"array_restore:p_data = %p, p_data_size = %ld\n", p_data, p_data_size);
			if (p_data) free(p_data);
			p_data = (void *)calloc(max_elements, field_size);
			p_data_size = p_data ? max_elements * field_size : 0;
		} else {
			memset(p_data, 0, p_data_size);
		}
		Debug(10,"Looking for up to %ld elements of field-size %d\n", max_elements, field_size);
		Debug(9,"save_restore: field_type is '%s' (%d)\n", pamapdbfType[field_type].strvalue, field_type);
		switch (field_type) {
		case DBF_STRING: case DBF_CHAR:                p_char = (char *)p_data;             break;
		case DBF_UCHAR:                                p_uchar = (unsigned char *)p_data;   break;
		case DBF_ENUM: case DBF_USHORT: case DBF_MENU: p_ushort = (unsigned short *)p_data; break;
		case DBF_SHORT:                                p_short = (short *)p_data;           break;
		case DBF_ULONG:                                p_ulong = (unsigned long *)p_data;   break;
		case DBF_LONG:                                 p_long = (long *)p_data;             break;
		case DBF_FLOAT:                                p_float = (float *)p_data;           break;
		case DBF_DOUBLE:                               p_double = (double *)p_data;         break;
		case DBF_NOACCESS:
			break; /* just go through the motions, so we can parse the file */
		default:
			errlogPrintf("save_restore: field_type '%s' not handled\n", pamapdbfType[field_type].strvalue);
			status = -1;
			break;
		}
		/** read array values **/
		Debug(11,"parsing buffer '%s'\n", value_string);
		if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) {
			begin_mark_found = 1;
			Debug(10,"parsing array buffer '%s'\n", bp);
			for (num_read=0; (num_read<max_elements) && bp && !end_mark_found; ) {
				/* Find beginning of array element */
				Debug(10,"looking for element[%ld] \n", num_read);
				while ((*bp != ELEMENT_BEGIN) && !end_mark_found && !end_of_file) {
					Debug(12,"...buffer contains '%s'\n", bp);
					switch (*bp) {
					case '\0':
						if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
							errlogPrintf("save_restore: *** EOF during array-parse\n");
							end_of_file = 1;
						}
						break;
					case ARRAY_END:
						end_mark_found = 1;
						break;
					default:
						++bp;
						break;
					}
				}
				/*
				 * Read one element: Accumulate characters of element value into string[],
				 * ignoring any nonzero control characters, and append the value to the local array.
				 */
				if (bp && !end_mark_found && !end_of_file) {
					/* *bp == ELEMENT_BEGIN */
					Debug(11,"Found element-begin; buffer contains '%s'\n", bp);
					for (bp++, j=0; (j < MAX_STRING_SIZE-1) && (*bp != ELEMENT_END); bp++) {
						if (*bp == '\0') {
							if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
								errlogPrintf("save_restore:array_restore: *** premature EOF.\n");
								end_of_file = 1;
								break;
							}
							Debug(11,"new buffer: '%s'\n", bp);
							if (*bp == ELEMENT_END) break;
						} else if ((*bp == ESCAPE) && ((bp[1] == ELEMENT_BEGIN) || (bp[1] == ELEMENT_END))) {
							/* escaped character */
							bp++;
						}
						if (isprint((int)(*bp))) string[j++] = *bp; /* Ignore, e.g., embedded newline */
					}
					string[j] = '\0';
					Debug(10,"element[%ld] value = '%s'\n", num_read, string);
					if (bp) Debug(11,"look for element-end: buffer contains '%s'\n", bp);
					/*
					 * We've accumulated all the characters, or all we can handle in string[].
					 * If there are more characters than we can handle, just pretend we read them.
					 */
					/* *bp == ELEMENT_END ,*/
 					for (found = 0; (found == 0) && !end_of_file; ) {
						while (*bp && (*bp != ELEMENT_END) && (*bp != ESCAPE)) bp++;
						switch (*bp) {
						case ELEMENT_END:
							found = 1; bp++; break;
						case ESCAPE:
							if (*(++bp) == ELEMENT_END) bp++; break;
						default:
							if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
								end_of_file = 1;
								found = 1;
							}
						}
					}
					/* Append value to local array. */
					if (p_data) {
						switch (field_type) {
						case DBF_STRING:
							/* future: translate escape sequence */
							strncpy(&(p_char[(num_read++)*MAX_STRING_SIZE]), string, MAX_STRING_SIZE);
							break;
						case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
							p_ushort[num_read++] = (unsigned short)atol(string);
							break;
						case DBF_UCHAR:
							p_uchar[num_read++] = (unsigned char)atol(string);
							break;
						case DBF_CHAR:
							p_char[num_read++] = (char)atol(string);
							break;
						case DBF_SHORT:
							p_short[num_read++] = (short)atol(string);
							break;
						case DBF_LONG:
							p_long[num_read++] = atol(string);
							break;
						case DBF_ULONG:
							p_ulong[num_read++] = (unsigned long)atol(string);
							break;
						case DBF_FLOAT:
							p_float[num_read++] = mySafeDoubleToFloat(atof(string));
							break;
						case DBF_DOUBLE:
							p_double[num_read++] = atof(string);
							break;
						case DBF_NOACCESS:
						default:
							break;
						}
					}
				}
			} /* for (num_read=0; (num_read<max_elements) && bp && !end_mark_found; ) */

			if ((save_restoreDebug >= 10) && p_data) {
				errlogPrintf("\nsave_restore: %ld array values:\n", num_read);
				for (j=0; j<num_read; j++) {
					switch (field_type) {
					case DBF_STRING:
						errlogPrintf("	'%s'\n", &(p_char[j*MAX_STRING_SIZE])); break;
					case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
						errlogPrintf("	%u\n", p_ushort[j]); break;
					case DBF_SHORT:
						errlogPrintf("	%d\n", p_short[j]); break;
					case DBF_UCHAR:
						errlogPrintf("	'%c' (%u)\n", p_uchar[j], p_uchar[j]); break;
					case DBF_CHAR:
						errlogPrintf("	'%c' (%d)\n", p_char[j], p_char[j]); break;
					case DBF_ULONG:
						errlogPrintf("	%lu\n", p_ulong[j]); break;
					case DBF_LONG:
						errlogPrintf("	%ld\n", p_long[j]); break;
					case DBF_FLOAT:
						errlogPrintf("	%f\n", p_float[j]); break;
					case DBF_DOUBLE:
						errlogPrintf("	%g\n", p_double[j]); break;
					case DBF_NOACCESS:
					default:
						break;
					}
				}
				errlogPrintf("save_restore: end of array values.\n\n");
				epicsThreadSleep(0.5);
			}

		} /* if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) */
	}
	/* leave the file pointer ready for next PV (next fgets() should yield next PV) */
	if (begin_mark_found) {
		/* find ARRAY_END (but ARRAY_END inside an element is just another character) */
		Debug(10, "looking for ARRAY_END\n");
		in_element = 0;
		while (!end_mark_found && !end_of_file) {
			Debug(11,"...buffer contains '%s'\n", bp);
			switch (*bp) {
			case ESCAPE:
				if (in_element && (bp[1] == ELEMENT_END)) bp++; /* two chars treated as one */
				break;
			case ARRAY_END:
				if (!in_element) end_mark_found = 1;
				break;
			case '\0':
				if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
					errlogPrintf("save_restore: *** EOF during array-end search\n");
					end_of_file = 1;
				}
				break;
			default:
				/* Can't use ELEMENT_BEGIN, ELEMENT_END as cases; they might be the same. */
				if ((*bp == ELEMENT_BEGIN) || (*bp == ELEMENT_END)) in_element = !in_element;
				break;
			}
			if (bp) ++bp;
		}
	} else {
		Debug(10, "ARRAY_BEGIN wasn't found; going to next line of input file\n");
		status = -1;
		/* just get next line, assuming it contains the next PV */
		if (!end_of_file) {
			if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) end_of_file = 1;
		}
	}
	if (!status && end_of_file) status = end_of_file;
	if (pass == 0) {
		Debug(1, "No array write in pass 0.\n");
	} else {
		if (!status && p_data) {
			Debug(1, "Writing array to database\n");
			status = SR_put_array_values(PVname, p_data, num_read);
		} else {
			Debug(1, "No array write to database attempted because of error condition\n");
		}
	}
	if (p_data == NULL) status = -1;
	return(status);
}


/*
 * file_restore
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database values during iocInit.  Before database initialization
 * must use STATIC database access routines.
 * Expect the following syntax:
 *    [<error string>]
 *    which is "! <number> <optional descriptive text>"
 * OR
 *    [<ignore>]<PV name><white>...<value>
 *    where
 *       <ignore> is the character '#'
 *       <PV name> is any legal EPICS PV name
 *       <white> is whitespace
 *       <value> is
 *          <printable>[<printable>...][<white>...][<anything>]
 *       e.g., "1.2"
 *       OR
 *          @array@ { "<val>" "<val>" }
 */
int reboot_restore(char *filename, initHookState init_state)
{
	char		PVname[80];
	char		bu_filename[259], fname[256] = "";
	char		buffer[BUF_SIZE], *bp;
	char		value_string[BUF_SIZE];
	char		datetime[32];
	char		c;
	FILE		*inp_fd;
	int			i, found_field, pass;
	DBENTRY		dbentry;
	DBENTRY		*pdbentry = &dbentry;
	long		status;
	int			n, write_backup, num_errors, is_scalar;
	long		*pStatusVal = 0;
	char		*statusStr = 0;
	
	errlogPrintf("reboot_restore (v%s): entry for file '%s'\n", VERSION, filename);
	/* initialize database access routines */
	if (!pdbbase) {
		errlogPrintf("No Database Loaded\n");
		return(OK);
	}
	dbInitEntry(pdbbase,pdbentry);

	/* what are we supposed to do here? */
	if (init_state >= initHookAfterInitDatabase) {
		pass = 1;
		for (i = 0; i < restoreFileList.pass1cnt; i++) {
			if (strcmp(filename, restoreFileList.pass1files[i]) == 0) {
				pStatusVal = &(restoreFileList.pass1Status[i]);
				statusStr = restoreFileList.pass1StatusStr[i];
			}
		}
	} else {
		pass = 0;
		for (i = 0; i < restoreFileList.pass0cnt; i++) {
			if (strcmp(filename, restoreFileList.pass0files[i]) == 0) {
				pStatusVal = &(restoreFileList.pass0Status[i]);
				statusStr = restoreFileList.pass0StatusStr[i];
			}
		}
	}

	if ((pStatusVal == 0) || (statusStr == 0)) {
		errlogPrintf("reboot_restore: Can't find filename '%s' in list.\n",
			filename);
	}

	/* open file */
	strncpy(fname, saveRestoreFilePath, sizeof(fname) -1);
	strncat(fname, filename, MAX(sizeof(fname) -1 - strlen(fname),0));
	errlogPrintf("*** restoring from '%s' at initHookState %d ***\n",
		fname, (int)init_state);
	if ((inp_fd = fopen_and_check(fname, "r", &status)) == NULL) {
		errlogPrintf("save_restore: Can't open save file.");
		if (pStatusVal) *pStatusVal = SR_STATUS_FAIL;
		if (statusStr) strcpy(statusStr, "Can't open save file.");
		return(ERROR);
	}
	if (status) {
		if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
		if (statusStr) strcpy(statusStr, "Bad .sav(B) files; used seq. backup");
	}

	(void)fgets(buffer, BUF_SIZE, inp_fd); /* discard header line */
	Debug(1, "reboot_restore: header line '%s'\n", buffer);
	/* restore from data file */
	num_errors = 0;
	while ((bp=fgets(buffer, BUF_SIZE, inp_fd))) {
		/*
		 * get PV_name, one space character, value
		 * (value may be a string with leading whitespace; it may be
		 * entirely whitespace; the number of spaces may be crucial;
		 * it might consist of zero characters; and it might be an array.)
		 * If the value is an array, it has the form @array@ { "val1" "val2" .. }
		 * the array of values may be broken at any point by '\n', or other
		 * character for which isprint() is false.
		 * sample input lines:
		 * xxx:interp.E 100
		 * xxx:interp.C @array@ { "1" "0.99" }
		 */
		n = sscanf(bp,"%s%c%[^\n]", PVname, &c, value_string);
		if (n<3) *value_string = 0;
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			if (strchr(PVname,'.') == 0) strcat(PVname,".VAL"); /* if no field name, add default */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (save_restoreDebug > 9) errlogPrintf("\n");
			Debug(10,"Attempting to put %s '%s' to '%s'\n", is_scalar?"scalar":"array", value_string, PVname);
			found_field = 1;
			if ((status = dbFindRecord(pdbentry, PVname)) != 0) {
				errlogPrintf("dbFindRecord for '%s' failed\n", PVname);
				num_errors++; found_field = 0;
			} else if (dbFoundField(pdbentry) == 0) {
				errlogPrintf("save_restore: dbFindRecord did not find field '%s'\n", PVname);
				num_errors++; found_field = 0;
			}
			if (found_field) {
				if (is_scalar) {
					status = scalar_restore(pass, pdbentry, PVname, value_string);
				} else {
					status = SR_array_restore(pass, inp_fd, PVname, value_string);
				}
				if (status) {
					errlogPrintf("save_restore: restore for PV '%s' failed\n", PVname);
					num_errors++;
				}
			} /* if (found_field) {... */
		} else if (PVname[0] == '!') {
			/*
			* string is an error message -- something like:
			* '! 7 channel(s) not connected - or not all gets were successful'
			*/
			n = (int)atol(&bp[1]);
			errlogPrintf("%d %s had no saved value.\n", n, n==1?"PV":"PVs");
			if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
			if (statusStr) strcpy(statusStr, ".sav file contained an error message");
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("aborting restore\n");
				fclose(inp_fd);
				dbFinishEntry(pdbentry);
				if (pStatusVal) *pStatusVal = SR_STATUS_FAIL;
				if (statusStr) strcpy(statusStr, "restore aborted");
				return(ERROR);
			}
		} else if (PVname[0] == '<') {
			/* end of file */
			break;
		}
	}
	fclose(inp_fd);
	dbFinishEntry(pdbentry);

	/* If this is the second pass for a restore file, don't write backup file again.*/
	write_backup = 1;
	if (init_state >= initHookAfterInitDatabase) {
		for(i = 0; i < restoreFileList.pass0cnt; i++) {
			if (strcmp(filename, restoreFileList.pass0files[i]) == 0) {
				write_backup = 0;
				break;
			}
		}
	}

	if (write_backup) {
		/* write  backup file*/
		strcpy(bu_filename,fname);
		if (save_restoreDatedBackupFiles && (fGetDateStr(datetime) == 0)) {
			strcat(bu_filename, "_");
			strcat(bu_filename, datetime);
		} else {
			strcat(bu_filename, ".bu");
		}
		Debug(1, "save_restore: writing boot-backup file '%s'.\n", bu_filename);
		status = (long)myFileCopy(fname,bu_filename);
		if (status) {
			errlogPrintf("save_restore: Can't write backup file.\n");
			if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
			if (statusStr) strcpy(statusStr, "Can't write backup file");
			return(OK);
		}
	}

	/* Record status */
	if (pStatusVal && statusStr) {
		if (*pStatusVal != 0) {
			/* Status and message have already been recorded */
			;
		} else if (num_errors != 0) {
			sprintf(statusStr, "%d %s", num_errors, num_errors==1?"PV error":"PV errors");
			*pStatusVal = SR_STATUS_WARN;
		} else {
			strcpy(statusStr, "No errors");
			*pStatusVal = SR_STATUS_OK;
		}
	}
	if (p_data) {
		free(p_data);
		p_data = NULL;
		p_data_size = 0;
	}
	errlogPrintf("reboot_restore: done with file '%s'\n\n", filename);
	return(OK);
}


int set_pass0_restoreFile( char *filename)
{
	char *cp;
	int fileNum = restoreFileList.pass0cnt;

	if (fileNum >= MAXRESTOREFILES) {
		errlogPrintf("set_pass0_restoreFile: MAXFILE count exceeded\n");
		return(ERROR);
	}
	cp = (char *)calloc(strlen(filename) + 4,sizeof(char));
	restoreFileList.pass0files[fileNum] = cp;
	if (cp == NULL) {
		errlogPrintf("set_pass0_restoreFile: calloc failed\n");
		restoreFileList.pass0StatusStr[fileNum] = (char *)0;
		return(ERROR);
	}
	strcpy(cp, filename);
	cp = (char *)calloc(40, 1);
	restoreFileList.pass0StatusStr[fileNum] = cp;
	strcpy(cp, "Unknown, probably failed");
	restoreFileList.pass0Status[fileNum] = SR_STATUS_FAIL;
	restoreFileList.pass0cnt++;
	return(OK);
}

int set_pass1_restoreFile(char *filename)
{
	char *cp;
	int fileNum = restoreFileList.pass1cnt;
	
	if (fileNum >= MAXRESTOREFILES) {
		errlogPrintf("set_pass1_restoreFile: MAXFILE count exceeded\n");
		return(ERROR);
	}
	cp = (char *)calloc(strlen(filename) + 4,sizeof(char));
	restoreFileList.pass1files[fileNum] = cp;
	if (cp == NULL) {
		errlogPrintf("set_pass1_restoreFile: calloc failed\n");
		restoreFileList.pass1StatusStr[fileNum] = (char *)0;
		return(ERROR);
	}
	strcpy(cp, filename);
	cp = (char *)calloc(40, 1);
	restoreFileList.pass1StatusStr[fileNum] = cp;
	strcpy(cp, "Unknown, probably failed");
	restoreFileList.pass1Status[fileNum] = SR_STATUS_FAIL;
	restoreFileList.pass1cnt++;
	return(OK);
}


FILE *fopen_and_check(const char *fname, const char *mode, long *status)
{
	FILE *inp_fd = NULL;
	char tmpstr[PATH_SIZE+50];
	char file[256];
	char datetime[32];
	int i, backup_sequence_num;
	struct stat fileStat;
	char *p;
	time_t currTime;
	double dTime, min_dTime;
	double version;
	
	*status = 0;	/* presume success */
	strncpy(file, fname, 255);
	if ((inp_fd = fopen(file, "r")) == NULL) {
		errlogPrintf("save_restore: Can't open file '%s'.\n", file);
	} else {
		fgets(tmpstr, 29, inp_fd);
		version = atof(strchr(tmpstr,(int)'V')+1);
		/* check out "successfully written" marker */
		if ((version > 1.79) && ((fseek(inp_fd, -6, SEEK_END)) ||
				(fgets(tmpstr, 6, inp_fd) == 0) ||
				(strncmp(tmpstr, "<END>", 5) != 0))) {
			fclose(inp_fd);
			/* File doesn't look complete, make a copy of it */
			errlogPrintf("save_restore: File '%s' is not trusted.\n",
					file);
			strcpy(tmpstr, file);
			strcat(tmpstr, "_RBAD_");
			if (save_restoreDatedBackupFiles) {
				fGetDateStr(datetime);
				strcat(tmpstr, datetime);
			}
			(void)myFileCopy(file, tmpstr);
		} else {
			fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
			return(inp_fd);
		}
	}

	/* Still here?  Try the backup file. */
	strncat(file, "B", 1);
	errlogPrintf("save_restore: Trying backup file '%s'\n", file);
	if ((inp_fd = fopen(file, "r")) == NULL) {
		errlogPrintf("save_restore: Can't open file '%s'\n", file);
	} else {
		fgets(tmpstr, 29, inp_fd);
		version = atof(strchr(tmpstr,(int)'V')+1);
		if ((version > 1.79) && ((fseek(inp_fd, -6, SEEK_END)) ||
				(fgets(tmpstr, 6, inp_fd) == 0) ||
				(strncmp(tmpstr, "<END>", 5) != 0))) {
			fclose(inp_fd);
			errlogPrintf("save_restore: File '%s' is not trusted.\n",
				file);
			fGetDateStr(datetime);
			strcpy(tmpstr, file);
			strcat(tmpstr, "_RBAD_");
			strcat(tmpstr, datetime);
			(void)myFileCopy(file, tmpstr);
		} else {
			fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
			return(inp_fd);
		}
	}

	/* Still haven't found a good file?  Try the sequenced backups */
	*status = 1;
	strcpy(file, fname);
	backup_sequence_num = -1;
	p = &file[strlen(file)];
	currTime = time(NULL);
	min_dTime = 1.e9;
	for (i=0; i<save_restoreNumSeqFiles; i++) {
		sprintf(p, "%1d", i);
		if (stat(file, &fileStat) == 0) {
			dTime = difftime(currTime, fileStat.st_mtime);
			if (save_restoreDebug >= 5) {
				errlogPrintf("'%s' modified at %s\n", file,
					ctime(&fileStat.st_mtime));
				errlogPrintf("'%s' is %f seconds old\n", file, dTime);
			}
			if (dTime < min_dTime) {
				min_dTime = dTime;
				backup_sequence_num = i;
			}
		}
	}

	/*
	 * Try the backup file. 
	 * Note these didn't exist before version 3.0, so no need
	 * to qualify the "<END>" test with a version number.
	 */
	for (i=0; i<save_restoreNumSeqFiles; i++) {
		sprintf(p, "%1d", backup_sequence_num);
		errlogPrintf("save_restore: Trying backup file '%s'\n", file);
		if ((inp_fd = fopen(file, "r")) == NULL) {
			errlogPrintf("save_restore: Can't open file '%s'\n", file);
		} else {
			if ((fseek(inp_fd, -6, SEEK_END)) ||
					(fgets(tmpstr, 6, inp_fd) == 0) ||
					(strncmp(tmpstr, "<END>", 5) != 0)) {
				fclose(inp_fd);
				errlogPrintf("save_restore: File '%s' is not trusted.\n",
					file);
				fGetDateStr(datetime);
				strcpy(tmpstr, file);
				strcat(tmpstr, "_RBAD_");
				strcat(tmpstr, datetime);
				(void)myFileCopy(file, tmpstr);
			} else {
				fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
				return(inp_fd);
			}
		}
		if (++backup_sequence_num >= save_restoreNumSeqFiles)
			backup_sequence_num = 0;
	}

	errlogPrintf("save_restore: Can't find a file to restore from...");
	errlogPrintf("save_restore: ...last tried '%s'. I give up.\n", file);
	errlogPrintf("save_restore: **********************************\n\n");
	return(0);
}


/*
 * These functions really belong to save_restore.c, but they use
 * database access, which is incompatible with cadef.h included in
 * save_restore.c.
 */
 
long SR_get_array_info(char *name, long *num_elements, long *field_size, long *field_type)
{
	DBADDR		dbaddr;
	DBADDR		*paddr = &dbaddr;
	long		status;

	status = dbNameToAddr(name, paddr);
	if (status) return(status);
	*num_elements = paddr->no_elements;
	*field_size = paddr->field_size;
	*field_type = paddr->field_type;
	return(0);
}


long SR_get_array(char *PVname, void *pArray, long *pnum_elements)
{
	DBADDR		dbaddr;
	DBADDR		*paddr = &dbaddr;
	long		status;
	dbfType		request_field_type;

	status = dbNameToAddr(PVname, paddr);
	if (status) return(status);
	dbScanLock((dbCommon *)paddr->precord);
	request_field_type = paddr->field_type;
	/*
	 * Not clear what we should do if someone has an array of enums
	 * or menu items.  For now, just do something that will work
	 * in the simplest case.
	 */
	if ((request_field_type == DBF_ENUM) || (request_field_type == DBF_MENU)) {
		errlogPrintf("save_restore:SR_get_array: field_type %s array read as DBF_USHORT\n",
			pamapdbfType[request_field_type].strvalue);
		request_field_type = DBF_USHORT;
	}
	status = dbGet(paddr, request_field_type, pArray, NULL, pnum_elements, NULL);
	dbScanUnlock((dbCommon *)paddr->precord);
	return(status);
}

long SR_write_array_data(FILE *out_fd, char *name, void *pArray, long num_elements)
{
	DBADDR		dbaddr;
	DBADDR		*paddr = &dbaddr;
	long		status;
	dbfType		field_type;
	long		i, j, n;
	char			*p_char = NULL, *pc;
	short			*p_short = NULL;
	long			*p_long = NULL;
	unsigned char	*p_uchar = NULL;
	unsigned short	*p_ushort = NULL;
	unsigned long	*p_ulong = NULL;
	float			*p_float = NULL;
	double			*p_double = NULL;

	status = dbNameToAddr(name, paddr);
	if (status) return(0);
	field_type = paddr->field_type;

	n = fprintf(out_fd, "%-s %1c ", ARRAY_MARKER, ARRAY_BEGIN);
	for (i=0; i<num_elements; i++) {
		switch(field_type) {
		case DBF_STRING:
			p_char = (char *)pArray;
			pc = &p_char[i*MAX_STRING_SIZE];
			n += fprintf(out_fd, "%1c", ELEMENT_BEGIN);
			for (j=0; j<MAX_STRING_SIZE-1 && *pc; j++, pc++) {
				if ((*pc == ELEMENT_BEGIN) || (*pc == ELEMENT_END)){
					n += fprintf(out_fd, "%1c", ESCAPE);
					j++;
				}
				n += fprintf(out_fd, "%1c", *pc);
			}
			n += fprintf(out_fd, "%1c ", ELEMENT_END);
			break;
		case DBF_CHAR:
			p_char = (char *)pArray;
			n += fprintf(out_fd, "%1c%d%1c ", ELEMENT_BEGIN, p_char[i], ELEMENT_END);
			break;
		case DBF_UCHAR:
			p_uchar = (unsigned char *)pArray;
			n += fprintf(out_fd, "%1c%u%1c ", ELEMENT_BEGIN, p_uchar[i], ELEMENT_END);
			break;
		case DBF_SHORT:
			p_short = (short *)pArray;
			n += fprintf(out_fd, "%1c%d%1c ", ELEMENT_BEGIN, p_short[i], ELEMENT_END);
			break;
		case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
			p_ushort = (unsigned short *)pArray;
			n += fprintf(out_fd, "%1c%u%1c ", ELEMENT_BEGIN, p_ushort[i], ELEMENT_END);
			break;
		case DBF_LONG:
			p_long = (long *)pArray;
			n += fprintf(out_fd, "%1c%ld%1c ", ELEMENT_BEGIN, p_long[i], ELEMENT_END);
			break;
		case DBF_ULONG:
			p_ulong = (unsigned long *)pArray;
			n += fprintf(out_fd, "%1c%lu%1c ", ELEMENT_BEGIN, p_ulong[i], ELEMENT_END);
			break;
		case DBF_FLOAT:
			p_float = (float *)pArray;
			n += fprintf(out_fd, "%1c", ELEMENT_BEGIN);
			n += fprintf(out_fd, FLOAT_FMT, p_float[i]);
			n += fprintf(out_fd, "%1c ", ELEMENT_END);
			break;
		case DBF_DOUBLE:
			p_double = (double *)pArray;
			n += fprintf(out_fd, "%1c", ELEMENT_BEGIN);
			n += fprintf(out_fd, DOUBLE_FMT, p_double[i]);
			n += fprintf(out_fd, "%1c ", ELEMENT_END);
			break;
		default:
			errlogPrintf("save_restore: field_type %d not handled.\n", field_type);
			break;
		}
	}
	n += fprintf(out_fd, "%1c\n", ARRAY_END);
	return(n);
}


/* set_pass0_restoreFile() */
STATIC const iocshArg set_passN_Arg = {"file",iocshArgString};
STATIC const iocshArg * const set_passN_Args[1] = {&set_passN_Arg};
STATIC const iocshFuncDef set_pass0_FuncDef = {"set_pass0_restoreFile",1,set_passN_Args};
STATIC void set_pass0_CallFunc(const iocshArgBuf *args)
{
    set_pass0_restoreFile(args[0].sval);
}

/* set_pass1_restoreFile() */
STATIC const iocshFuncDef set_pass1_FuncDef = {"set_pass1_restoreFile",1,set_passN_Args};
STATIC void set_pass1_CallFunc(const iocshArgBuf *args)
{
    set_pass1_restoreFile(args[0].sval);
}

/* void dbrestoreShow(void) */
STATIC const iocshFuncDef dbrestoreShow_FuncDef = {"dbrestoreShow",0,NULL};
STATIC void dbrestoreShow_CallFunc(const iocshArgBuf *args)
{
    dbrestoreShow();
}

void dbrestoreRegister(void)
{
    iocshRegister(&set_pass0_FuncDef, set_pass0_CallFunc);
    iocshRegister(&set_pass1_FuncDef, set_pass1_CallFunc);
	iocshRegister(&dbrestoreShow_FuncDef, dbrestoreShow_CallFunc);
}

epicsExportRegistrar(dbrestoreRegister);
