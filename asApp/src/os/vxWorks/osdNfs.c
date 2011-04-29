/***********************************************
 * osdNfs.c
 * 
 * Realize the vxWorks specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#include "osdNfs.h"
extern char saveRestoreFilePath[];              /* path to save files */

/**
 * Global variables
 */
int save_restoreNFSOK    = 0;  /* for vxWorks, NFS has not been mounted before autosave starts */
int save_restoreIoErrors = 0;  /* for accumulate the IO error numbers, when the number larger than threshold, remount NFS */

/**
 * Mount the NFS
 *
 * Input:
 *    uidhost  - string. The NFS server and uid/gid
 *    addr     - string. IP address, in case host has not already been added
 *    path     - string. Absolute path on the NFS server
 *    mntpoint - string. Local path
 *
 * Output:
 *    See the definition of NFS operation error codes
 */
int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint)
{
    /* check the input parameters */
    if (!uidhost || !uidhost[0])   return ERROR;
    if (!path || !path[0])         return ERROR;
    if (!mntpoint || !mntpoint[0]) return ERROR;

    /* mount the file system */
    if (hostGetByName(uidhost) == ERROR) (void)hostAdd(uidhost, addr);
    if (nfsMount(uidhost, path, mntpoint) == OK) {
        save_restoreNFSOK    = 1;
        save_restoreIoErrors = 0;                      /* clean the counter */
        return OK;
    } else {
        save_restoreNFSOK = 0;
        return ERROR;
    }
}

/**
 * unmount the NFS
 *
 * Input:
 *    mntpoint - string. Local path. 
 *
 * Output:
 *    See the definition of NFS operation error codes
 */ 
int dismountFileSystem(char *mntpoint)
{
    /* check the input parameters */
    if (!mntpoint || !mntpoint[0]) {
		if (saveRestoreFilePath && saveRestoreFilePath[0]) {
		    strncpy(mntpoint, saveRestoreFilePath, (NFS_PATH_LEN-1));
		} else {
			return ERROR;
		}
	}

    /* unmount the file system */
    
    if (nfsUnmount(mntpoint) == OK) {
        save_restoreNFSOK = 0;
        return OK;
    } else {
        return ERROR;
    }
}



