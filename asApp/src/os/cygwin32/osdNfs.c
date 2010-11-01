/***********************************************
 * osdNfs.c
 * 
 * File for an OS that does not support NFS client
 *
 ***********************************************/
#include "osdNfs.h"

/**
 * Global variables
 */
int save_restoreNFSOK    = 0;  /* Don't support NFS on this OS */
int save_restoreIoErrors = 0;  /* for accumulate the IO error numbers, when the number larger than threshold, remount NFS */

int nfsMount(char *uidhost, char *path, char *mntpoint)
{
    printf("nfsMount not supported on this OS\n");
	return(0);  
}

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
