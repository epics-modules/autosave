/***********************************************
 * osdNfs.h
 * 
 * Dummy file NFS operation on hosts that don't support NFS client
 *
 ***********************************************/
#ifndef OSDNFS_H
#define OSDNFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* definition except for vxWorks */
#define OK     0
#define ERROR -1
#define logMsg errlogPrintf

/* NFS operation error codes */
#define NFS_SUCCESS          0           /* succeed */
#define NFS_FAILURE         -1           /* failure with unknown reasons */
#define NFS_INVALID_HOST     1           /* uid_gid_host parameter is invalid */
#define NFS_INVALID_PATH     2           /* path on the NFS server is invalid */
#define NFS_INVALID_MNTPOINT 3           /* mount point in invalid */

/* NFS operation definitions */
#define UIDSEP            '@'

/* routines for NFS operation */
int nfsMount(char *uidhost, char *path, char *mntpoint);          /* mount the NFS (details) */

int mountFileSystem(char *uidhost, char *path, char *mntpoint);   /* mount the NFS */
int dismountFileSystem(char *mntpoint);                           /* dismount the NFS */

#endif
