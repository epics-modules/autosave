#ifndef NFS_UTILS_H
#define NFS_UTILS_H

#include "osdNfs.h"
#include "save_restore_common.h"
#include <epicsTime.h>

#define REMOUNT_CHECK_INTERVAL_SECONDS 60
extern char save_restoreNFSHostName[NFS_PATH_LEN];
extern char save_restoreNFSHostAddr[NFS_PATH_LEN];
extern char save_restoreNFSMntPoint[NFS_PATH_LEN];
extern int saveRestoreFilePathIsMountPoint;
extern volatile int save_restoreRemountThreshold;

extern char saveRestoreFilePath[NFS_PATH_LEN];

STATIC int do_mount();

int restore_mount(epicsTimeStamp remount_check_time, int *just_remounted);

int set_savefile_path_nfs();

void save_restoreSet_NFSHost(char *hostname, char *address, char *mntpoint);

#endif
