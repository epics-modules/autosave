/***********************************************
 * osdNfs.h
 * 
 * Linux specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#ifndef OSDNFS_H
#define OSDNFS_H

/* header files for Linux */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define NFS_PATH_LEN 128                /* string length for NFS related path */

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

/* routines for NFS operation */

int mountFileSystem(char *uidhost, char *path, char *mntpoint);   /* mount the NFS */
int dismountFileSystem(char *mntpoint);                           /* dismount the NFS */

#endif
