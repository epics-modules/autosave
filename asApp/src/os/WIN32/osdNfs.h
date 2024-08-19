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

#define NFS_PATH_LEN 255 /* string length for NFS related path */

/* definition except for vxWorks */
#define OK 0
#define ERROR -1
#define logMsg errlogPrintf

int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint); /* mount the NFS */
int dismountFileSystem(char *mntpoint);                                     /* dismount the NFS */

#endif
