/***********************************************
 * osdNfs.c
 * 
 * Realize the Linux specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#include "osdNfs.h"

/**
 * Global variables
 */
int save_restoreNFSOK = 1; /* for Linux, NFS has been mounted before autosave starts */
int save_restoreIoErrors =
    0; /* for accumulate the IO error numbers, when the number larger than threshold, remount NFS */
extern volatile int save_restoreDebug;

/**
 * Mount the NFS system (use the RTEMS nfsMount code as reference)
 */
int nfsMount(char *uidhost, char *path, char *mntpoint)
{
    struct stat st;
    int devl;
    char *host;    /* host string */
    int rval = -1; /* return value */
    char *dev = 0; /* buffer for the full source path */

    /* check inputs */
    if (!uidhost || !path || !mntpoint) {
        fprintf(stderr, "usage: nfsMount(\"[uid.gid@]host\",\"path\",\"mountpoint\")\n");
        return -1;
    }

    /* allocate buffer for the whole source */
    if (!(dev = malloc((devl = strlen(uidhost) + 20 + strlen(path) + 1)))) {
        fprintf(stderr, "nfsMount: out of memory\n");
        return -1;
    }

    /* Try to create the mount point if nonexistent */
    if (stat(mntpoint, &st)) {
        if (ENOENT != errno) {
            perror("nfsMount trying to create mount point - stat failed");
            goto cleanup;
        } else if (mkdir(mntpoint, 0777)) {
            perror("nfsMount trying to create mount point");
            goto cleanup;
        }
    }

    /* get the host name or IP address string */
    if (!(host = strchr(uidhost, UIDSEP))) {
        host = uidhost;
    } else {
        host++;
    }

    /* get the full source path */
    if (isdigit(*host)) {
        /* avoid using gethostbyname (for IP address) */
        sprintf(dev, "%s:%s", uidhost, path);
    } else {
        struct hostent *h;

        /* copy the uid part (hostname will be
         * overwritten) */
        strcpy(dev, uidhost);

        /* for string host name, get the IP address */
        h = gethostbyname(host);

        if (!h ||
            !inet_ntop(AF_INET, (struct in_addr *)h->h_addr_list[0], dev + (host - uidhost), devl - (host - uidhost))) {
            fprintf(stderr, "nfsMount: host '%s' not found\n", host);
            goto cleanup;
        }

        /* append ':<path>' */
        strcat(dev, ":");
        strcat(dev, path);
    }

    /* mount the NFS */
    printf("Trying to mount %s on %s\n", dev, mntpoint);

    if (mount(dev, mntpoint, "nfs", MS_SYNCHRONOUS, "rsize=8192,wsize=8192")) {
        perror("nfsMount - mount");
        goto cleanup;
    }

    rval = 0;

cleanup:
    free(dev);
    return rval;
}

#define MANAGE_MOUNT 0
int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint)
{
#if MANAGE_MOUNT
    /* check the input parameters */
    if (!uidhost || !uidhost[0]) return NFS_INVALID_HOST;
    if (!path || !path[0]) return NFS_INVALID_PATH;
    if (!mntpoint || !mntpoint[0]) return NFS_INVALID_MNTPOINT;
    /* mount the file system */
    if (nfsMount(uidhost, path, mntpoint) == OK) { /* 0 - succeed; -1 - failed */
        save_restoreNFSOK = 1;
        save_restoreIoErrors = 0; /* clean the counter */
        return OK;
    } else {
        save_restoreNFSOK = 0;
        return ERROR;
    }
#else
    printf("Autosave is not configured to manage the file-system mount point.\n");
    return (OK);
#endif
}

int dismountFileSystem(char *mntpoint)
{
#if MANAGE_MOUNT

    /* check the input parameters */
    if (!mntpoint || !mntpoint[0]) return NFS_INVALID_MNTPOINT;

    /* unmount the file system */
    save_restoreNFSOK = 0;
    if (umount(mntpoint) == 0) {
        return (OK);
    } else {
        return (ERROR);
    }

#else

    printf("Autosave is not configured to manage the file-system mount point.\n");
    return (OK);

#endif
}
