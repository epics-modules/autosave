/* asVerify.c  -- given the name of an autosave .sav file, and access
 * to the running ioc whose PV's have been saved in the file, verify that
 * values in the .sav file agree with live values.
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <epicsGetopt.h>

#include <ctype.h> /* isalpha */
#include <math.h> /* fabs */
#include <float.h>	/* for safeDoubleToFloat() */
#include "cadef.h"

#include "save_restore.h"
#include "autosave_release.h"





void printUsage(void) {
	fprintf(stderr,"usage: asVerify [-vr] <autosave_file>\n");
	fprintf(stderr,"         -v (verbose) causes all PV's to be printed out\n");
	fprintf(stderr,"             Otherwise, only PV's whose values differ are printed.\n");
	fprintf(stderr,"         -r (restore_file) causes restore files named\n");
	fprintf(stderr,"            '<autosave_file>.asVerify' and '...B'to be written.\n");
	fprintf(stderr,"         -d (debug) increment debug level by one.\n");
	fprintf(stderr,"         -rv (or -vr) does both\n");
	fprintf(stderr,"examples:\n");
	fprintf(stderr,"    asVerify auto_settings.sav\n");
	fprintf(stderr,"        (reports only PVs whose values differ from saved values)\n");
	fprintf(stderr,"    asVerify -v auto_settings.sav\n");
	fprintf(stderr,"        (reports all PVs, marking differences with '***'.)\n");
	fprintf(stderr,"    asVerify -vr auto_settings.sav\n");
	fprintf(stderr,"        (reports all PVs, and writes a restore file.)\n");
	fprintf(stderr,"    asVerify auto_settings.sav\n");
	fprintf(stderr,"    caput <myStatusPV> $?\n");
	fprintf(stderr,"        (writes number of differences found to a PV.)\n\n");
	fprintf(stderr,"NOTE: For the purpose of writing a restore file, you can specify a .req\n");
	fprintf(stderr,"file (or any file that contains PV names, one per line) instead of a\n");
	fprintf(stderr,".sav file.  However, this program will misunderstand any 'file' commands\n");
	fprintf(stderr,"that occur in a .req file.  (It will look for a PV named 'file'.)\n");
}

void printVersion(void) {
	printf("asVerify, built from %s\n", AUTOSAVE_RELEASE);
}

int main(int argc,char **argv)
{
	FILE	*fp=NULL, *ftmp=NULL;
	char	s[BUF_SIZE], filename[PATH_SIZE], restoreFileName[PATH_SIZE];
	char	*tempname;
	int		n;
	int		numDifferences;
	int		status;
	int		verbose=0, debug=0, write_restore_file=0;
	int		opt;

	while ((opt = getopt(argc, argv, "Vvrdh")) != -1) {
		switch (opt) {
			case 'V': printVersion(); exit(0);
			case 'v': verbose = 1; break;
			case 'r': write_restore_file = 1; break;
			case 'd': printf("debug=%d\n", ++debug); break;
			case 'h': printUsage(); exit(1);
		}
	}

	if (argc <= optind) {
		printUsage();
		exit(1);
	}

	strcpy(filename, argv[optind]);

	status = ca_context_create(ca_disable_preemptive_callback);
	if (!(status & CA_M_SUCCESS)) {
		printf("Can't create CA context.  I quit.\n");
		return(-1);
	}

	/*
	 * Copy to temporary file.
	 * The .sav file is likely to be overwritten while we're using it.
	 */
	fp = fopen(filename,"r");
	if (fp == NULL) {printf("Can't open %s\n", filename); return(-1);}
	tempname = tmpnam(NULL);
	ftmp = fopen(tempname,"w");
	if (ftmp == NULL) {
		printf("Can't open temp file.\n");
		fclose(fp);
		return(-1);
	}
	while (!feof(fp) && (n=fread(s,1,BUF_SIZE,fp))) {
		fwrite(s,1,n,ftmp);
	}
	fclose(fp); fp = NULL;
	fclose(ftmp); ftmp = NULL;

	if (write_restore_file) {
		strcpy(restoreFileName, filename);
		strcat(restoreFileName, ".asVerify");
	} else {
		strcpy(restoreFileName, "");
	}
	numDifferences = do_asVerify(tempname, verbose, debug, write_restore_file, restoreFileName);

	remove(tempname);
	ca_context_destroy();
	return(numDifferences);
}

