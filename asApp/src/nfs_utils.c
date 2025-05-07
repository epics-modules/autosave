#include <iocsh.h>
#include <epicsExport.h>
#include <epicsStdio.h>

#include "osdNfs.h"
#include "save_restore_common.h"
#include "nfs_utils.h"

char save_restoreNFSHostName[NFS_PATH_LEN] = "";
char save_restoreNFSHostAddr[NFS_PATH_LEN] = "";
char save_restoreNFSMntPoint[NFS_PATH_LEN] = "";
int saveRestoreFilePathIsMountPoint = 1;
volatile int save_restoreRemountThreshold = 10;

epicsExportAddress(int, save_restoreRemountThreshold);

STATIC int do_mount()
{
    if (mountFileSystem(save_restoreNFSHostName, save_restoreNFSHostAddr, save_restoreNFSMntPoint,
                        save_restoreNFSMntPoint) == OK) {
        /*
         * NOTE: If NFS mounts are not supported this will take this
         * code path (i.e. it returns OK [=0] and prints this message)
         */
        printf("save_restore:mountFileSystem:successfully mounted '%s'\n", save_restoreNFSMntPoint);
        return OK;
    }
    printf("save_restore: Can't mount '%s'\n", save_restoreNFSMntPoint);
    return ERROR;
}

int restore_mount(epicsTimeStamp remount_check_time, int *just_remounted)
{
    epicsTimeStamp currTime;
    double timeDiff;

    epicsTimeGetCurrent(&currTime);

    timeDiff = epicsTimeDiffInSeconds(&currTime, &remount_check_time);
    if (timeDiff > REMOUNT_CHECK_INTERVAL_SECONDS) {
        remount_check_time = currTime; /* struct copy */
        printf("save_restore: attempting to remount filesystem\n");
        dismountFileSystem(save_restoreNFSMntPoint); /* first dismount it */
        /* We don't care if dismountFileSystem fails.
         * It could fail simply because an earlier dismount, succeeded.
         */
        if (mountFileSystem(save_restoreNFSHostName, save_restoreNFSHostAddr, save_restoreNFSMntPoint,
                            save_restoreNFSMntPoint) == OK) {
            *just_remounted = 1;
            printf("save_restore: remounted '%s'\n", save_restoreNFSMntPoint);
        } else {
            printf("save_restore: failed to remount '%s' \n", save_restoreNFSMntPoint);
            return ERROR;
        }
    }
    return OK;
}

int set_savefile_path_nfs()
{
    if (mountFileSystem(save_restoreNFSHostName, save_restoreNFSHostAddr, save_restoreNFSMntPoint,
                        save_restoreNFSMntPoint) == OK) {
        printf("save_restore:mountFileSystem:successfully mounted '%s'\n", save_restoreNFSMntPoint);
        return OK;
    }
    printf("save_restore: Can't mount '%s'\n", save_restoreNFSMntPoint);
    return ERROR;
}

void save_restoreSet_NFSHost(char *hostname, char *address, char *mntpoint)
{
    /* If file system is mounted (save_restoreNFSOK) and we mounted it (save_restoreNFSMntPoint[0]),
     * then dismount, presuming that caller wants us to remount from new information.  If we didn't
     * mount it, presume that caller did, and that caller wants us to manage the mount point.
     */
    if (save_restoreNFSOK && save_restoreNFSMntPoint[0]) dismountFileSystem(save_restoreNFSMntPoint);

    /* get the settings */
    strNcpy(save_restoreNFSHostName, hostname, NFS_PATH_LEN);
    strNcpy(save_restoreNFSHostAddr, address, NFS_PATH_LEN);
    if (mntpoint && mntpoint[0]) {
        saveRestoreFilePathIsMountPoint = 0;
        strNcpy(save_restoreNFSMntPoint, mntpoint, NFS_PATH_LEN);
        if (saveRestoreFilePath[0]) {
            /* If we already have a file path, make sure it begins with the mount point. */
            if (strstr(saveRestoreFilePath, save_restoreNFSMntPoint) != saveRestoreFilePath) {
                concatenate_paths(saveRestoreFilePath, save_restoreNFSMntPoint, saveRestoreFilePath);
            }
        }
    } else if (saveRestoreFilePath[0]) {
        strNcpy(save_restoreNFSMntPoint, saveRestoreFilePath, NFS_PATH_LEN);
        saveRestoreFilePathIsMountPoint = 1;
    }

    /* mount the file system */
    do_mount();
}

int nfs_managed()
{
    return save_restoreNFSHostName[0] && save_restoreNFSHostAddr[0] && save_restoreNFSMntPoint[0];
}

void save_restore_nfs_show()
{
    printf("  NFS host: '%s'; address:'%s'\n", save_restoreNFSHostName, save_restoreNFSHostAddr);
    printf("  NFS mount point:\n    '%s'\n", save_restoreNFSMntPoint);
    printf("  NFS mount status: %s\n", save_restoreNFSOK ? "Ok" : "Failed");
}

IOCSH_ARG save_restoreSet_NFSHost_Arg0 = {"hostname", iocshArgString};
IOCSH_ARG save_restoreSet_NFSHost_Arg1 = {"address", iocshArgString};
IOCSH_ARG save_restoreSet_NFSHost_Arg2 = {"mntpoint", iocshArgString};
IOCSH_ARG_ARRAY save_restoreSet_NFSHost_Args[3] = {&save_restoreSet_NFSHost_Arg0, &save_restoreSet_NFSHost_Arg1,
                                                   &save_restoreSet_NFSHost_Arg2};
IOCSH_FUNCDEF save_restoreSet_NFSHost_FuncDef = {"save_restoreSet_NFSHost", 3, save_restoreSet_NFSHost_Args};
static void save_restoreSet_NFSHost_CallFunc(const iocshArgBuf *args)
{
    save_restoreSet_NFSHost(args[0].sval, args[1].sval, args[2].sval);
}

void save_restoreNFSRegister(void)
{
    iocshRegister(&save_restoreSet_NFSHost_FuncDef, save_restoreSet_NFSHost_CallFunc);
}

epicsExportRegistrar(save_restoreNFSRegister);
