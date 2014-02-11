/* verify.c  -- given the name of an autosave .sav file, and access
 * to the running ioc whose PV's have been saved in the file, verify that
 * values in the .sav file agree with live values.
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h> /* isalpha */
#include <math.h> /* fabs */
#include <float.h>	/* for safeDoubleToFloat() */
#include "cadef.h"

#include "save_restore.h"

#ifndef PVNAME_STRINGSZ
#define PVNAME_STRINGSZ 61	/* includes terminating null */
#endif
#define PEND_TIME 5.0

#define FSMALL 1.e-6
#define DSMALL 1.e-8
#define	ASVERSION "asVerify R5.1"

#define WRITE_HEADER if (!wrote_head) {printf("    PVname saved_value live_value\n"); \
printf("    =============================\n"); wrote_head=1;}

long read_array(FILE *fp, char *PVname, char *value_string, short field_type, long element_count,
	char *read_buffer, int debug);

long asVerify(char *fileName, int verbose, int debug, int write_restore_file, char *restoreFileName) {
	float	*pfvalue, *pf_read;
	double	*pdvalue, *pd_read, diff, max_diff=0.;
	short	*penum_value, *penum_value_read;
	char	*svalue, *svalue_read;
	chid	chid;
	FILE	*fp=NULL, *fr=NULL, *fr1=NULL;
	char	c, s[BUF_SIZE], *bp, PVname[PV_NAME_LEN], value_string[BUF_SIZE];
	char	trial_restoreFileName[PATH_SIZE];
	char	*CA_buffer=NULL, *read_buffer=NULL, *pc=NULL;
	short	field_type;
	int		i, j, n, is_scalar, is_scalar_in_file, is_long_string;
	int		numPVs, numDifferences, numPVsNotConnected, nspace;
	int		different, wrote_head=0, status, file_ok=0;
	long 	element_count=0, storageBytes=0, alloc_CA_buffer=0;


	fp = fopen(fileName,"r");
	if (fp == NULL) {printf("asVerify: Can't open '%s'.\n", fileName); return(-1);}

	if (write_restore_file) {
		strcpy(trial_restoreFileName, restoreFileName);
		strcat(trial_restoreFileName, "B");
		if (debug) {printf("asVerify: restoreFileName '%s'.\n", restoreFileName);}
		fr = fopen(trial_restoreFileName,"w");
		if (fr == NULL) {
			printf("asVerify: Can't open restore_file '%s' for writing.\n", trial_restoreFileName);
			write_restore_file = 0;
		} else {
			fprintf(fr,"# %s\tAutomatically generated - DO NOT MODIFY - datetime\n", ASVERSION);
		}
	}
	/* check that (copy of) .sav file is good */
	status = fseek(fp, -6, SEEK_END);
	fgets(s, 6, fp);
	if (strncmp(s, "<END>", 5) == 0) file_ok = 1;
	if (!file_ok) {
		status = fseek(fp, -7, SEEK_END);
		fgets(s, 7, fp);
		if (strncmp(s, "<END>", 5) == 0) file_ok = 1;
	}
	if (status || !file_ok) {
		if (verbose) printf("asVerify: Can't find <END> marker.  File may be bad.\n");
		if (write_restore_file) fprintf(fr,"# # # Could not find end marker in original .sav file.  File may be bad.\n");
	}
	status = fseek(fp, 0, SEEK_SET); /* file is ok.  go to beginning */
	if (status) {
		printf("asVerify: Can't go back to beginning of file.  I quit.\n");
		fclose(fp); fp = NULL;
		if (write_restore_file) fclose(fr); fr = NULL;
		return(-1);
	}

	numDifferences = numPVs = numPVsNotConnected = 0;

	/* init CA buffer space */
	storageBytes = 200;
	CA_buffer = malloc(2*storageBytes); alloc_CA_buffer = storageBytes;
	if (CA_buffer == NULL) {
		printf("asVerify: Can't allocate CA buffer.  I quit.\n");
		fclose(fp); fp = NULL;
		if (write_restore_file) fclose(fr); fr = NULL;
		return(-1);
	}

	while ((bp=fgets(s, BUF_SIZE, fp))) {
		if (debug>3) printf("\nasVerify: buffer '%s'\n", bp);
		if (bp[0] == '#') {
			/* A PV to which autosave could not connect, or just a comment in the file. */
			if (strstr(bp, "Search Issued")) numPVsNotConnected++;
			if (write_restore_file) fprintf(fr, "%s", bp);
			continue;
		}
		/* NOTE value_string must have room for nearly  BUF_SIZE characters */
		n = sscanf(bp,"%80s%c%[^\n\r]", PVname, &c, value_string);
		/* if (!debug && !verbose) printf("%-61s\r", PVname); */
		if (debug>3) printf("\nasVerify: PVname='%s', value_string[%d]='%s'\n",
				PVname, (int)strlen(value_string), value_string);
		if (n<3) *value_string = 0;
		if (strlen(PVname) >= PVNAME_STRINGSZ) {
			/* Impossible PV name */
			if (write_restore_file) fprintf(fr, "#? %s", bp);
			continue;
		}
		if (isalpha((int)PVname[0]) || isdigit((int)PVname[0])) {
			status = ca_create_channel(PVname,NULL,NULL,10,&chid);
			if (status & CA_M_SUCCESS) status = ca_pend_io(PEND_TIME);
			if ((chid == NULL) || (ca_state(chid) != cs_conn)) {
				printf("asVerify: *** '%s' not connected.  Saved value='%s'\n", PVname, value_string);
				numPVsNotConnected++;
				if (chid) ca_clear_channel(chid);
				if (write_restore_file) fprintf(fr, "#'%s' not connected\n", PVname);
				continue;
			}
			numPVs++;
			field_type = ca_field_type(chid);
			if (debug>3) printf("'%s' native field_type=%d\n", PVname, field_type);
			is_long_string = (field_type==DBF_CHAR) && (PVname[strlen(PVname)-1]=='$');
			/* If DBF_STRING will work, use it. */
			if (field_type!=DBF_FLOAT && field_type!=DBF_DOUBLE && field_type!=DBF_ENUM)
				field_type = DBF_STRING;
			if (field_type==DBF_ENUM) field_type = DBF_SHORT;

			element_count = ca_element_count(chid);
			is_scalar_in_file = strncmp(value_string, ARRAY_MARKER, ARRAY_MARKER_LEN) != 0;
			is_scalar = is_scalar_in_file;
			if (element_count > 1) is_scalar = 0;
			if (debug>3) printf("asVerify: is_scalar=%d, is_scalar_in_file=%d, is_long_string=%d\n",
				is_scalar, is_scalar_in_file, is_long_string);

			/* allocate storage for CA and for reading the file */
			storageBytes = dbr_size_n(field_type, element_count);
			if (is_long_string) storageBytes = dbr_size_n(DBF_CHAR, element_count);
			if (debug>3) printf("asVerify:type=%d,elements=%ld, storageBytes=%ld\n",
					field_type, element_count, storageBytes);
			if (alloc_CA_buffer < storageBytes) {
				if (CA_buffer) free(CA_buffer);
				CA_buffer = malloc(2*storageBytes); alloc_CA_buffer = storageBytes;
			}
			if (CA_buffer == NULL) {
				printf("asVerify: Can't allocate CA buffer.  I quit.\n");
				fclose(fp); fp = NULL;
				if (write_restore_file) fclose(fr); fr = NULL;
				return(-1);
			}
			/* use second half of CA_buffer for values read from .sav file */
			read_buffer = CA_buffer+storageBytes;

			if (!is_scalar_in_file) {
				if (debug>3) printf("asVerify: calling read_array\n");
				memset(read_buffer, 0, storageBytes);
				read_array(fp, PVname, value_string, field_type, element_count, read_buffer, debug);
			}

			switch (field_type) {
			case DBF_FLOAT:
				pfvalue = (float *)CA_buffer;
				status = ca_array_get(DBR_FLOAT,element_count,chid,(void *)pfvalue);
				if (status & CA_M_SUCCESS) status = ca_pend_io(PEND_TIME);
				if (!(status & CA_M_SUCCESS)) printf("asVerify: Can't get value from '%s'.\n", PVname);
				if (is_scalar == is_scalar_in_file) {
					if (is_scalar) {
						different = fabs((float)(atof(value_string)) - *pfvalue) > FSMALL;
					} else {
						pf_read = (float *)read_buffer;
						for (i=0, different=0, max_diff=0.; i<element_count; i++) {
							diff = fabs(pf_read[i] - pfvalue[i]);
							if (diff > max_diff) max_diff = diff;
							different += diff > FSMALL;
						}
					}
					if (different) numDifferences++;
					if (different || verbose) {
						WRITE_HEADER;
						if (is_scalar) {
							printf("%s%-25s %-25f %f\n", different?"*** ":"    ", PVname, (float)(atof(value_string)), *pfvalue);
						} else {
							printf("%s%-25s (array) %d diff%1c", different?"*** ":"    ", PVname, different, different==1?' ':'s');
							if (different) printf(", maxDiff=%f", max_diff);
							printf("\n");
						}
					}
				} else {
					printf("asVerify: *** %-25s is %s in file, but %s in ioc.\n", PVname,
						is_scalar_in_file?"scalar":"array", is_scalar?"scalar":"array");
				}
				if (write_restore_file) {
					if (is_scalar) {
						fprintf(fr, "%s %.7g\n", PVname, *pfvalue);	/* cf. FLOAT_FMT */
					} else {
						fprintf(fr, "%s %-s %1c ", PVname, ARRAY_MARKER, ARRAY_BEGIN);
						for (i=0; i<element_count; i++) {
							fprintf(fr, "%1c%.7g%1c ", ELEMENT_BEGIN, pfvalue[i], ELEMENT_END);
						}
						fprintf(fr, "%1c\n", ARRAY_END);
					}
				}
				break;
			case  DBF_DOUBLE:
				pdvalue = (double *)CA_buffer;
				status = ca_array_get(DBR_DOUBLE,element_count,chid,(void *)pdvalue);
				if (status & CA_M_SUCCESS) status = ca_pend_io(PEND_TIME);
				if (!(status & CA_M_SUCCESS)) printf("asVerify: Can't get value from '%s'.\n", PVname);
				if (is_scalar == is_scalar_in_file) {
					if (is_scalar) {
						different = fabs(atof(value_string) - *pdvalue) > DSMALL;
					} else {
						pd_read = (double *)read_buffer;
						for (i=0, different=0, max_diff=0.; i<element_count; i++) {
							diff = fabs(pd_read[i] - pdvalue[i]);
							if (diff > max_diff) max_diff = diff;
							different += diff > DSMALL;
						}
					}
					if (different) numDifferences++;
					if (different || verbose) {
						WRITE_HEADER;
						if (is_scalar) {
							printf("%s%-25s %-25f %f\n", different?"*** ":"    ", PVname, atof(value_string), *pdvalue);
						} else {
							printf("%s%-25s (array) %d diff%1c", different?"*** ":"    ", PVname, different, different==1?' ':'s');
							if (different) printf(", maxDiff=%f", max_diff);
							printf("\n");
						}
					}
				} else {
					printf("*** %-25s is %s in file, but %s in ioc.\n", PVname,
						is_scalar_in_file?"scalar":"array", is_scalar?"scalar":"array");
				}
				if (write_restore_file) {
					if (is_scalar) {
						fprintf(fr, "%s %.14g\n", PVname, *pdvalue);	/* cf. DOUBLE_FMT */
					} else {
						fprintf(fr, "%s %-s %1c ", PVname, ARRAY_MARKER, ARRAY_BEGIN);
						for (i=0; i<element_count; i++) {
							fprintf(fr, "%1c%.14g%1c ", ELEMENT_BEGIN, pdvalue[i], ELEMENT_END);
						}
						fprintf(fr, "%1c\n", ARRAY_END);
					}
				}
				break;
			case  DBF_SHORT:
				penum_value = (short *)CA_buffer;
				status = ca_array_get(DBR_SHORT,element_count,chid,(void *)penum_value);
				if (status & CA_M_SUCCESS) status = ca_pend_io(PEND_TIME);
				if (!(status & CA_M_SUCCESS)) printf("Can't get value from '%s'.\n", PVname);
				if (is_scalar == is_scalar_in_file) {
					if (is_scalar) {
						different = atoi(value_string) != *penum_value;
					} else {
						penum_value_read = (short *)read_buffer;
						for (i=0, different=0; i<element_count; i++) {
							different += (penum_value_read[i] != penum_value[i]);
						}
					}
					if (different) numDifferences++;
					if (different || verbose) {
						WRITE_HEADER;
						if (is_scalar) {
							printf("%s%-25s %-25d %d\n", different?"*** ":"    ", PVname, atoi(value_string), *penum_value);
						} else {
							printf("%s%-25s (array) %d diff%1c\n", different?"*** ":"    ", PVname, different, different==1?' ':'s');
						}
					}
				} else {
					printf("*** %-25s is %s in file, but %s in ioc.\n", PVname,
						is_scalar_in_file?"scalar":"array", is_scalar?"scalar":"array");
				}
				if (write_restore_file) {
					if (is_scalar) {
						fprintf(fr, "%s %d\n", PVname, *penum_value);
					} else {
						fprintf(fr, "%s %-s %1c ", PVname, ARRAY_MARKER, ARRAY_BEGIN);
						for (i=0; i<element_count; i++) {
							fprintf(fr, "%1c%d%1c ", ELEMENT_BEGIN, penum_value[i], ELEMENT_END);
						}
						fprintf(fr, "%1c\n", ARRAY_END);
					}
				}
				break;
			case  DBF_STRING:
				svalue = (char *)CA_buffer;
				if (is_long_string) {
					/* See if we got the whole line */
					if (bp[strlen(bp)-1] != '\n') {
						/* No, we didn't.  One more read will certainly accumulate a value string of length BUF_SIZE */
						if (debug>3) printf("did not reach end of line for long-string PV\n");
						bp = fgets(s, BUF_SIZE, fp);
						n = BUF_SIZE-strlen(value_string)-1;
						strncat(value_string, bp, n);
						if (value_string[strlen(value_string)-1] == '\n') value_string[strlen(value_string)-1] = '\0';
					}
					/* Discard additional characters until end of line */
					while (bp[strlen(bp)-1] != '\n') fgets(s, BUF_SIZE, fp);

					status = ca_array_get(DBR_CHAR,element_count,chid,(void *)svalue);
				} else {
					status = ca_array_get(DBR_STRING,element_count,chid,(void *)svalue);
				}
				if (status & CA_M_SUCCESS) status = ca_pend_io(PEND_TIME);
				if (!(status & CA_M_SUCCESS)) printf("Can't get value from '%s'.\n", PVname);

				if ((is_scalar != is_scalar_in_file) && !is_long_string) {
					printf("*** %-25s is %s in file, but %s in ioc.\n", PVname,
						is_scalar_in_file?"scalar":"array", is_scalar?"scalar":"array");
				} else {
					if (is_long_string) {
						different = strncmp(value_string, svalue, BUF_SIZE);
					} else if (is_scalar) {
						different = strcmp(value_string, svalue);
					} else {
						svalue_read = (char *)read_buffer;
						for (i=0, different=0; i<element_count; i++) {
							j = strncmp(&svalue_read[i*MAX_STRING_SIZE], &svalue[i*MAX_STRING_SIZE], MAX_STRING_SIZE);
							different += (j != 0);
							if (debug>3) printf("'%40s' != '%40s'\n", &svalue_read[i*MAX_STRING_SIZE], &svalue[i*MAX_STRING_SIZE]);
						}
					}
					if (different) numDifferences++;
					if (different || verbose) {
						WRITE_HEADER;
						if (is_scalar || is_long_string) {
							nspace = 24-strlen(value_string); if (nspace < 1) nspace = 1;
							printf("%s%-24s '%s'%*s'%s'\n", different?"*** ":"    ", PVname, value_string, nspace, "", svalue);
						} else {
							printf("%s%-25s (array) %d diff%1c\n", different?"*** ":"    ", PVname, different, different==1?' ':'s');
						}
					}
				}
				if (write_restore_file) {
					if (is_long_string) {
						svalue[BUF_SIZE-1] = '\0';
						fprintf(fr, "%s %s\n", PVname, svalue);
					} else if (is_scalar) {
						fprintf(fr, "%s %s\n", PVname, svalue);
					} else {
						fprintf(fr, "%s %-s %1c ", PVname, ARRAY_MARKER, ARRAY_BEGIN);
						for (i=0; i<element_count; i++) {
							pc = &svalue[i*MAX_STRING_SIZE];
							fprintf(fr, "%1c", ELEMENT_BEGIN);
							for (j=0; j<MAX_STRING_SIZE-1 && *pc; j++, pc++) {
								if ((*pc == ELEMENT_BEGIN) || (*pc == ELEMENT_END)){
									fprintf(fr, "%1c", ESCAPE);
									j++;
								}
								fprintf(fr, "%1c", *pc);
							}
							fprintf(fr, "%1c ", ELEMENT_END);
						}
						fprintf(fr, "%1c\n", ARRAY_END);
					}
				}
				break;
			default:
				printf("!!!%s mishandled field type (%d)\n", PVname, field_type);
				break;
			}
			ca_clear_channel(chid);
		}
	}
	if (write_restore_file) {
		fprintf(fr, "<END>\n");
		fclose(fr);
		if (numPVsNotConnected < numPVs/2)  {
			/* copy trial restore file to real restore file */
			fr = fopen(trial_restoreFileName,"r");
			fr1 = fopen(restoreFileName,"w");
			while ((bp=fgets(s, BUF_SIZE, fr))) {
				fputs(s, fr1);
			}
			fclose(fr);
			fclose(fr1);
		}
	}
	if (CA_buffer) free(CA_buffer);
	fclose(fp);

	printf("%d PV%sdiffered.  (%d PV%schecked; %d PV%snot connected)\n",
		numDifferences, numDifferences==1?" ":"s ", numPVs, numPVs==1?" ":"s ",
		numPVsNotConnected, numPVsNotConnected==1?" ":"s ");

	return(numDifferences + numPVsNotConnected);
}

static float mySafeDoubleToFloat(double d)
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

long read_array(FILE *fp, char *PVname, char *value_string, short field_type, long element_count,
	char *read_buffer, int debug)
{
	int		i, j, end_mark_found=0, begin_mark_found=0, end_of_file=0;
	int		found=0, in_element=0;
	long	status=0, num_read=0;
	char	buffer[BUF_SIZE], *bp = NULL;
	char	string[MAX_STRING_SIZE];
	char	*p_char = NULL;
	short	*p_short = NULL;
	float	*p_float = NULL;
	double	*p_double = NULL;

	/** read array values **/
	if (debug > 1) printf("array_read: line='%80s'\n", value_string);

	switch (field_type) {
	case DBF_DOUBLE: p_double = (double *)read_buffer; break;
	case DBF_FLOAT:  p_float = (float *)read_buffer; break;
	case DBF_ENUM:   p_short = (short *)read_buffer; break;
	default:         p_char = (char *)read_buffer; break;
	}
	if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) {
		if (debug > 1) printf("array_read: line='%s'\n", bp);
		begin_mark_found = 1;
		for (num_read=0; bp && !end_mark_found; ) {
			/* Find beginning of array element */
			if (debug > 1) printf("array_read: looking for element[%ld] \n", num_read);
			while ((*bp != ELEMENT_BEGIN) && !end_mark_found && !end_of_file) {
				if (debug > 1) printf("array_read: buffer contains '%s'\n", bp);
				switch (*bp) {
				case '\0':
					if (debug > 1) printf("array_read: end-of-string\n");
					if ((bp = fgets(buffer, BUF_SIZE, fp)) == NULL) {
						printf("read_array: *** EOF during array-parse\n");
						end_of_file = 1;
					}
					break;
				case ARRAY_END:
					if (debug > 1) printf("array_read: array-end\n");
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
				if (debug > 1) printf("array_read: found element-begin: '%s'\n", bp);
				for (bp++, j=0; (j < MAX_STRING_SIZE-1) && (*bp != ELEMENT_END); bp++) {
					if (*bp == '\0') {
						if ((bp = fgets(buffer, BUF_SIZE, fp)) == NULL) {
							printf("read_array:array_restore: *** premature EOF.\n");
							end_of_file = 1;
							break;
						}
						if (debug > 1) printf("array_read: new buffer: '%s'\n", bp);
						if (*bp == ELEMENT_END) break;
					} else if ((*bp == ESCAPE) && ((bp[1] == ELEMENT_BEGIN) || (bp[1] == ELEMENT_END))) {
						/* escaped character */
						if (debug > 1) printf("array_read: escaped element-begin/end '%s'\n", bp);
						bp++;
					}
					if (isprint((int)(*bp))) string[j++] = *bp; /* Ignore, e.g., embedded newline */
				}
				string[j] = '\0';

				/*
				 * We've accumulated all the characters, or all we can handle in string[].
				 * If there are more characters than we can handle, just pretend we read them.
				 */
				/* *bp == ELEMENT_END ,*/
				if (debug > 1) printf("array_read: looking for element-end: '%s'\n", bp);
				for (found = 0; (found == 0) && !end_of_file; ) {
					while (*bp && (*bp != ELEMENT_END) && (*bp != ESCAPE)) bp++;
					switch (*bp) {
					case ELEMENT_END:
						found = 1; bp++; break;
					case ESCAPE:
						if (*(++bp) == ELEMENT_END) bp++; break;
					default:
						if ((bp = fgets(buffer, BUF_SIZE, fp)) == NULL) {
							end_of_file = 1;
							found = 1;
						}
					}
				}

				/* Append value to local array. */
				if (read_buffer) {
					switch (field_type) {
					case DBF_ENUM:
						p_short[num_read++] = (short)atol(string);
						break;
					case DBF_FLOAT:
						p_float[num_read++] = mySafeDoubleToFloat(atof(string));
						break;
					case DBF_DOUBLE:
						p_double[num_read++] = atof(string);
						break;
					default:
						strncpy(&(p_char[(num_read++)*MAX_STRING_SIZE]), string, MAX_STRING_SIZE);
						break;
					}
				}
			}
		} /* for (num_read=0; bp && !end_mark_found; ) */
	} /* if ((bp = strchr(value_string, (int)ARRAY_BEGIN)) != NULL) */

	/* clear unused read_buffer space */
	for (i=num_read; i<element_count; i++) {
		switch (field_type) {
		case DBF_ENUM:		p_short[i] = 0; break;
		case DBF_FLOAT:		p_float[i] = 0.; break;
		case DBF_DOUBLE:	p_double[i] = 0.; break;
		default: 			strncpy(&(p_char[i*MAX_STRING_SIZE]), "", MAX_STRING_SIZE); break;
		}
	}

	/* leave the file pointer ready for next PV (next fgets() should yield next PV) */
	if (debug > 1) printf("array_read: positioning for next PV '%s'\n", bp);
	if (begin_mark_found) {
		/* find ARRAY_END (but ARRAY_END inside an element is just another character) */
		in_element = 0;
		while (!end_mark_found && !end_of_file) {
			switch (*bp) {
			case ESCAPE:
				if (in_element && (bp[1] == ELEMENT_END)) bp++; /* two chars treated as one */
				break;
			case ARRAY_END:
				if (!in_element) end_mark_found = 1;
				break;
			case '\0':
				if ((bp = fgets(buffer, BUF_SIZE, fp)) == NULL) {
					printf("read_array: *** EOF during array-end search\n");
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
		status = -1;
		/* just get next line, assuming it contains the next PV */
		if (!end_of_file) {
			if ((bp = fgets(buffer, BUF_SIZE, fp)) == NULL) end_of_file = 1;
		}
	}
	if (debug > 1) printf("array_read: positioned for next PV '%s'\n", bp);
	if (!status && end_of_file) status = end_of_file;

	return(status);
}
