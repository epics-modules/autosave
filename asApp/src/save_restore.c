/* WORK TO DO:::::::
 * Set filename - explicit - based on time
 * adjustable priority of the save/restore task.
 * disable/remove requests
 */

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
 */

#include	<stdioLib.h>
#include	<taskLib.h>
#include	<wdLib.h>
#include	<nfsDrv.h>
#include	<ioLib.h>
#include	<string.h>
#include	<ctype.h>
extern int	logMsg (char *fmt, ...);
#include	<tickLib.h>
#include	<usrLib.h>

#include	"dbDefs.h"
#include	"cadef.h"

/*** Debugging variables, macros ***/
/* #define NODEBUG */
#ifdef NODEBUG
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V) {  if (l <= save_restoreDebug) \
			{ printf("%s(%d):",__FILE__,__LINE__); printf(FMT,V); } }
#endif
volatile int    save_restoreDebug = 0;

volatile int save_restore_test_fopen = 0;

#define FLOAT_FMT "%.7g"
#define DOUBLE_FMT "%.14g"

/*
 * data structure definitions 
 */
struct chlist 		*lptr;		/* save set listhead */
static SEM_ID 		sr_mutex;	/* mut(ual) ex(clusion) for list of save sets */

/* save_methods - used to determine when a file should be deleted */
#define PERIODIC	0x01		/* set when timer goes off */
#define TRIGGERED	0x02		/* set when trigger channel goes off */
#define	TIMER		0x04
#define	CHANGE		0x08
#define MONITORED	(TIMER|CHANGE)	/* set when timer expires and channel changes */
#define MANUAL		0x10		/* set on request */
#define	SINGLE_EVENTS	(PERIODIC|TRIGGERED|MANUAL)

struct chlist {							/* save set list element */
	struct chlist	*pnext;				/* next list */
	struct channel	*pchan_list;		/* channel list head */
	char 			filename[80];		/* configuration file name */
	char 			last_save_file[80];	/* file name last used for save */
	char			save_file[80];		/* file name to use on next save */
	int				save_method;		/* bit for each save method requested */
	int				enabled_method;		/* bit for each save method enabled */
	int				save_status;		/* status of this save set 	*/
	int				period;			/* time between saves (max for on change) */
	int				monitor_period;	/* time between saves (max for on change) */
	char			trigger_channel[40];	/* db channel to trigger save  */
	WDOG_ID			saveWdId;
	int				not_connected;		/* # of bad channels - not successfully saved/connected */
};

struct channel {					/* database channel list element */
	struct channel *pnext;			/* next channel */
	char		name[64];		/* channel name */
	chid 		chid;			/* channel access id */
	char 		value[64];		/* value string */
	short		enum_val;		/* short value of an enumerated field */
	short		valid;			/* we think we got valid data for this channel */
};

/*
 * module global variables
 */
static short		save_restore_init = 0;
static char 		*sr_version = {"save/restore V2.2"};

/* configuration parameters */
double	min_period	= 4.0;	/* save no more frequently than every 4 seconds */
double	min_delay	= 1.0;	/* check need to save every 1 second */
				/* worst case wait can be min_period + min_delay */
unsigned int	sr_save_incomplete_sets_ok = 1;		/* will save incomplete sets to disk */
unsigned int	sr_restore_incomplete_sets_ok = 1;	/* will restore incomplete sets from disk */

/* functions */
int periodic_save(struct chlist	*plist);
void triggered_save(struct event_handler_args);
int on_change_timer(struct chlist *plist);
void on_change_save(struct event_handler_args);
int manual_save(char *config_file);
int save_restore(void);
int connect_list(struct channel	*pchannel);
int enable_list(struct chlist *plist);
int get_channel_list(struct channel *pchannel);
int save_file(struct chlist	*plist);
int set_savefile_name(char *filename, char *save_filename);
int create_periodic_set(char *filename, double period);
int create_triggered_set(char *filename, char *trigger_channel);
int create_monitor_set(char *filename, double period);
int create_manual_set(char *filename);
int create_data_set(char *filename,	int save_method, double period,
		char *trigger_channel, double mon_period);
int fdbrestore(char *filename);
void fdblist(void);

/*
 * periodic save event - watch dog timer went off
 */
int periodic_save(struct chlist	*plist)
{
	plist->save_status |= PERIODIC;
	return(0);
}


/*
 * triggered save event - watch dog timer went off
 */
void triggered_save(struct event_handler_args event)
{
    struct chlist *plist = (struct chlist *) event.usr;

    if (event.dbr)
	plist->save_status |= TRIGGERED;
}


/*
 * on change channel event - one of the channels changed
 */
int on_change_timer(struct chlist *plist)
{
	if (save_restoreDebug >= 10) logMsg("on_change_timer for %s (period is %d ticks)\n",
			plist->filename, plist->monitor_period);
	plist->save_status |= TIMER;
	return(0);
}


/*
 * on change channel event - one of the channels changed
 * get all channels; write to disk; restart timer
 */
void on_change_save(struct event_handler_args event)
{
    struct chlist *plist;
	if (save_restoreDebug >= 10) logMsg("on_change_save: event = 0x%x, event.usr=0x%x\n",
		event, event.usr);
    plist = (struct chlist *) event.usr;

    plist->save_status |= CHANGE;
}


/*
 * manual_save - save routine
 *
 */
int manual_save(char *config_file)
{
	struct chlist	*plist;

	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while ((plist != 0) && strcmp(plist->filename,config_file)) {
		plist = plist->pnext;
	}
	if (plist != 0)
		plist->save_status |= MANUAL;
	else
		logMsg("saveset %s not found", config_file);
	semGive(sr_mutex);
	return(0);
}


/*
 * save_restore task
 *
 * all requests are flagged in the common data store -
 * they are satisfied here - with all channel access contained within
 */
int save_restore(void)
{
    struct chlist	*plist;

    while(1) {

	/* look at each list */
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while (plist != 0) {
		Debug(30, "save_restore: save_status = 0x%x\n", plist->save_status);
		/* connect the channels on the first instance of this set */
		if (plist->enabled_method == 0) {
			plist->not_connected = connect_list(plist->pchan_list);
			enable_list(plist);
		}

		/* save files that have triggered */
		if ( (plist->save_status & SINGLE_EVENTS)
		  || ((plist->save_status & MONITORED) == MONITORED) ) {

			/* fetch all of the channels */
			plist->not_connected = get_channel_list(plist->pchan_list);

			/* write the data to disk */
			if ((plist->not_connected == 0) || (sr_save_incomplete_sets_ok))
				save_file(plist);

		}

		/* restart timers and reset save requests */
		if (plist->save_status & PERIODIC) {
			if (wdStart(plist->saveWdId, plist->period, periodic_save, (int)plist) < 0) {
				logMsg("could not add %s to the period scan", plist->filename);
			}
		}
		if (plist->save_status & SINGLE_EVENTS) {
			plist->save_status = plist->save_status & ~SINGLE_EVENTS;
		}
		if ((plist->save_status & MONITORED) == MONITORED) {
			if(wdStart(plist->saveWdId, plist->monitor_period, on_change_timer, (int)plist)
			  < 0 ) {
				logMsg("could not add %s to the period scan", plist->filename);
			}
			plist->save_status = plist->save_status & ~MONITORED;
		}

		/* next list */
		plist = plist->pnext;
	}

	/* release the list */
	semGive(sr_mutex);

	/* go to sleep for a while */
	ca_pend_event(min_delay);
    }
	return(0);
}
	

/*
 * connect all of the channels in a save set
 *
 * NOTE: Assumes that the sr_mutex is taken
 */
int connect_list(struct channel	*pchannel)
{
	struct channel	*sav_pchannel = pchannel;
	int		errors = 0;
	short		num_channels = 0;
	float fval;
	double dval;

	/* connect all channels in the list */
	while (pchannel != 0) {
		if (ca_search(pchannel->name,&pchannel->chid) == ECA_NORMAL) {
			strcpy(pchannel->value,"Search Issued");
		} else {
			strcpy(pchannel->value,"Search Not Issued");
		}
		pchannel = pchannel->pnext;
	}
	if (ca_pend_io(2.0) == ECA_TIMEOUT) {
		logMsg("not all searches successful\n");
	}
		
	/* fetch all channels in the list */
	for (pchannel = sav_pchannel; pchannel != 0; pchannel = pchannel->pnext) {
		if (ca_state(pchannel->chid) == cs_conn) {
			num_channels++;
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,&fval);
				sprintf(pchannel->value, FLOAT_FMT, fval);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,&dval);
				sprintf(pchannel->value, DOUBLE_FMT, dval);
			} else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			if (ca_field_type(pchannel->chid) == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
		} else {
			errors++;
			logMsg("%s is not connected\n",pchannel->name);
		}

		if (ca_pend_io(num_channels*0.1) == ECA_TIMEOUT) {
			logMsg("create_data_set: not all channels initialized\n");
			errors++;
		}
	}
	return(errors);
}


/*
 * enable new save methods
 *
 * NOTE: Assumes the sr_mutex is taken
 */
int enable_list(struct chlist *plist)
{
	struct channel	*pchannel;
	chid 		chid;			/* channel access id */

	Debug(4, "enable_list: entry\n", 0);
	/* enable a periodic set */
	if ((plist->save_method & PERIODIC) && !(plist->enabled_method & PERIODIC)) {
		if (wdStart(plist->saveWdId, plist->period, periodic_save, (int)plist) < 0) {
			logMsg("could not add %s to the period scan", plist->filename);
		}
		plist->enabled_method |= PERIODIC;
	}

	/* enable a triggered set */
	if ((plist->save_method & TRIGGERED) && !(plist->enabled_method & TRIGGERED)) {
		if (ca_search(plist->trigger_channel, &chid) != ECA_NORMAL) {
			logMsg("trigger %s search failed\n", plist->trigger_channel);
		} else if (ca_pend_io(2.0) != ECA_NORMAL) {
			logMsg("timeout on search of %s\n", plist->trigger_channel);
		} else if (ca_state(chid) != cs_conn) {
			logMsg("trigger %s search not connected\n", plist->trigger_channel);
		} else if (ca_add_event(DBR_FLOAT, chid, triggered_save, (void *)plist, 0) !=ECA_NORMAL) {
			logMsg("trigger event for %s failed\n", plist->trigger_channel);
		} else{
			plist->enabled_method |= TRIGGERED;
		}
	}

	/* enable a monitored set */
	if ((plist->save_method & MONITORED) && !(plist->enabled_method & MONITORED)) {
		for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
			Debug(10, "enable_list: calling ca_add_event for '%s'\n", pchannel->name);
			if (save_restoreDebug >= 10) logMsg("enable_list: arg = 0x%x\n", plist);
			/*
			 * Work around obscure problem affecting USHORTS by making DBR type different from any
			 * possible field type.  This avoids tickling a bug that causes dbGet to overwrite the
			 * source field with its own value converted to LONG.  (Changed DBR_LONG to DBR_TIME_LONG.)
			 */
			if (ca_add_event(DBR_TIME_LONG, pchannel->chid, on_change_save, (void *)plist, 0) != ECA_NORMAL) {
				logMsg("could not add event for %s in %s\n",pchannel->name,plist->filename);
			}
		}
		Debug(4 ,"enable_list: done calling ca_add_event for list channels\n",0);
		if (ca_pend_io(1.0) != ECA_NORMAL) {
			logMsg("timeout on monitored set: %s to monitored scan\n",plist->filename);
		}
		if (wdStart(plist->saveWdId, plist->monitor_period, on_change_timer, (int)plist) < 0) {
			logMsg("watchdog for set %s not started\n",plist->filename);
		}
		plist->enabled_method |= MONITORED;
	}

	/* enable a manual request set */
	if ((plist->save_method & MANUAL) && !(plist->enabled_method & MANUAL)) {
		plist->enabled_method |= MANUAL;
	}
	return(0);
}


/*
 * fetch all channels in the save set
 *
 * NOTE: Assumes sr_mutex is taken
 */
#define INIT_STRING "!@#$%^&*()"
int get_channel_list(struct channel *pchannel)
{
	struct channel 	*tpchannel = pchannel;
	int	not_connected = 0;
	unsigned short	num_channels = 0;
	float fval;
	double dval;

	/* attempt to fetch all channels that are connected */
	while (pchannel != 0) {
		pchannel->valid = 0;
		if (ca_state(pchannel->chid) == cs_conn) {
			strcpy(pchannel->value, INIT_STRING);
			if (ca_field_type(pchannel->chid) == DBF_FLOAT) {
				ca_array_get(DBR_FLOAT,1,pchannel->chid,&fval);
				sprintf(pchannel->value, FLOAT_FMT, fval);
			} else if (ca_field_type(pchannel->chid) == DBF_DOUBLE) {
				ca_array_get(DBR_DOUBLE,1,pchannel->chid,&dval);
				sprintf(pchannel->value, DOUBLE_FMT, dval);
			}
			else {
				ca_array_get(DBR_STRING,1,pchannel->chid,pchannel->value);
			}
			num_channels++;
			if (ca_field_type(pchannel->chid) == DBF_ENUM) {
				ca_array_get(DBR_SHORT,1,pchannel->chid,&pchannel->enum_val);
				num_channels++;
			}
			pchannel->valid = 1;
		}else{
			not_connected++;
		}
		pchannel = pchannel->pnext;
	}
	if (ca_pend_io(.1*num_channels) != ECA_NORMAL) {
		logMsg("get_channel_list: not all gets completed");
		not_connected++;
	}

	/* check to see which gets completed */
	for (pchannel=tpchannel; pchannel; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			/* then we at least had a CA connection.  Did it produce? */
			pchannel->valid = strcmp(pchannel->value, INIT_STRING);
		}
	}

	return(not_connected);
}

/* Actually write the file
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
int write_it(char *filename, struct chlist	*plist)
{
	FILE *out_fd;
	struct channel	*pchannel;

	if ((out_fd = fopen(filename,"w")) == NULL) {
		logMsg("save_file - unable to open file %s", filename);
		return (-1);
	}
	fprintf(out_fd,"# %s\tAutomatically generated - DO NOT MODIFY\n", sr_version);
	if (plist->not_connected) {
		fprintf(out_fd,"! %d channel(s) not connected - or not all gets were successful\n",
			plist->not_connected);
	}
	for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
		if (pchannel->valid) {
			fprintf(out_fd, "%s ", pchannel->name);
		} else {
			fprintf(out_fd, "#%s ", pchannel->name);
		}
		if (pchannel->enum_val >= 0) {
			fprintf(out_fd, "%d\n",pchannel->enum_val);
		} else {
			fprintf(out_fd, "%-s\n", pchannel->value);
		}
	}
	if (save_restoreDebug==999) {
		logMsg("save_restore: simulating task crash.  Bye, bye!\n");
		exit(-1);
	}
	fprintf(out_fd, "<END>\n");
	fflush(out_fd);
	ioctl(fileno(out_fd),FIOSYNC,0);	/* NFS flush needs a little extra push */
	fclose(out_fd);
	return(0);
}


/* state of backup restore file */
#define BS_BAD 	0
#define BS_OK	1
#define BS_NEW	2

/*
 * save_file - save routine
 *
 * Write a list of channel names and values to an ASCII file.
 *
 * NOTE: Assumes that the sr_mutex is taken!!!!!!!!!!
 */
int save_file(struct chlist	*plist)
{
	FILE	*inp_fd, *out_fd;
	char	save_file_backup[80], tmpstr[20], tmpfile[80];
	int		backup_state = BS_OK, cant_open=0;

	if (save_restore_test_fopen) {
		/* Ensure that we can create a new file in the current directory */
		(void)tmpnam(tmpstr);
		strcpy(tmpfile, "save_restore_"); strcat(tmpfile, tmpstr);
		if ((out_fd = fopen(tmpfile, "w")) == NULL) {
			cant_open = 1;
			logMsg("save_file:  Can't open new file in current directory.\n");
		} else {
			fclose(out_fd);
			remove(tmpfile);
			if (save_restoreDebug) printf("fopen('%s') succeeded\n", tmpfile);
		}
	}

	/*
	 * Check out the backup copy of the save file before we blow away the save file.
	 * If the backup isn't ok, write a new one.
	 */
	strncpy(save_file_backup, plist->save_file, 78);
	strncat(save_file_backup, "B", 1);
	if ((inp_fd = fopen(save_file_backup, "r")) == NULL) {
		backup_state = BS_BAD;
	} else {
		if ((fseek(inp_fd, -6, SEEK_END)) ||
			(fgets(tmpstr, 6, inp_fd) == 0) ||
			(strncmp(tmpstr, "<END>", 5) != 0)) {
				printf("tmpstr = '%s'\n", tmpstr);
				backup_state = BS_BAD;
		}
		fclose(inp_fd);
	}
	if (backup_state == BS_BAD) {
		logMsg("save_file: Backup file (%s) is bad.  Writing a new one.\n", 
			save_file_backup);
		if (write_it(save_file_backup, plist) == -1) return(-1);
		backup_state = BS_NEW;
	}

	/*** Write the save file ***/
	Debug(1, "save_file: saving to %s\n", plist->save_file);
	if (write_it(plist->save_file, plist) == -1) return(-1);

	if (cant_open) {
		logMsg("save_file:  Used existing file for autosave,\n");
		logMsg("...but this file is vulnerable to corruption\n");
		logMsg("...because I can't change its length.\n");
	}

	/* keep the name of the last saved file */
	strcpy(plist->last_save_file,plist->save_file);		

	/*** Write a backup copy of the save file ***/
	if (backup_state != BS_NEW) {
		/* make a backup copy */
		strncpy(save_file_backup, plist->save_file, 78);
		strncat(save_file_backup, "B", 1);
		copy(plist->save_file, save_file_backup);
	}
	return(0);
}


int set_savefile_name(char *filename, char *save_filename)
{
	struct chlist	*plist;

	/* is save set defined - add new save mode if necessary */
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->filename,filename)) {
			strcpy(plist->save_file,save_filename);
			semGive(sr_mutex);
			return(0);
		}
		plist = plist->pnext;
	}
	logMsg("No save set enabled for %s\n",filename);
	semGive(sr_mutex);
	return(-1);
}


int create_periodic_set(char *filename, double period)
{
	return (create_data_set(filename, PERIODIC, period, 0, 0.0));
}


int create_triggered_set(char *filename, char *trigger_channel)
{
	if (trigger_channel && (isalpha(trigger_channel[0]) || isdigit(trigger_channel[0]))) {
		return (create_data_set(filename, TRIGGERED, 0.0, trigger_channel, 0.0));
	}
	else {
		logMsg("create_triggered_set: Error: trigger-channel name is required.\n");
		return(-1);
	}
}


int create_monitor_set(char *filename, double period)
{
	return (create_data_set(filename, MONITORED, 0.0, 0, period));
}


int create_manual_set(char *filename)
{
	return (create_data_set(filename, MANUAL, 0.0, 0, 0.0) );
}


/*
 * create a data set
 */
int create_data_set(
	char	*filename,			/* save set configuration file */
	int		save_method,
	double	period,				/* maximum time between saves  */
	char	*trigger_channel,	/* db channel to trigger save  */
	double	mon_period			/* minimum time between saves  */
)
{
	struct chlist	*plist;
	struct channel	*pchannel;
	int		inx;			/* i/o status 	       */
	FILE   		*inp_fd;
	char		temp[80];
	char		inchar;
	ULONG		ticks;

	if (save_restoreDebug) {
		logMsg("create_data_set: file '%s', method %x, period %d, trig_chan '%s', mon_period %f\n",
			filename, save_method, period, trigger_channel ? trigger_channel : "NONE", mon_period);
	}

	/* initialize save_restore routines */
	if (!save_restore_init) {
		if ((sr_mutex = semMCreate(SEM_Q_FIFO)) == 0) {
			logMsg("create_data_set: could not create list header mutex");
			return(-1);
		}
		if(taskSpawn("save_restore",190,VX_FP_TASK,10000,save_restore,0,0,0,0,0,0,0,0,0,0)
		  == ERROR) {
			logMsg("create_data_set: could not create save_restore task");
			return(-1);
		}
		save_restore_init = 1;
	}

	/* is save set defined - add new save mode if necessary */
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while (plist != 0) {
		if (!strcmp(plist->filename,filename)) {
			if (plist->save_method & save_method) {
				semGive(sr_mutex);
				logMsg("create_data_set: %s in %x mode",filename,save_method);
				return (-1);
			}else{
				if (save_method == TRIGGERED)
					if (trigger_channel) {
						strcpy(plist->trigger_channel,trigger_channel);
					} else {
						logMsg("create_data_set: no trigger channel");
						return(-1);
					}
				else if (save_method == PERIODIC)
					plist->period = (min_period > period)?
					  min_period*vxTicksPerSecond : period*vxTicksPerSecond;
				else if (save_method == MONITORED)
					plist->monitor_period = (min_period > mon_period)?
	 				  min_period*vxTicksPerSecond : mon_period*vxTicksPerSecond;
				plist->save_method |= save_method;
				enable_list(plist);

				semGive(sr_mutex);
				return (0);
			}
		}
		plist = plist->pnext;
	}
	semGive(sr_mutex);

	/* create a new channel list */
	if ((plist = (struct chlist *)calloc(1,sizeof (struct chlist)))
	  == (struct chlist *)0) {
		logMsg("create_data_set: channel list calloc failed");
		return(-1);
	}
	if ((plist->saveWdId = wdCreate()) == 0) {
		logMsg("create_data_set: could not create watchdog");
		return(-1);
	}
	strcpy(plist->filename,filename);
	plist->pchan_list = (struct channel *)0;
	plist->period = (min_period > period)?min_period*vxTicksPerSecond : period*vxTicksPerSecond;
	if (trigger_channel) strcpy(plist->trigger_channel,trigger_channel);
	plist->last_save_file[0] = 0;
	plist->save_method = save_method;
	plist->enabled_method = 0;
	plist->save_status = 0;
	plist->monitor_period = (min_period > mon_period)?
	  min_period*vxTicksPerSecond : mon_period*vxTicksPerSecond;
	/* save_file name */
	strcpy(plist->save_file,plist->filename);
	inx = 0;
	while ((plist->save_file[inx] != 0) && (plist->save_file[inx] != '.') && (inx < 74)) inx++;
	plist->save_file[inx] = 0;
	strcat(plist->save_file,".sav");

	/* open save configuration file */
	if ((inp_fd = fopen(filename, "r")) == NULL) {
		logMsg("create_data_set: unable to open file %s\n",filename);
		free(plist);
		return (-1);
	}

	/* place all of the channels in the group */
	while (fscanf(inp_fd,"%s",temp) != EOF) {

		/* put all legal channel names into the list */
		if (isalpha(temp[0]) || isdigit(temp[0])) {
			if ((pchannel = (struct channel *)calloc(1,sizeof (struct channel)))
			  == (struct channel *)0) {
				logMsg("create_data_set: channel calloc failed");
			}else{
				/* add it to the list */
				pchannel->pnext = plist->pchan_list;
				plist->pchan_list = pchannel;
				strcpy(pchannel->name,temp);
				strcpy(pchannel->value,"Not Connected");
				pchannel->enum_val = -1;
			}
		}

		/* skip to the end of this line */
		inchar = fgetc(inp_fd);
		while ((inchar != '\n') && (inchar != EOF)) inchar = fgetc(inp_fd);
	}
	/* close file */
	fclose(inp_fd);

	/* link it to the save set list */
	semTake(sr_mutex,WAIT_FOREVER);
	plist->pnext = lptr;
	lptr = plist;
	semGive(sr_mutex);

	/* this should be absolute time as a string */
	ticks = tickGet();

	return(0);
}


/*
 * fdbrestore - restore routine
 *
 * Read a list of channel names and values from an ASCII file,
 * and update database vaules.
 *
 */
int fdbrestore(char *filename)
{	
	struct channel	*pchannel;
	struct chlist	*plist;
	int		found;
	char	channel[80];
	char	bu_filename[120];
	char	fname[120];
	char	buffer[120], *bp, c;
	char	input_line[120];
	int		n;
	FILE	*inp_fd;
	chid	chanid;
	unsigned short	i, j, try_backup=0;

	/* if this is the current file name for a save set - restore from there */
	found = FALSE;
	semTake(sr_mutex,WAIT_FOREVER);
	plist = lptr;
	while ((plist != 0) && !found) { 
		if (strcmp(plist->last_save_file,filename) == 0) {
			found = TRUE;
		}else{
			plist = plist->pnext;
		}
	}
	if (found ) {
		/* verify quality of the save set */
		if (plist->not_connected > 0) {
			logMsg("%d Channel(s) not connected or fetched\n",plist->not_connected);
			if (!sr_restore_incomplete_sets_ok) {
				logMsg("aborting restore\n");
				return(-1);
			}
		}

		for (pchannel = plist->pchan_list; pchannel !=0; pchannel = pchannel->pnext) {
			ca_put(DBR_STRING,pchannel->chid,pchannel->value);
		}
		if (ca_pend_io(1.0) != ECA_NORMAL) {
			logMsg("fdbrestore: not all channels restored\n");
		}
		semGive(sr_mutex);
		return 0;
	}
	semGive(sr_mutex);

	/* open file */
	strncpy(fname, filename, 118);
	if ((inp_fd = fopen(fname, "r")) == NULL) {
		logMsg("fdbrestore - unable to open file %s.\n", fname);
		try_backup=1;
	} else {
		/* check out "successfully written" marker */
		c = fgetc(inp_fd);
		if (c != '#') {
			/* file write did not complete */
			logMsg("fdbrestore - file '%s' is incomplete.\n", fname);
			try_backup=1;
		}
		fclose(inp_fd);
	}

	if (try_backup) {
		/* try the backup file */
		strncat(fname, "B", 1);
		logMsg("fdbrestore - trying backup file '%s'\n", fname);
		if ((inp_fd = fopen(fname, "r")) == NULL) {
			logMsg("fdbrestore - unable to open file '%s'\n", fname);
			return -1;
		} else {
			c = fgetc(inp_fd);
			if (c != '#') {
				logMsg("fdbrestore - file '%s' is incomplete.  Nothing done.\n",
					fname);
				fclose(inp_fd);
				return -1;
			}
		}
	}

	(void)fgets(buffer, 120, inp_fd); /* discard header line */
	/* restore from data file */
	while ((bp=fgets(buffer, 120, inp_fd))) {
		/* get PV_name, one space character, value */
		/* (value may be a string with leading whitespace; it may be */
		/* entirely whitespace; the number of spaces may be crucial; */
		/* it might also consist of zero characters) */
		n = sscanf(bp,"%s%c%[^\n]", channel, &c, input_line);
		if (n < 3) *input_line = 0;
		if (isalpha(channel[0]) || isdigit(channel[0])) {
			if (ca_search(channel, &chanid) != ECA_NORMAL) {
				logMsg("ca_search for %s failed\n", channel);
			} else if (ca_pend_io(0.5) != ECA_NORMAL) {
				logMsg("ca_search for %s timeout\n", channel);
			} else if (ca_put(DBR_STRING, chanid, input_line) != ECA_NORMAL) {
				logMsg("ca_put of %s to %s failed\n", input_line,channel);
			}
		} else if (channel[0] == '!') {
			for (i = 0; isspace(input_line[i]); i++);	/* skip blanks */
			for (j = 0; isdigit(input_line[i]); i++, j++)
				channel[j] = input_line[i];
			channel[j] = 0;
			logMsg("%s channels not connected / fetch failed", channel);
			if (!sr_restore_incomplete_sets_ok) {
				logMsg("aborting restore\n");
				fclose(inp_fd);
				return(-1);
			}
		}
	}
	fclose(inp_fd);

	/* make  backup */
	strcpy(bu_filename,filename);
	strcat(bu_filename,".bu");		/* this may want to be the date */
	copy(fname,bu_filename);

	return(0);
}


/*
 * fdblist -  list save sets
 *
 */
void fdblist(void)
{
	struct chlist	*plist;
	struct channel 	*pchannel;

	printf("fdblist\n");
	semTake(sr_mutex,WAIT_FOREVER);
	for (plist = lptr; plist != 0; plist = plist->pnext) {
		printf("%s: \n",plist->filename);
		printf("  last saved file - %s\n",plist->last_save_file);
		printf("  method %x period %d trigger chan %s monitor period %d\n",
		   plist->save_method,plist->period,plist->trigger_channel,plist->monitor_period);
		printf("  %d channels not connected - or failed gets\n",plist->not_connected);
		for (pchannel = plist->pchan_list; pchannel != 0; pchannel = pchannel->pnext) {
			printf("\t%s\t%s",pchannel->name,pchannel->value);
			if (pchannel->enum_val >= 0) printf("\t%d\n",pchannel->enum_val);
			else printf("\n");
		}
	}
	semGive(sr_mutex);
}
