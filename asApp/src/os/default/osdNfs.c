/***********************************************
 * osdNfs.c
 * 
 * File for an OS that does not support NFS client
 *
 ***********************************************/
#include "osdNfs.h"

/* Global variables */
int save_restoreNFSOK    = 0;
int save_restoreIoErrors = 0;

int mountFileSystem(char *uidhost, char *path, char *mntpoint)
{
    printf("mountFileSytem not supported on this OS\n");
	return(0);  
}

int dismountFileSystem(char *mntpoint)
{
    printf("dismountFileSytem not supported on this OS\n"); 
	return(0);  
}
