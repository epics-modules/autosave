/* fGetDateStr.c - */
/*
 * Author:      Frank Lenkszus
 * Date:        08/30/94
 *
 *      Experimental Physics and Industrial Control System (EPICS)
*/
/*
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1994 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/
/*
* Modification Log:
 * -----------------
 * .01  11-18-98	mrk	removed from writeSub.c and save_restore.c
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <callback.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbCommon.h>
#include <recGbl.h>
#include <devSup.h>
#include <tsDefs.h>
#include <fGetDateStr.h>

/* get date,time as yymmdd-HHMMSS */
int	fGetDateStr( char datetime[])
{ 
	TS_STAMP   now;

	epicsTimeGetCurrent(&now);
	epicsTimeToStrftime(datetime, 28, "%y%m%d-%H%M%S", &now);
	return(0);
}

