---
layout: default
title: Release Notes
nav_order: 3
---


autosave Release Notes
======================

### Unreleased

#### Breaking changes

- Changes to `set_savefile_path` handling:
  - No longer mounts NFS
  - If the mountpoint (third argument) is not specified by `save_restoreSet_NFSHost`,
    then you must call `set_savefile_path` before `save_restoreSet_NFSHost`.

#### Fixes

- Fix segmentation faults that can occur when saving long strings

### 5.11

- Resource leak fixes
- Proper restoration of long strings
- Int64 Support Added
- Proper handling of epicsStrPrintEscaped if it doesn't write anything
- Added version flag (-v) to asVerify
- Fixed incorrect error checking in fdbrestore


### 5.10.2

- IOC shell files are now installed to the top level iocsh folder from the asApp/iocsh folder

### 5.10.1

- Converted adl files to bob and edl.
- Warn user if requestFileCmd is too long
- Handle both one and two character end of lines

### 5.10

- Added the user callable function save\_restoreSet\_periodicDatedBackups(periodInMinutes).
- Added PVs SR\_disable and SR\_disableMaxSecs.
- Fixed problem with new versions of base (7.x, 3.15.6). Empty long string fields were being saved as the string "0", rather than an empty string.

### 5.9

- Added op/ui/autoconvert directory.
- New version of \*Main.cpp file from base 3.15.5; contains epicsExit() after iocsh(NULL) which is needed for epicsAtExit() to work on Windows
- Use dbLoadRecordsHook by default. (This enables automated build of autosave request files.)

### 5.8

- Modified save\_restore.c to create message queue before returning from create\_data\_set().
- Modified configMenuSub.c to defend against .cfg file with no "currDesc" PV.
- Don't try to set file permissions for vxWorks Added top level directory, iocsh, and added the scripts autosaveBuild.iocsh, configMenu.iocsh, autosave\_settings.iocsh, and save\_restore.iocsh which are callable by iocshLoad(). (See [IOC Shell Scripts](https://github.com/epics-modules/xxx/wiki/IOC-Shell-Scripts), for more information.)
- Martin Konrad's fix to allow all legal PV names in an autosave-request file.
- Keenan Lang: Added callable iocsh scripts. Further information about callable iocsh scripts can be found in the [xxx module wiki](https://github.com/epics-modules/xxx/wiki/IOC-Shell-Scripts)
- marcio.paduan.donadio: Added function to print human readable error message case fchmod fails. fchmod should be called only if save\_restoreSet\_FilePermissions is used.
- marcio.paduan.donadio: save\_restoreStatus.db: Alarms are set by default in mbbo records when a failure/warning condition is met
- dbrestore.c: Removed dbVerify calls, because that function is not in EPICS 3.16.
- Modified .adl files so caQtDM conversion yields better results.
- use dbPut() for DBF\_NOACCESS fields in dbrestore:scaler\_restore. This fixes the problem of one-element waveform and aao records not being restored in 3.15.
- Modified configMenu\*.adl so that caQtDM translations performed better.

### 5.7.1

- Modified .adl files so they translate better to caQtDM.
- Modified .ui (caQtDM) files because adl2ui 3.9.3 and earlier don't honor direction=right.

### 5.7

- configMenu now supports an unlimited number of config files. This made it necessary to provide a new PV by which CA clients can directly write the config-file name.
- sprintf -&gt; epicsSnprintf, strcat -&gt; strncat, etc.
- speed up autosaveBuild by caching names of five most recently found/not-found request files.

### 5.6.2

- save\_restoreSet\_FilePermissions() wasn't working. open() wasn't setting file permissions, but fchmod() does.
- autosaveBuild was failing to correctly handle lines &gt; 119 significant characters. Increased max line length to 199.
- More string-length checking.

### 5.6.1

- asVerify can now be called from the IOC console

### 5.6

- Manual restore of array PV now uses channel access, rather than database access, so it behaves like manual restore of a scalar. (Previously, when configMenu restored an array to a VAL field, the field value didn't get posted, because database access doesn't cause record processing.)

### 5.5.1

- myDbLoadRecordsHook() now calls macEnvExpand() for the filename
- Defend against long lines in readReqFile: don't truncate string in the middle of a macro definition.

### 5.5

- In versions R5-4-\*, array restore could fail to parse lines longer than 120 characters. This is fixed.
- New function, autosaveBuild(), directs autosave to build request files automatically, from arguments supplied to dbLoadRecords().
- New functions, eraseFile() and appendToFile(), intended to work with autosaveBuild().

### 5.4.2

- set\_pass1\_restoreFile(), called from iocsh had wrong second argument.

### 5.4.1

- Previously, restoring an array which had been saved with zero or one values failed. Also, manual restore (including restore by configMenu) of any array PV caused a seg fault.
- Previously, manual restore (including restore by configMenu) of a long string PV neglected to restore the trailing null character.

### 5.4

- Can now do boot-time restore from a read-only .sav file located anywhere, and can do macro substitution on the file contents. This is intended to support a generic, default configuration for highly mobile iocs, such as those supporting areaDetector.
- Fixed bug in boot-time treatment of long-string values that are longer than 120 chars.

### 5.3.1

- configMenu: In building the list of .cfg files from the autosave directory, now require that .cfg file names begin with the config name followed by an underscore, and end with ".cfg". Previously, they were only required to begin with the config name (so config names SG and SG2 got mixed) and contain .cfg without also containing .cfg\_.

### 5.3

- Beginning with R5.0, autosave failed to detect and reject a truncated .sav file, and would segfault if the file was truncated in the middle of an array. Thanks to Jon Thompson (Diamond) for reporting the problem.
- In 5.1 through 5.2, "asVerify -r" was failing to write reboot files.

### 5.2

- In autosave 5.1 and 5.1.1, asVerify did not print anything if all PVs had the same values as in the supplied .sav file. Now asVerify goes back to the original behavior, printing, for example, "0 PVs differed. (8 PVs checked; 0 PVs not connected)".
- In autosave 5.1 and 5.1.1, the presence of the macro "CONFIG" anywhere in a request file, or in a create\_xxx\_set() command, told autosave to refrain from writing backup (".savB") and sequence (".savN") files. This didn't work very well, because you generally want to include configMenu\_settings.req in auto\_settings.req, and that requires the CONFIG macro. Now, autosave will refrain from backups and sequence files if the macro CONFIGMENU is defined with any value.

### 5.1.1

- Declare pass0List and pass1List extern in save\_restore.h

### 5.1

- In autosave versions 4-8 through 5-0, if the save\_restore task connected to a remote PV that later was disconnected, it would crash the next time that PV was used.
- Modified autosave to correctly restore unsigned long values. Thanks to Kukhee Kim (SSRL) for find and fixing the bug.
- configMenu: new support for manually saving, restoring, and verifying sets of PVs. configMenu operations can be driven by an EPICS record, or by any channel-access client. configMenu list can be populated from the existing config files on disk.
- Assignment of status PVs to save sets didn't account for save sets being removed.
- Manual restore failed to correctly handle strings longer than 39 characters, did not honor comments in .sav files, and failed to call ca\_clear\_channel.
- manual\_save() no longer takes an argument specifying how long to wait for the save to complete.
- Refactored asVerify so that the core can be called from a client wrapper and from the save\_restore task. Manual save and restore operations called from configMenu are verified.
- Previously, create\_XXX\_set() called with no request-file name created an empty PV list. That's fixed, and it's also ok to specify no arguments.
- Previously, the number of restore files was limited to eight. Now it is unlimited.
- Added file tr\_dirent.h, a replacement for the posix header dirent.h, which windows doesn't have.
- Modified parsing of "file" commands in request files. It's now ok to include the '#' character in a macro definition. Consequently, it's no longer legal to have a comment on the same line as a file command.
- use 24-hour time-format string

### 5.0

- Modified autosave and asVerify to handle long strings (PVs of type DBF\_STRING whose declared size is larger than the number of characters channel access will transport as a DBF\_STRING). Currently, only the first 120 characters of long strings are saved and restored. Request files must specify the PV name with a trailing '$' to have a PV treated as a long string; otherwise, only the first 39 characters of the string will be saved/restored.
- Fixed handling of arrays of DBF\_LONG, DBF\_ULONG for 64-bit architectures.
- Rewrote fGetDateStr using epicsTimeToStrftime.
- Modified autosave to build and run with either EPICS 3.15.0 or 3.14.

### 4.8

- Folded in modifications by Zheqiao Geng (SLAC): 
    - Introduced NFS remount function for RTEMS in case of too many file-write failures
    - Separated the NFS mounting code to src/os folder for vxWorks, RTEMS and Linux (added WIN32, cygwin, solaris, default)
    - Added timeout checking for PERIODIC or MONITORED callback functions
    - Enabled to reconnect the unconnected CA channels
    - Used ca\_create\_channel + callback instead of ca\_search
    - Added illegal pointer checking at callback functions
    - Added file checking after writing to the disk
    - Disconnected all CA channels before exiting the program
- check mount point before using it.
- Increased the size of info buffer for save list auto generation from database
- Include Michael Davidsaver's mods to handle info node of arbitrary length
- Added ZNAM, ONAM to records in saveRestoreStatus.db
- Don't require successful dismount as prerequisite for mount. Most likely case is that dismount will succeed on first try, remount will fail, and then dismount will fail from then on because the file system has already been dismounted.
- If array data is found in a .sav file, parse to end whether or not the array will be restored.
- Added .opi display files for CSS-BOY.
- Fixed wrong seek distance on WIN32.
- Compile-time option for Linux to manage or not manage NFS mount.
- Make makeAutosaveFiles() callable from the vxWorks shell. (Previously, it was callable only from iocsh.)

### 4.7

- Previously, array-valued PV's whose representation in a .sav file was exactly 119 bytes long would cause reboot\_restore to process the PV twice, the second time as a scalar with no restore data. In the case of an array of char, this resulted in the first element of the array being overwritten with a zero.

### 4.6

- Previously, autosave determined the element count of a PV in connect\_list(), never revisited it, and ignored PV's whose element counts had not been determined. This prevented autosave from saving PV's that failed to connect promptly.
- Don't search for info nodes if a record is an alias.

### 4.5

- Previously, autosave restored auto\_positions.sav and auto\_settings.sav if no save file had been specified in a call to set\_pass*n*\_restoreFile(). Now, autosave does not restore anything you don't tell it to restore.
- Can now build with tornado 2.2.2.
- save\_restore.c:write\_it() was calling open(), fdopen(), fclose(), and close(), in that order, and this worked routinely. However, some doc suggests close() should not be called, and we found a task suspended in the close() call, so the call has been deleted.

### 4.4

- If file close fails, retry once. (Previously, would retry up to 100 times.)
- If file write (including close) fails, retry after save\_restoreRetrySeconds (default value is 60 seconds). Previously, would retry only after the file system was remounted, or the next time the set was triggered.
- New function save\_restoreSet\_RetrySeconds(int seconds).
- Don't copy .sav file to .sav&lt;n&gt; (i.e., don't write sequence files) if .sav file write failed.
- new function save\_restoreSet\_UseStatusPVs(int ok)
- allow save-file name with more than one embedded '.' character.

### 4.3

- New function, save\_restoreSet\_FilePermissions(), allows control over permissions with which .sav files are created.
- create\_xxx\_set() can now specify PV names from which file path and file name are to be read, by including SAVENAMEPVand/or SAVEPATHPV in the macro string supplied as an argument. Note this can be done only on the first call to create\_xxx\_set()for a save set, because only the first call results in a call to readReqFile(), in which function the macro string is parsed. If either macro is specified, save\_restore will not maintain backup or sequence files for the save set.
- Don't print `errno` unless function returns an error.
- New functions `makeAutosaveFiles()`, and `makeAutosaveFileFromDbInfo()`, search the database for info nodes indicating fields to be autosaved.

### 4.2.1

- Added date/time to error messages printed to the console.
- If asVerify is directed to create a restore file, it now writes a trial restore file first, and overwrites the real restore file only if more than half of the PV's were actually connected.
- myPrintErrno modified to print the line and file of caller, and called only if there was an actual error.
- Don't hold epicsMutex for a long time, because any priority inheritance that occurs while the mutex is held will persist until the mutex is released, even if it's no longer needed. Now, the mutex is used to protect a variable which, in turn, protects save\_restore's save-set list.
- Use binary mode for fopen() calls in myFileCopy() (dbrestore.c) to avoid file-size differences resulting from different line terminators on different operating systems. (Thanks to Kay Kasemir for this fix.)
- EPICS 3.14.9 will no longer allow empty calc (special==SPC\_CALC) strings, so dbrestore now detects such strings in the .sav file and changes them to "0" before restoring.

### 4.2

- Added asVerify, a client-side tool to compare autosave .sav files with current PV values. Also can write a .sav file useable for restoring values.

### 4.1.3

- Debug macros made more compliant with various compilers.
- save\_restore made less sensitive to errors. Previously, any file write that set errno would abort the save, but this was just stupid, and caused way more problems than it solved.
- Status PV's were restricted to STRING\_LEN (30) characters, instead of PV\_NAME\_LEN (61).
- Increased stack size to epicsThreadStackBig.
- Don't treat unsupported fsync() as an error.

### 4.1.2

- Request-file parsing fixed. Previously, blank but not empty lines (e.g., containing a space character) seemed to contain the first word of the previous line, because name\[\] was not cleared before parsing a new line.
- Dirk Zimoch (SLS) found and fixed problems with .sav files that lack a header line, or a version number. Check return code from selected fseek() calls.

### 4.1.1

- Cleaned up asApp/src/Makefile so it did not put as\_registerRecordDeviceDriver in all applications using autosave.

### v4.1

- Allow user to comment out a line from a .sav file by beginning the line with '#'
- PV\_NAME\_LEN increased from 40 to 80
- Check that chid is not NULL before calling ca\_state()
- The test for valid .sav files now permits files to end either with "`<END>?`" or "`<END>??`", where "?" is any character. This allows dos files, which end with "\\r\\n", to be restored.
- Use epicsTime() instead of time(). Defend against partial-init failures
- Array support now understands max and curr number of elements, and is prepared to see the current number of elements change.
- Use epicsThreadStackSizeGet instead of hard-coded stack size.
- [cvs log](cvsLog.txt)

### v4.0

- Status PV's have been reworked. Now you don't have to modify either the database or the medm-display file to use your own save-set names.

### v3.5

- Sometime in the past, I made fdbrestore() and fdbrestoreX() unavailable to console users by making them macros. This is fixed.
- Beginning with version 3.5, arrays can be saved and restored much as scalar PV's. Arrays are read and written using run-time database access, which implies that they can only be written during pass 1 of the restore, and that you cannot save the value of a non-local array PV. (Scalar PV's are read using channel access and normally are written using static database access.)
- Beginning with v3.5, scalar PV's which have type DBF\_NOACCESS in the .dbd file, and are set to some other DBF type during record initialization, can be restored in pass 1. (Some fields of the genSub record behave in this way, and previous versions of save\_restore could not restore them.)
- Prior to v3.5, PV's were listed 'backwards' in the save file -- i.e., the first PV in the request file became the last PV in the save file. Now, both files have the same ordering.

### save\_restore v3.3; dbrestore v3.3

- In versions earlier than 3.3, save\_restore's job was simply to save parameters through a reboot. The file server to which save files were written was assumed competent to protect those files, and when the server said a .sav file was safe, save\_restore believed it. Beginning with 3.3, the file server is viewed as the enemy: save\_restore expects it to lie about file status, to suddenly stop recognizing previously valid file handles, and to return error codes that don't make sense. Save\_restore can now defend its NFS mount against a server reboot, defend parameter values against a server power failure, and attempts to defend against a user messing around with the directory into which save files are being written.
- Save\_restore now allows you to change, at run time, the directory and/or the server to which save files are written. Here's an example: 

    ```
    	# The normal save_restore directory setup, in st.cmd: 
    	save_restoreSet_NFSHost("oxygen", "164.54.52.4")
    	set_savefile_path(startup, "autosave")
    	...
    	iocBoot()
    	...
    	(end of st.cmd)
    	...
    	# Type the following into the ioc's console
    	save_restoreSet_NFSHost("wheaties", "164.54.53.101")
    	# You'll get an immediate error if the old save file path doesn't
    	# exist, or has adverse permissions, on the new server.
    	set_savefile_path("/local/autosave", "")
    	# save_restore will immediately attempt to mount the new file path
    
    ```
    
    The above code assumes the ioc has permission to mount file systems on the host ('wheaties', in this case) and that the ioc has read/write/execute permission to the directory in which it is supposed to write .sav files (in this case, '/local/autosave'). Neither of these conditions are likely to be met by default; you must take action to arrange them.
    
    Save\_restore cannot defend against adverse file permissions, or against the save directory (NFS mount point) disappearing altogether.
- Several variables and user-callable functions have been renamed, as their meanings have changed. In preparation for 3.14, most user-settable variables can now be set by a function call. Here's a list of variables and functions added, deleted, or changed:  

    | OLD | NEW |
    |---|---|
    | sr\_save\_incomplete\_sets\_ok | save\_restoreIncompleteSetsOk |
    | sr\_restore\_incomplete\_sets\_ok | save\_restoreIncompleteSetsOk |
    | --no comparable function-- | save\_restoreSet\_IncompleteSetsOk(value) |
    | reboot\_restoreDebug | save\_restoreDebug |
    | --no comparable function-- | save\_restoreSet\_Debug(value) |
    | reboot\_restoreDatedBu | save\_restoreDatedBackupFiles |
    | --no comparable function-- | save\_restoreSet\_DatedBackupFiles(value) |
    | --no comparable variable-- | save\_restoreNumSeqFiles |
    | --no comparable function-- | save\_restoreSet\_NumSeqFiles(value) |
    | --no comparable variable-- | save\_restoreSeqPeriodInSeconds |
    | --no comparable function-- | save\_restoreSet\_SeqPeriodInSeconds(value) |
    | NOTE: if save\_restoreNumSeqFiles &gt; 0, redundant .sav file will be written every save\_restoreSeqPeriodInSeconds seconds |
    | --no comparable function-- | dbrestoreShow() |
    | fdblist() | save\_restoreShow(verbose) |
    | NOTE: save\_restoreShow() calls dbrestoreShow() |
    | save\_restore\_test\_fopen | --no comparable variable-- |
    | NOTE: test is no longer done |
    | create\_periodic\_set(file,period) | create\_periodic\_set(file,period,macro) |
    | create\_triggered\_set(file,trigPV) | create\_triggered\_set(file,trigPV,macro) |
    | create\_monitor\_set(file,period) | create\_monitor\_set(file,period,macro) |
    | create\_manual\_set(file) | create\_manual\_set(file,macro) |
    | reload\_periodic\_set(file,period) | reload\_periodic\_set(file,period,macro) |
    | reload\_triggered\_set(file,trigPV) | reload\_triggered\_set(file,trigPV,macro) |
    | reload\_monitor\_set(file,period) | reload\_monitor\_set(file,period,macro) |
    | reload\_manual\_set(file) | reload\_manual\_set(file,macro) |
    | NOTE: macro string is optional and supplements any macro strings defined in request files. You can only reload one set at a time; you must wait for a reload to complete before starting another one. |
    | NOTE: IN VERSIONS EARLIER THAN 2.7, Time PERIODS WERE SPECIFIED USING FLOATING POINT NUMBERS. THEY ARE NOW SPECIFIED AS INTEGERS (IN SECONDS). |
    | set\_savefile\_path(path) | set\_savefile\_path(path,pathsub) |
    | set\_requestfile\_path(path) -&gt; set\_requestfile\_path(path,pathsub) |
    | NOTE: if pathsub is specified, it is appended to path. The purpose of this change was to make it easier to use variabled defined in cdCommands as path elements. Example: set\_requestfile\_path(std,"stdApp/Db") |
    | --no comparable function-- | save\_restoreSet\_status\_prefix(value) |
    | NOTE: save\_restore expects PV's with which it can display its status. Status PV's are request-file specific; the supplied database contains PV's for auto\_positions.req and auto\_settings.req only. If you use other request files, you can add your own PV's. It's OK if the PV's don't exist, but you will get informational messages when their absence is noticed. |
    | --no comparable function-- | save\_restoreSet\_NFSHost(host,IPaddress) |
    | --no comparable variable-- | save\_restoreRemountThreshold |
    | NOTE: If save\_restore is managing its own NFS mount (i.e., if calls to save\_restoreSet\_NFSHost() and set\_savefile\_path() succeeded) it will dismount and remount after save\_restoreRemountThreshold consecutive I/O errors (e.g. to fix a stale file handle). |
    
    
- Save\_restore now maintains status PV's for each PV list, and rolled-up status PV's for overall save status and reboot status.
- fdblist() renamed as save\_restoreShow(), now enhanced with more status information, including rolled-up reboot status.
- Save\_restore can now maintain up to ten "sequence files" (named \*.savX, where X is a digit in \[0..(save\_restoreNumSeqFiles-1)\]), which are copies of the most recent .sav or .savB file, copied at user-specified intervals. If no valid .sav or .savB file exists, save\_restore will write sequence files directly from its PV list, as though it were writing a .sav file. At boot time, if .sav and .savB are both corrupt or missing, save\_restore will restore from the most recent sequence file.
- Save\_restore no longer tests fopen() by writing a temporary file, before saving.
- Save\_restore will manage its own NFS mount, if the host name and address has been specified with a call to save\_restoreSet\_NFSHost(), and the save-file path (used as the mount point) has been set by a call to set\_savefile\_path(). When more than 'save\_restoreRemountThreshold' consecutive, unexpected I/O errors (the default number is ten) have occurred, save\_restore will attempt to remount the file system.
- Previously, save\_restore saved floats and doubles using an algorithm that was prone to failure, especially for PV's from another ioc. (Thanks to Mark Rivers for finding and fixing this bug.)
- Now use errlogPrintf() for all messages except those from callback routines, where logMsg() is used.
- All CA monitors are now set up by the save\_restore task. Previously, some were setup by whatever task called create\_XXX\_set().
- Previously, if a save-file write failed, it would not be retried until the normal conditions (e.g., a PV's value changed) were again met. Now, if save\_restore is managing its own NFS mount, and enough I/O errors have accumulated to cause an NFS remount, all save sets in failure are written immediately after the remount succeeds.
- Some ca\_pend timeouts adjusted or limited. Previous timeouts were inappropriate if many thousands of PV's were specified in a request file.
- Save files now contain the time at which they were written, in the format of fGetDateStr() -- yymmdd-hhmmss.
- Dated backup files now have an underscore between the name and the date.
- Previously, if the second fopen() call in myFileCopy() failed, the function would return without closing the first file.
- Previously, save\_restore would overwrite corrupted files in the normal course of operation. Now, whenever a corrupted file is encountered, a copy is made. If the copy is made at restore time, the copy is named, e.g., auto\_settings.sav\_RBAD\_030818-134850; if at save time, it's named, e.g., auto\_settings.sav\_SBAD\_030818-134850.
- save\_restore no longer uses the vxWorks 'copy' command for anything, because it writes files with inconvenient permissions.

### save\_restore v2.9a; dbrestore v2.7

- Previously, create\_data\_set() would return without releasing a semaphore (causing the save\_restore task to hang) if called with save\_method==TRIGGERED, but no trigger channel was specified.
- Previously, fdbrestore() would return without releasing a semaphore (causing the save\_restore task to hang) if called with the name of a save set currently being maintained, and the save list contained unconnected PV's, and the variable 'sr\_restore\_incomplete\_sets\_ok' was false.
- Added macro-string argument to create\_xxx\_set()
- Added argument 'verbose' to fdblist()
- Added second argument, 'subpath', to set\_XXXfile\_path(). This allows the caller to pass the path as two args to be concatenated, making it easier to build the path string using a variable set in the cdCommands file.
- Use logMsg() instead of epicsPrintf() for several informational messages.
- The variables chlist and reqFilePathList are now private.
- Increased ca\_pend\_io search timeout in connect\_list() from 2 to 5 seconds. Decreased fetch timeout from num\_channels/10 to 10 seconds.
- Decreased ca\_pend\_io clear-channel timeout from num\_channels/10 to 10 seconds in remove\_data\_set().
- Changed call to macParseDefns() to specify the macro-substitution handle.

### save\_restore v2.7; dbrestore v2.7

| __ATTENTION__ |
|---|
| This version is incompatible in one important respect with versions of save\_restore.c numbered lower than 2.6:  
The functions - create\_periodic\_set() - create\_monitor\_set() - reload\_periodic\_set() - reload\_monitor\_set()  now take 'int' arguments to specify time periods, instead of 'double' arguments. This change was required for compatibility with PPC processors. |

- In Frank Lenkszus' version, the calls set\_pass\_restoreFile() were required to specify files to be restored. In this version, if no restore files have been specified, initHooks specifies the default files auto\_positions.sav and auto\_settings.sav for you, for backward compatibility with my previous version. If you really don't want to restore any files, you now must either not load initHooks, or replace it with your own version. 
- In Frank Lenkszus' version, set\_requestfile\_path() could specify only a single directory. Now that include files are a possibility, you can specify several request-file directories by calling set\_requestfile\_path() several times.
 - Backup files made as part of a restore operation (i.e., files ending in ".bu", or "YYMMDD-HHMMSS") are no longer created using the VxWorks "copy" command.

 Suggestions and Comments to:   
 [Tim Mooney ](mailto:mooney@aps.anl.gov): (mooney@aps.anl.gov)
