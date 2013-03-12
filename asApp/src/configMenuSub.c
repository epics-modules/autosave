#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aSubRecord.h>

extern int fdbrestoreX(char *filename);

volatile int configMenuDebug=0;

static long configMenu_init(aSubRecord *pasub) {
	return(0);
}

/*
 * a - name of scan configuration to restore
 * b - return value from fdbrestore()
 */
static long configMenu_do(aSubRecord *pasub) {
	char *a = (char *)pasub->a;
	int *b = (int *)pasub->b;

	if (configMenuDebug) printf("configMenu_do:a='%s'\n", a);
	if (strlen(a)<1) return(-1);
	*b = fdbrestoreX(a);
	if (configMenuDebug) printf("configMenu_do:fdbrestore returned %d\n", *b);
	return(0);
}



#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(int, configMenuDebug);

static registryFunctionRef configMenuRef[] = {
	{"configMenu_init", (REGISTRYFUNCTION)configMenu_init},
	{"configMenu_do", (REGISTRYFUNCTION)configMenu_do}
};

static void configMenuRegistrar(void) {
	registryFunctionRefAdd(configMenuRef, NELEMENTS(configMenuRef));
}

epicsExportRegistrar(configMenuRegistrar);
