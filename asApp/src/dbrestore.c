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
 *                Previously, restoreFileList.pass<n>Status wasn't initialized.
 * 10/04/04  tmm  Allow DOS line termination (CRLF) or unix (LF) in .sav files.
 *                Also, collapsed some code, and modified the way sequenced
 *                backup-file dates are compared to current date.
 * 10/29/04  tmm  v4.1 Added revision descriptions that should have been in
 *                previous revisions.  Changed VERSION number, in agreement
 *                with save_restore's SRVERSION string.
 * 11/30/04  tmm  v4.3 Added debug for curr_num_elements.
 * 01/26/05  tmm  v4.4 Strip trailing '\r' from value string.  (Previously, only
 *                '\n' was stripped.)
 * 01/28/05  tmm  v4.5 Filenames specified in set_pass<n>_restoreFile() now
 *                initialized with status SR_STATUS_INIT ('No Status')
 * 02/03/05  tmm  v4.6 copy VERSION to a variable, instead of using it directly
 *                as a string arg.  Check that restoreFileList.pass<n>files[i]
 *                is not NULL before comparing the string it's supposed to be
 *                pointing to.  Neither of those things fixed the crash, but
 *                replacing errlogPrintf with printf in reboot_restore did.
 * 06/27/05  tmm  v4.7 Dirk Zimoch (SLS) found and fixed problems with .sav
 *                files that lack header lines, or lack a version number.
 *                Check return codes from some calls to fseek().
 * 03/28/06  tmm  Replaced all Debug macros with straight code
 * 10/05/06  tmm  v4.8 Use binary mode for fopen() calls in myFileCopy, to avoid
 *                file-size differences caused by different line terminators
 *                on different operating systems.  (Thanks to Kay Kasemir.)
 * 01/02/07  tmm  v4.9 Convert empty SPC_CALC fields to "0" before restoring.
 * 03/19/07  tmm  v4.10 Don't print errno unless function returns an error.
 * 08/03/07  tmm  v4.11 Added functions makeAutosaveFileFromDbInfo() and makeAutosaveFiles()
 *                which search through the loaded database, looking for info nodes indicating
 *                fields that are to be autosaved.
 * 09/11/09  tmm  v4.12 If recordname is an alias (>=3.14.11), don't search for info nodes.
 *                
 */
#define VERSION "5.1"

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
#include	<errlog.h>
#include	<iocsh.h>
#include 	"fGetDateStr.h"
#include	"save_restore.h"
#include	<special.h>
#include	<macLib.h>
#include	<epicsString.h>
#include	<dbAccessDefs.h>
#include	<epicsStdio.h>
#include	<epicsExport.h>
#include    <epicsStdlib.h>

#ifndef vxWorks
#define OK 0
#define ERROR -1
#endif

/* EPICS base version tests.*/
#define LT_EPICSBASE(v,r,l) ((EPICS_VERSION<=(v)) && (EPICS_REVISION<=(r)) && (EPICS_MODIFICATION<(l)))
#define GE_EPICSBASE(v,r,l) ((EPICS_VERSION>=(v)) && (EPICS_REVISION>=(r)) && (EPICS_MODIFICATION>=(l)))

int restoreFileListsInitialized=0;

ELLLIST pass0List;
ELLLIST pass1List;

void myPrintErrno(char *s, char *file, int line) {
	errlogPrintf("%s(%d): [0x%x]=%s:%s\n", file, line, errno, s, strerror(errno));
}

float mySafeDoubleToFloat(double d)
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

void maybeInitRestoreFileLists() {
	if (!restoreFileListsInitialized) {
		ellInit(&pass0List);
		ellInit(&pass1List);
		restoreFileListsInitialized = 1;
	}
}

void dbrestoreShow(void)
{
	struct restoreFileListItem *pLI;

	maybeInitRestoreFileLists();

	printf("  '     filename     ' -  status  - 'message' - 'macro string'\n");
	printf("  pass 0:\n");
	pLI = (struct restoreFileListItem *) ellFirst(&pass0List);
	while (pLI) {
		printf("  '%s' - %s - '%s' - '%s'\n", pLI->filename,
			SR_STATUS_STR[pLI->restoreStatus], pLI->restoreStatusStr,
			pLI->macrostring ? pLI->macrostring : "None");
		pLI = (struct restoreFileListItem *) ellNext(&(pLI->node));
	}

	printf("  pass 1:\n");
	pLI = (struct restoreFileListItem *) ellFirst(&pass1List);
	while (pLI) {
		printf("  '%s' - %s - '%s'\n", pLI->filename,
			SR_STATUS_STR[pLI->restoreStatus], pLI->restoreStatusStr);
		pLI = (struct restoreFileListItem *) ellNext(&(pLI->node));
	}
}

STATIC int myFileCopy(const char *source, const char *dest)
{
	FILE 	*source_fd, *dest_fd;
	char	buffer[BUF_SIZE], *bp;
	struct stat fileStat;
	int		chars_printed, size=0;

	if (save_restoreDebug >= 5)
		errlogPrintf("dbrestore:myFileCopy: copying '%s' to '%s'\n", source, dest);

	if (stat(source, &fileStat) == 0) size = (int)fileStat.st_size;
	errno = 0;
	if ((source_fd = fopen(source,"rb")) == NULL) {
		errlogPrintf("save_restore:myFileCopy: Can't open file '%s'\n", source);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
		if (++save_restoreIoErrors > save_restoreRemountThreshold) 
			save_restoreNFSOK = 0;
		return(ERROR);
	}
	errno = 0;
	/* Note: under vxWorks, the following fopen() frequently will set errno
	 * to S_nfsLib_NFSERR_NOENT even though it succeeds.  Probably this means
	 * a failed attempt was retried. (System calls never set errno to zero.)
	 */
	if ((dest_fd = fopen(dest,"wb")) == NULL) {
		errlogPrintf("save_restore:myFileCopy: Can't open file '%s'\n", dest);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
		fclose(source_fd);
		return(ERROR);
	}
	chars_printed = 0;
	while ((bp=fgets(buffer, BUF_SIZE, source_fd))) {
		errno = 0;
		chars_printed += fprintf(dest_fd, "%s", bp);
		/* if (errno) {myPrintErrno("myFileCopy", __FILE__, __LINE__); errno = 0;} */
	}
	errno = 0;
	if (fclose(source_fd) != 0){
                errlogPrintf("save_restore:myFileCopy: Error closing file '%s'\n", source);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
	}
	errno = 0;
	if (fclose(dest_fd) != 0){
		errlogPrintf("save_restore:myFileCopy: Error closing file '%s'\n", dest);
		/* if (errno) myPrintErrno("myFileCopy", __FILE__, __LINE__); */
	}
	errno = 0;
	if (size && (chars_printed != size)) {
		errlogPrintf("myFileCopy: size=%d, chars_printed=%d\n",
			size, chars_printed);
		return(ERROR);
	}
	return(OK);
}


STATIC long scalar_restore(int pass, DBENTRY *pdbentry, char *PVname, char *value_string, int is_long_string)
{
	long 	n, status = 0;
	DBADDR	dbaddr;
	DBADDR	*paddr = &dbaddr;
	dbfType field_type = pdbentry->pflddes->field_type;
	short special = pdbentry->pflddes->special;
	/* The buffer holding the string value must be at least one byte longer than
	   the actual value (due to the terminating null byte). */
	size_t value_string_len = strlen(value_string) + 1;

	/* We do know the length of the buffer for sure, because this depends on the
	   calling code, so we limit to the actual string size. */
	epicsStrnRawFromEscaped(value_string, value_string_len, value_string, value_string_len);

	if (save_restoreDebug >= 5) errlogPrintf("dbrestore:scalar_restore:entry:field type '%s'\n", pamapdbfType[field_type].strvalue);
	switch (field_type) {
	case DBF_STRING: case DBF_ENUM:
	case DBF_CHAR:   case DBF_UCHAR:
	case DBF_SHORT:  case DBF_USHORT:
	case DBF_LONG:   case DBF_ULONG:
	#ifdef DBR_INT64
	case DBF_INT64:  case DBF_UINT64:
	#endif
	case DBF_FLOAT:  case DBF_DOUBLE:
		/*
		 * check SPC_CALC fields against new (3.13.9) requirement that CALC
		 * fields not be empty.
		 */
		if ((field_type==DBF_STRING) && (special==SPC_CALC)){
			if (*value_string == 0) strcpy(value_string, "0");
		}

		status = dbPutString(pdbentry, value_string);
		if (save_restoreDebug >= 15) {
			errlogPrintf("dbrestore:scalar_restore: dbPutString() returns %ld:", status);
			errMessage(status, " ");
		}
		
		break;

	case DBF_INLINK: case DBF_OUTLINK: case DBF_FWDLINK:
		/* Can't restore links in pass 1 */
		if (pass == 0) {
			status = dbPutString(pdbentry, value_string);
			if (save_restoreDebug >= 15) {
				errlogPrintf("dbrestore:scalar_restore: dbPutString() returns %ld:", status);
				errMessage(status, " ");
			}
		} else if (save_restoreDebug > 1) {
				errlogPrintf("dbrestore:scalar_restore: Can't restore link field (%s) in pass 1.\n", PVname);
		}
		break;

	case DBF_MENU:
		n = (int)atol(value_string);
		status = dbPutMenuIndex(pdbentry, n);
		if (save_restoreDebug >= 15) {
			errlogPrintf("dbrestore:scalar_restore: dbPutMenuIndex() returns %ld:", status);
			errMessage(status, " ");
		}
		break;

	case DBF_NOACCESS:
		if (pass == 1) {
			status = dbNameToAddr(PVname, paddr);
			if (!status) {
				if (is_long_string && paddr->field_type == DBF_CHAR) {
					status = dbPut(paddr, DBF_CHAR, value_string, strlen(value_string) + 1);
				} else {
					status = dbPut(paddr, DBF_STRING, value_string, 1);
				}
			}
		} else if (save_restoreDebug > 1) {
			errlogPrintf("dbrestore:scalar_restore: Can't restore DBF_NOACCESS field (%s) in pass 0.\n", PVname);
		}
		break;

	default:
		status = -1;
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:scalar_restore: field_type '%d' not handled\n", field_type);
		}
		break;
	}
	if (status) {
		errlogPrintf("dbrestore:scalar_restore: restore of '%s' for '%s' failed\n",
			value_string, PVname);
		errMessage(status," ");
	}
	if (save_restoreDebug >= 15) {
		errlogPrintf("dbrestore:scalar_restore: dbGetString() returns '%s'\n",dbGetString(pdbentry));
	}
	return(status);
}

static void *p_data = NULL;
static long p_data_size = 0;

long SR_put_array_values(char *PVname, void *p_data, long num_values)
{
	DBADDR dbaddr;
	DBADDR *paddr = &dbaddr;
	long status, max_elements=0;
	STATIC long curr_no_elements=0, offset=0;
	rset *prset;
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
	if (save_restoreDebug >= 5) {
		errlogPrintf("dbrestore:SR_put_array_values: restoring %ld values to %s (max_elements=%ld)\n", num_values, PVname, max_elements);
	}
	if (VALID_DB_REQ(field_type)) {
		status = (*dbPutConvertRoutine[field_type][field_type])(paddr,p_data,num_values,max_elements,offset);
	} else {
		errlogPrintf("save_restore:SR_put_array_values: PV %s: bad field type '%d'\n",
			PVname, (int) field_type);
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
 * to PV named *PVname.  If gobble, just parse the array data and position inp_fd at the
 * next input line.
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
long SR_array_restore(int pass, FILE *inp_fd, char *PVname, char *value_string, int gobble)
{
	int				j, end_mark_found=0, begin_mark_found=0, end_of_file=0, found=0, in_element=0;
	long			status=0, max_elements=0, num_read=0;
	char			buffer[BUF_SIZE], *bp = NULL;
	char			string[MAX_STRING_SIZE];
	DBADDR			dbaddr;
	DBADDR			*paddr = &dbaddr;
	dbfType			field_type = DBF_NOACCESS;
	int				field_size = 0;
	char			*p_char = NULL;
	short			*p_short = NULL;
	epicsInt32		*p_long = NULL;
	epicsInt64		*p_int64 = NULL;
	unsigned char	*p_uchar = NULL;
	unsigned short	*p_ushort = NULL;
	epicsUInt32		*p_ulong = NULL;
	epicsUInt64		*p_uint64 = NULL;
	float			*p_float = NULL;
	double			*p_double = NULL;


	if (save_restoreDebug >= 1) {
		errlogPrintf("dbrestore:SR_array_restore:entry: PV = '%s'\n", PVname);
	}
	if (!gobble) {
		status = dbNameToAddr(PVname, paddr);
		if (status != 0) {
			errlogPrintf("save_restore: dbNameToAddr can't find PV '%s'\n", PVname);
			gobble = 1;
		}
	}

	if (!gobble) {
		/*** set up infrastructure for collecting array elements from file into local array ***/
		max_elements = paddr->no_elements;
		field_type = paddr->field_type;
		field_size = paddr->field_size;
		/* if we've already allocated a big enough memory block, use it */
		if ((p_data == NULL) || ((max_elements * field_size) > p_data_size)) {
			if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:SR_array_restore: p_data = %p, p_data_size = %ld\n", p_data, p_data_size);
			}
			if (p_data) free(p_data);
			p_data = (void *)calloc(max_elements, field_size);
			p_data_size = p_data ? max_elements * field_size : 0;
			if (save_restoreDebug >= 10) errlogPrintf("dbrestore:SR_array_restore: allocated p_data = %p, p_data_size = %ld\n", p_data, p_data_size);
		} else {
			memset(p_data, 0, p_data_size);
		}
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: Looking for up to %ld elements of field-size %d\n", max_elements, field_size);
			errlogPrintf("dbrestore:SR_array_restore: ...field_type is '%s' (%d)\n", pamapdbfType[field_type].strvalue, field_type);
		}

		switch (field_type) {
		case DBF_STRING: case DBF_CHAR:                p_char = (char *)p_data;             break;
		case DBF_UCHAR:                                p_uchar = (unsigned char *)p_data;   break;
		case DBF_ENUM: case DBF_USHORT: case DBF_MENU: p_ushort = (unsigned short *)p_data; break;
		case DBF_SHORT:                                p_short = (short *)p_data;           break;
		case DBF_ULONG:                                p_ulong = (epicsUInt32 *)p_data;     break;
		case DBF_LONG:                                 p_long = (epicsInt32 *)p_data;       break;
		#ifdef DBR_INT64
		case DBF_INT64:                                p_int64 = (epicsInt64 *)p_data;      break;
		case DBF_UINT64:                               p_uint64 = (epicsUInt64 *)p_data;    break;
		#endif
		case DBF_FLOAT:                                p_float = (float *)p_data;           break;
		case DBF_DOUBLE:                               p_double = (double *)p_data;         break;
		case DBF_NOACCESS:
			break; /* just go through the motions, so we can parse the file */
		default:
			errlogPrintf("save_restore: field_type '%s' not handled\n", pamapdbfType[field_type].strvalue);
			status = -1;
			break;
		}
	}


	/** read array values **/
	if (save_restoreDebug >= 11) {
		errlogPrintf("dbrestore:SR_array_restore: parsing buffer '%s'\n", value_string);
	}

	if (value_string==NULL || *value_string=='\0') {
		if (save_restoreDebug >= 11) {
			errlogPrintf("dbrestore:SR_array_restore: value_string is null or empty\n");
		}
		/* nothing to write; write zero or "" */
		if (p_data) {
			switch (field_type) {
			case DBF_STRING:
				strcpy(p_char, "");
				break;
			case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
				p_ushort[num_read++] = (unsigned short)0;
				break;
			case DBF_UCHAR:
				p_uchar[num_read++] = (unsigned char)0;
				break;
			case DBF_CHAR:
				p_char[num_read++] = (char)0;
				break;
			case DBF_SHORT:
				p_short[num_read++] = (short)0;
				break;
			case DBF_LONG:
				p_long[num_read++] = (epicsInt32) 0;
				break;
			case DBF_ULONG:
				p_ulong[num_read++] = (epicsUInt32) 0;
				break;
			#ifdef DBR_INT64
			case DBF_INT64:
				p_int64[num_read++] = (epicsInt64) 0;
				break;
			case DBF_UINT64:
				p_uint64[num_read++] = (epicsUInt64) 0;
				break;
			#endif
			case DBF_FLOAT:
				p_float[num_read++] = 0;
				break;
			case DBF_DOUBLE:
				p_double[num_read++] = 0;
				break;
			case DBF_NOACCESS:
			default:
				break;
			}
		}
	} else if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) == NULL) {
		if (save_restoreDebug >= 11) {
			errlogPrintf("dbrestore:SR_array_restore: ARRAY_BEGIN not found\n");
		}
		/* doesn't look like array data.  just restore what we have */
		if (p_data) {
			/* We do know the length of the buffer for sure, because this
			   depends on the calling code, so we limit to the actual string
			   size. The buffer must be one byte longer, due to the terminating
			   null byte. */
			size_t value_string_len = strlen(value_string) + 1;
			epicsStrnRawFromEscaped(value_string, value_string_len, value_string, value_string_len);
			switch (field_type) {
			case DBF_STRING:
				/* future: translate escape sequence */
				strNcpy(&(p_char[(num_read++)*MAX_STRING_SIZE]), value_string, MAX_STRING_SIZE);
				break;
			case DBF_ENUM: case DBF_USHORT: case DBF_MENU:
				p_ushort[num_read++] = (unsigned short)atol(value_string);
				break;
			case DBF_UCHAR:
				p_uchar[num_read++] = (unsigned char)atol(value_string);
				break;
			case DBF_CHAR:
				p_char[num_read++] = (char)atol(value_string);
				break;
			case DBF_SHORT:
				p_short[num_read++] = (short)atol(value_string);
				break;
			case DBF_LONG:
				p_long[num_read++] = (epicsInt32) atol(value_string);
				break;
			case DBF_ULONG:
				p_ulong[num_read++] = (epicsUInt32) strtoul(value_string,NULL,0);
				break;
			#ifdef DBR_INT64
			case DBF_INT64:
				epicsParseInt64(value_string, &p_int64[num_read++], 10, NULL);
				break;
			case DBF_UINT64:
				epicsParseUInt64(value_string, &p_uint64[num_read++], 10, NULL);
				break;
			#endif
			case DBF_FLOAT:
				p_float[num_read++] = mySafeDoubleToFloat(atof(value_string));
				break;
			case DBF_DOUBLE:
				p_double[num_read++] = atof(value_string);
				break;
			case DBF_NOACCESS:
			default:
				break;
			}
		}
	} else if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) {
		begin_mark_found = 1;
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: parsing array buffer '%s'\n", bp);
		}
		for (num_read=0; bp && !end_mark_found; ) {
			/* Find beginning of array element */
			if (save_restoreDebug >= 10) {
				errlogPrintf("dbrestore:SR_array_restore: looking for element[%ld] \n", num_read);
			}
			/* If truncated-file detector (checkFile) fails, test for end of file before
			 * using *bp */
			while (!end_mark_found && !end_of_file && (*bp != ELEMENT_BEGIN)) {
				if (save_restoreDebug >= 12) {
					errlogPrintf("dbrestore:SR_array_restore: ...buffer contains '%s'\n", bp);
				}
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
				if (save_restoreDebug >= 11) {
					errlogPrintf("dbrestore:SR_array_restore: Found element-begin; buffer contains '%s'\n", bp);
				}
				for (bp++, j=0; (j < MAX_STRING_SIZE-1) && (*bp != ELEMENT_END); bp++) {
					if (save_restoreDebug >= 11) errlogPrintf("dbrestore:SR_array_restore: *bp=%c (%d)\n", *bp, (int)*bp);
					if (*bp == '\0') {
						if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
							errlogPrintf("save_restore:array_restore: *** premature EOF.\n");
							end_of_file = 1;
							break;
						}
						if (save_restoreDebug >= 11) {
							errlogPrintf("dbrestore:SR_array_restore: new buffer: '%s'\n", bp);
						}
						if (*bp == ELEMENT_END) break;
					} else if ((*bp == ESCAPE) && ((bp[1] == ELEMENT_BEGIN) || (bp[1] == ELEMENT_END) || (bp[1] == ESCAPE))) {
						/* escaped character */
						bp++;
					}
					if (isprint((int)(*bp))) string[j++] = *bp; /* Ignore, e.g., embedded newline */
				}
				string[j] = '\0';
				if (save_restoreDebug >= 10) {
					errlogPrintf("dbrestore:SR_array_restore: element[%ld] value = '%s'\n", num_read, string);
					if (bp) errlogPrintf("dbrestore:SR_array_restore: look for element-end: buffer contains '%s'\n", bp);
				}
				/*
				 * We've accumulated all the characters, or all we can handle in string[].
				 * If there are more characters than we can handle, just pretend we read them.
				 */
				/* *bp == ELEMENT_END ,*/
				for (found = 0; (found == 0) && !end_of_file; ) {
					while (*bp && (*bp != ELEMENT_END) && (*bp != ESCAPE)) bp++;
					switch (*bp) {
					case ELEMENT_END:
						found = 1; 
						bp++; 
						break;
					case ESCAPE:
						++bp;
						if (*bp == ELEMENT_END || *bp == ESCAPE) {
							++bp;
						}
						break;
					default:
						if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
							end_of_file = 1;
							found = 1;
						}
					}
				}
				if ((num_read<max_elements) && !gobble) {
					/* Append value to local array. */
					if (p_data) {
						epicsStrnRawFromEscaped(string, MAX_STRING_SIZE, string, MAX_STRING_SIZE);
						switch (field_type) {
						case DBF_STRING:
							strNcpy(&(p_char[(num_read++)*MAX_STRING_SIZE]), string, MAX_STRING_SIZE);
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
							p_long[num_read++] = (epicsInt32) atol(string);
							break;
						case DBF_ULONG:
							/*p_ulong[num_read++] = (epicsUInt32) atol(string);*/
							p_ulong[num_read++] = (epicsUInt32) strtoul(string,NULL,0);
							break;
						#ifdef DBR_INT64
						case DBF_INT64:
							epicsParseInt64(string, &p_int64[num_read++], 10, NULL);
							break;
						case DBF_UINT64:
							epicsParseUInt64(string, &p_uint64[num_read++], 10, NULL);
							break;
						#endif
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
			}
		} /* for (num_read=0; bp && !end_mark_found; ) */

		if ((save_restoreDebug >= 10) && p_data && !gobble) {
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
					errlogPrintf("	%u\n", p_ulong[j]); break;
				case DBF_LONG:
					errlogPrintf("	%d\n", p_long[j]); break;
				#ifdef DBR_INT64
				case DBF_UINT64:
					errlogPrintf("	%llu\n", p_uint64[j]); break;
				case DBF_INT64:
					errlogPrintf("	%lld\n", p_int64[j]); break;
				#endif
				case DBF_FLOAT:
					errlogPrintf("	%f\n", p_float[j]); break;
				case DBF_DOUBLE:
					errlogPrintf("	%g\n", p_double[j]); break;
				case DBF_NOACCESS:
				default:
					break;
				}
			}
			errlogPrintf("save_restore: end of %ld array values.\n\n", num_read);
			epicsThreadSleep(0.5);
		}

	} /* if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) */


	/* leave the file pointer ready for next PV (next fgets() should yield next PV) */
	if (begin_mark_found) {
		/* find ARRAY_END (but ARRAY_END inside an element is just another character) */
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: looking for ARRAY_END\n");
		}
		in_element = 0;
		while (!end_mark_found && !end_of_file) {
			if (save_restoreDebug >= 11) {
				errlogPrintf("dbrestore:SR_array_restore: ...buffer contains '%s'\n", bp);
			}
			switch (*bp) {
			case ESCAPE:
				if (in_element && (bp[1] == ELEMENT_END)) bp++; /* two chars treated as one */
				break;
			case ARRAY_END:
				if (save_restoreDebug >= 10) {
					errlogPrintf("dbrestore:SR_array_restore: found ARRAY_END.  in_element=%d\n", in_element);
				}
				if (!in_element) end_mark_found = 1;
				break;
			case '\0':
				if ((bp = fgets(buffer, BUF_SIZE, inp_fd)) == NULL) {
					errlogPrintf("dbrestore:SR_array_restore: *** EOF during array-end search\n");
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
		if (save_restoreDebug >= 10) {
			errlogPrintf("dbrestore:SR_array_restore: ARRAY_BEGIN wasn't found.\n");
		}
	}
	if (!status && end_of_file) {
		status = end_of_file;
		errlogPrintf("dbrestore:SR_array_restore: status = end_of_file.\n");
	}

	if (gobble) {
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:SR_array_restore: Gobbled unused array data.\n");
		}
	} else if (pass == 0) {
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:SR_array_restore: No array write in pass 0.\n");
		}
	} else {
		if (!status && p_data) {
			if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:SR_array_restore: Writing array to database\n");
			}
			status = SR_put_array_values(PVname, p_data, num_read);
		} else {
			if (save_restoreDebug >= 1) {
				errlogPrintf("dbrestore:SR_array_restore: No array write to database attempted because of error condition\n");
				errlogPrintf("dbrestore:SR_array_restore: status=%ld, p_data=%p\n", status, p_data);
			}
		}
	}
	if ((p_data == NULL) && !gobble) status = -1;
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
    char		PVname[PV_NAME_LEN+1]; /* Must be greater than max field width ("%80s") in the sscanf format below */
	char		bu_filename[PATH_SIZE+1], fname[PATH_SIZE+1] = "";
	char		buffer[BUF_SIZE], *bp;
	char		ebuffer[EBUF_SIZE]; /* make room for macro expansion */
	char		value_string[BUF_SIZE];
	char		datetime[32];
	char		c;
	FILE		*inp_fd;
	int			found_field, pass;
	DBENTRY		dbentry;
	DBENTRY		*pdbentry = &dbentry;
	long		status;
	int			n, write_backup, num_errors, is_scalar;
	long		*pStatusVal = 0;
	char		*statusStr = 0;
	char		realName[64];	/* name without trailing '$' */
	int			is_long_string;
	struct restoreFileListItem *pLI;
	/* macrostring */
	MAC_HANDLE	*handle = NULL;
	char		**pairs = NULL;
	char		*macrostring = NULL;

	if (save_restoreDebug)
		errlogPrintf("reboot_restore: entry for file '%s'\n", filename);
	/* initialize database access routines */
	if (!pdbbase) {
		errlogPrintf("reboot_restore: No Database Loaded\n");
		return(OK);
	}
	dbInitEntry(pdbbase,pdbentry);

	maybeInitRestoreFileLists();
	/* what are we supposed to do here? */
	if (init_state >= initHookAfterInitDatabase) {
		pass = 1;
		pLI = (struct restoreFileListItem *) ellFirst(&pass1List);
	} else {
		pass = 0;
		pLI = (struct restoreFileListItem *) ellFirst(&pass0List);
	}
	while (pLI) {
		if (pLI->filename && (strcmp(filename, pLI->filename) == 0)) {
			pStatusVal = &(pLI->restoreStatus);
			statusStr = pLI->restoreStatusStr;
			macrostring = pLI->macrostring;
			break;
		}
		pLI = (struct restoreFileListItem *) ellNext(&(pLI->node));
	}

	if ((pStatusVal == 0) || (statusStr == 0)) {
		errlogPrintf("reboot_restore: Can't find filename '%s' in list.\n",
			filename);
	} else if (save_restoreDebug) {
		errlogPrintf("reboot_restore: Found filename '%s' in restoreFileList.\n",
			filename);
	}

	/* open file */
	if (isAbsolute(filename)) {
		strNcpy(fname, filename, PATH_SIZE);
	} else {
		makeNfsPath(fname, saveRestoreFilePath, filename);
	}
	if (save_restoreDebug)
		errlogPrintf("*** restoring from '%s' at initHookState %d (%s record/device init) ***\n",
			fname, (int)init_state, pass ? "after" : "before");
	if ((inp_fd = fopen_and_check(fname, &status)) == NULL) {
		errlogPrintf("save_restore: Can't open save file.");
		if (pStatusVal) *pStatusVal = SR_STATUS_FAIL;
		if (statusStr) strNcpy(statusStr, "Can't open save file.", STATUS_STR_LEN-1);
		dbFinishEntry(pdbentry);
		return(ERROR);
	}
	if (status) {
		if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
		if (statusStr) strNcpy(statusStr, "Bad .sav(B) files; used seq. backup", STATUS_STR_LEN-1);
	}

	/* Prepare to use macro substitution */
	if (macrostring && macrostring[0]) {
		macCreateHandle(&handle, NULL);
		if (handle) {
			macParseDefns(handle, macrostring, &pairs);
			if (pairs) macInstallMacros(handle, pairs);
			if (save_restoreDebug >= 5) {
				errlogPrintf("save_restore:reboot_restore: Current macro definitions:\n");
				macReportMacros(handle);
				errlogPrintf("save_restore:reboot_restore: --------------------------\n");
			}
		}
	}

	(void)fgets(buffer, BUF_SIZE, inp_fd); /* discard header line */
	if (save_restoreDebug >= 1) {
		errlogPrintf("dbrestore:reboot_restore: header line '%s'\n", buffer);
	}
	status = fseek(inp_fd, 0, SEEK_SET); /* go to beginning */
	if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);

	/* restore from data file */
	num_errors = 0;
	while ((bp=fgets(buffer, BUF_SIZE, inp_fd))) {
		if (handle && pairs) {
			ebuffer[0] = '\0';
			macExpandString(handle, buffer, ebuffer, EBUF_SIZE);
			bp = ebuffer;
			if (save_restoreDebug >= 5) {
				printf("dbrestore:reboot_restore: buffer='%s'\n", buffer);
				printf("                         ebuffer='%s'\n", ebuffer);
			}
		}

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
		PVname[0] = '\0';
		value_string[0] = '\0';
		n = sscanf(bp,"%80s%c%[^\n\r]", PVname, &c, value_string);
		if (n<3) *value_string = 0;
		if ((n<1) || (PVname[0] == '\0')) {
			if (save_restoreDebug >= 10) {
				errlogPrintf("dbrestore:reboot_restore: line (fragment) '%s' ignored.\n", bp);
			}
			continue;
		}
		if (strncmp(PVname, "<END>", 5) == 0) {
			break;
		}
		if (PVname[0] == '#') {
			/* user must have edited the file manually; accept this line as a comment */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (!is_scalar) {
				/* Parse and gobble up the whole array. */
				status = SR_array_restore(pass, inp_fd, PVname, value_string, 1);
			}
			continue;
		}
		if (strlen(PVname) >= 80) {
			/* must be a munged input line */
			errlogPrintf("dbrestore:reboot_restore: '%s' is too long to be a PV name.\n", PVname);
			continue;
		}
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			if (strchr(PVname,'.') == 0) strcat(PVname,".VAL"); /* if no field name, add default */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (save_restoreDebug > 9) errlogPrintf("\n");
			if (is_scalar) {
				long num_elements, field_size, field_type;
				/* check the field itself, because an empty string is saved as no value at all , which would look like a scalar. */
				SR_get_array_info(PVname, &num_elements, &field_size, &field_type);
				if (num_elements > 1) {
					if (save_restoreDebug >= 5) {
						printf("reboot_restore: PV '%s' is scalar in .sav file, but has %ld elements.  Treating as array.\n",
							PVname, num_elements);
					}
					is_scalar = 0;
				}
			}
			if (save_restoreDebug >= 10) {
				errlogPrintf("dbrestore:reboot_restore: Attempting to put %s '%s' to '%s'\n",
					is_scalar?"scalar":"array", value_string, PVname);
			}

			/* dbStatic doesn't know about long-string fields (PV name with appended '$'). */
			is_long_string = 0;
			strNcpy(realName, PVname, 63);
			if (realName[strlen(realName)-1] == '$') {
				realName[strlen(realName)-1] = '\0';
				is_long_string = 1;
				/* See if we got the whole line */
				if (bp[strlen(bp)-1] != '\n') {
					/* No, we didn't.  One more read will certainly accumulate a value string of length BUF_SIZE */
					if (save_restoreDebug > 9) printf("reboot_restore: did not reach end of line for long-string PV\n");
					bp = fgets(buffer, BUF_SIZE, inp_fd);
					if (handle && pairs) {
						ebuffer[0] = '\0';
						macExpandString(handle, buffer, ebuffer, EBUF_SIZE);
						bp = ebuffer;
						if (save_restoreDebug >= 1) {
							printf("dbrestore:reboot_restore: buffer='%s'\n", buffer);
							printf("                         ebuffer='%s'\n", ebuffer);
						}
					}
					n = BUF_SIZE-strlen(value_string)-1;
					strncat(value_string, bp, n);
					/* we don't want that '\n' in the string */
					if (value_string[strlen(value_string)-1] == '\n') value_string[strlen(value_string)-1] = '\0';
				}
				/* We aren't prepared to handle more than BUF_SIZE characters.  Discard additional characters until end of line */
				while (bp[strlen(bp)-1] != '\n') bp = fgets(buffer, BUF_SIZE, inp_fd);
			}

			found_field = 1;
			if ((status = dbFindRecord(pdbentry, realName)) != 0) {
				errlogPrintf("dbFindRecord for '%s' failed\n", PVname);
				num_errors++; found_field = 0;
			} else if (dbFoundField(pdbentry) == 0) {
				errlogPrintf("dbrestore:reboot_restore: dbFindRecord did not find field '%s'\n", PVname);
				num_errors++; found_field = 0;
			}
			if (found_field) {
				if (is_scalar || is_long_string) {
					status = scalar_restore(pass, pdbentry, PVname, value_string, is_long_string);
				} else {
					status = SR_array_restore(pass, inp_fd, PVname, value_string, 0);
				}
				if (status) {
					errlogPrintf("dbrestore:reboot_restore: restore for PV '%s' failed\n", PVname);
					num_errors++;
				}
			} else {
				if (!is_scalar) {
					/* Parse and gobble up the whole array.  We don't have  PV to restore to,
					 * but we don't want to trip over the unused array data.
					 */
					status = SR_array_restore(pass, inp_fd, PVname, value_string, 1);
				}
			} /* if (found_field) {} else {... */
		} else if (PVname[0] == '!') {
			/*
			* string is an error message -- something like:
			* '! 7 channel(s) not connected - or not all gets were successful'
			*/
			n = (int)atol(&bp[1]);
			errlogPrintf("%d %s had no saved value.\n", n, n==1?"PV":"PVs");
			if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
			if (statusStr) strNcpy(statusStr, ".sav file contained an error message", STATUS_STR_LEN-1);
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("aborting restore\n");
				fclose(inp_fd);
				if (handle) macDeleteHandle(handle);
				if (pairs) free(pairs);
				dbFinishEntry(pdbentry);
				if (pStatusVal) *pStatusVal = SR_STATUS_FAIL;
				if (statusStr) strNcpy(statusStr, "restore aborted", STATUS_STR_LEN-1);
				return(ERROR);
			}
		} else if (PVname[0] == '<') {
			/* end of file */
			break;
		}
	}
	fclose(inp_fd);
	if (handle) macDeleteHandle(handle);
	if (pairs) free(pairs);
	dbFinishEntry(pdbentry);

	/* If this is the second pass for a restore file, don't write backup file again.*/
	write_backup = 1;
	if (init_state >= initHookAfterInitDatabase) {
		pLI = (struct restoreFileListItem *) ellFirst(&pass0List);
		while (pLI) {
			if (strcmp(filename, pLI->filename) == 0) {
				write_backup = 0;
				break;
			}
			pLI = (struct restoreFileListItem *) ellNext(&(pLI->node));
		}
	}

	/* For now, don't write boot-time backups for files specified with full path. */
	if (isAbsolute(filename)) write_backup = 0;

	if (write_backup) {
		/* write  backup file*/
		if (save_restoreDatedBackupFiles && (fGetDateStr(datetime) == 0)) {
			strNcpy(bu_filename, fname, sizeof(bu_filename) - 1 - strlen(datetime));
			strcat(bu_filename, "_");
			strcat(bu_filename, datetime);
		} else {
			strNcpy(bu_filename, fname, sizeof(bu_filename) - 3);
			strcat(bu_filename, ".bu");
		}
		if (save_restoreDebug >= 1) {
			errlogPrintf("dbrestore:reboot_restore: writing boot-backup file '%s'.\n", bu_filename);
		}
		status = (long)myFileCopy(fname,bu_filename);
		if (status) {
			errlogPrintf("save_restore: Can't write backup file.\n");
			if (pStatusVal) *pStatusVal = SR_STATUS_WARN;
			if (statusStr) strNcpy(statusStr, "Can't write backup file", STATUS_STR_LEN-1);
			return(OK);
		}
	}

	/* Record status */
	if (pStatusVal && statusStr) {
		if (*pStatusVal != 0) {
			/* Status and message have already been recorded */
			;
		} else if (num_errors != 0) {
			epicsSnprintf(statusStr, STATUS_STR_LEN-1, "%d %s", num_errors, num_errors==1?"PV error":"PV errors");
			*pStatusVal = SR_STATUS_WARN;
		} else {
			strNcpy(statusStr, "No errors", STATUS_STR_LEN-1);
			*pStatusVal = SR_STATUS_OK;
		}
	}
	if (p_data) {
		free(p_data);
		p_data = NULL;
		p_data_size = 0;
	}
	if (save_restoreDebug)
		errlogPrintf("reboot_restore: done with file '%s'\n\n", filename);
	return(OK);
}


static int set_restoreFile(int pass, char *filename, char *macrostring)
{
	struct restoreFileListItem *pLI;

	maybeInitRestoreFileLists();

	pLI = calloc(1, sizeof(struct restoreFileListItem));
	if (pLI == NULL) {
		errlogPrintf("set_pass%d_restoreFile: calloc failed\n", pass);
		return(ERROR);
	}

	pLI->filename = (char *)calloc(strlen(filename) + 4,sizeof(char));
	if (pLI->filename == NULL) {
		errlogPrintf("set_pass%d_restoreFile: calloc failed\n", pass);
		free(pLI);
		return(ERROR);
	}
	strcpy(pLI->filename, filename);

	pLI->restoreStatusStr = (char *)calloc(STATUS_STR_LEN, 1);
	if (pLI->restoreStatusStr == NULL) {
		errlogPrintf("set_pass%d_restoreFile: calloc failed\n", pass);
		free(pLI->filename);
		free(pLI);
		return(ERROR);
	}
	strNcpy(pLI->restoreStatusStr, "Unknown, probably failed", STATUS_STR_LEN-1);

	if (macrostring && macrostring[0]) {
		pLI->macrostring = (char *)calloc(strlen(macrostring)+1,sizeof(char));
		strcpy(pLI->macrostring, macrostring);
	}


	pLI->restoreStatus = SR_STATUS_INIT;

	if (pass==1) {
		ellAdd(&pass1List, &(pLI->node));
	} else {
		ellAdd(&pass0List, &(pLI->node));
	}
	return(OK);
}

int set_pass0_restoreFile(char *filename, char *macrostring)
{
	return(set_restoreFile(0, filename, macrostring));
}

int set_pass1_restoreFile(char *filename, char *macrostring)
{
	return(set_restoreFile(1, filename, macrostring));
}

/* file is ok if it ends in either of the two following ways:
 * <END>?
 * <END>??
 * where '?' is any character - typically \n or \r
 */
FILE *checkFile(const char *file)
{
	FILE *inp_fd = NULL;
	char tmpstr[PATH_SIZE+50], *versionstr;
	double version;
	char datetime[32];
	int status;

	if (save_restoreDebug >= 2) printf("checkFile: entry\n");

	if ((inp_fd = fopen(file, "r")) == NULL) {
		errlogPrintf("save_restore: Can't open file '%s'.\n", file);
		return(0);
	}

	/* Get the version number of the code that wrote the file */
	fgets(tmpstr, 29, inp_fd);
	versionstr = strchr(tmpstr,(int)'R');
	if (!versionstr) versionstr = strchr(tmpstr,(int)'V');
	if (!versionstr) {
		/* file has no version number */
		status = fseek(inp_fd, 0, SEEK_SET); /* go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);	/* Assume file is ok */
	}
	if (isdigit((int)versionstr[1]))
		version = atof(versionstr+1);
	else
		version = 0;
	if (save_restoreDebug >= 2) printf("checkFile: version=%f\n", version);

	/* <END> check started in v1.8 */
	if (version < 1.8) {
		status = fseek(inp_fd, 0, SEEK_SET); /* go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);	/* Assume file is ok. */
	}
	/* check out "successfully written" marker */
	status = fseek(inp_fd, -6, SEEK_END);
	if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
	fgets(tmpstr, 6, inp_fd);
	if (save_restoreDebug >= 5) printf("checkFile: files ends with '%s'\n", tmpstr);
	if (strncmp(tmpstr, "<END>", 5) == 0) {
		status = fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);
	}
	
	status = fseek(inp_fd, -7, SEEK_END);
	if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
	fgets(tmpstr, 7, inp_fd);
	if (save_restoreDebug >= 5) printf("checkFile: files ends with '%s'\n", tmpstr);
	if (strncmp(tmpstr, "<END>", 5) == 0) {
		status = fseek(inp_fd, 0, SEEK_SET); /* file is ok.  go to beginning */
		if (status) myPrintErrno("checkFile: fseek error ", __FILE__, __LINE__);
		return(inp_fd);
	}

	/* file is bad */
	fclose(inp_fd);
	errlogPrintf("save_restore: File '%s' is not trusted.\n", file);
	strNcpy(tmpstr, file, PATH_SIZE+49);
	strncat(tmpstr, "_RBAD_", PATH_SIZE+49-strlen(tmpstr));
	if (save_restoreDatedBackupFiles) {
		fGetDateStr(datetime);
		strncat(tmpstr, datetime, PATH_SIZE+49-strlen(tmpstr));
	}
	(void)myFileCopy(file, tmpstr);
	return(0);
}


FILE *fopen_and_check(const char *fname, long *status)
{
	FILE *inp_fd = NULL;
	char file[PATH_SIZE+1];
	int i, backup_sequence_num;
	struct stat fileStat;
	char *p;
	time_t currTime;
	double dTime, min_dTime;
	
	*status = 0;	/* presume success */
	strNcpy(file, fname, PATH_SIZE);
	inp_fd = checkFile(file);
	if (save_restoreDebug >=1) printf("fopen_and_check: checkFile returned %p\n", inp_fd);
	if (inp_fd) return(inp_fd);

	/* Still here?  Try the backup file. */
	strncat(file, "B", PATH_SIZE - strlen(file));
	errlogPrintf("save_restore: Trying backup file '%s'\n", file);
	inp_fd = checkFile(file);
	if (inp_fd) return(inp_fd);

	/*** Still haven't found a good file?  Try the sequenced backups ***/
	/* Find the most recent one. */
	*status = 1;
	strNcpy(file, fname, PATH_SIZE);
	backup_sequence_num = -1;
	p = &file[strlen(file)];
	currTime = time(NULL);
	min_dTime = 1.e9;
	for (i=0; i<save_restoreNumSeqFiles; i++) {
		epicsSnprintf(p, PATH_SIZE-strlen(file), "%1d", i);

		if (stat(file, &fileStat) == 0) {
			/*
			 * Clocks might be unsynchronized, so it's possible
			 * the most recent file has a time in the future.
			 * For now, just choose the file whose date/time is
			 * closest to the current date/time.
			 */
			dTime = fabs(difftime(currTime, fileStat.st_mtime));
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

	if (backup_sequence_num == -1) {
		/* Clock are way messed up.  Just try backup 0. */
		backup_sequence_num = 0;
		epicsSnprintf(p, PATH_SIZE-strlen(file), "%1d", backup_sequence_num);
		errlogPrintf("save_restore: Can't figure out which seq file is most recent,\n");
		errlogPrintf("save_restore: so I'm just going to start with '%s'.\n", file);
	}

	/* Try the sequenced backup files. */
	for (i=0; i<save_restoreNumSeqFiles; i++) {
		epicsSnprintf(p, PATH_SIZE-strlen(file), "%1d", backup_sequence_num);
		errlogPrintf("save_restore: Trying backup file '%s'\n", file);
		inp_fd = checkFile(file);
		if (inp_fd) return(inp_fd);

		/* Next.  Order might be, e.g., "1,2,0", if 1 is most recent of 3 files */
		if (++backup_sequence_num >= save_restoreNumSeqFiles)
			backup_sequence_num = 0;
	}

	errlogPrintf("save_restore: Can't find a file to restore from...");
	errlogPrintf("save_restore: ...last tried '%s'. I give up.\n", file);
	printf("save_restore: **********************************\n\n");
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

	*num_elements = 0;
	*field_size = 0;
	*field_type = 0;
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
	if (save_restoreDebug >= 10) {
		errlogPrintf("dbrestore:SR_get_array: '%s' currently has %ld elements\n", PVname, *pnum_elements);
	}
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
	epicsInt32		*p_long = NULL;
	unsigned char	*p_uchar = NULL;
	unsigned short	*p_ushort = NULL;
	epicsUInt32		*p_ulong = NULL;
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
				if ((*pc == ELEMENT_BEGIN) || (*pc == ELEMENT_END) || (*pc == ESCAPE)) {
					n += fprintf(out_fd, "%1c", ESCAPE);
					j++;
				}
				if (*pc == '\n') {
					n += fprintf(out_fd, "%1cn", ESCAPE);
				} else if (*pc == '\r') {
					n += fprintf(out_fd, "%1cr", ESCAPE);
				} else {
					n += fprintf(out_fd, "%1c", *pc);
				}
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
			p_long = (epicsInt32 *)pArray;
			n += fprintf(out_fd, "%1c%d%1c ", ELEMENT_BEGIN, p_long[i], ELEMENT_END);
			break;
		case DBF_ULONG:
			p_ulong = (epicsUInt32 *)pArray;
			n += fprintf(out_fd, "%1c%u%1c ", ELEMENT_BEGIN, p_ulong[i], ELEMENT_END);
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
			errlogPrintf("save_restore: field_type %d not handled.\n", (int) field_type);
			break;
		}
	}
	n += fprintf(out_fd, "%1c\n", ARRAY_END);
	return(n);
}

/* Surely this is enough... */
#define MAX_FIELD_SIZE 30
/*
 * Look through the database for info nodes with the specified info_name, and get the
 * associated info_value string.  Interpret this string as a list of field names.  Write
 * the PV's thus accumulated to the file <fileBaseName>.  (If <fileBaseName> doesn't contain
 * ".req", append it.)
 */
void makeAutosaveFileFromDbInfo(char *fileBaseName, char *info_name)
{
	DBENTRY		dbentry;
	DBENTRY		*pdbentry = &dbentry;
	const char *info_value, *pbegin, *pend;
	char		*fname, *falloc=NULL, field[MAX_FIELD_SIZE], realfield[MAX_FIELD_SIZE];
	FILE 		*out_fd;
	int			searchRecord, flen;

	if (!pdbbase) {
		errlogPrintf("autosave:makeAutosaveFileFromDbInfo: No Database Loaded\n");
		return;
	}
	if (strstr(fileBaseName, ".req")) {
		fname=fileBaseName;
	} else {
		fname=falloc=malloc(strlen(fileBaseName)+sizeof(".req")+1);
		if (!fname) {
			errlogPrintf("save_restore:makeAutosaveFileFromDbInfo - allocation failed\n");
			return;
		}
		epicsSnprintf(fname, strlen(fileBaseName)+sizeof(".req"), "%s.req", fileBaseName);
	}
	if ((out_fd = fopen(fname,"w")) == NULL) {
		errlogPrintf("save_restore:makeAutosaveFileFromDbInfo - unable to open file '%s'\n", fname);
		free(falloc);
		return;
	}
	free(falloc);

	dbInitEntry(pdbbase,pdbentry);
	/* loop over all record types */
	dbFirstRecordType(pdbentry);
	do {
		/* loop over all records of current type*/
		dbFirstRecord(pdbentry);
#if GE_EPICSBASE(3,14,11)
		searchRecord = dbIsAlias(pdbentry) ? 0 : 1;
#else
		searchRecord = 1;
#endif
		do {
			if (searchRecord) {
				info_value = dbGetInfo(pdbentry, info_name);
				if (info_value) {
					/* printf("record %s.autosave = '%s'\n", dbGetRecordName(pdbentry), info_value); */

					for (pbegin=info_value; *pbegin && isspace((int)*pbegin); pbegin++) {} /* skip leading whitespace */

					while (pbegin && *pbegin && !isspace((int)*pbegin)) {
						/* find end of field */
						for (pend=pbegin; *pend && !isspace((int)*pend); pend++) {}
						/* pend points to whitespace or \0 */

						flen = pend-pbegin;
						if (flen >= sizeof(field)-1) flen = sizeof(field)-1;
						memcpy(field, pbegin, flen);
						field[flen]='\0';
						strNcpy(realfield, field, MAX_FIELD_SIZE-1);
						if (realfield[strlen(realfield)-1] == '$') realfield[strlen(realfield)-1] = '\0';

						if (dbFindField(pdbentry, realfield) == 0) {
							fprintf(out_fd, "%s.%s\n", dbGetRecordName(pdbentry), field);
						} else {
							printf("makeAutosaveFileFromDbInfo: %s.%s not found\n", dbGetRecordName(pdbentry), field);
						}

						for (pbegin=pend; *pbegin && isspace((int)*pbegin); pbegin++) {} /* skip leading whitespace */
					}
				}
			}
		} while (dbNextRecord(pdbentry) == 0);
	} while (dbNextRecordType(pdbentry) == 0);
	dbFinishEntry(pdbentry);
	fclose(out_fd);
	return;
}

/**************************************************************************/
/* support for building autosave-request files automatically from dbLoadRecords, dbLoadTemplate */

int eraseFile(const char *filename) {
	FILE *fd;
	char *fname;

	fname = macEnvExpand(filename);
	if (fname == NULL) {
		printf("save_restore:eraseFile: macEnvExpand('%s') returned NULL\n", filename);
		return(ERROR);
	}
	if ((fd = fopen(fname, "w")) != NULL) {
		fclose(fd);
	}
	free(fname);
	return(0);
}

int appendToFile(const char *filename, const char *line) {
	FILE *fd;
	char *fname;
	int status=0;

	fname = macEnvExpand(filename);
	if (fname == NULL) {
		printf("save_restore:appendToFile: macEnvExpand('%s') returned NULL\n", filename);
		return(ERROR);
	}
	if ((fd = fopen(fname, "a")) != NULL) {
		fprintf(fd, "%s\n", line);
		fclose(fd);
	} else {
		errlogPrintf("save_restore:appendToFile: Can't open file '%s'\n", fname);
		status = -1;
	}
	free(fname);
	return(status);
}

#ifdef DBLOADRECORDSHOOKREGISTER
static DB_LOAD_RECORDS_HOOK_ROUTINE previousHook=NULL;
#endif
static ELLLIST buildInfoList = ELLLIST_INIT;

struct buildInfoItem {
	ELLNODE node;
	char *filename;
	char *suffix;
	int enabled;
};

static int autosaveBuildInitialized=0;
static char requestFileCmd[MAXSTRING];
static char requestFileBase[MAXSTRING];
static char requestFileName[MAXSTRING];
static char macroString[MAXSTRING], emacroString[MAXSTRING];

static void myDbLoadRecordsHook(const char* fname, const char* macro) {
	struct buildInfoItem *pitem;
	char *p, *dbFileName;
	int n;
	MAC_HANDLE      *handle = NULL;
	char            **pairs = NULL;

	dbFileName = macEnvExpand(fname);

	if (save_restoreDebug >= 5) {
		printf("myDbLoadRecordsHook: dbFileName='%s'; subs='%s'\n", dbFileName, macroString);
	}

#ifdef DBLOADRECORDSHOOKREGISTER
	if (previousHook) previousHook(dbFileName, macro);
#endif

	/* Should probably call basename(), but is it available on Windows? */
	p = strrchr(dbFileName, (int)'/');
	if (p==NULL) p = strrchr(dbFileName, (int)'\\');
	if (p) {
		strNcpy(requestFileBase, p+1, MAXSTRING-strlen(requestFileBase)-1);
	} else {
		strNcpy(requestFileBase, dbFileName, MAXSTRING-strlen(requestFileBase)-1);
	}
	p = strstr(requestFileBase, ".db");
	if (p == NULL) p = strstr(requestFileBase, ".vdb");
	if (p == NULL) p = strstr(requestFileBase, ".template");
	if (p == NULL) {
		printf("myDbLoadRecordsHook: Can't make request-file name from '%s'\n", dbFileName);
		free(dbFileName);
		return;
	}
	*p = '\0';

	pitem = (struct buildInfoItem *)ellFirst(&buildInfoList);
	for (; pitem; pitem = (struct buildInfoItem *)ellNext(&(pitem->node)) ) {
		if (pitem->enabled) {
			n = epicsSnprintf(requestFileName, MAXSTRING, "%s%s", requestFileBase, pitem->suffix);
			if ((n < MAXSTRING) && (openReqFile(requestFileName, NULL))) {
				if (save_restoreDebug >= 5) {
					printf("myDbLoadRecordsHook: found '%s'\n", requestFileName);
				}
				/* Expand any internal macros in macroString e.g., "N=1,M=m$(N)" */
				macCreateHandle(&handle, NULL);
				macSuppressWarning(handle, 1);
				strNcpy(macroString, macro, MAXSTRING-1);
				if (handle) {
					macParseDefns(handle, macroString, &pairs);
					if (pairs) {
						macInstallMacros(handle, pairs);
						emacroString[0] = '\0';
						macExpandString(handle, macroString, emacroString, MAXSTRING-1);
						strNcpy(macroString, emacroString, MAXSTRING-1);
					}
				}
				n = epicsSnprintf(requestFileCmd, MAXSTRING, "file %s %s", requestFileName, macroString);
				if (n < MAXSTRING) {
					appendToFile(pitem->filename, requestFileCmd);
				} else {
					printf("myDbLoadRecordsHook: Can't include %s; requestFileCmd is too long (n = %i, MAXSTRING = %i)\n", 
					requestFileName, n, MAXSTRING);
				}
			}
		}
	}
	free(dbFileName);
}

int autosaveBuild(char *filename, char *reqFileSuffix, int on) {

	struct buildInfoItem *pitem;
	int fileFound = 0, itemFound = 0;

	if (!autosaveBuildInitialized) {
		autosaveBuildInitialized = 1;
#ifdef DBLOADRECORDSHOOKREGISTER
        previousHook = dbLoadRecordsHook;
        dbLoadRecordsHook = myDbLoadRecordsHook;
#else
		printf("pretending to register a dbLoadRecords hook\n");
#endif
	}
	if (!filename || filename[0]==0) {
		printf("autosaveBuild: bad filename\n");
		return(-1);
	}

	pitem = (struct buildInfoItem *)ellFirst(&buildInfoList);
	for (; pitem; pitem = (struct buildInfoItem *)ellNext(&(pitem->node)) ) {
		if ((pitem->filename && strcmp(pitem->filename, filename)==0)) {
			fileFound = 1;
			if ((pitem->suffix && (reqFileSuffix==NULL || reqFileSuffix[0]=='*' ||
					strcmp(pitem->suffix, reqFileSuffix)==0))) {
				/* item exists */
				if (save_restoreDebug) {
					printf("autosaveBuild: %s filename '%s' and suffix '%s'.\n",
					on ? "enabled" : "disabled", filename, pitem->suffix);
				}
				pitem->enabled = on;
				itemFound = 1;
			}
		}
		if (itemFound) return(0);
	}

	if (!reqFileSuffix || reqFileSuffix[0]==0) {
		printf("autosaveBuild: bad suffix\n");
		return(-1);
	}

	/* If this is the first mention of filename, erase the file */
	if (!fileFound) eraseFile(filename);
	pitem = (struct buildInfoItem *)calloc(1, sizeof(struct buildInfoItem));
	ellAdd(&buildInfoList, &(pitem->node));
	pitem->filename = epicsStrDup(filename);
	pitem->suffix = epicsStrDup(reqFileSuffix);
	pitem->enabled = on;
	if (save_restoreDebug) {
	 	printf("autosaveBuild: initialized and %s filename '%s' and suffix '%s'.\n",
		pitem->enabled ? "enabled" : "disabled", pitem->filename, pitem->suffix);
	}
	return(0);
}

/**************************************************************************/

void makeAutosaveFiles() {
    makeAutosaveFileFromDbInfo("info_settings.req", "autosaveFields");
    makeAutosaveFileFromDbInfo("info_positions.req", "autosaveFields_pass0");
}

/* set_pass0_restoreFile() */
STATIC const iocshArg set_passN_Arg1 = {"file",iocshArgString};
STATIC const iocshArg set_passN_Arg2 = {"macrostring",iocshArgString};
STATIC const iocshArg * const set_passN_Args[2] = {&set_passN_Arg1, &set_passN_Arg2};
STATIC const iocshFuncDef set_pass0_FuncDef = {"set_pass0_restoreFile",2,set_passN_Args};
STATIC void set_pass0_CallFunc(const iocshArgBuf *args)
{
    set_pass0_restoreFile(args[0].sval, args[1].sval);
}

/* set_pass1_restoreFile() */
STATIC const iocshFuncDef set_pass1_FuncDef = {"set_pass1_restoreFile",2,set_passN_Args};
STATIC void set_pass1_CallFunc(const iocshArgBuf *args)
{
    set_pass1_restoreFile(args[0].sval, args[1].sval);
}

/* void dbrestoreShow(void) */
STATIC const iocshFuncDef dbrestoreShow_FuncDef = {"dbrestoreShow",0,NULL};
STATIC void dbrestoreShow_CallFunc(const iocshArgBuf *args)
{
    dbrestoreShow();
}

/* void makeAutosaveFileFromDbInfo(char *filename, char *info_name) */
STATIC const iocshArg makeAutosaveFileFromDbInfo_Arg0 = {"filename",iocshArgString};
STATIC const iocshArg makeAutosaveFileFromDbInfo_Arg1 = {"info_name",iocshArgString};
STATIC const iocshArg * const makeAutosaveFileFromDbInfo_Args[2] = {&makeAutosaveFileFromDbInfo_Arg0, &makeAutosaveFileFromDbInfo_Arg1};
STATIC const iocshFuncDef makeAutosaveFileFromDbInfo_FuncDef = {"makeAutosaveFileFromDbInfo",2,makeAutosaveFileFromDbInfo_Args};
STATIC void makeAutosaveFileFromDbInfo_CallFunc(const iocshArgBuf *args)
{
    makeAutosaveFileFromDbInfo(args[0].sval, args[1].sval);
}

/* void makeAutosaveFiles(void) */
STATIC const iocshFuncDef makeAutosaveFiles_FuncDef = {"makeAutosaveFiles",0,NULL};
STATIC void makeAutosaveFiles_CallFunc(const iocshArgBuf *args)
{
    makeAutosaveFiles();
}

/* int eraseFile(char *filename) */
STATIC const iocshArg eraseFile_Arg0 = {"filename",iocshArgString};
STATIC const iocshArg * const eraseFile_Args[1] = {&eraseFile_Arg0};
STATIC const iocshFuncDef eraseFile_FuncDef = {"eraseFile",1,eraseFile_Args};
STATIC void eraseFile_CallFunc(const iocshArgBuf *args)
{
    eraseFile(args[0].sval);
}

/* int appendToFile(char *filename, char *line) */
STATIC const iocshArg appendToFile_Arg0 = {"filename",iocshArgString};
STATIC const iocshArg appendToFile_Arg1 = {"line",iocshArgString};
STATIC const iocshArg * const appendToFile_Args[2] = {&appendToFile_Arg0, &appendToFile_Arg1};
STATIC const iocshFuncDef appendToFile_FuncDef = {"appendToFile",2,appendToFile_Args};
STATIC void appendToFile_CallFunc(const iocshArgBuf *args)
{
    appendToFile(args[0].sval, args[1].sval);
}

/* int autosaveBuild(char *filename, char *reqFileSuffix, int on) */
STATIC const iocshArg autosaveBuild_Arg0 = {"filename",iocshArgString};
STATIC const iocshArg autosaveBuild_Arg1 = {"reqFileSuffix",iocshArgString};
STATIC const iocshArg autosaveBuild_Arg2 = {"on",iocshArgInt};
STATIC const iocshArg * const autosaveBuild_Args[3] = {&autosaveBuild_Arg0, &autosaveBuild_Arg1, &autosaveBuild_Arg2};
STATIC const iocshFuncDef autosaveBuild_FuncDef = {"autosaveBuild",3,autosaveBuild_Args};
STATIC void autosaveBuild_CallFunc(const iocshArgBuf *args)
{
    autosaveBuild(args[0].sval, args[1].sval, args[2].ival);
}


void dbrestoreRegister(void)
{
    iocshRegister(&set_pass0_FuncDef, set_pass0_CallFunc);
    iocshRegister(&set_pass1_FuncDef, set_pass1_CallFunc);
	iocshRegister(&dbrestoreShow_FuncDef, dbrestoreShow_CallFunc);
	iocshRegister(&makeAutosaveFileFromDbInfo_FuncDef, makeAutosaveFileFromDbInfo_CallFunc);
	iocshRegister(&makeAutosaveFiles_FuncDef, makeAutosaveFiles_CallFunc);
	iocshRegister(&eraseFile_FuncDef, eraseFile_CallFunc);
	iocshRegister(&appendToFile_FuncDef, appendToFile_CallFunc);
	iocshRegister(&autosaveBuild_FuncDef, autosaveBuild_CallFunc);
}

epicsExportRegistrar(dbrestoreRegister);
