/* initHooks.c	ioc initialization hooks */
/*
 *      Author:		Marty Kraimer
 *      Date:		06-01-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  09-05-92	rcz	initial version
 * .02  09-10-92	rcz	changed return from void to long
 * .03  09-10-92	rcz	changed completely
 * .04  09-10-92	rcz	bug - moved call to setMasterTimeToSelf later
 *
 */

#include <stdio.h>
#include <initHooks.h>
#include <epicsPrint.h>
#include "save_restore.h"
#include <iocsh.h>
#include <epicsExport.h>

/*
 * INITHOOKS
 *
 * called by iocInit at various points during initialization
 *
 */

/* If this function (initHooks) is loaded, iocInit calls this function
 * at certain defined points during IOC initialization */
static void asInitHooks(initHookState state)
{
    struct restoreFileListItem *pLI;

    switch (state) {
        case initHookAtBeginning: break;
        case initHookAfterCallbackInit: break;
        case initHookAfterCaLinkInit: break;
        case initHookAfterInitDrvSup: break;
        case initHookAfterInitRecSup: break;
        case initHookAfterInitDevSup:

            /* restore fields needed in init_record() */
            maybeInitRestoreFileLists();
            pLI = (struct restoreFileListItem *)ellFirst(&pass0List);
            while (pLI) {
                reboot_restore(pLI->filename, state);
                pLI = (struct restoreFileListItem *)ellNext(&(pLI->node));
            }
            break;

        case initHookAfterInitDatabase:
            /*
		 * restore fields that init_record() would have overwritten with
		 * info from the dol (desired output location).
		 */
            maybeInitRestoreFileLists();
            pLI = (struct restoreFileListItem *)ellFirst(&pass1List);
            while (pLI) {
                reboot_restore(pLI->filename, state);
                pLI = (struct restoreFileListItem *)ellNext(&(pLI->node));
            }
            break;
        case initHookAfterFinishDevSup: break;
        case initHookAfterScanInit: break;
        case initHookAfterInitialProcess: break;
        case initHookAfterInterruptAccept: break;
        case initHookAtEnd: break;
        default: break;
    }
    return;
}

void asInitHooksRegister(void) { initHookRegister(asInitHooks); }

epicsExportRegistrar(asInitHooksRegister);
