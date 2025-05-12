#ifndef NFS_UTILS_H
#define NFS_UTILS_H

#include "save_restore_common.h"
#include <epicsTime.h>

#define REMOUNT_CHECK_INTERVAL_SECONDS 60
extern char save_restoreNFSHostName[NFS_PATH_LEN];
extern char save_restoreNFSHostAddr[NFS_PATH_LEN];
extern char save_restoreNFSMntPoint[NFS_PATH_LEN];
extern volatile int save_restoreRemountThreshold;

extern char saveRestoreFilePath[NFS_PATH_LEN];

STATIC int do_mount();

int restore_mount(epicsTimeStamp remount_check_time, int *just_remounted);

int set_savefile_path_nfs();

int nfs_managed();

void save_restore_nfs_show();

#endif
