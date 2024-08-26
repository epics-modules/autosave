/***********************************************
 * osdNfs.h
 *
 * RTEMS specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#ifndef OSDNFS_H
#define OSDNFS_H

/* header files for RTEMS */
#include <rtems.h>
#include <bsp.h>
#include <librtemsNfs.h>

#define NFS_PATH_LEN 255 /* string length for NFS related path */

/* definition except for vxWorks */
#define OK 0
#define ERROR -1
#define logMsg errlogPrintf

/* NFS operation error codes */
#define NFS_SUCCESS 0          /* succeed */
#define NFS_FAILURE -1         /* failure with unknown reasons */
#define NFS_INVALID_HOST 1     /* uid_gid_host parameter is invalid */
#define NFS_INVALID_PATH 2     /* path on the NFS server is invalid */
#define NFS_INVALID_MNTPOINT 3 /* mount point in invalid */

#if __RTEMS_MAJOR__ > 4 || (__RTEMS_MAJOR__ == 4 && __RTEMS_MINOR__ > 9) || \
    (__RTEMS_MAJOR__ == 4 && __RTEMS_MINOR__ == 9 && __RTEMS_REVISION__ == 99)
/* Dropped from librtemsNfs.h in RTEMS 4.10, now defined in EPICS */
int nfsMount(char *uidhost, char *path, char *mntpoint);
#endif

/* routines for NFS operation */
int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint); /* mount the NFS */
int dismountFileSystem(char *mntpoint);                                     /* dismount the NFS */

#endif
