/***********************************************
 * osdNfs.h
 * 
 * vxWorks specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#ifndef OSDNFS_H
#define OSDNFS_H

/* header files for vxWorks */
#include <vxWorks.h>
#include <hostLib.h>
#include <stdioLib.h>
#include <ioLib.h>
#include <string.h>

#define NFS_PATH_LEN 255 /* string length for NFS related path */

/* nfsDrv.h was renamed nfsDriver.h in Tornado 2.2.2 */
/* #include <nfsDrv.h> */
extern STATUS nfsMount(char *host, char *fileSystem, char *localName);
extern STATUS nfsUnmount(char *localName);
extern int logMsg(char *fmt, ...);

/* NFS operation error codes */
#define NFS_SUCCESS 0          /* succeed */
#define NFS_FAILURE -1         /* failure with unknown reasons */
#define NFS_INVALID_HOST 1     /* uid_gid_host parameter is invalid */
#define NFS_INVALID_PATH 2     /* path on the NFS server is invalid */
#define NFS_INVALID_MNTPOINT 3 /* mount point in invalid */

/* routines for NFS operation */
int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint); /* mount the NFS */
int dismountFileSystem(char *mntpoint);                                     /* dismount the NFS */

#endif
