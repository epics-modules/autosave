/* asVerify.c  -- given the name of an autosave .sav file, and access
 * to the running ioc whose PV's have been saved in the file, verify that
 * values in the .sav file agree with live values.
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h> /* isalpha */
#include <math.h> /* fabs */

#include "cadef.h"

#define FSMALL 1.e-6
#define DSMALL 1.e-8
#define BUF_SIZE 10000
#define ARRAY_MARKER "@array@"
#define ARRAY_MARKER_LEN 7
#define FLOAT_FMT "%.7g"
#define DOUBLE_FMT "%.14g"
#define WRITE_HEADER if (!wrote_head) {printf("PVname (type) saved_value live_value\n"); \
printf("====================================\n"); wrote_head=1;}
#define PEND_TIME 30.0
#define		ASVERSION "asVerify V1.0"

int main(int argc,char **argv)
{
	float	fvalue;
	double	dvalue;
	short	enum_value;
	chid	chid;
	FILE	*fp, *ftmp, *fr=NULL;
	char	c, s[BUF_SIZE], *bp, PVname[70], value_string[100], svalue[100], filename[300], restore_filename[300], *tempname;
	short	field_type;
	int		n, is_scalar, numPVs, numDifferences, numPVsNotConnected, verbose = 0, different, wrote_head=0, status, file_ok=0;
	int	restore=0;

	if (argc == 1) {
		fprintf(stderr,"usage: asVerify [-vr] <autosave_file>\n");
		fprintf(stderr,"         -v (verbose) causes all PV's to be printed out\n");
		fprintf(stderr,"             Otherwise, only PV's whose values differ are printed.\n");
		fprintf(stderr,"         -r (restore_file) causes a restore file named\n");
		fprintf(stderr,"            '<autosave_file>.as' to be written.\n");
		fprintf(stderr,"         -rv (or -vr) does both\n");
		fprintf(stderr,"example: asVerify -v auto_settings.sav\n");
		fprintf(stderr,"NOTE: Don't specify a .req file as the autosave file.\n");
		fprintf(stderr,"This program is not smart enough to parse a request file.\n");
		exit(1);
	}
	if (*argv[1] == '-') {
		for (n=1; n<strlen(argv[1]); n++) {
			if (argv[1][n] == 'v') verbose = 1;
			if (argv[1][n] == 'r') restore = 1;
		}
		strcpy(filename, argv[2]);
	} else {
		strcpy(filename, argv[1]);
	}
	SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
	if (restore) {
		strcpy(restore_filename, filename);
		strcat(restore_filename, ".as");
	}


	/*
	 * Copy to temporary file to minimize the likelihood that .sav file will be overwritten
	 * while we're using it.
	 */
	fp = fopen(filename,"r");
	if (fp == NULL) {
		printf("Can't open %s\n", filename);
		return(-1);
	}
	tempname = tmpnam(NULL);
	ftmp = fopen(tempname,"w");
	if (ftmp == NULL) {
		printf("Can't open temp file.\n");
		return(-1);
	}
	while (!feof(fp) && (n=fread(s,1,BUF_SIZE,fp))) {
		fwrite(s,1,n,ftmp);
	}
	fclose(fp);
	fclose(ftmp);

	fp = fopen(tempname,"r");
	if (fp == NULL) {
		printf("Can't open copy of %s.\n", filename);
		return(-1);
	}
	if (restore) {
		fr = fopen(restore_filename,"w");
		if (fr == NULL) {
			printf("Can't open restore_file '%s' for writing.\n", restore_filename);
			restore = 0;
		} else {
			fprintf(fr,"# %s\tAutomatically generated - DO NOT MODIFY - datetime\n", ASVERSION);
		}
	}
	/* check that file is good */
	status = fseek(fp, -6, SEEK_END);
	fgets(s, 6, fp);
	if (strncmp(s, "<END>", 5) == 0) file_ok = 1;
	if (!file_ok) {
		status = fseek(fp, -7, SEEK_END);
		fgets(s, 7, fp);
		if (strncmp(s, "<END>", 5) == 0) file_ok = 1;
	}
	if (status || !file_ok) {
		printf("Can't find <END> marker.  File may be bad.\n");
		if (restore) fprintf(fr,"# # # Could not find end marker in original .sav file.  File may be bad.\n");
	}
	status = fseek(fp, 0, SEEK_SET); /* file is ok.  go to beginning */
	if (status) printf("Can't go back to beginning of file.");

	bp = fgets(s, BUF_SIZE, fp); /* skip over header line */
	numDifferences = numPVs = numPVsNotConnected = 0;

	while ((bp=fgets(s, BUF_SIZE, fp))) {
		n = sscanf(bp,"%80s%c%[^\n\r]", PVname, &c, value_string);
		if (n<3) *value_string = 0;
		if (PVname[0] == '#') { /* Normally indicates a PV to which autosave could not connect. */
			if (strncmp(value_string, "Search Issued", 13) == 0) numPVsNotConnected++;
			if (restore) fprintf(fr, "%s", bp);
			continue;
		}
		if (strlen(PVname) >= 80) /* munged input line */
			continue;
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			/* if (strchr(PVname,'.') == 0) strcat(PVname,".VAL");*/ /* if no field name, add default */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (!is_scalar) {
				printf("%s is an array.  Not yet implemented.\n", PVname);
				continue;
			}
			SEVCHK(ca_create_channel(PVname,NULL,NULL,10,&chid),"ca_create_channel failure");
			SEVCHK(ca_pend_io(PEND_TIME),"ca_pend_io failure");
			if ((chid == NULL) || (ca_state(chid) != cs_conn)) {
				printf("%s not connected.  Saved value='%s'\n", PVname, value_string);
				numPVsNotConnected++;
				if (chid) ca_clear_channel(chid);
				continue;
			}
			numPVs++;
			field_type = ca_field_type(chid);
			switch (field_type) {
			case DBF_FLOAT:
				SEVCHK(ca_get(DBR_FLOAT,chid,(void *)&fvalue),"ca_get failure");
				if (restore) SEVCHK(ca_get(DBR_STRING,chid,(void *)&svalue),"ca_get failure");
				SEVCHK(ca_pend_io(PEND_TIME),"ca_pend_io failure");
				sprintf(svalue, FLOAT_FMT, fvalue);
				different = fabs((float)(atof(value_string))-fvalue) > FSMALL;
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (float) %f %f\n", different?"*** ":"", PVname, (float)(atof(value_string)), fvalue);
				}
				if (restore) fprintf(fr, "%s %s\n", PVname, svalue);
				break;
			case  DBF_DOUBLE:
				SEVCHK(ca_get(DBR_DOUBLE,chid,(void *)&dvalue),"ca_get failure");
				SEVCHK(ca_pend_io(PEND_TIME),"ca_pend_io failure");
				sprintf(svalue, DOUBLE_FMT, dvalue);
				different = fabs(atof(value_string)-dvalue) > DSMALL;
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (double) %f %f\n", different?"*** ":"", PVname, atof(value_string), dvalue);
				}
				if (restore) fprintf(fr, "%s %s\n", PVname, svalue);
				break;
			case  DBF_ENUM:
				SEVCHK(ca_get(DBR_SHORT,chid,(void *)&enum_value),"ca_get failure");
				SEVCHK(ca_pend_io(PEND_TIME),"ca_pend_io failure");
				different = atoi(value_string) != enum_value;
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (enum) %d %d\n", different?"*** ":"", PVname, atoi(value_string), enum_value);
				}
				if (restore) fprintf(fr, "%s %d\n", PVname, enum_value);
				break;
			default:
				SEVCHK(ca_get(DBR_STRING,chid,(void *)&svalue),"ca_get failure");
				SEVCHK(ca_pend_io(PEND_TIME),"ca_pend_io failure");
				different = strcmp(value_string, svalue);
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (string) '%s' '%s'\n", different?"*** ":"", PVname, value_string, svalue);
				}
				if (restore) fprintf(fr, "%s %s\n", PVname, svalue);
				break;
			}
			ca_clear_channel(chid);
		}
	}
	if (restore) {
		fprintf(fr, "<END>\n");
		fclose(fr);
	}
	remove(tempname);
	printf("%d PVs differed.  (%d PVs checked; %d PVs not connected)\n",
		numDifferences, numPVs, numPVsNotConnected);
	return(numDifferences);
}
