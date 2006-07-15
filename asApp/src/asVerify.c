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

#define WRITE_HEADER if (!wrote_head) {printf("PVname saved_value live_value\n"); wrote_head=1;}


int main(int argc,char **argv)
{
	float	fvalue;
	double	dvalue;
	short	enum_value;
	chid	chid;
	FILE	*fp;
	char	c, s[BUF_SIZE], *bp, PVname[70], value_string[100], svalue[100], filename[300];
	short	field_type;
	int		n, is_scalar, numDifferences, verbose = 0, different, wrote_head=0, status, file_ok=0;

	if (argc == 1) {
		fprintf(stderr,"usage: asVerify [-v] <autosave_file>\n");
		fprintf(stderr,"example: asVerify -v auto_settings.sav\n");
		fprintf(stderr,"         -v causes all PV's to be reported\n");
		exit(1);
	}
	if (*argv[1] == '-') {
		if (argv[1][1] == 'v') verbose = 1;
		strcpy(filename, argv[2]);
	} else {
		strcpy(filename, argv[1]);
	}
	SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
	
	fp = fopen(filename,"r");

	/* check that file is good */
	status = fseek(fp, -6, SEEK_END);
	fgets(s, 6, fp);
	if (strncmp(s, "<END>", 5) == 0) file_ok = 1;
	if (!file_ok) {
		status = fseek(fp, -7, SEEK_END);
		fgets(s, 7, fp);
		if (strncmp(s, "<END>", 5) == 0) file_ok = 1;
	}
	if (status || !file_ok) printf("Can't find <END> marker.  File may be bad.\n");
	status = fseek(fp, 0, SEEK_SET); /* file is ok.  go to beginning */
	if (status) printf("Can't go back to beginning of file.");

	bp = fgets(s, BUF_SIZE, fp); /* skip over header line */
	numDifferences = 0;

	while ((bp=fgets(s, BUF_SIZE, fp))) {
		n = sscanf(bp,"%80s%c%[^\n\r]", PVname, &c, value_string);
		if (n<3) *value_string = 0;
		if (PVname[0] == '#') /* user must have edited the file manually; accept this line as a comment */
			continue;
		if (strlen(PVname) >= 80) /* munged input line */
			continue;
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			if (strchr(PVname,'.') == 0) strcat(PVname,".VAL"); /* if no field name, add default */
			is_scalar = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN);
			if (!is_scalar) {
				printf("%s is an array.  Not yet implemented.\n", PVname);
				continue;
			}
			SEVCHK(ca_create_channel(PVname,NULL,NULL,10,&chid),"ca_create_channel failure");
			SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
			if (chid && (ca_state(chid) != cs_conn)) {
				printf("%s not connected.  Saved value='%s'\n", PVname, value_string);
				continue;
			}
			field_type = ca_field_type(chid);
			switch (field_type) {
			case DBF_FLOAT:
				SEVCHK(ca_get(DBR_FLOAT,chid,(void *)&fvalue),"ca_get failure");
				SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
				different = fabs((float)(atof(value_string))-fvalue) > FSMALL;
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (float) %f %f\n", different?"*** ":"", PVname, (float)(atof(value_string)), fvalue);
				}
				break;
			case  DBF_DOUBLE:
				SEVCHK(ca_get(DBR_DOUBLE,chid,(void *)&dvalue),"ca_get failure");
				SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
				different = fabs(atof(value_string)-dvalue) > DSMALL;
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (double) %f %f\n", different?"*** ":"", PVname, atof(value_string), dvalue);
				}
				break;
			case  DBF_ENUM:
				SEVCHK(ca_get(DBR_SHORT,chid,(void *)&enum_value),"ca_get failure");
				SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
				different = atoi(value_string) != enum_value;
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (enum) %d %d\n", different?"*** ":"", PVname, atoi(value_string), enum_value);
				}
				break;
			default:
				SEVCHK(ca_get(DBR_STRING,chid,(void *)&svalue),"ca_get failure");
				SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
				different = strcmp(value_string, svalue);
				if (different) numDifferences++;
				if (different || verbose) {
					WRITE_HEADER;
					printf("%s%s (string) '%s' '%s'\n", different?"*** ":"", PVname, value_string, svalue);
				}
				break;
			}
			ca_clear_channel(chid);
		}
	}
	printf("%d PV's differed.\n", numDifferences);
	return(numDifferences);
}
