#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <dbScan.h>
#include <dbAccess.h>
#include <aSubRecord.h>
#include <epicsStdio.h>

#include "configMenuClient.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

volatile int configMenuDebug = 0;
void makeLegal(char *name);

/* We need to know when a save or restore operation has completed, so client software
 * can wait for the operation to complete before acting on the result.
 */
void configMenuCallback(int status, void *puserPvt)
{
    aSubRecord *pasub = (aSubRecord *)puserPvt;
    epicsInt32 *d = (epicsInt32 *)pasub->d;

    if (configMenuDebug) printf("configMenuCallback:status=%d, puserPvt=%p\n", status, puserPvt);
    dbScanLock((dbCommon *)pasub);
    *d = (epicsInt32)status;
    dbScanUnlock((dbCommon *)pasub);
    scanOnce((dbCommon *)puserPvt);
}

static long configMenu_init(aSubRecord *pasub) { return (0); }

/*
 * a - name of scan configuration to save or restore
 * b - return value from fdbrestore()
 * c - current state of busy record
 * d - status from callback (0 or -1)
 * e - restore(0) or save(1)
 * f - reqFile
 * g - configMenu name
 * vala - desired state of busy record (used to set)
 * valb - desired state of busy record (used to clear)
 * valc - status value for output: 0=Success, 1=Error
 */
static long configMenu_do(aSubRecord *pasub)
{
    char *a = (char *)pasub->a;
    epicsInt32 *b = (epicsInt32 *)pasub->b;
    char *c = (char *)pasub->c;
    epicsInt32 *d = (epicsInt32 *)pasub->d;
    short *e = (short *)pasub->e;
    char *f = (char *)pasub->f;
    char *g = (char *)pasub->g;
    epicsInt32 *vala = (epicsInt32 *)pasub->vala;
    epicsInt32 *valb = (epicsInt32 *)pasub->valb;
    epicsInt32 *valc = (epicsInt32 *)pasub->valc;
    char *macrostring = NULL;
    char filename[100];

    if (configMenuDebug) printf("configMenu_do:c='%s' (%s)\n", c, *e ? "save" : "restore");

    if (*e == 0) {
        /* restore */
        if (strcmp(c, "Done") == 0) {
            /* start restore operation */
            if (configMenuDebug) printf("configMenu_do:a='%s', c='%s', pasub=%p\n", a, c, pasub);
            if (strlen(a) < 1) {
                *d = 1;
                *valc = 1;
                return (0);
            }
            if (f) { macrostring = getMacroString(f); }
            makeLegal(a);
            epicsSnprintf(filename, 99, "%s_%s.cfg", g, a);
            *b = fdbrestoreX(filename, macrostring, configMenuCallback, (void *)pasub);
            if (configMenuDebug) printf("configMenu_do:fdbrestore returned %d\n", *b);
            *vala = 1;
            *valb = 1;
        } else {
            /* this is a callback from restore operation */
            if (configMenuDebug) printf("configMenu_do:callback status=%d\n", *valc);
            *valc = (*d ? 1 : 0);
            *vala = 0;
            *valb = 0;
        }
    } else {
        /* save */
        if (strcmp(c, "Done") == 0) {
            /* start save operation */
            if (configMenuDebug) printf("configMenu_do:a='%s', c='%s', pasub=%p\n", a, c, pasub);
            if (strlen(a) < 1) {
                *d = 1;
                *valc = 1;
                return (0);
            }
            makeLegal(a);
            epicsSnprintf(filename, 99, "%s_%s.cfg", g, a);
            *b = (epicsInt32)manual_save(f, filename, configMenuCallback, (void *)pasub);
            if (configMenuDebug) printf("configMenu_do:manual_save returned %d\n", *b);
            *vala = 1;
            *valb = 1;
        } else {
            /* this is a callback from a save operation */
            if (configMenuDebug) printf("configMenu_do:save callback status=%d\n", *valc);
            *valc = (*d ? 1 : 0);
            *vala = 0;
            *valb = 0;
        }
    }
    return (0);
}

void makeLegal(char *name)
{
    int i;
    for (i = 0; i < strlen(name); i++) {
        if (isalnum((int)name[i])) continue;
        name[i] = '_';
    }
}

static long configMenuList_init(aSubRecord *pasub)
{
    ELLLIST *configMenuList;
    configMenuList = calloc(1, sizeof(ELLLIST));
    pasub->dpvt = configMenuList;
    ellInit(configMenuList);
    return (0);
}
#define NUM_ITEMS 10

static long configMenuList_do(aSubRecord *pasub)
{
    ELLLIST *configMenuList = (ELLLIST *)pasub->dpvt;
    struct configFileListItem *pLI;
    char *configName = (char *)pasub->a;
    short *page = (short *)pasub->b;
    short *findFiles = (short *)pasub->c;
    short jStart;
    char *f[NUM_ITEMS * 2] = {0};
    int i, status = 0;

    pLI = (struct configFileListItem *)ellFirst(configMenuList);
    if (pLI == NULL) { *findFiles = 1; }
    if (*findFiles || (pLI->name == NULL) || (pLI->name[0] == '\0')) {
        status = findConfigFiles(configName, configMenuList);
        if (configMenuDebug || status)
            printf("configMenuList_do(%s): findConfigFiles returned %d\n", configName, status);
        *findFiles = 0;
    }
    if (status == 0) {
        /* names */
        f[0] = (char *)pasub->vala;
        f[1] = (char *)pasub->valb;
        f[2] = (char *)pasub->valc;
        f[3] = (char *)pasub->vald;
        f[4] = (char *)pasub->vale;
        f[5] = (char *)pasub->valf;
        f[6] = (char *)pasub->valg;
        f[7] = (char *)pasub->valh;
        f[8] = (char *)pasub->vali;
        f[9] = (char *)pasub->valj;

        /* descriptions */
        f[10] = (char *)pasub->valk;
        f[11] = (char *)pasub->vall;
        f[12] = (char *)pasub->valm;
        f[13] = (char *)pasub->valn;
        f[14] = (char *)pasub->valo;
        f[15] = (char *)pasub->valp;
        f[16] = (char *)pasub->valq;
        f[17] = (char *)pasub->valr;
        f[18] = (char *)pasub->vals;
        f[19] = (char *)pasub->valt;

        for (i = 0; i < NUM_ITEMS; i++) {
            f[i][0] = '\0';
            f[i + NUM_ITEMS][0] = '\0';
        }

        if (configMenuDebug) printf("configMenuList_do(%s): page %d\n", configName, *page);
        jStart = MAX(0, *page * NUM_ITEMS);

        pLI = (struct configFileListItem *)ellFirst(configMenuList);
        for (i = 0; i < jStart && pLI; i++) {
            if (configMenuDebug) {
                printf("configMenuList_do(%s): skipping name '%s'\n", configName, pLI->name ? pLI->name : "(null)");
            }
            pLI = (struct configFileListItem *)ellNext(&(pLI->node));
        }

        for (i = 0; i < NUM_ITEMS; i++) {
            if (pLI) {
                strncpy(f[i], pLI->name, 39);
                if (pLI->description) {
                    strncpy(f[i + NUM_ITEMS], pLI->description, 39);
                } else {
                    strncpy(f[i + NUM_ITEMS], "no description", 39);
                }
                pLI = (struct configFileListItem *)ellNext(&(pLI->node));
            } else {
                f[i][0] = '\0';
                f[i + NUM_ITEMS][0] = '\0';
            }
        }
    }
    return (0);
}

#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(int, configMenuDebug);

static registryFunctionRef configMenuRef[] = {{"configMenuList_init", (REGISTRYFUNCTION)configMenuList_init},
                                              {"configMenuList_do", (REGISTRYFUNCTION)configMenuList_do},
                                              {"configMenu_init", (REGISTRYFUNCTION)configMenu_init},
                                              {"configMenu_do", (REGISTRYFUNCTION)configMenu_do}};

static void configMenuRegistrar(void) { registryFunctionRefAdd(configMenuRef, NELEMENTS(configMenuRef)); }

epicsExportRegistrar(configMenuRegistrar);
