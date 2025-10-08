/***********************************************
 * osdNfs.c
 * 
 * File for an OS that does not support NFS client
 *
 ***********************************************/
#include "osdNfs.h"

/* Global variables */
int save_restoreNFSOK = 1;
int save_restoreIoErrors = 0;
extern volatile int save_restoreDebug;

int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint)
{
    printf("mountFileSytem not supported on this OS\n");
    return (ERROR);
}

int dismountFileSystem(char *mntpoint)
{
    printf("dismountFileSytem not supported on this OS\n");
    return (ERROR);
}
