/* 12/12/96  tmm  v1.3 get doubles and doubles native (independent of
 *                the EPICS "precision" setting, as is BURT) and convert
 *                them using #define FLOAT_FMT, DOUBLE_FMT, below.
 * 01/03/97  tmm  v1.4 make backup save file if save succeeds
 * 06/03/97  tmm  v1.5 write doubles and doubles in g format. Fixed fdbrestore.
 * 04/26/99  tmm  v1.6 set first character of save file to '&' until we've
 *                written and flushed everything, then set it to '#'.
 * 05/11/99  tmm  v1.7 make sure backup save file is ok before writing new
 *                save file.
 * 11/23/99  tmm  v1.8 defend against condition in which ioc does not have
 *                write access to directory, but does have write access to
 *                existing autosave file.  In this case, we can't erase the
 *                file or change its length.
 * 01/05/01  tmm  v1.9 proper treatment of event_handler_args in function
 *                argument list.
 * 05/11/01  tmm  v2.0 defend against null trigger channel.
 * 12/20/01  tmm  v2.1 Test file open only if save_restore_test_fopen != 0
 * 02/22/02  tmm  v2.2 Work around for problems with USHORT fields 
 * 02/26/02  tmm  v2.3 Adding features from Frank Lenkszus' version of the code:
 *                replaced logMsg with epicsPrintf  [that was a mistake!!]
 *                added set_savefile_path(char *path)
 *			      added set_requestfile_path(char *path)
 *			      added set_saveTask_priority(int priority)
 *			      added remove_data_set(char *filename)
 *                added reload_periodic_set(char * filename, double period)
 *                added reload_triggered_set(char *filename, char *trig_channel)
 *                added reload_monitor_set(char * filename, double period)
 *                added reload_manual_set(char * filename)
 *                check for errors on backup file creation
 *                check for errors on writing save file
 *                don't overwrite backup file if previous save failed
 *                added fdbrestoreX(char *filename) to restore from backup file
 *
 *                in set_XXX_path(), allow path with or without trailing '/'
 * 03/08/02  tmm  v2.4 Use William Lupton's macLib to simplify request files.
 *                Command-line arguments redefined from double to int. (Required
 *                for PPC.)
 * 03/13/02  tmm  v2.5 Allow for multiple directories in reqFilePath.
 * 03/15/02  tmm  v2.6 check saveRestoreFilePath before using it.
 * 04/22/03  tmm  v2.7 Add add_XXXfile_path(): like set_XXXfile_path(),
 *                but allows caller to pass path as two args to be concatenated.
 * 04/23/03  tmm  v2.8 Add macro-string argument to create_xxx_set(), added
 *                argument 'verbose' to save_restoreShow();
 * 04/28/03  tmm  v2.9 Drop add_XXXfile_path, and add second argument to
 *                set_XXXfile_path();
 * 05/20/03  tmm  v2.9a Looking for problems that cause save_restore() to hang
 * 07/14/03  tmm  v3.0 In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.  No longer
 *                test fopen() before save.  This caused more problems than it
 *                solved.
 * 07/21/03  tmm  v3.1 More checks during file writing.
 * 08/13/03  tmm  v3.2 Status PV's.  Merge bug fixes from 3.13 and 3.14 versions into
 *                something that will work under 3.13  
 * 08/19/03  tmm  v3.3 Manage NFS mount.  Replace epicsPrintf with errlogPrintf.
 *                Replace logMsg with errlogPrintf except in callback routines.
 * 09/04/03  tmm  v3.4 Do manual restore operations (fdbrestore*) in the save_restore
 *                task.  With this change, all CA work is done by the save_restore task.
 * 04/12/04  tmm  v3.5 Added array support. fdbrestore* was reimplmented as a macro,
 *                but this meant that user could not invoke from command line.
 * 04/20/04  tmm  v4.0 3.13-compatible v3.5, with array support, converted to
 *                EPICS 3.14, though NFS stuff is #ifdef vxWorks
 *                In addition to .sav and .savB, can save/restore <= 10
 *                sequenced files .sav0 -.sav9, which are written at preset
 *                intervals independent of the channel-list settings.
 *                Status PV's have been reworked so database and medm file
 *                don't have to contain the names of save sets.
 * 04/23/04  tmm  List linking; was failing when readReqFile was called recursively.
 * 05/14/04  tmm  New status PV's.  The old ones had the save-set name as part of
 *                the record names.  Now, PV's are generic, and the save-set name
 *                is written to them.
 * 06/28/04  tmm  Check that chid is non-NULL before calling ca_state().
 * 10/04/04  tmm  Allow DOS line termination (CRLF) or unix (LF) in .sav file.
 * 10/29/04  tmm  v4.1 Init all strings to "".  Check that sr_mutex exists in
 *                save_restoreShow().
 * 11/03/04  tmm  v4.2 Changed the way time strings are processed.  Added some code
 *                to defend against partial init failures.
 * 11/30/04  tmm  v4.3 Arrays now maintain max and curr number of elements, and
 *                save_restore tracks changes in curr_elements.  Previously,
 *                if dbGet() set num_elements to zero for a PV, that was the last
 *                time save_restore looked at the PV.
 * 12/13/04  tmm  v4.4 Use epicsThreadGetStackSize(epicsThreadStackMedium)
 *                instead of hard-coded stack size.
 * 01/28/05  tmm  v4.5 Increased size of status-prefix to 29 bytes+NULL.  Added
 *                new status value: 'No Status'
 * 02/16/05  tmm  v4.6 Fixed faulty building of PV-name strings for status PV's
 *                Post selected save-set status strings promptly.
 * 06/27/05  tmm  v4.7 Request-file parsing fixed.  Previously, blank but not
 *                empty lines (e.g., containing a space character) seemed to
 *                contain the first word of the previous line, because name[]
 *                was not cleared before parsing a new line.
 * 02/02/06  tmm  v4.8 Don't quit trying to write file just because errno got set.
 *                Status PV's were being restricted to STRING_LEN chars, instead
 *                of PV_NAME_LEN chars.  (Thanks to Kay Kasemir for diagnosing and
 *                fixing these problems.)
 */
#define		SRVERSION "save/restore V4.8"

#ifdef vxWorks
#include	<vxWorks.h>
#include	<stdioLib.h>
#include	<nfsDrv.h>
#include	<ioLib.h>
extern int logMsg(char *fmt, ...);
#else
#define OK 0
#define ERROR -1
#define logMsg errlogPrintf
#endif

#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#ifndef _WIN32
#include	<unistd.h>
#endif
#include	<string.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<time.h>

#include	<dbDefs.h>
#include	<cadef.h>		/* includes dbAddr.h */
#include	<epicsPrint.h>
#include	<epicsThread.h>
#include	<epicsExport.h>
#include	<iocsh.h>
#include 	<tsDefs.h>
#include    <macLib.h>
#include	<callback.h>
#include	<epicsMutex.h>
#include	<epicsEvent.h>
#include	<epicsTime.h>
#include	"save_restore.h"
#include 	"fGetDateStr.h"

#define TIME2WAIT 20		/* time to wait for semaphores sem_remove and sem_do_manual_op */
#define BACKWARDS_LIST 0	/* old list order was backwards */

/*** data structure definitions  ***/

/* save_methods - used to determine when a file should be deleted */
#define PERIODIC	0x01		/* set when timer goes off */
#define TRIGGERED	0x02		/* set when trigger channel goes off */
#define	TIMER		0x04
#define	CHANGE		0x08
#define MONITORED	(TIMER|CHANGE)	/* set when timer expires and channel changes */
#define MANUAL		0x10		/* set on request */
#define	SINGLE_EVENTS	(PERIODIC|TRIGGERED|MANUAL)

#define TIMEFMT "%a %b %d %I:%M:%S %Y\n"	/* e.g. 'Fri Sep 13 00:00:00 1986\n'	*/
#define TIMEFMT_noY "%a %b %d %I:%M:%S"		/* e.g. 'Fri Sep 13 00:00:00'			*/

struct chlist {								/* save set list element */
	struct chlist	*pnext;					/* next list */
	struct channel	*pchan_list;			/* channel list head */
	struct channel	*plast_chan;		/* channel list tail */
	char			reqFile[FN_LEN];		/* request file name */
	char			saveFile[PATH_SIZE+1];	/* full save file name */	
	char 			last_save_file[FN_LEN];	/* file name last used for save */
	char			save_file[FN_LEN];		/* file name to use on next save */
	int				save_method;			/* bit for each save method requested */
	int				enabled_method;			/* bit for each save method enabled */
	short			save_ok;				/* !=0 -> last save ok */
	int				save_state;				/* state of this save set 	*/
	int				period;					/* time (s) between saves (max for on change) */
	int				monitor_period;			/* time (s) between saves (max for on change) */
	char			trigger_channel[PV_NAME_LEN];	/* db channel to trigger save  */
	CALLBACK		periodicCb;
	CALLBACK		monitorCb;
	int				not_connected;			/* # bad channels not saved/connected */
	int				backup_sequence_num;	/* appended to backup files */
	epicsTimeStamp	backup_time;
	epicsTimeStamp	save_time;
	/* time_t			backup_time; */
	/* time_t			save_time; */
	int				listNumber;				/* future: identify this list's status reporting variables */
	char			name_PV[PV_NAME_LEN];	/* future: write save-set name to generic PV */
	chid			name_chid;
	long			status;
	char			status_PV[PV_NAME_LEN];
	chid			status_chid;
	char			save_state_PV[PV_NAME_LEN];
	chid			save_state_chid;
	char			statusStr[STRING_LEN];
	char			statusStr_PV[PV_NAME_LEN];
	chid			statusStr_chid;
	char			timeStr[STRING_LEN];
	char			time_PV[PV_NAME_LEN];
	chid			time_chid;
};

struct channel {					/* database channel list element */
	struct channel	*pnext;			/* next channel */
	char			name[64];		/* channel name */
	chid			chid;			/* channel access id */
	char			value[64];		/* value string */
	short			enum_val;		/* short value of an enumerated field */
	short			valid;			/* we think we got valid data for this channel */
	long			max_elements;	/* number of elements, initially from ca, but then from dbAddr */
	long			curr_elements;	/* number of elements from dbGet */
	long			field_type;		/* field type from dbAddr */
	void			*pArray;
};

struct pathListElement {
	struct pathListElement *pnext;
	char path[PATH_SIZE+1];
};

/*** module global variables ***/

volatile int save_restoreDebug = 0;
epicsExportAddress(int, save_restoreDebug);

STATIC struct chlist *lptr = NULL;				/* save-set listhead */
STATIC int listNumber = 0;						/* future: list number, to associate lists with status PV's */
STATIC epicsMutexId	sr_mutex = NULL;			/* mut(ual) ex(clusion) for list of save sets */
STATIC epicsEventId	sem_remove = NULL;			/* delete list semaphore */
STATIC epicsEventId	sem_do_manual_op = NULL;	/* semaphore signalling completion of a manual operation */

STATIC short	save_restore_init = 0;
STATIC char 	*SRversion = SRVERSION;
STATIC struct pathListElement
				*reqFilePathList = NULL;
char			saveRestoreFilePath[PATH_SIZE] = "";	/* path to save files, also used by dbrestore.c */
STATIC unsigned int
				taskPriority = 20; /* epicsThreadPriorityCAServerLow -- initial task priority */
				
STATIC epicsThreadId
				taskID = 0;					/* save_restore task ID */
STATIC char		remove_filename[FN_LEN] = "";	/* name of list to delete */
STATIC int		remove_dset = 0;			/* instructs save_restore task to remove list named by remove_filename */ 
STATIC int		remove_status = 0;			/* status of remove operation */

#define FROM_SAVE_FILE 1
#define FROM_ASCII_FILE 2
STATIC char		manual_restore_filename[FN_LEN] = "";	/* name of file to restore from */
/* tells save_restore a manual restore is requested, and identifies the file type */
STATIC int		manual_restore_type = 0;
STATIC int		manual_restore_status = 0;		/* result of manual_restore operation */

/*** stuff for reporting status to EPICS client ***/
STATIC char	status_prefix[30] = "";

STATIC long	SR_status = SR_STATUS_INIT, SR_heartbeat = 0;
/* Make SR_recentlyStr huge because sprintf may overrun (vxWorks has no snprintf) */
STATIC char	SR_statusStr[STRING_LEN] = "", SR_recentlyStr[300] = "";
STATIC char	SR_status_PV[PV_NAME_LEN] = "", SR_heartbeat_PV[PV_NAME_LEN] = ""; 
STATIC char	SR_statusStr_PV[PV_NAME_LEN] = "", SR_recentlyStr_PV[PV_NAME_LEN] = "";
STATIC chid	SR_status_chid, SR_heartbeat_chid,
			SR_statusStr_chid, SR_recentlyStr_chid;

STATIC long	SR_rebootStatus;
STATIC char	SR_rebootStatusStr[STRING_LEN] = "";
STATIC char	SR_rebootStatus_PV[PV_NAME_LEN] = "", SR_rebootStatusStr_PV[PV_NAME_LEN] = "";
STATIC chid	SR_rebootStatus_chid, SR_rebootStatusStr_chid;
STATIC char	SR_rebootTime_PV[PV_NAME_LEN] = "";
STATIC char	SR_rebootTimeStr[STRING_LEN] = "";
STATIC chid	SR_rebootTime_chid;

volatile int	save_restoreNumSeqFiles = 3;			/* number of sequence files to maintain */
volatile int	save_restoreSeqPeriodInSeconds = 60;	/* period between sequence-file writes */
volatile int	save_restoreIncompleteSetsOk = 1;		/* will save/restore incomplete sets? */
volatile int	save_restoreDatedBackupFiles = 1;		/* save backups as <filename>.bu or <filename>_YYMMDD-HHMMSS */

epicsExportAddress(int, save_restoreNumSeqFiles);
epicsExportAddress(int, save_restoreSeqPeriodInSeconds);
epicsExportAddress(int, save_restoreIncompleteSetsOk);
epicsExportAddress(int, save_restoreDatedBackupFiles);

/* variables for managing NFS mount */
char save_restoreNFSHostName[STRING_LEN] = "";
char save_restoreNFSHostAddr[STRING_LEN] = "";
STATIC int save_restoreNFSOK=0;
STATIC int save_restoreIoErrors=0;
volatile int save_restoreRemountThreshold=10;
epicsExportAddress(int, save_restoreRemountThreshold);

/* configuration parameters */
STATIC int	min_period	= 4;	/* save no more frequently than every 4 seconds */
STATIC int	min_delay	= 1;	/* check need to save every 1 second */
				/* worst case wait can be min_period + min_delay */

/*** private functions ***/
STATIC int mountFileSystem(void);
STATIC void dismountFileSystem(void);
STATIC void periodic_save(CALLBACK *pcallback);
STATIC void triggered_save(struct event_handler_args);
STATIC void on_change_timer(CALLBACK *pcallback);
STATIC void on_change_save(struct event_handler_args);
STATIC int save_restore(void);
STATIC int connect_list(struct chlist *plist);
STATIC int enable_list(struct chlist *plist);
STATIC int get_channel_values(struct chlist *plist);
STATIC int write_it(char *filename, struct chlist *plist);
STATIC int write_save_file(struct chlist *plist);
STATIC void do_seq(struct chlist *plist);
STATIC int create_data_set(char *filename, int save_method, int period,
		char *trigger_channel, int mon_period, char *macrostring);
STATIC int do_manual_restore(char *filename, int file_type);
STATIC int readReqFile(const char *file, struct chlist *plist, char *macrostring);

/*** user-callable functions ***/
int fdbrestore(char *filename);
int fdbrestoreX(char *filename);
int manual_save(char *request_file);
int set_savefile_name(char *filename, char *save_filename);
int create_periodic_set(char *filename, int period, char *macrostring);
int create_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int create_monitor_set(char *filename, int period, char *macrostring);
int create_manual_set(char *filename, char *macrostring);
void save_restoreShow(int verbose);
int set_requestfile_path(char *path, char *pathsub);
int set_savefile_path(char *path, char *pathsub);
int set_saveTask_priority(int priority);
int remove_data_set(char *filename);
int reload_periodic_set(char *filename, int period, char *macrostring);
int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring);
int reload_monitor_set(char * filename, int period, char *macrostring);
int reload_manual_set(char * filename, char *macrostring);
int request_manual_restore(char *filename, int file_type);

/* functions to set save_restore parameters */
void save_restoreSet_Debug(int level) {save_restoreDebug = level;}
void save_restoreSet_NumSeqFiles(int numSeqFiles) {save_restoreNumSeqFiles = numSeqFiles;}
void save_restoreSet_SeqPeriodInSeconds(int period) {save_restoreSeqPeriodInSeconds = MAX(10, period);}
void save_restoreSet_IncompleteSetsOk(int ok) {save_restoreIncompleteSetsOk = ok;}
void save_restoreSet_DatedBackupFiles(int ok) {save_restoreDatedBackupFiles = ok;}
void save_restoreSet_status_prefix(char *prefix) {strncpy(status_prefix, prefix, 29);}

/********************************* code *********************************/

/*** callbacks ***/

/* method PERIODIC - timer has elapsed */
STATIC void periodic_save(CALLBACK *pcallback)
{
	void *userArg;
	struct chlist *plist;

    callbackGetUser(userArg, pcallback);
	plist = (struct chlist *)userArg;
	plist->save_state |= PERIODIC;
}


/* method TRIGGERED - ca_monitor received for trigger PV */
STATIC void triggered_save(struct event_handler_args event)
{
    struct chlist *plist = (struct chlist *) event.usr;

    if (event.dbr)
	plist->save_state |= TRIGGERED;
}


/* method MONITORED - timer has elapsed */
STATIC void on_change_timer(CALLBACK *pcallback)
{
	void *userArg;
	struct chlist *plist;

    callbackGetUser(userArg, pcallback);
	plist = (struct chlist *)userArg;

	if (save_restoreDebug >= 10) logMsg("on_change_timer for %s (period is %d seconds)\n",
			plist->reqFile, plist->monitor_period);
	plist->save_state |= TIMER;
}


/* method MONITORED - ca_monitor received for a PV */
STATIC void on_change_save(struct event_handler_args event)
{
    struct chlist *plist;
	if (save_restoreDebug >= 10) {
		logMsg("on_change_save: event.usr=0x%lx\n", (unsigned long)event.usr);
	}
    plist = (struct chlist *) event.usr;

    plist->save_state |= CHANGE;
}



/* manual_save - user-callable routine to cause a manual set to be saved */
int manual_save(char *request_file)
{
	struct chlist	*plist;

	epicsMutexLock(sr_mutex);
	plist = lptr;
	while ((plist != 0) && strcmp(plist->reqFile, request_file)) {
		plist = plist->pnext;
	}
	if (plist != 0)
		plist->save_state |= MANUAL;
	else
		errlogPrintf("saveset %s not found", request_file);
	epicsMutexUnlock(sr_mutex);
	return(OK);
}

/*** functions to manage NFS mount ***/

void save_restoreSet_NFSHost(char *hostname, char *address) {
	if (save_restoreNFSOK) dismountFileSystem();
	strncpy(save_restoreNFSHostName, hostname, (STRING_LEN-1));
	strncpy(save_restoreNFSHostAddr, address, (STRING_LEN-1));
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && saveRestoreFilePath[0]) 
		mountFileSystem();
}

STATIC int mountFileSystem()
{
	errlogPrintf("save_restore:mountFileSystem:entry\n");
	strncpy(SR_recentlyStr, "nfsMount failed", (STRING_LEN-1));
	
#ifdef vxWorks
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && saveRestoreFilePath[0]) {
		if (hostGetByName(save_restoreNFSHostName) == ERROR) {
			(void)hostAdd(save_restoreNFSHostName, save_restoreNFSHostAddr);
		}
		if (nfsMount(save_restoreNFSHostName, saveRestoreFilePath, saveRestoreFilePath)==OK) {
			errlogPrintf("save_restore:mountFileSystem:successfully mounted '%s'\n", saveRestoreFilePath);
			save_restoreNFSOK = 1;
			save_restoreIoErrors = 0;
			strncpy(SR_recentlyStr, "nfsMount succeeded", (STRING_LEN-1));
			return(1);
		} else {
			errlogPrintf("save_restore: Can't nfsMount '%s'\n", saveRestoreFilePath);
		}
	}
#else
	errlogPrintf("save_restore:mountFileSystem: not implemented for this OS.\n");
#endif
	return(0);
}

STATIC void dismountFileSystem()
{
#ifdef vxWorks
	if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && saveRestoreFilePath[0]) {
		nfsUnmount(saveRestoreFilePath);
		errlogPrintf("save_restore:dismountFileSystem:dismounted '%s'\n", saveRestoreFilePath);
		save_restoreNFSOK = 0;
		strncpy(SR_recentlyStr, "nfsUnmount", (STRING_LEN-1));
	}
#else
	errlogPrintf("save_restore:dismountFileSystem: not implemented for this OS.\n");
#endif
}

/*** save_restore task ***/

/*
 * Init, then loop forever to service save sets, and user requests.
 * This task does all of the channel-access stuff.
 */
STATIC int save_restore(void)
{
	struct chlist *plist;
	char *cp, nameString[FN_LEN];
	int i, do_seq_check, just_remounted, n;
	long status;
	epicsTimeStamp currTime, last_seq_check, remount_check_time;

	if (save_restoreDebug)
			errlogPrintf("save_restore:save_restore: entry; status_prefix='%s'\n", status_prefix);

	epicsTimeGetCurrent(&currTime);
	last_seq_check = remount_check_time = currTime; /* struct copy */

	ca_context_create(ca_enable_preemptive_callback);

#ifdef vxWorks
	if (save_restoreNFSOK == 0) mountFileSystem();
#endif

	/* Build names for save_restore general status PV's with status_prefix */
	if (*status_prefix && (*SR_status_PV == '\0')) {
		strcpy(SR_status_PV, status_prefix);
		strcat(SR_status_PV, "SR_status");
		strcpy(SR_heartbeat_PV, status_prefix);
		strcat(SR_heartbeat_PV, "SR_heartbeat");
		strcpy(SR_statusStr_PV, status_prefix);
		strcat(SR_statusStr_PV, "SR_statusStr");
		strcpy(SR_recentlyStr_PV, status_prefix);
		strcat(SR_recentlyStr_PV, "SR_recentlyStr");
		TATTLE(ca_search(SR_status_PV, &SR_status_chid), "save_restore: ca_search(%s) returned %s", SR_status_PV);
		TATTLE(ca_search(SR_heartbeat_PV, &SR_heartbeat_chid), "save_restore: ca_search(%s) returned %s", SR_heartbeat_PV);
		TATTLE(ca_search(SR_statusStr_PV, &SR_statusStr_chid), "save_restore: ca_search(%s) returned %s", SR_statusStr_PV);
		TATTLE(ca_search(SR_recentlyStr_PV, &SR_recentlyStr_chid), "save_restore: ca_search(%s) returned %s", SR_recentlyStr_PV);

		strcpy(SR_rebootStatus_PV, status_prefix);
		strcat(SR_rebootStatus_PV, "SR_rebootStatus");
		strcpy(SR_rebootStatusStr_PV, status_prefix);
		strcat(SR_rebootStatusStr_PV, "SR_rebootStatusStr");
		strcpy(SR_rebootTime_PV, status_prefix);
		strcat(SR_rebootTime_PV, "SR_rebootTime");
		TATTLE(ca_search(SR_rebootStatus_PV, &SR_rebootStatus_chid), "save_restore: ca_search(%s) returned %s", SR_rebootStatus_PV);
		TATTLE(ca_search(SR_rebootStatusStr_PV, &SR_rebootStatusStr_chid), "save_restore: ca_search(%s) returned %s", SR_rebootStatusStr_PV);
		TATTLE(ca_search(SR_rebootTime_PV, &SR_rebootTime_chid), "save_restore: ca_search(%s) returned %s", SR_rebootTime_PV);
		if (ca_pend_io(0.5)!=ECA_NORMAL) {
			errlogPrintf("save_restore: Can't connect to all status PV(s)\n");
		}
		/* Show reboot status */
		SR_rebootStatus = SR_STATUS_OK;
		strcpy(SR_rebootStatusStr, "Ok");
		for (i = 0; i < restoreFileList.pass0cnt; i++) {
			if (restoreFileList.pass0Status[i] < SR_rebootStatus) {
				SR_rebootStatus = restoreFileList.pass0Status[i];
				strncpy(SR_rebootStatusStr, restoreFileList.pass0StatusStr[i], (STRING_LEN-1));
			}
		}
		for (i = 0; i < restoreFileList.pass1cnt; i++) {
			if (restoreFileList.pass1Status[i] < SR_rebootStatus) {
				SR_rebootStatus = restoreFileList.pass1Status[i];
				strncpy(SR_rebootStatusStr, restoreFileList.pass1StatusStr[i], (STRING_LEN-1));
			}
		}
		if (SR_rebootStatus_chid && (ca_state(SR_rebootStatus_chid) == cs_conn))
			ca_put(DBR_LONG, SR_rebootStatus_chid, &SR_rebootStatus);
		if (SR_rebootStatusStr_chid && (ca_state(SR_rebootStatusStr_chid) == cs_conn))
			ca_put(DBR_STRING, SR_rebootStatusStr_chid, &SR_rebootStatusStr);
		epicsTimeGetCurrent(&currTime);
		epicsTimeToStrftime(SR_rebootTimeStr, sizeof(SR_rebootTimeStr),
			TIMEFMT_noY, &currTime);
		if (SR_rebootTime_chid && (ca_state(SR_rebootTime_chid) == cs_conn))
			ca_put(DBR_STRING, SR_rebootTime_chid, &SR_rebootTimeStr);
	}

	while(1) {

		SR_status = SR_STATUS_OK;
		strcpy(SR_statusStr, "Ok");
		save_restoreSeqPeriodInSeconds = MAX(10, save_restoreSeqPeriodInSeconds);
		save_restoreNumSeqFiles = MIN(10, MAX(0, save_restoreNumSeqFiles));
		epicsTimeGetCurrent(&currTime);
		do_seq_check = (epicsTimeDiffInSeconds(&currTime, &last_seq_check) >
			save_restoreSeqPeriodInSeconds/2);
		if (do_seq_check) last_seq_check = currTime; /* struct copy */

		just_remounted = 0;
#ifdef vxWorks
		if ((save_restoreNFSOK == 0) && save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0]) {
			/* NFS problem: Try, every 60 seconds, to remount */
			if (epicsTimeDiffInSeconds(&currTime, &remount_check_time) > 60.) {
				dismountFileSystem();
				just_remounted = mountFileSystem();
				remount_check_time = currTime; /* struct copy */
			}
		}
#endif
		/* look at each list */
		epicsMutexLock(sr_mutex);
		plist = lptr;
		while (plist != 0) {
			if (save_restoreDebug >= 30)
				errlogPrintf("save_restore: '%s' save_state = 0x%x\n", plist->reqFile, plist->save_state);
			/* connect the channels on the first instance of this set */
			/*
			 * We used to call enable_list() from create_data_set(), if the
			 * list already existed and was just getting a new method.  In that
			 * case, we'd only enable new lists (those with enabled_method==0) here.
			 * Now, we want to avoid having the task that called create_data_set()
			 * setting up CA monitors that we're going to have to manage, so we
			 * make all the calls to enable_list().
			 */ 
			if (plist->enabled_method == 0) plist->not_connected = connect_list(plist);
			if (plist->enabled_method != plist->save_method) enable_list(plist);

			/*
			 * Save lists that have triggered.  If we've just successfully remounted,
			 * save lists that are in failure.
			 */
			if ( (plist->save_state & SINGLE_EVENTS)
			  || ((plist->save_state & MONITORED) == MONITORED)
			  || ((plist->status <= SR_STATUS_FAIL) && just_remounted)) {

				/* fetch values all of the channels */
				plist->not_connected = get_channel_values(plist);

				/* write the data to disk */
				if ((plist->not_connected == 0) || (save_restoreIncompleteSetsOk))
					write_save_file(plist);
			}

			/*** Periodically make sequenced backup of most recent saved file ***/
			if (do_seq_check) {
				if (save_restoreNumSeqFiles && plist->last_save_file &&
					(epicsTimeDiffInSeconds(&currTime, &plist->backup_time) >
						save_restoreSeqPeriodInSeconds)) {
					do_seq(plist);
				}
			}

			/*** restart timers and reset save requests ***/
			if (plist->save_state & PERIODIC) {
				callbackRequestDelayed(&plist->periodicCb, (double)plist->period);
			}
			if (plist->save_state & SINGLE_EVENTS) {
				/* Note that this clears PERIODIC, TRIGGERED, and MANUAL bits */
				plist->save_state = plist->save_state & ~SINGLE_EVENTS;
			}
			if ((plist->save_state & MONITORED) == MONITORED) {
				callbackRequestDelayed(&plist->monitorCb, (double)plist->monitor_period);
				plist->save_state = plist->save_state & ~MONITORED;
			}

			/* find and record worst status */
			if (plist->status <= SR_status ) {
				SR_status = plist->status;
				strcpy(SR_statusStr, plist->statusStr);
			}
			if (SR_rebootStatus < SR_status) {
				SR_status = SR_rebootStatus;
				strcpy(SR_statusStr, SR_rebootStatusStr);
			}

			/* next list */
			plist = plist->pnext;
		}

		/* release the list */
		epicsMutexUnlock(sr_mutex);

		/* report status */
		SR_heartbeat = (SR_heartbeat+1) % 2;
		if (SR_status_chid && (ca_state(SR_status_chid) == cs_conn))
			ca_put(DBR_LONG, SR_status_chid, &SR_status);
		if (SR_heartbeat_chid && (ca_state(SR_heartbeat_chid) == cs_conn))
			ca_put(DBR_LONG, SR_heartbeat_chid, &SR_heartbeat);
		if (SR_statusStr_chid && (ca_state(SR_statusStr_chid) == cs_conn))
			ca_put(DBR_STRING, SR_statusStr_chid, &SR_statusStr);
		if (SR_recentlyStr_chid && (ca_state(SR_recentlyStr_chid) == cs_conn)) {
			SR_recentlyStr[(STRING_LEN-1)] = '\0';
			status = ca_put(DBR_STRING, SR_recentlyStr_chid, &SR_recentlyStr);
		}

		/*** set up list-specific status PV's for any new lists ***/
		for (plist = lptr; plist; plist = plist->pnext) {
			/* If this is the first time for a list, connect to its status PV's */
			if (plist->status_PV[0] == '\0') {
				/*** Build PV names ***/
				/* make common portion of PVname strings */
				n = (PV_NAME_LEN-1) - sprintf(plist->status_PV, "%sSR_%1d_", status_prefix, plist->listNumber);
				strcpy(plist->name_PV, plist->status_PV);
				strcpy(plist->save_state_PV, plist->status_PV);
				strcpy(plist->statusStr_PV, plist->status_PV);
				strcpy(plist->time_PV, plist->status_PV);
				/* make all PVname strings */
				strncat(plist->status_PV, "Status", n);
				strncat(plist->name_PV, "Name", n);
				strncat(plist->save_state_PV, "State", n);
				strncat(plist->statusStr_PV, "StatusStr", n);
				strncat(plist->time_PV, "Time", n);
				/* connect with PV's */
				TATTLE(ca_search(plist->status_PV, &plist->status_chid), "save_restore: ca_search(%s) returned %s", plist->status_PV);
				TATTLE(ca_search(plist->name_PV, &plist->name_chid), "save_restore: ca_search(%s) returned %s", plist->name_PV);
				TATTLE(ca_search(plist->save_state_PV, &plist->save_state_chid), "save_restore: ca_search(%s) returned %s", plist->save_state_PV);
				TATTLE(ca_search(plist->statusStr_PV, &plist->statusStr_chid), "save_restore: ca_search(%s) returned %s", plist->statusStr_PV);
				TATTLE(ca_search(plist->time_PV, &plist->time_chid), "save_restore: ca_search(%s) returned %s", plist->time_PV);
				if (ca_pend_io(0.5)!=ECA_NORMAL) {
					errlogPrintf("Can't connect to status PV(s) for list '%s'\n", plist->save_file);
				}
			}

			if (plist->status_chid && (ca_state(plist->status_chid) == cs_conn))
				ca_put(DBR_LONG, plist->status_chid, &plist->status);
			if (plist->name_chid && (ca_state(plist->name_chid) == cs_conn)) {
				strncpy(nameString, plist->save_file, STRING_LEN-1);
				cp = strrchr(nameString, (int)'.');
				if (cp) *cp = 0;
				ca_put(DBR_STRING, plist->name_chid, &nameString);
			}
			if (plist->save_state_chid && (ca_state(plist->save_state_chid) == cs_conn))
				ca_put(DBR_LONG, plist->save_state_chid, &plist->save_state);
			if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn))
				ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			if ((plist->status >= SR_STATUS_WARN) && (plist->save_time.secPastEpoch != 0)) {
				epicsTimeToStrftime(plist->timeStr, sizeof(plist->timeStr),
					TIMEFMT_noY, &plist->save_time);
				if (plist->time_chid && (ca_state(plist->time_chid) == cs_conn))
					ca_put(DBR_STRING, plist->time_chid, &plist->timeStr);
			}
		}

		/*** service user commands ***/
		if (remove_dset) {
			remove_status = remove_data_set(remove_filename);
			remove_filename[0] = 0;
			remove_dset = 0;
			epicsEventSignal(sem_remove);
		}

		if (manual_restore_type) {
			manual_restore_status = do_manual_restore(manual_restore_filename,
				manual_restore_type);
			manual_restore_filename[0] = 0;
			manual_restore_type = 0;
			epicsEventSignal(sem_do_manual_op);
		}

		/* go to sleep for a while */
		ca_pend_event((double)min_delay);
    }

	/* We're never going to exit */
	return(OK);
}
	

/*
 * connect all of the channels in a save set
 *
 * NOTE: Assumes sr_mutex is locked
 */
STATIC int connect_list(struct chlist *plist)
{
	struct channel	*pchan;
	int				n, m;
	long			status, field_size;

	/* connect all channels in the list */
	for (pchan = plist->pchan_list, n=0; pchan != 0; pchan = pchan->pnext) {
		if (save_restoreDebug >= 10)
			errlogPrintf("save_restore:connect_list: channel '%s'\n", pchan->name);
		if (ca_search(pchan->name,&pchan->chid) == ECA_NORMAL) {
			strcpy(pchan->value,"Search Issued");
			n++;
		} else {
			strcpy(pchan->value,"Search Failed");
		}
	}
	if (ca_pend_io(MAX(5.0, n * 0.01)) == ECA_TIMEOUT) {
		errlogPrintf("save_restore:connect_list: not all searches successful\n");
	}

	for (pchan = plist->pchan_list, n=m=0; pchan != 0; pchan = pchan->pnext, m++) {
		if (pchan->chid && (ca_state(pchan->chid) == cs_conn)) {
			strcpy(pchan->value,"Connected");
			n++;
		}
		pchan->max_elements = ca_element_count(pchan->chid);	/* just to see if it's an array */
		pchan->curr_elements = pchan->max_elements;				/* begin with this assumption */
		if (save_restoreDebug >= 10)
			errlogPrintf("save_restore:connect_list: '%s' has, at most, %ld elements\n",
				pchan->name, pchan->max_elements);
		if (pchan->max_elements > 1) {
			/* We use database access for arrays, so get that info */
			status = SR_get_array_info(pchan->name, &pchan->max_elements, &field_size, &pchan->field_type);
			/* info resulting from dbNameToAddr() might be different, but it's still not the actual element count */
			pchan->curr_elements = pchan->max_elements;
			if (save_restoreDebug >= 10)
				errlogPrintf("save_restore:connect_list:(after SR_get_array_info) '%s' has, at most, %ld elements\n",
					pchan->name, pchan->max_elements);
			if (status == 0)
				pchan->pArray = calloc(pchan->max_elements, field_size);
			if (pchan->pArray == NULL) {
				errlogPrintf("save_restore:connect_list: can't alloc array for '%s'\n", pchan->name);
				pchan->max_elements = 0;
				pchan->curr_elements = 0;
			}
		}
	}
	sprintf(SR_recentlyStr, "%s: %d of %d PV's connected", plist->save_file, n, m);
	return(get_channel_values(plist));
}


/*
 * enable new save methods
 */
STATIC int enable_list(struct chlist *plist)
{
	struct channel	*pchannel;
	chid 			chid;			/* channel access id */

	if (save_restoreDebug >= 4) errlogPrintf("enable_list: entry\n");

	epicsMutexLock(sr_mutex);

	/* enable a periodic set */
	if ((plist->save_method & PERIODIC) && !(plist->enabled_method & PERIODIC)) {
		callbackRequestDelayed(&plist->periodicCb, (double)plist->period);
		plist->enabled_method |= PERIODIC;
	}

	/* enable a triggered set */
	if ((plist->save_method & TRIGGERED) && !(plist->enabled_method & TRIGGERED)) {
		if (ca_search(plist->trigger_channel, &chid) != ECA_NORMAL) {
			errlogPrintf("trigger %s search failed\n", plist->trigger_channel);
		} else if (ca_pend_io(2.0) != ECA_NORMAL) {
			errlogPrintf("timeout on search of %s\n", plist->trigger_channel);
		} else if (chid == NULL) {
			errlogPrintf("no CHID for trigger channel '%s'\n", plist->trigger_channel);
		} else if (ca_state(chid) != cs_conn) {
			errlogPrintf("trigger %s search not connected\n", plist->trigger_channel);
		} else if (ca_add_event(DBR_FLOAT, chid, triggered_save, (void *)plist, 0) !=ECA_NORMAL) {
			errlogPrintf("trigger event for %s failed\n", plist->trigger_channel);
		} else{
			plist->enabled_method |= TRIGGERED;
		}
	}

	/* enable a monitored set */
	if ((plist->save_method & MONITORED) && !(plist->enabled_method & MONITORED)) {
		for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
			if (save_restoreDebug >= 10) errlogPrintf("enable_list: calling ca_add_event for '%s'\n", pchannel->name);
			if (save_restoreDebug >= 10) errlogPrintf("enable_list: arg = %p\n", plist);
			/*
			 * Work around obscure problem affecting USHORTS by making DBR type different
			 * from any possible field type.  This avoids tickling a bug that causes dbGet
			 * to overwrite the source field with its own value converted to LONG.
			 * (Changed DBR_LONG to DBR_TIME_LONG.)
			 */
			if (ca_add_event(DBR_TIME_LONG, pchannel->chid, on_change_save,
					(void *)plist, 0) != ECA_NORMAL) {
				errlogPrintf("could not add event for %s in %s\n",
					pchannel->name,plist->reqFile);
			}
		}
		if (save_restoreDebug >= 4) errlogPrintf("enable_list: done calling ca_add_event for list channels\n");
		if (ca_pend_io(5.0) != ECA_NORMAL) {
			errlogPrintf("timeout on monitored set: %s to monitored scan\n",plist->reqFile);
		}
		callbackRequestDelayed(&plist->monitorCb, (double)plist->monitor_period);
		plist->enabled_method |= MONITORED;
	}

	/* enable a manual request set */
	if ((plist->save_method & MANUAL) && !(plist->enabled_method & MANUAL)) {
		plist->enabled_method |= MANUAL;
	}

	sprintf(SR_recentlyStr, "list '%s' enabled", plist->save_file);
	epicsMutexUnlock(sr_mutex);
	return(OK);
}


/*
 * fetch values for all channels in the save set
 */
#define INIT_STRING "!@#$%^&*()"
STATIC int get_channel_values(struct chlist *plist)
{

	struct channel *pchannel;
	int				not_connected = 0;
	unsigned short	num_channels = 0;
	short			field_type;

	epicsMutexLock(sr_mutex);

	/* attempt to fetch all channels that are connected */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		pchannel->valid = 0;
		if (pchannel->chid && (ca_state(pchannel->chid) == cs_conn) && (pchannel->max_elements >= 1)) {
			field_type = ca_field_type(pchannel->chid);
			strcpy(pchannel->value, INIT_STRING);
			if (field_type == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,(float *)pchannel->value);
			} else if (field_type == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,(double *)pchannel->value);
			} else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			if (field_type == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
			num_channels++;
			pchannel->valid = 1;
			if (pchannel->max_elements > 1) {
				pchannel->curr_elements = pchannel->max_elements;
				(void)SR_get_array(pchannel->name, pchannel->pArray, &pchannel->curr_elements);
			}
			if (save_restoreDebug >= 15) {
				errlogPrintf("save_restore:get_channel_values: '%s' currently has %ld elements\n",
					pchannel->name, pchannel->curr_elements);
			}
		} else {
			not_connected++;
			if (pchannel->chid == NULL) {
				if (save_restoreDebug >= 1) errlogPrintf("get_channel_values: no CHID for '%s'\n", pchannel->name);
			} else if (ca_state(pchannel->chid) != cs_conn) {
				if (save_restoreDebug >= 1) errlogPrintf("get_channel_values: %s not connected\n", pchannel->name);
			}
			if ((pchannel->max_elements < 1)) {
				if (save_restoreDebug >= 1) errlogPrintf("get_channel_values: %s has, at most, %ld elements\n",
					pchannel->name, pchannel->max_elements);
			}
		}
	}
	if (ca_pend_io(MIN(10.0, .1*num_channels)) != ECA_NORMAL) {
		errlogPrintf("get_channel_values: not all gets completed");
		not_connected++;
	}

	/* convert floats and doubles, check to see which get's completed */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				sprintf(pchannel->value, FLOAT_FMT, *(float *)pchannel->value);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				sprintf(pchannel->value, DOUBLE_FMT, *(double *)pchannel->value);
			}
			/* then we at least had a CA connection.  Did it produce? */
			pchannel->valid = strcmp(pchannel->value, INIT_STRING);
		} else {
			if (save_restoreDebug >= 1) errlogPrintf("get_channel_values: invalid channel %s\n", pchannel->name);
		}
	}

	epicsMutexUnlock(sr_mutex);
	return(not_connected);
}

/*** Actually write the file ***/
 
STATIC int write_it(char *filename, struct chlist *plist)
{
	FILE 			*out_fd;
	struct channel	*pchannel;
	int 			n, i;
	char			datetime[32];
	
	epicsMutexLock(sr_mutex);

	/* open the file */
	errno = 0;
	if ((out_fd = fopen(filename,"w")) == NULL) {
		errlogPrintf("save_restore:write_it - unable to open file '%s'\n", filename);
		if (errno) myPrintErrno("write_it");
		if (++save_restoreIoErrors > save_restoreRemountThreshold) {
			save_restoreNFSOK = 0;
			strncpy(SR_recentlyStr, "Too many I/O errors",(STRING_LEN-1));
		}
		epicsMutexUnlock(sr_mutex);
		return(ERROR);
	}

	/* write header info */
	fGetDateStr(datetime);
	errno = 0;
	n = fprintf(out_fd,"# %s\tAutomatically generated - DO NOT MODIFY - %s\n",
			SRversion, datetime);
	if (errno) myPrintErrno("write_it");
	if (n <= 0) {
		errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
		goto trouble;
	}

	if (plist->not_connected) {
		errno = 0;
		n = fprintf(out_fd,"! %d channel(s) not connected - or not all gets were successful\n",
				plist->not_connected);
		if (errno) myPrintErrno("write_it");
		if (n <= 0) {
			errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
			goto trouble;
		}
	}

	/* write PV names and values */
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		errno = 0;
		if (pchannel->valid) {
			n = fprintf(out_fd, "%s ", pchannel->name);
		} else {
			n = fprintf(out_fd, "#%s ", pchannel->name);
		}
		if (errno) myPrintErrno("write_it");
		if (n <= 0) {
			errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
			goto trouble;
		}

		errno = 0;
		if (pchannel->curr_elements <= 1) {
			/* treat as scalar */
			if (pchannel->enum_val >= 0) {
				n = fprintf(out_fd, "%d\n",pchannel->enum_val);
			} else {
				n = fprintf(out_fd, "%-s\n", pchannel->value);
			}
			if (errno) myPrintErrno("write_it");
			if (n <= 0) {
				errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
				goto trouble;
			}
		} else {
			/* treat as array */
			n = SR_write_array_data(out_fd, pchannel->name, (void *)pchannel->pArray, pchannel->curr_elements);
			if (errno) myPrintErrno("write_it");
			if (n <= 0) {
				errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
				goto trouble;
			}
		}
	}

	/* debug: simulate task crash */
#if 0
	if (save_restoreDebug == 999) {
		errlogPrintf("save_restore: simulating task crash.  Bye, bye!\n");
		exit(-1);
	}
#endif

	/* write file-is-ok marker */
	errno = 0;
	n = fprintf(out_fd, "<END>\n");
	if (errno) myPrintErrno("write_it");
	if (n <= 0) {
		errlogPrintf("save_restore:write_it: fprintf returned %d.\n", n);
		goto trouble;
	}

	/* flush everything to disk */
	errno = 0;
	n = fflush(out_fd);
	if (errno) myPrintErrno("write_it");
	if (n != 0) errlogPrintf("save_restore:write_it: fflush returned %d\n", n);

	errno = 0;
#if defined(vxWorks)
	n = ioctl(fileno(out_fd),FIOSYNC,0);	/* NFS flush to disk */
	if (n == ERROR) errlogPrintf("save_restore: ioctl(,FIOSYNC,) returned %d\n", n);
#elif defined(_WIN32)
        /* WIN32 has no real equivalent to fsync? */
#else
	n = fsync(fileno(out_fd));
	if (n != 0) errlogPrintf("save_restore:write_it: fsync returned %d\n", n);
#endif
	if (errno) myPrintErrno("write_it");

	/* close the file */
	errno = 0;
	n = fclose(out_fd);
	if (errno) myPrintErrno("write_it");
	if (n != 0) {
		errlogPrintf("save_restore:write_it: fclose returned %d\n", n);
		goto trouble;
	}
	epicsMutexUnlock(sr_mutex);
	return(OK);

trouble:
	/* try to close file */
	for (i=1, n=1; n && i<100;i++) {
		n = fclose(out_fd);
		if (n && ((i%10)==0)) {
			errlogPrintf("save_restore:write_it: fclose('%s') returned %d (%d tries)\n",
				plist->save_file, n, i);
			epicsThreadSleep(10.0);
		}
	}
	if (i >= 60) {
		errlogPrintf("save_restore:write_it: Can't close '%s'; giving up.\n", plist->save_file);
	}
	epicsMutexUnlock(sr_mutex);
	return(ERROR);
}


/* state of a backup restore file */
#define BS_NONE 	0	/* Couldn't open the file */
#define BS_BAD		1	/* File exists but looks corrupted */
#define BS_OK		2	/* File is good */
#define BS_NEW		3	/* Just wrote the file */

STATIC int check_file(char *file)
{
	FILE *fd;
	char tmpstr[20];
	int	 file_state = BS_NONE;

	if ((fd = fopen(file, "r")) != NULL) {
		if ((fseek(fd, -6, SEEK_END)) ||
			(fgets(tmpstr, 6, fd) == 0) ||
			(strncmp(tmpstr, "<END>", 5) != 0)) {
				file_state = BS_BAD;
		}
		fclose(fd);
		file_state = BS_OK;
	}
	return(file_state);
}


/*
 * save_file - save routine
 *
 * Write a list of channel names and values to an ASCII file.
 *
 */
STATIC int write_save_file(struct chlist *plist)
{
	char	save_file[PATH_SIZE+3] = "", backup_file[PATH_SIZE+3] = "";
	char	tmpstr[PATH_SIZE+50];
	int		backup_state = BS_OK;
	char	datetime[32];

	epicsMutexLock(sr_mutex);
	plist->status = SR_STATUS_OK;
	strcpy(plist->statusStr, "Ok");

	/* Make full file names */
	strncpy(save_file, saveRestoreFilePath, sizeof(save_file) - 1);
	strncat(save_file, plist->save_file, MAX(0, sizeof(save_file) - 1 - strlen(save_file)));
	strcpy(backup_file, save_file);
	strncat(backup_file, "B", 1);

	/* Ensure that backup is ok before we overwrite .sav file. */
	backup_state = check_file(backup_file);
	if (backup_state != BS_OK) {
		errlogPrintf("write_save_file: Backup file (%s) bad or not found.  Writing a new one.\n", 
			backup_file);
		if (backup_state == BS_BAD) {
			/* make a backup copy of the corrupted file */
			strcpy(tmpstr, backup_file);
			strcat(tmpstr, "_SBAD_");
			if (save_restoreDatedBackupFiles) {
				fGetDateStr(datetime);
				strcat(tmpstr, datetime);
				sprintf(SR_recentlyStr, "Bad file: '%sB'", plist->save_file);
			}
			(void)myFileCopy(backup_file, tmpstr);
		}
		if (write_it(backup_file, plist) == ERROR) {
			errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
			errlogPrintf("save_restore:write_save_file: Can't write new backup file. \n");
			errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n");
			plist->status = SR_STATUS_FAIL;
			strcpy(plist->statusStr, "Can't write .savB file");
			if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
				ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
				ca_flush_io();
			}
			epicsMutexUnlock(sr_mutex);
			return(ERROR);
		}
		plist->status = SR_STATUS_WARN;
		strcpy(plist->statusStr, ".savB file was bad");
		if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
			ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			ca_flush_io();
		}
		backup_state = BS_NEW;
	}

	/*** Write the save file ***/
	if (save_restoreDebug >= 1) errlogPrintf("write_save_file: saving to %s\n", save_file);
	if (write_it(save_file, plist) == ERROR) {
		errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		errlogPrintf("save_restore:write_save_file: Can't write save file.\n");
		errlogPrintf("*** *** *** *** *** *** *** *** *** *** *** *** ***\n");
		plist->status = SR_STATUS_FAIL;
		strcpy(plist->statusStr, "Can't write .sav file");
		if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
			ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			ca_flush_io();
		}
		sprintf(SR_recentlyStr, "Can't write '%s'", plist->save_file);
		epicsMutexUnlock(sr_mutex);
		return(ERROR);
	}

	/* keep the name and time of the last saved file */
	epicsTimeGetCurrent(&plist->save_time);
	strcpy(plist->last_save_file, plist->save_file);

	/*** Write a backup copy of the save file ***/
	if (backup_state != BS_NEW) {
		/* make a backup copy */
		if (myFileCopy(save_file, backup_file) != OK) {
			errlogPrintf("save_restore:write_save_file - Couldn't make backup '%s'\n",
				backup_file);
			plist->status = SR_STATUS_WARN;
			strcpy(plist->statusStr, "Can't write .savB file");
			if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
				ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
				ca_flush_io();
			}
			sprintf(SR_recentlyStr, "Can't write '%sB'", plist->save_file);
			epicsMutexUnlock(sr_mutex);
			return(ERROR);
		}
	}
	if (plist->not_connected) {
		plist->status = SR_STATUS_WARN;
		sprintf(plist->statusStr,"%d %s not saved", plist->not_connected, 
			plist->not_connected==1?"value":"values");
		if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
			ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			ca_flush_io();
		}
	}
	sprintf(SR_recentlyStr, "Wrote '%s'", plist->save_file);
	epicsMutexUnlock(sr_mutex);
	return(OK);
}


/*
 * do_seq - copy .sav file to .savX, where X is in
 * [0..save_restoreNumSeqFiles].  If .sav file can't be opened,
 * write .savX file explicitly, as we would write .sav file.
 *
 */
STATIC void do_seq(struct chlist *plist)
{
	char	*p, save_file[PATH_SIZE+3] = "", backup_file[PATH_SIZE+3] = "";
	int		i;
	struct stat fileStat;

	epicsMutexLock(sr_mutex);

	/* Make full file names */
	strncpy(save_file, saveRestoreFilePath, sizeof(save_file) - 1);
	strncat(save_file, plist->save_file, MAX(0, sizeof(save_file) - 1 - strlen(save_file)));
	strcpy(backup_file, save_file);
	p = &backup_file[strlen(backup_file)];

	/* If first time for this list, determine which existing file is oldest. */
	if (plist->backup_sequence_num == -1) {
		double dTime, max_dTime = -1.e9;

		plist->backup_sequence_num = 0;
		for (i=0; i<save_restoreNumSeqFiles; i++) {
			sprintf(p, "%1d", i);	/* (over)write sequence number */
			if (stat(backup_file, &fileStat)) {
				/* can't check date; just assume this file is oldest */
				plist->backup_sequence_num = i;
				break;
			}
			dTime = difftime(time(NULL), fileStat.st_mtime);
			if (dTime > max_dTime) {
				max_dTime = dTime;
				plist->backup_sequence_num = i;
			}
		}
	}

	if (check_file(save_file) == BS_NONE) {
		errlogPrintf("save_restore:do_seq - '%s' not found.  Writing a new one.\n",
			save_file);
		(void) write_save_file(plist);
	}
	sprintf(p, "%1d", plist->backup_sequence_num);
	if (myFileCopy(save_file, backup_file) != OK) {
		errlogPrintf("save_restore:do_seq - Can't copy save file to '%s'\n",
			backup_file);
		if (write_it(backup_file, plist) == ERROR) {
			errlogPrintf("save_restore:do_seq - Can't write seq. file from PV list.\n");
			if (plist->status >= SR_STATUS_WARN) {
				plist->status = SR_STATUS_SEQ_WARN;
				strcpy(plist->statusStr, "Can't write sequence file");
			}
			sprintf(SR_recentlyStr, "Can't write '%s%1d'",
				plist->save_file, plist->backup_sequence_num);
			epicsMutexUnlock(sr_mutex);
			return;
		} else {
			errlogPrintf("save_restore:do_seq: Wrote seq. file from PV list.\n");
			sprintf(SR_recentlyStr, "Wrote '%s%1d'",
				plist->save_file, plist->backup_sequence_num);
		}
	} else {
		sprintf(SR_recentlyStr, "Wrote '%s%1d'",
			plist->save_file, plist->backup_sequence_num);
	}

	epicsTimeGetCurrent(&plist->backup_time);
	if (++(plist->backup_sequence_num) >=  save_restoreNumSeqFiles)
		plist->backup_sequence_num = 0;

	epicsMutexUnlock(sr_mutex);
}


int set_savefile_name(char *filename, char *save_filename)
{
	struct chlist	*plist;

	/* is save set defined - add new save mode if necessary */
	epicsMutexLock(sr_mutex);
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->reqFile,filename)) {
			strcpy(plist->save_file,save_filename);
			epicsMutexUnlock(sr_mutex);
			sprintf(SR_recentlyStr, "New save file: '%s'", save_filename);
			return(OK);
		}
		plist = plist->pnext;
	}
	errlogPrintf("No save set enabled for %s\n",filename);
	epicsMutexUnlock(sr_mutex);
	return(ERROR);
}


int create_periodic_set(char *filename, int period, char *macrostring)
{
	return(create_data_set(filename, PERIODIC, period, 0, 0, macrostring));
}


int create_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	if (trigger_channel && (isalpha((int)trigger_channel[0]) || isdigit((int)trigger_channel[0]))) {
		return(create_data_set(filename, TRIGGERED, 0, trigger_channel, 0, macrostring));
	}
	else {
		errlogPrintf("create_triggered_set: Error: trigger-channel name is required.\n");
		return(ERROR);
	}
}


int create_monitor_set(char *filename, int period, char *macrostring)
{
	return(create_data_set(filename, MONITORED, 0, 0, period, macrostring));
}


int create_manual_set(char *filename, char *macrostring)
{
	return(create_data_set(filename, MANUAL, 0, 0, 0, macrostring));
}


/*
 * create a data set
 */
STATIC int create_data_set(
	char	*filename,			/* save set request file */
	int		save_method,
	int		period,				/* maximum time between saves  */
	char	*trigger_channel,	/* db channel to trigger save  */
	int		mon_period,			/* minimum time between saves  */
	char	*macrostring
)
{
	struct chlist	*plist;
	int				inx;			/* i/o status 	       */

	if (save_restoreDebug) {
		errlogPrintf("create_data_set: file '%s', method %x, period %d, trig_chan '%s', mon_period %d\n",
			filename, save_method, period, trigger_channel ? trigger_channel : "NONE", mon_period);
	}

	/* initialize save_restore routines */
	if (!save_restore_init) {
		if ((sr_mutex = epicsMutexCreate()) == 0) {
			errlogPrintf("create_data_set: could not create list header mutex");
			return(ERROR);
		}
		if ((sem_remove = epicsEventCreate(epicsEventEmpty)) == 0) {
			errlogPrintf("create_data_set: could not create delete-list semaphore\n");
			return(ERROR);
		}
		if ((sem_do_manual_op = epicsEventCreate(epicsEventEmpty)) == 0) {
			errlogPrintf("create_data_set: could not create do_manual_op semaphore\n");
			return(ERROR);
		}
		taskID = epicsThreadCreate("save_restore", taskPriority,
			epicsThreadGetStackSize(epicsThreadStackMedium),
			(EPICSTHREADFUNC)save_restore, 0);
		if (taskID == NULL) {
			errlogPrintf("create_data_set: could not create save_restore task");
			return(ERROR);
		}

		save_restore_init = 1;
	}

	/* is save set defined - add new save mode if necessary */
	epicsMutexLock(sr_mutex);
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->reqFile,filename)) {
			if (plist->save_method & save_method) {
				errlogPrintf("create_data_set: '%s' already in %x mode",filename,save_method);
				epicsMutexUnlock(sr_mutex);
				return(ERROR);
			} else {
				/* Add a new method to an existing list */
				if (save_method == TRIGGERED) {
					if (trigger_channel) {
						strcpy(plist->trigger_channel,trigger_channel);
					} else {
						errlogPrintf("create_data_set: no trigger channel");
						epicsMutexUnlock(sr_mutex);
						return(ERROR);
					}
				} else if (save_method == PERIODIC) {
					plist->period = period;
				} else if (save_method == MONITORED) {
					plist->monitor_period = mon_period;
				}
				plist->save_method |= save_method;
				/*
				 * We used to call enable_list() from here, but it starts CA
				 * monitors that will be handled by the save_restore task.
				 * Now, we let the save_restore task do all of its own CA stuff.
				 */
				/* enable_list(plist); */

				epicsMutexUnlock(sr_mutex);
				return(OK);
			}
		}
		plist = plist->pnext;
	}
	epicsMutexUnlock(sr_mutex);

	/* create a new channel list */
	if ((plist = (struct chlist *)calloc(1,sizeof (struct chlist))) == (struct chlist *)0) {
		errlogPrintf("create_data_set: channel list calloc failed");
		return(ERROR);
	}
	callbackSetCallback(periodic_save, &plist->periodicCb);
	callbackSetUser(plist, &plist->periodicCb);
	callbackSetCallback(on_change_timer, &plist->monitorCb);
	callbackSetUser(plist, &plist->monitorCb);
	strncpy(plist->reqFile, filename, sizeof(plist->reqFile)-1);
	plist->pchan_list = (struct channel *)0;
	plist->period = MAX(period, min_period);
	if (trigger_channel) {
	    strncpy(plist->trigger_channel, trigger_channel, sizeof(plist->trigger_channel)-1);
	} else {
	    plist->trigger_channel[0]=0;
	}
	plist->last_save_file[0] = 0;
	plist->save_method = save_method;
	plist->enabled_method = 0;
	plist->save_state = 0;
	plist->save_ok = 0;
	plist->monitor_period = MAX(mon_period, min_period);
	epicsTimeGetCurrent(&plist->backup_time);
	plist->backup_sequence_num = -1;
	plist->save_ok = 0;
	plist->not_connected = -1;
	plist->status = SR_STATUS_INIT;
	strcpy(plist->statusStr,"Initializing list");

	/** construct the save_file name **/
	strcpy(plist->save_file, plist->reqFile);
	inx = 0;
	while ((plist->save_file[inx] != 0) && (plist->save_file[inx] != '.') && (inx < (FN_LEN-6))) inx++;
	plist->save_file[inx] = 0;	/* truncate if necessary to leave room for ".sav" + null */
	strcat(plist->save_file,".sav");
	/* make full name, including file path */
	strncpy(plist->saveFile, saveRestoreFilePath, sizeof(plist->saveFile) - 1);
	strncat(plist->saveFile, plist->save_file, MAX(sizeof(plist->saveFile) - 1 -
			strlen(plist->saveFile),0));

	/* read the request file and populate plist with the PV names */
	if (readReqFile(plist->reqFile, plist, macrostring) == ERROR) {
		free(plist);
		return(ERROR);
	}

	/* future: associate the list with a set of status PV's */
	plist->listNumber = listNumber++;

	/* link it to the save set list */
	epicsMutexLock(sr_mutex);
	plist->pnext = lptr;
	lptr = plist;
	epicsMutexUnlock(sr_mutex);

	return(OK);
}




/*
 * save_restoreShow -  Show state of save_restore; optionally, list save sets
 *
 */
void save_restoreShow(int verbose)
{
	struct chlist	*plist;
	struct channel 	*pchannel;
	struct pathListElement *p = reqFilePathList;
	char tmpstr[50];
	int NFS_managed = save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && 
		saveRestoreFilePath[0];

	printf("BEGIN save_restoreShow\n");
	printf("  Status: '%s' - '%s'\n", SR_STATUS_STR[SR_status], SR_statusStr);
	printf("  Debug level: %d\n", save_restoreDebug);
	printf("  Save/restore incomplete save sets? %s\n", save_restoreIncompleteSetsOk?"YES":"NO");
	printf("  Write dated backup files? %s\n", save_restoreDatedBackupFiles?"YES":"NO");
	printf("  Number of sequence files to maintain: %d\n", save_restoreNumSeqFiles);
	printf("  Time interval between sequence files: %d seconds\n", save_restoreSeqPeriodInSeconds);
	printf("  NFS host: '%s'; address:'%s'\n", save_restoreNFSHostName, save_restoreNFSHostAddr);
	printf("  NFS mount status: %s\n",
		save_restoreNFSOK?"Ok":NFS_managed?"Failed":"not managed by save_restore");
	printf("  I/O errors: %d\n", save_restoreIoErrors);
	printf("  request file path list:\n");
	while (p) {
		printf("    '%s'\n", p->path);
		p = p->pnext;
	}
	printf("  save file path:\n    '%s'\n", saveRestoreFilePath);
	if (sr_mutex && (epicsMutexLock(sr_mutex) == epicsMutexLockOK)) {
		for (plist = lptr; plist != 0; plist = plist->pnext) {
			printf("  %s: \n",plist->reqFile);
			printf("    Status PV: %s\n", plist->status_PV);
			printf("    Status: '%s' - '%s'\n", SR_STATUS_STR[plist->status], plist->statusStr);
			epicsTimeToStrftime(tmpstr, sizeof(tmpstr), TIMEFMT, &plist->save_time);
			printf("    Last save time  :%s", tmpstr);
			epicsTimeToStrftime(tmpstr, sizeof(tmpstr), TIMEFMT, &plist->backup_time);
			printf("    Last backup time:%s", tmpstr);
			strcpy(tmpstr, "[ ");
			if (plist->save_method & PERIODIC) strcat(tmpstr, "PERIODIC ");
			if (plist->save_method & TRIGGERED) strcat(tmpstr, "TRIGGERED ");
			if ((plist->save_method & MONITORED)==MONITORED) strcat(tmpstr, "TIMER+CHANGE ");
			if (plist->save_method & MANUAL) strcat(tmpstr, "MANUAL ");
			strcat(tmpstr, "]");
			printf("    methods: %s\n", tmpstr);
			strcpy(tmpstr, "[ ");
			if (plist->save_state & PERIODIC) strcat(tmpstr, "PERIOD ");
			if (plist->save_state & TRIGGERED) strcat(tmpstr, "TRIGGER ");
			if (plist->save_state & TIMER) strcat(tmpstr, "TIMER ");
			if (plist->save_state & CHANGE) strcat(tmpstr, "CHANGE ");
			if (plist->save_state & MANUAL) strcat(tmpstr, "MANUAL ");
			strcat(tmpstr, "]");
			printf("    save_state = 0x%x\n", plist->save_state);
			printf("    period: %d; trigger chan: '%s'; monitor period: %d\n",
			   plist->period,plist->trigger_channel,plist->monitor_period);
			printf("    last saved file - %s\n",plist->last_save_file);
			printf("    %d channel%c not connected (or ca_get failed)\n",plist->not_connected,
				(plist->not_connected == 1) ? ' ' : 's');
			if (verbose) {
				for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
					printf("\t%s (max:%ld curr:%ld elements)\t%s", pchannel->name,
						pchannel->max_elements, pchannel->curr_elements, pchannel->value);
					if (pchannel->enum_val >= 0) printf("\t%d\n",pchannel->enum_val);
					else printf("\n");
				}
			}
		}
		epicsMutexUnlock(sr_mutex);
	} else {
		if (!sr_mutex)
			printf("  The save_restore task apparently is not running.\n");
		else
			printf("  Can't lock sr_mutex.\n");
	}
	printf("reboot-restore status:\n");
	dbrestoreShow();
	printf("END save_restoreShow\n");
}


int set_requestfile_path(char *path, char *pathsub)
{
	struct pathListElement *p, *pnew;
	char fullpath[PATH_SIZE+1] = "";
	int path_len=0, pathsub_len=0;

	if (path && *path) path_len = strlen(path);
	if (pathsub && *pathsub) pathsub_len = strlen(pathsub);
	if (path_len + pathsub_len > (PATH_SIZE-1)) {	/* may have to add '/' */
		errlogPrintf("set_requestfile_path: 'path'+'pathsub' is too long\n");
		return(ERROR);
	}

	if (path && *path) {
		strcpy(fullpath, path);
		if (pathsub && *pathsub) {
			if (*pathsub != '/' && path[strlen(path)-1] != '/') {
				strcat(fullpath, "/");
			}
			strcat(fullpath, pathsub);
		}
	} else if (pathsub && *pathsub) {
		strcpy(fullpath, pathsub);
	}

	if (*fullpath) {
		/* return(set_requestfile_path(fullpath)); */
		pnew = (struct pathListElement *)calloc(1, sizeof(struct pathListElement));
		if (pnew == NULL) {
			errlogPrintf("set_requestfile_path: calloc failed\n");
			return(ERROR);
		}
		strcpy(pnew->path, fullpath);
		if (pnew->path[strlen(pnew->path)-1] != '/') {
			strcat(pnew->path, "/");
		}

		if (reqFilePathList == NULL) {
			reqFilePathList = pnew;
		} else {
			for (p = reqFilePathList; p->pnext; p = p->pnext)
				;
			p->pnext = pnew;
		}
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_savefile_path(char *path, char *pathsub)
{
	char fullpath[PATH_SIZE+1] = "";
	int path_len=0, pathsub_len=0;

	if (save_restoreNFSOK) dismountFileSystem();

	if (path && *path) path_len = strlen(path);
	if (pathsub && *pathsub) pathsub_len = strlen(pathsub);
	if (path_len + pathsub_len > (PATH_SIZE-1)) {	/* may have to add '/' */
		errlogPrintf("set_requestfile_path: 'path'+'pathsub' is too long\n");
		return(ERROR);
	}

	if (path && *path) {
		strcpy(fullpath, path);
		if (pathsub && *pathsub) {
			if (*pathsub != '/' && path[strlen(path)-1] != '/') {
				strcat(fullpath, "/");
			}
			strcat(fullpath, pathsub);
		}
	} else if (pathsub && *pathsub) {
		strcpy(fullpath, pathsub);
	}

	if (*fullpath) {
		strcpy(saveRestoreFilePath, fullpath);
		if (saveRestoreFilePath[strlen(saveRestoreFilePath)-1] != '/') {
			strcat(saveRestoreFilePath, "/");
		}
		if (save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0]) mountFileSystem();
		return(OK);
	} else {
		return(ERROR);
	}
}

int set_saveTask_priority(int priority)
{
	if ((priority < epicsThreadPriorityMin) || (priority > epicsThreadPriorityMax)) {
		epicsPrintf("save_restore - priority must be >= %d and <= %d\n",
			epicsThreadPriorityMin, epicsThreadPriorityMax);
		return(ERROR);
	}
	taskPriority = priority;
	if (taskID != NULL) {
		epicsThreadSetPriority(taskID, priority);
	}
	return(OK);
}


/*** remove a data set from the list ***/
int remove_data_set(char *filename)
{
	int found = 0;
	int numchannels = 0;
	struct chlist *plist, *previous;
	struct channel *pchannel, *pchannelt;

	/* find the data set */
	plist = lptr;
	previous = 0;
	while(plist) {
		if (!strcmp(plist->reqFile, filename)) {  
			found = 1;
			break;
		}
		previous = plist;
		plist = plist->pnext;
	}

	if (found) {
		epicsMutexLock(sr_mutex);

		pchannel = plist->pchan_list;
		while (pchannel) {
			if (ca_clear_channel(pchannel->chid) != ECA_NORMAL) {
				errlogPrintf("remove_data_set: couldn't remove ca connection for %s\n", pchannel->name);
			}
			pchannel = pchannel->pnext;
			numchannels++;
		}
		if (ca_pend_io(MIN(10.0, numchannels*0.1)) != ECA_NORMAL) {
		       errlogPrintf("remove_data_set: ca_pend_io() timed out\n");
		}
		pchannel = plist->pchan_list;
		while (pchannel) {
			pchannelt = pchannel->pnext;
			if (pchannel->pArray) free(pchannel->pArray);
			free(pchannel);
			pchannel = pchannelt;
		}
		if (previous == 0) {
			lptr = plist->pnext;
		} else {
			previous->pnext = plist->pnext;
		}
		free(plist);

		epicsMutexUnlock(sr_mutex);

	} else {
		errlogPrintf("remove_data_set: Couldn't find '%s'\n", filename);
		sprintf(SR_recentlyStr, "Can't remove data set '%s'", filename);
		return(ERROR);
	}
	sprintf(SR_recentlyStr, "Removed data set '%s'", filename);
	return(OK);
}

int reload_periodic_set(char *filename, int period, char *macrostring)
{
	epicsEventWaitStatus s;

	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	s = epicsEventWaitWithTimeout(sem_remove, (double)TIME2WAIT);
	if (s) errlogPrintf("reload_periodic_set: epicsEventWaitWithTimeout -> %d\n", (int)s);
	if (s || remove_status) {
		errlogPrintf("reload_periodic_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(SR_recentlyStr, "Reloaded data set '%s'", filename);
		return(create_periodic_set(filename, period, macrostring));
	}
}

int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring)
{
	epicsEventWaitStatus s;

	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	s = epicsEventWaitWithTimeout(sem_remove, (double)TIME2WAIT);
	if (s) errlogPrintf("reload_triggered_set: epicsEventWaitWithTimeout -> %d\n", s);
	if (s || remove_status) {
		errlogPrintf("reload_triggered_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(SR_recentlyStr, "Reloaded data set '%s'", filename);
		return(create_triggered_set(filename, trigger_channel, macrostring));
	}
}


int reload_monitor_set(char * filename, int period, char *macrostring)
{
	epicsEventWaitStatus s;

	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	s = epicsEventWaitWithTimeout(sem_remove, (double)TIME2WAIT);
	if (s) epicsPrintf("reload_monitor_set: epicsEventWaitWithTimeout -> %d\n", s);
	if (s || remove_status) {
		errlogPrintf("reload_monitor_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(SR_recentlyStr, "Reloaded data set '%s'", filename);
		return(create_monitor_set(filename, period, macrostring));
	}
}

int reload_manual_set(char * filename, char *macrostring)
{
	epicsEventWaitStatus s;

	strncpy(remove_filename, filename, sizeof(remove_filename) -1);
	remove_dset = 1;
	s = epicsEventWaitWithTimeout(sem_remove, (double)TIME2WAIT);
	if (s) epicsPrintf("reload_manual_set: epicsEventWaitWithTimeout -> %d\n", s);
	if (s || remove_status) {
		errlogPrintf("reload_manual_set: error removing %s\n", filename);
		return(ERROR);
	} else {
		sprintf(SR_recentlyStr, "Reloaded data set '%s'", filename);
		return(create_manual_set(filename, macrostring));
	}
}

int fdbrestore(char *filename)
{
	return(request_manual_restore(filename, FROM_SAVE_FILE));
}

int fdbrestoreX(char *filename)
{
	return(request_manual_restore(filename, FROM_ASCII_FILE));
}

int request_manual_restore(char *filename, int file_type)
{
	epicsEventWaitStatus s;

	/* wait until any pending restore completes.  Should probably be a semaphore */
	while (manual_restore_type) epicsThreadSleep(1.0);
	strncpy(manual_restore_filename, filename, sizeof(manual_restore_filename) -1);
	manual_restore_type = file_type;	/* Tell save_restore to do a manual restore */
	s = epicsEventWaitWithTimeout(sem_do_manual_op, (double)TIME2WAIT);
	if (s) epicsPrintf("request_manual_restore: epicsEventWaitWithTimeout -> %d\n", s);
	if (s || manual_restore_status) {
		errlogPrintf("request_manual_restore: error restoring %s\n", filename);
		return(ERROR);
	} else {
		sprintf(SR_recentlyStr, "Restored data set '%s'", filename);
		return(OK);
	}
}


/*
 * do_manual_restore - restore routine
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database vaules.
 *
 */
int do_manual_restore(char *filename, int file_type)
{	
	struct channel	*pchannel;
	struct chlist	*plist;
	int				found, is_scalar;
	char			PVname[80];
	char			restoreFile[PATH_SIZE+1] = "";
	char			bu_filename[PATH_SIZE+1] = "";
	char			buffer[BUF_SIZE], *bp, c;
	char			value_string[BUF_SIZE];
	int				n;
	long			status, num_errs=0;
	FILE			*inp_fd;
	chid			chanid;

	if (file_type == FROM_SAVE_FILE) {
		/* if this is the current file name for a save set - restore from there */
		epicsMutexLock(sr_mutex);
		for (plist=lptr, found=0; plist && !found; ) {
			if (strcmp(plist->last_save_file,filename) == 0) {
				found = 1;
			}else{
				plist = plist->pnext;
			}
		}
		if (found) {
			/* verify quality of the save set */
			if (plist->not_connected > 0) {
				errlogPrintf("do_manual_restore: %d channel(s) not connected or fetched\n",
					plist->not_connected);
				if (!save_restoreIncompleteSetsOk) {
					errlogPrintf("do_manual_restore: aborting restore\n");
					epicsMutexUnlock(sr_mutex);
					strncpy(SR_recentlyStr, "Manual restore failed",(STRING_LEN-1));
					return(ERROR);
				}
			}

			for (pchannel = plist->pchan_list; pchannel !=0; pchannel = pchannel->pnext) {
				if (pchannel->curr_elements <= 1) {
					status = ca_put(DBR_STRING, pchannel->chid, pchannel->value);
				} else {
					status = SR_put_array_values(pchannel->name, pchannel->pArray, pchannel->curr_elements);
				}
			}
			if (status) num_errs++;
			if (ca_pend_io(1.0) != ECA_NORMAL) {
				errlogPrintf("do_manual_restore: not all channels restored\n");
			}
			epicsMutexUnlock(sr_mutex);
			if (num_errs == 0) {
				strncpy(SR_recentlyStr, "Manual restore succeeded",(STRING_LEN-1));
			} else {
				sprintf(SR_recentlyStr, "%ld errors during manual restore", num_errs);
			}
			return(OK);
		}
		epicsMutexUnlock(sr_mutex);
	}

	/* open file */
	strncpy(restoreFile, saveRestoreFilePath, sizeof(restoreFile) - 1);
	strncat(restoreFile, filename, MAX(sizeof(restoreFile) -1 - strlen(restoreFile),0));

	if (file_type == FROM_SAVE_FILE) {
		inp_fd = fopen_and_check(restoreFile, &status);
	} else {
		inp_fd = fopen(restoreFile, "r");
	}
	if (inp_fd == NULL) {
		errlogPrintf("do_manual_restore: Can't open save file.");
		strncpy(SR_recentlyStr, "Manual restore failed",(STRING_LEN-1));
		return(ERROR);
	}

	if (file_type == FROM_SAVE_FILE) {
		(void)fgets(buffer, BUF_SIZE, inp_fd); /* discard header line */
	}
	/* restore from data file */
	while ((bp=fgets(buffer, BUF_SIZE, inp_fd))) {
		/* get PV_name, one space character, value */
		/* (value may be a string with leading whitespace; it may be */
		/* entirely whitespace; the number of spaces may be crucial; */
		/* it might also consist of zero characters) */
		n = sscanf(bp,"%s%c%[^\n]", PVname, &c, value_string);
		if (n < 3) *value_string = 0;
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (is_scalar) {
				if (ca_search(PVname, &chanid) != ECA_NORMAL) {
					errlogPrintf("do_manual_restore: ca_search for %s failed\n", PVname);
				} else if (ca_pend_io(0.5) != ECA_NORMAL) {
					errlogPrintf("do_manual_restore: ca_search for %s timeout\n", PVname);
				} else if (ca_put(DBR_STRING, chanid, value_string) != ECA_NORMAL) {
					errlogPrintf("do_manual_restore: ca_put of %s to %s failed\n", value_string,PVname);
				}
			} else {
				status = SR_array_restore(1, inp_fd, PVname, value_string);
			}
		} else if (PVname[0] == '!') {
			n = atoi(value_string);	/* value_string actually contains 2nd word of error msg */
			errlogPrintf("do_manual_restore: %d PV%c had no saved value\n",
				n, (n==1) ? ' ':'s');
			if (!save_restoreIncompleteSetsOk) {
				errlogPrintf("do_manual_restore: aborting restore\n");
				fclose(inp_fd);
				strncpy(SR_recentlyStr, "Manual restore failed",(STRING_LEN-1));
				return(ERROR);
			}
		}
	}
	fclose(inp_fd);

	if (file_type == FROM_SAVE_FILE) {
		/* make  backup */
		strcpy(bu_filename,restoreFile);
		strcat(bu_filename,".bu");
		(void)myFileCopy(restoreFile,bu_filename);
	}
	strncpy(SR_recentlyStr, "Manual restore succeeded",(STRING_LEN-1));
	return(OK);
}


STATIC int readReqFile(const char *reqFile, struct chlist *plist, char *macrostring)
{
	struct channel	*pchannel = NULL;
	FILE   			*inp_fd = NULL;
	char			name[80] = "", *t=NULL, line[BUF_SIZE]="", eline[BUF_SIZE]="";
	char            templatefile[FN_LEN] = "";
	char            new_macro[80] = "";
	int             i=0;
	MAC_HANDLE      *handle = NULL;
	char            **pairs = NULL;
	struct pathListElement *p;
	char tmpfile[PATH_SIZE+1] = "";

	if (save_restoreDebug >= 1) {
		errlogPrintf("save_restore:readReqFile: entry: reqFile='%s', plist=%p, macrostring='%s'\n",
			reqFile, plist, macrostring?macrostring:"NULL");
	}

	/* open request file */
	if (reqFilePathList) {
		/* try to find reqFile in every directory specified in reqFilePathList */
		for (p = reqFilePathList; p; p = p->pnext) {
			strcpy(tmpfile, p->path);
			strcat(tmpfile, reqFile);
			inp_fd = fopen(tmpfile, "r");
			if (inp_fd) break;
		}
	} else {
		/* try to find reqFile only in current working directory */
		inp_fd = fopen(reqFile, "r");
	}

	if (!inp_fd) {
		plist->status = SR_STATUS_FAIL;
		strcpy(plist->statusStr, "Can't open .req file");
		if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
			ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
			ca_flush_io();
		}
		errlogPrintf("save_restore:readReqFile: unable to open file %s. Exiting.\n", reqFile);
		return(ERROR);
	}

	if (macrostring && macrostring[0]) {
		macCreateHandle(&handle, NULL);
		if (handle) {
			macParseDefns(handle, macrostring, &pairs);
			if (pairs) macInstallMacros(handle, pairs);
			if (save_restoreDebug >= 5) {
				errlogPrintf("save_restore:readReqFile: Current macro definitions:\n");
				macReportMacros(handle);
				errlogPrintf("save_restore:readReqFile: --------------------------\n");
			}
		}
	}

	/* place all of the channels in the group */
	while (fgets(line, BUF_SIZE, inp_fd)) {
		/* Expand input line. */
		name[0] = '\0';
		eline[0] = '\0';
		if (handle && pairs) {
			macExpandString(handle, line, eline, BUF_SIZE-1);
		} else {
			strcpy(eline, line);
		}
		sscanf(eline, "%s", name);
		if (save_restoreDebug >= 2) errlogPrintf("save_restore:readReqFile: line='%s', eline='%s', name='%s'\n", line, eline, name);
		if (name[0] == '#') {
			/* take the line as a comment */
		} else if (strncmp(eline, "file", 4) == 0) {
			/* handle include file */
			if (save_restoreDebug >= 2) errlogPrintf("save_restore:readReqFile: preparing to include file: eline='%s'\n", eline);

			/* parse template-file name and fix obvious problems */
			templatefile[0] = '\0';
			t = &(eline[4]);
			while (isspace((int)(*t))) t++;  /* delete leading whitespace */
			if (*t == '"') t++;  /* delete leading quote */
			while (isspace((int)(*t))) t++;  /* delete any additional whitespace */
			/* copy to filename; terminate at whitespace or quote or comment */
			for (	i = 0;
					i<PATH_SIZE && !(isspace((int)(*t))) && (*t != '"') && (*t != '#');
					t++,i++) {
				templatefile[i] = *t;
			}
			templatefile[i] = 0;

			/* parse new macro string and fix obvious problems */
			for (i=0; *t && *t != '#'; t++) {
				if (isspace((int)(*t)) || *t == ',') {
					if (i>=3 && (new_macro[i-1] != ','))
						new_macro[i++] = ',';
				} else if (*t != '"') {
					new_macro[i++] = *t;
				}
			}
			new_macro[i] = 0;
			if (i && new_macro[i-1] == ',') new_macro[--i] = 0;
			if (i < 3) new_macro[0] = 0; /* if macro has less than 3 chars, punt */
			readReqFile(templatefile, plist, new_macro);
		} else if (isalpha((int)name[0]) || isdigit((int)name[0]) || name[0] == '$') {
			pchannel = (struct channel *)calloc(1,sizeof (struct channel));
			if (pchannel == (struct channel *)0) {
				plist->status = SR_STATUS_WARN;
				strcpy(plist->statusStr, "Can't alloc channel memory");
				if (plist->statusStr_chid && (ca_state(plist->statusStr_chid) == cs_conn)) {
					ca_put(DBR_STRING, plist->statusStr_chid, &plist->statusStr);
					ca_flush_io();
				}
				errlogPrintf("readReqFile: channel calloc failed");
			} else {
				/* add new element to the list */
#if BACKWARDS_LIST
				pchannel->pnext = plist->pchan_list;
				if (plist->pchan_list==NULL) plist->plast_chan = pchannel;
				plist->pchan_list = pchannel;
#else
				if (plist->plast_chan) {
					plist->plast_chan->pnext = pchannel;
				} else {
					plist->pchan_list = pchannel;
				}
				plist->plast_chan = pchannel;
#endif
				strcpy(pchannel->name, name);
				strcpy(pchannel->value,"Not Connected");
				pchannel->enum_val = -1;
				pchannel->max_elements = 0;
				pchannel->curr_elements = 0;
			}
		}
	}
	/* close file */
	fclose(inp_fd);
	if (handle) {
		macDeleteHandle(handle);
		if (pairs) free(pairs);
	}

	if (save_restoreDebug >= 1)
		errlogPrintf("save_restore:readReqFile: exit: reqFile='%s'.\n", reqFile);
	return(OK);
}


/*-------------------------------------------------------------------------------*/
/*** ioc-shell command registration (sheesh!) ***/

#define IOCSH_ARG		static const iocshArg
#define IOCSH_ARG_ARRAY	static const iocshArg * const
#define IOCSH_FUNCDEF	static const iocshFuncDef

/* int fdbrestore(char *filename); */
IOCSH_ARG       fdbrestore_Arg0    = {"filename",iocshArgString};
IOCSH_ARG_ARRAY fdbrestore_Args[1] = {&fdbrestore_Arg0};
IOCSH_FUNCDEF   fdbrestore_FuncDef = {"fdbrestore",1,fdbrestore_Args};
static void     fdbrestore_CallFunc(const iocshArgBuf *args) {fdbrestore(args[0].sval);}

/* int fdbrestoreX(char *filename); */
IOCSH_ARG       fdbrestoreX_Arg0    = {"filename",iocshArgString};
IOCSH_ARG_ARRAY fdbrestoreX_Args[1] = {&fdbrestoreX_Arg0};
IOCSH_FUNCDEF   fdbrestoreX_FuncDef = {"fdbrestoreX",1,fdbrestoreX_Args};
static void     fdbrestoreX_CallFunc(const iocshArgBuf *args) {fdbrestoreX(args[0].sval);}

/* int manual_save(char *request_file); */
IOCSH_ARG       manual_save_Arg0    = {"request file",iocshArgString};
IOCSH_ARG_ARRAY manual_save_Args[1] = {&manual_save_Arg0};
IOCSH_FUNCDEF   manual_save_FuncDef = {"manual_save",1,manual_save_Args};
static void     manual_save_CallFunc(const iocshArgBuf *args) {manual_save(args[0].sval);}
	
/* int set_savefile_name(char *filename, char *save_filename); */
IOCSH_ARG       set_savefile_name_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       set_savefile_name_Arg1    = {"save_filename",iocshArgString};
IOCSH_ARG_ARRAY set_savefile_name_Args[2] = {&set_savefile_name_Arg0,&set_savefile_name_Arg1};
IOCSH_FUNCDEF   set_savefile_name_FuncDef = {"set_savefile_name",2,set_savefile_name_Args};
static void     set_savefile_name_CallFunc(const iocshArgBuf *args) {set_savefile_name(args[0].sval,args[1].sval);}
	
/* int create_periodic_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       create_periodic_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_periodic_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       create_periodic_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY create_periodic_set_Args[3] = {&create_periodic_set_Arg0,&create_periodic_set_Arg1,&create_periodic_set_Arg2};
IOCSH_FUNCDEF   create_periodic_set_FuncDef = {"create_periodic_set",3,create_periodic_set_Args};
static void     create_periodic_set_CallFunc(const iocshArgBuf *args) {create_periodic_set(args[0].sval,args[1].ival,args[2].sval);}

/* int create_triggered_set(char *filename, char *trigger_channel, char *macrostring); */
IOCSH_ARG       create_triggered_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_triggered_set_Arg1    = {"trigger_channel",iocshArgString};
IOCSH_ARG       create_triggered_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY create_triggered_set_Args[3] = {&create_triggered_set_Arg0,&create_triggered_set_Arg1,&create_triggered_set_Arg2};
IOCSH_FUNCDEF   create_triggered_set_FuncDef = {"create_triggered_set",3,create_triggered_set_Args};
static void     create_triggered_set_CallFunc(const iocshArgBuf *args) {create_triggered_set(args[0].sval,args[1].sval,args[2].sval);}

/* int create_monitor_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       create_monitor_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_monitor_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       create_monitor_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY create_monitor_set_Args[3] = {&create_monitor_set_Arg0,&create_monitor_set_Arg1,&create_monitor_set_Arg2};
IOCSH_FUNCDEF   create_monitor_set_FuncDef = {"create_monitor_set",3,create_monitor_set_Args};
static void     create_monitor_set_CallFunc(const iocshArgBuf *args) {create_monitor_set(args[0].sval,args[1].ival,args[2].sval);}
	
/* int create_manual_set(char *filename, char *macrostring); */
IOCSH_ARG       create_manual_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       create_manual_set_Arg1    = {"macrostring",iocshArgString};
IOCSH_ARG_ARRAY create_manual_set_Args[2] = {&create_manual_set_Arg0,&create_manual_set_Arg1};
IOCSH_FUNCDEF   create_manual_set_FuncDef = {"create_manual_set",2,create_manual_set_Args};
static void     create_manual_set_CallFunc(const iocshArgBuf *args) {create_manual_set(args[0].sval,args[1].sval);}
	
/* void save_restoreShow(int verbose); */
IOCSH_ARG       save_restoreShow_Arg0    = {"verbose",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreShow_Args[1] = {&save_restoreShow_Arg0};
IOCSH_FUNCDEF   save_restoreShow_FuncDef = {"save_restoreShow",1,save_restoreShow_Args};
static void     save_restoreShow_CallFunc(const iocshArgBuf *args) {save_restoreShow(args[0].ival);}

/* int set_requestfile_path(char *path, char *pathsub); */
IOCSH_ARG       set_requestfile_path_Arg0    = {"path",iocshArgString};
IOCSH_ARG       set_requestfile_path_Arg1    = {"subpath",iocshArgString};
IOCSH_ARG_ARRAY set_requestfile_path_Args[2] = {&set_requestfile_path_Arg0,&set_requestfile_path_Arg1};
IOCSH_FUNCDEF   set_requestfile_path_FuncDef = {"set_requestfile_path",2,set_requestfile_path_Args};
static void     set_requestfile_path_CallFunc(const iocshArgBuf *args) {set_requestfile_path(args[0].sval,args[1].sval);}
	
/* int set_savefile_path(char *path, char *pathsub); */
IOCSH_ARG       set_savefile_path_Arg0    = {"path",iocshArgString};
IOCSH_ARG       set_savefile_path_Arg1    = {"subpath",iocshArgString};
IOCSH_ARG_ARRAY set_savefile_path_Args[2] = {&set_savefile_path_Arg0,&set_savefile_path_Arg1};
IOCSH_FUNCDEF   set_savefile_path_FuncDef = {"set_savefile_path",2,set_savefile_path_Args};
static void     set_savefile_path_CallFunc(const iocshArgBuf *args) {set_savefile_path(args[0].sval,args[1].sval);}
	
/* int set_saveTask_priority(int priority); */
IOCSH_ARG       set_saveTask_priority_Arg0    = {"priority",iocshArgInt};
IOCSH_ARG_ARRAY set_saveTask_priority_Args[1] = {&set_saveTask_priority_Arg0};
IOCSH_FUNCDEF   set_saveTask_priority_FuncDef = {"set_saveTask_priority",1,set_saveTask_priority_Args};
static void     set_saveTask_priority_CallFunc(const iocshArgBuf *args) {set_saveTask_priority(args[0].ival);}
	
/* void save_restoreSet_NFSHost(char *hostname, char *address); */
IOCSH_ARG       save_restoreSet_NFSHost_Arg0    = {"hostname",iocshArgString};
IOCSH_ARG       save_restoreSet_NFSHost_Arg1    = {"address",iocshArgString};
IOCSH_ARG_ARRAY save_restoreSet_NFSHost_Args[2] = {&save_restoreSet_NFSHost_Arg0,&save_restoreSet_NFSHost_Arg1};
IOCSH_FUNCDEF   save_restoreSet_NFSHost_FuncDef = {"save_restoreSet_NFSHost",2,save_restoreSet_NFSHost_Args};
static void     save_restoreSet_NFSHost_CallFunc(const iocshArgBuf *args) {save_restoreSet_NFSHost(args[0].sval,args[1].sval);}
	
/* int remove_data_set(char *filename); */
IOCSH_ARG       remove_data_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG_ARRAY remove_data_set_Args[1] = {&remove_data_set_Arg0};
IOCSH_FUNCDEF   remove_data_set_FuncDef = {"remove_data_set",1,remove_data_set_Args};
static void     remove_data_set_CallFunc(const iocshArgBuf *args) {remove_data_set(args[0].sval);}

/* int reload_periodic_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       reload_periodic_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_periodic_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       reload_periodic_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY reload_periodic_set_Args[3] = {&reload_periodic_set_Arg0, &reload_periodic_set_Arg1,&reload_periodic_set_Arg2};
IOCSH_FUNCDEF   reload_periodic_set_FuncDef = {"reload_periodic_set",3,reload_periodic_set_Args};
static void     reload_periodic_set_CallFunc(const iocshArgBuf *args) {reload_periodic_set(args[0].sval,args[1].ival,args[2].sval);}
	
/* int reload_triggered_set(char *filename, char *trigger_channel, char *macrostring); */
IOCSH_ARG       reload_triggered_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_triggered_set_Arg1    = {"trigger_channel",iocshArgString};
IOCSH_ARG       reload_triggered_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY reload_triggered_set_Args[3] = {&reload_triggered_set_Arg0, &reload_triggered_set_Arg1,&reload_triggered_set_Arg2};
IOCSH_FUNCDEF   reload_triggered_set_FuncDef = {"reload_triggered_set",3,reload_triggered_set_Args};
static void     reload_triggered_set_CallFunc(const iocshArgBuf *args) {reload_triggered_set(args[0].sval,args[1].sval,args[2].sval);}
	
/* int reload_monitor_set(char *filename, int period, char *macrostring); */
IOCSH_ARG       reload_monitor_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_monitor_set_Arg1    = {"period",iocshArgInt};
IOCSH_ARG       reload_monitor_set_Arg2    = {"macro string",iocshArgString};
IOCSH_ARG_ARRAY reload_monitor_set_Args[3] = {&reload_monitor_set_Arg0, &reload_monitor_set_Arg1,&reload_monitor_set_Arg2};
IOCSH_FUNCDEF   reload_monitor_set_FuncDef = {"reload_monitor_set",3,reload_monitor_set_Args};
static void     reload_monitor_set_CallFunc(const iocshArgBuf *args) {reload_monitor_set(args[0].sval,args[1].ival,args[2].sval);}

/* int reload_manual_set(char *filename, char *macrostring); */
IOCSH_ARG       reload_manual_set_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       reload_manual_set_Arg1    = {"macrostring",iocshArgString};
IOCSH_ARG_ARRAY reload_manual_set_Args[2] = {&reload_manual_set_Arg0,&reload_manual_set_Arg1};
IOCSH_FUNCDEF   reload_manual_set_FuncDef = {"reload_manual_set",2,reload_manual_set_Args};
static void     reload_manual_set_CallFunc(const iocshArgBuf *args) {reload_manual_set(args[0].sval,args[1].sval);}

/* int request_manual_restore(char *filename, int file_type); */
IOCSH_ARG       request_manual_restore_Arg0    = {"filename",iocshArgString};
IOCSH_ARG       request_manual_restore_Arg1    = {"file_type",iocshArgInt};
IOCSH_ARG_ARRAY request_manual_restore_Args[2] = {&request_manual_restore_Arg0,&request_manual_restore_Arg1};
IOCSH_FUNCDEF   request_manual_restore_FuncDef = {"request_manual_restore",2,request_manual_restore_Args};
static void     request_manual_restore_CallFunc(const iocshArgBuf *args) {request_manual_restore(args[0].sval,args[1].ival);}

/* void save_restoreSet_Debug(int level); */
IOCSH_ARG       save_restoreSet_Debug_Arg0    = {"level",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_Debug_Args[1] = {&save_restoreSet_Debug_Arg0};
IOCSH_FUNCDEF   save_restoreSet_Debug_FuncDef = {"save_restoreSet_Debug",1,save_restoreSet_Debug_Args};
static void     save_restoreSet_Debug_CallFunc(const iocshArgBuf *args) {save_restoreSet_Debug(args[0].ival);}

/* void save_restoreSet_NumSeqFiles(int numSeqFiles); */
IOCSH_ARG       save_restoreSet_NumSeqFiles_Arg0    = {"numSeqFiles",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_NumSeqFiles_Args[1] = {&save_restoreSet_NumSeqFiles_Arg0};
IOCSH_FUNCDEF   save_restoreSet_NumSeqFiles_FuncDef = {"save_restoreSet_NumSeqFiles",1,save_restoreSet_NumSeqFiles_Args};
static void     save_restoreSet_NumSeqFiles_CallFunc(const iocshArgBuf *args) {save_restoreSet_NumSeqFiles(args[0].ival);}
	
/* void save_restoreSet_SeqPeriodInSeconds(int period); */
IOCSH_ARG       save_restoreSet_SeqPeriodInSeconds_Arg0    = {"period",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_SeqPeriodInSeconds_Args[1] = {&save_restoreSet_SeqPeriodInSeconds_Arg0};
IOCSH_FUNCDEF   save_restoreSet_SeqPeriodInSeconds_FuncDef = {"save_restoreSet_SeqPeriodInSeconds",1,save_restoreSet_SeqPeriodInSeconds_Args};
static void     save_restoreSet_SeqPeriodInSeconds_CallFunc(const iocshArgBuf *args) {save_restoreSet_SeqPeriodInSeconds(args[0].ival);}

/* void save_restoreSet_IncompleteSetsOk(int ok); */
IOCSH_ARG       save_restoreSet_IncompleteSetsOk_Arg0    = {"ok",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_IncompleteSetsOk_Args[1] = {&save_restoreSet_IncompleteSetsOk_Arg0};
IOCSH_FUNCDEF   save_restoreSet_IncompleteSetsOk_FuncDef = {"save_restoreSet_IncompleteSetsOk",1,save_restoreSet_IncompleteSetsOk_Args};
static void     save_restoreSet_IncompleteSetsOk_CallFunc(const iocshArgBuf *args) {save_restoreSet_IncompleteSetsOk(args[0].ival);}

/* void save_restoreSet_DatedBackupFiles(int ok); */
IOCSH_ARG       save_restoreSet_DatedBackupFiles_Arg0    = {"ok",iocshArgInt};
IOCSH_ARG_ARRAY save_restoreSet_DatedBackupFiles_Args[1] = {&save_restoreSet_DatedBackupFiles_Arg0};
IOCSH_FUNCDEF   save_restoreSet_DatedBackupFiles_FuncDef = {"save_restoreSet_DatedBackupFiles",1,save_restoreSet_DatedBackupFiles_Args};
static void     save_restoreSet_DatedBackupFiles_CallFunc(const iocshArgBuf *args) {save_restoreSet_DatedBackupFiles(args[0].ival);}

/* void save_restoreSet_status_prefix(char *prefix); */
IOCSH_ARG       save_restoreSet_status_prefix_Arg0    = {"prefix",iocshArgString};
IOCSH_ARG_ARRAY save_restoreSet_status_prefix_Args[1] = {&save_restoreSet_status_prefix_Arg0};
IOCSH_FUNCDEF   save_restoreSet_status_prefix_FuncDef = {"save_restoreSet_status_prefix",1,save_restoreSet_status_prefix_Args};
static void     save_restoreSet_status_prefix_CallFunc(const iocshArgBuf *args) {save_restoreSet_status_prefix(args[0].sval);}


void save_restoreRegister(void)
{
    iocshRegister(&fdbrestore_FuncDef, fdbrestore_CallFunc);
    iocshRegister(&fdbrestoreX_FuncDef, fdbrestoreX_CallFunc);
    iocshRegister(&manual_save_FuncDef, manual_save_CallFunc);
    iocshRegister(&set_savefile_name_FuncDef, set_savefile_name_CallFunc);
    iocshRegister(&create_periodic_set_FuncDef, create_periodic_set_CallFunc);
    iocshRegister(&create_triggered_set_FuncDef, create_triggered_set_CallFunc);
    iocshRegister(&create_monitor_set_FuncDef, create_monitor_set_CallFunc);
    iocshRegister(&create_manual_set_FuncDef, create_manual_set_CallFunc);
    iocshRegister(&save_restoreShow_FuncDef, save_restoreShow_CallFunc);
    iocshRegister(&set_requestfile_path_FuncDef, set_requestfile_path_CallFunc);
    iocshRegister(&set_savefile_path_FuncDef, set_savefile_path_CallFunc);
    iocshRegister(&set_saveTask_priority_FuncDef, set_saveTask_priority_CallFunc);
    iocshRegister(&save_restoreSet_NFSHost_FuncDef, save_restoreSet_NFSHost_CallFunc);
    iocshRegister(&remove_data_set_FuncDef, remove_data_set_CallFunc);
    iocshRegister(&reload_periodic_set_FuncDef, reload_periodic_set_CallFunc);
    iocshRegister(&reload_triggered_set_FuncDef, reload_triggered_set_CallFunc);
    iocshRegister(&reload_monitor_set_FuncDef, reload_monitor_set_CallFunc);
    iocshRegister(&reload_manual_set_FuncDef, reload_manual_set_CallFunc);
    iocshRegister(&request_manual_restore_FuncDef, request_manual_restore_CallFunc);
    iocshRegister(&save_restoreSet_Debug_FuncDef, save_restoreSet_Debug_CallFunc);
    iocshRegister(&save_restoreSet_NumSeqFiles_FuncDef, save_restoreSet_NumSeqFiles_CallFunc);
    iocshRegister(&save_restoreSet_SeqPeriodInSeconds_FuncDef, save_restoreSet_SeqPeriodInSeconds_CallFunc);
    iocshRegister(&save_restoreSet_IncompleteSetsOk_FuncDef, save_restoreSet_IncompleteSetsOk_CallFunc);
    iocshRegister(&save_restoreSet_DatedBackupFiles_FuncDef, save_restoreSet_DatedBackupFiles_CallFunc);
    iocshRegister(&save_restoreSet_status_prefix_FuncDef, save_restoreSet_status_prefix_CallFunc);
}

epicsExportRegistrar(save_restoreRegister);
