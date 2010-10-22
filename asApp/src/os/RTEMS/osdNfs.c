/***********************************************
 * osdNfs.c
 * 
 * Realize the RTEMS specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#include "osdNfs.h"

/**
 * Global variables
 */
int save_restoreNFSOK    = 1;  /* for RTEMS, NFS has been mounted before autosave starts */
int save_restoreIoErrors = 0;  /* for accumulate the IO error numbers, when the number larger than threshold, remount NFS */

/**
 * Mount the NFS
 *
 * Pre conditions:
 *    Initialization has been done: rpcUdpInit() and nfsInit(smallPoolDepth, bigPoolDepth)
 *    Please note that when using CEXP to load the RTEMS modules, the initialization will be 
 *    done automatically
 *
 * Input:
 *    uidhost  - string. The NFS server and uid/gid, [<uid>'.'<gid>'@']<host>. Note better to use IP address
 *               with 'dot' notation for <host>
 *    path     - string. Absolute path on the NFS server
 *    mntpoint - string. Local path. Please note that if "mntpoint" does not exist, it will be created
 *
 * Output:
 *    See the definition of NFS operation error codes
 */
int mountFileSystem(char *uidhost, char *path, char *mntpoint)
{
    /* check the input parameters */
    if (!uidhost || !uidhost[0])   return NFS_INVALID_HOST;
    if (!path || !path[0])         return NFS_INVALID_PATH;
    if (!mntpoint || !mntpoint[0]) return NFS_INVALID_MNTPOINT;

    /* mount the file system */
    if (nfsMount(uidhost, path, mntpoint) == OK) {     /* 0 - succeed; -1 - failed */
        save_restoreNFSOK    = 1;
        save_restoreIoErrors = 0;                      /* clean the counter */
        return NFS_SUCCESS;
    } else {
        save_restoreNFSOK = 0;
        return NFS_FAILURE;
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
    if (!mntpoint || !mntpoint[0]) return NFS_INVALID_MNTPOINT;

    /* unmount the file system */
    if (unmount(mntpoint) == OK) {                     /* 0 - succeed; -1 - failed */
        save_restoreNFSOK    = 0;
        return NFS_SUCCESS;
    } else {
        return NFS_FAILURE;
    }
}



