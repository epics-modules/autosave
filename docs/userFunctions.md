---
layout: default
title: User Functions
nav_order: 4
---


# Autosave Restore: User-Callable Functions

This document provides detailed information about all user-callable functions in the Autosave module. These functions allow you to configure, control, and monitor the save and restore operations.

## Table of Contents

- [Configuration Functions](#configuration-functions)
- [Save Set Management Functions](#save-set-management-functions)
- [File Path Functions](#file-path-functions)
- [Restore Functions](#restore-functions)
- [Status and Debug Functions](#status-and-debug-functions)
- [autosaveBuild Functions](#autosavebuild-functions)
- [Verification Functions](#verification-functions)

---

## Configuration Functions

### save_restoreSet_DatedBackupFiles

**Purpose**: Controls whether backup files are dated or overwritten.

**Parameters**:
- `ok`: If non-zero, backup files will be named with date-time stamps (e.g., `xxx.sav_YYMMDD-HHMMSS`). If zero, backup files will be named `xxx.sav.bu` and overwritten on each reboot.

**Notes**:
- Dated backups are not overwritten
- If a save file is restored in both pass 0 and pass 1, the backup file is written only during pass 0

**Example**:
```c
// void save_restoreSet_DatedBackupFiles(int ok)
save_restoreSet_DatedBackupFiles(1);  // Enable dated backup files
```

### save_restoreSet_periodicDatedBackups

**Purpose**: Enables periodic creation of dated backup files at a specified interval.

**Parameters**:
- `periodInMinutes`: Interval in minutes between dated backups. If greater than 0, periodic dated backups are enabled. If 0 or negative, they are disabled.

**Notes**:
- Requires `save_restoreSet_DatedBackupFiles(1)` to also be set
- Useful for maintaining a time-series of backup files without relying solely on reboot-triggered backups

**Example**:
```c
// void save_restoreSet_periodicDatedBackups(int periodInMinutes)
save_restoreSet_periodicDatedBackups(60);  // Create a dated backup every hour
```

### save_restoreSet_NumSeqFiles

**Purpose**: Sets the number of sequenced backup files to maintain.

**Parameters**:
- `numSeqFiles`: Number of sequence files (0-10)

**Notes**: Sequence files are named `xxx.sav0`, `xxx.sav1`, etc.

**Example**:
```c
// void save_restoreSet_NumSeqFiles(int numSeqFiles)
save_restoreSet_NumSeqFiles(3);  // Maintain 3 sequence files
```

### save_restoreSet_SeqPeriodInSeconds

**Purpose**: Sets the time interval between sequenced backups.

**Parameters**:
- `period`: Time in seconds (minimum 10)

**Example**:
```c
// void save_restoreSet_SeqPeriodInSeconds(int period)
save_restoreSet_SeqPeriodInSeconds(600);  // 10 minutes between sequence files
```

### save_restoreSet_RetrySeconds

**Purpose**: Sets the delay between a failed save-file write and the retry.

**Parameters**:
- `seconds`: Time in seconds (minimum 10, default 60)

**Example**:
```c
// void save_restoreSet_RetrySeconds(int seconds)
save_restoreSet_RetrySeconds(60);  // Retry failed writes after 60 seconds
```

### save_restoreSet_CallbackTimeout

**Purpose**: Sets the time interval between forced save-file writes.

**Parameters**:
- `timeout`: Time in seconds (-1 means never force writes)

**Notes**: This ensures files are written even if the normal trigger mechanism fails.

**Example**:
```c
// void save_restoreSet_CallbackTimeout(int timeout)
save_restoreSet_CallbackTimeout(-1);  // Disable forced writes
```

### save_restoreSet_CAReconnect

**Purpose**: Controls whether autosave periodically retries connecting to PVs.

**Parameters**:
- `ok`: If non-zero, autosave will retry connecting to PVs every 60 seconds

**Example**:
```c
// void save_restoreSet_CAReconnect(int ok)
save_restoreSet_CAReconnect(1);  // Enable reconnection attempts
```

### save_restoreSet_IncompleteSetsOk

**Purpose**: Controls behavior when PV connections are missing.

**Parameters**:
- `ok`: If non-zero (default), save files will be restored even if incomplete, and will be overwritten even if some PVs are disconnected

**Example**:
```c
// void save_restoreSet_IncompleteSetsOk(int ok)
save_restoreSet_IncompleteSetsOk(1);  // Allow incomplete sets
```

### save_restoreSet_FilePermissions

**Purpose**: Specifies file permissions for new .sav files.

**Parameters**:
- `permissions`: File permission bits (e.g., 0640)

**Notes**: This value is passed directly to `open()` and `fchmod()`

**Example**:
```c
// void save_restoreSet_FilePermissions(int permissions)
save_restoreSet_FilePermissions(0640);  // rw-r-----
```

### save_restoreSet_NFSHost

**Purpose**: Specifies the NFS host for file operations.

**Parameters**:
- `hostname`: Host name
- `address`: IP address

**Notes**:
- Only available on vxWorks and RTEMS
- Allows autosave to manage its own NFS mount
- Enables recovery from stale file handles

**Example**:
```c
// void save_restoreSet_NFSHost(char *hostname, char *address)
save_restoreSet_NFSHost("oxygen", "164.54.49.4");
```

### save_restoreSet_status_prefix

**Purpose**: Sets the prefix for status PVs.

**Parameters**:
- `prefix`: Prefix string (e.g., "xxx:")

**Notes**:
- Must be called before the first `create_xxx_set()`
- Requires loading `save_restoreStatus.db` with the same prefix

**Example**:
```c
// void save_restoreSet_status_prefix(char *prefix)
save_restoreSet_status_prefix("xxx:");
```

### save_restoreSet_UseStatusPVs

**Purpose**: Controls whether status PVs are used.

**Parameters**:
- `ok`: If non-zero, status PVs will be updated

**Notes**: Should be called before the first `create_xxx_set()`

**Example**:
```c
// void save_restoreSet_UseStatusPVs(int ok)
save_restoreSet_UseStatusPVs(1);  // Enable status PVs
```

### set_saveTask_priority

**Purpose**: Sets the priority of the save_restore task.

**Parameters**:
- `priority`: Task priority value

**Returns**: Status (0 for success)

**Example**:
```c
// int set_saveTask_priority(int priority)
set_saveTask_priority(50);
```

---

## Save Set Management Functions

### create_monitor_set

**Purpose**: Creates a save set that writes files when PVs change.

**Parameters**:
- `request_file`: Name of the request file
- `period`: Time in seconds between checks for changes
- `macrostring`: Optional macro substitutions (e.g., "P=xxx:")

**Returns**: Status (0 for success)

**Notes**:
- Can be called any time after iocInit
- If called with no arguments, starts the save task without creating a save set
- The macro string can include SAVEPATHPV and SAVENAMEPV to override default paths/names

**Example**:
```c
// int create_monitor_set(char *request_file, int period, char *macrostring)
create_monitor_set("auto_settings.req", 30, "P=xxx:");
```

### create_periodic_set

**Purpose**: Creates a save set that writes files at regular intervals.

**Parameters**:
- `request_file`: Name of the request file
- `period`: Time in seconds between saves
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

**Example**:
```c
// int create_periodic_set(char *request_file, int period, char *macrostring)
create_periodic_set("auto_settings.req", 300, "P=xxx:");  // Save every 5 minutes
```

### create_triggered_set

**Purpose**: Creates a save set that writes files when a trigger PV changes.

**Parameters**:
- `request_file`: Name of the request file
- `trigger_channel`: Name of the PV that triggers saves
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

**Example**:
```c
// int create_triggered_set(char *request_file, char *trigger_channel, char *macrostring)
create_triggered_set("auto_settings.req", "xxx:saveTrigger", "P=xxx:");
```

### create_manual_set

**Purpose**: Creates a save set that writes files only when manually requested.

**Parameters**:
- `request_file`: Name of the request file
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

**Example**:
```c
// int create_manual_set(char *request_file, char *macrostring)
create_manual_set("auto_settings.req", "P=xxx:");
```

### manual_save

**Purpose**: Manually saves the current PV values for a request file.

**Parameters**:
- `request_file`: Name of the request file

**Returns**: Status (0 for success)

**Example**:
```c
// int manual_save(char *request_file)
manual_save("auto_settings.req");
```

### manual_save (extended)

**Purpose**: Extended version of manual_save with callback support.

**Parameters**:
- `request_file`: Name of the request file
- `save_file`: Optional alternative save file name
- `callbackFunction`: Function to call when save completes
- `puser`: User pointer passed to callback

**Returns**: Status (0 for success)

**Notes**: Used by configMenu implementation

### reload_manual_set

**Purpose**: Changes the PVs associated with a manual save set.

**Parameters**:
- `request_file`: Name of the request file
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

**Example**:
```c
// int reload_manual_set(char *request_file, char *macrostring)
reload_manual_set("auto_settings.req", "P=xxx:");
```

### reload_monitor_set

**Purpose**: Changes the PVs and period for a monitor save set.

**Parameters**:
- `request_file`: Name of the request file
- `period`: New time period in seconds
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

**Example**:
```c
// int reload_monitor_set(char *request_file, int period, char *macrostring)
reload_monitor_set("auto_settings.req", 60, "P=xxx:");
```

### reload_periodic_set

**Purpose**: Changes the PVs and period for a periodic save set.

**Parameters**:
- `request_file`: Name of the request file
- `period`: New time period in seconds
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

### reload_triggered_set

**Purpose**: Changes the PVs and trigger for a triggered save set.

**Parameters**:
- `request_file`: Name of the request file
- `trigger_channel`: New trigger PV name
- `macrostring`: Optional macro substitutions

**Returns**: Status (0 for success)

### remove_data_set

**Purpose**: Deletes a save set.

**Parameters**:
- `request_file`: Name of the request file

**Returns**: Status (0 for success)

**Example**:
```c
// int remove_data_set(char *request_file)
remove_data_set("auto_settings.req");
```

### set_savefile_name

**Purpose**: Changes the save file name for an existing save set.

**Parameters**:
- `request_file`: Name of the request file
- `save_file`: New save file name

**Returns**: Status (0 for success)

**Example**:
```c
// int set_savefile_name(char *request_file, char *save_file)
set_savefile_name("auto_settings.req", "custom_settings.sav");
```

---

## File Path Functions

### set_requestfile_path

**Purpose**: Specifies directories to search for request files.

**Parameters**:
- `path`: Base path
- `pathsub`: Optional subdirectory

**Returns**: Status (0 for success)

**Notes**:
- Multiple directories can be specified by calling this function multiple times
- Directories are searched in the order specified
- If never called, the current working directory is searched

**Example**:
```c
// int set_requestfile_path(char *path, char *pathsub)
set_requestfile_path(startup, "");
set_requestfile_path(startup, "autosave");
```

### set_savefile_path

**Purpose**: Specifies the directory for save files.

**Parameters**:
- `path`: Base path
- `pathsub`: Optional subdirectory

**Returns**: Status (0 for success)

**Notes**:
- If autosave is managing its own NFS mount, this function specifies the mount point
- Can be called at any time

**Example**:
```c
// int set_savefile_path(char *path, char *pathsub)
set_savefile_path(startup, "autosave");
```

---

## Restore Functions

### set_pass0_restoreFile

**Purpose**: Specifies a file to restore before record initialization.

**Parameters**:
- `save_file`: Name of the save file
- `macroSubstitutions`: Optional macro substitutions

**Returns**: Status (0 for success)

**Notes**:
- Multiple files can be specified by calling this function multiple times
- If the file name begins with "/", the path is used as specified
- Otherwise, the path from `set_savefile_path()` is prepended

**Example**:
```c
// int set_pass0_restoreFile(char *save_file, char *macroSubstitutions)
set_pass0_restoreFile("auto_positions.sav", "P=xxx:");
```

### set_pass1_restoreFile

**Purpose**: Specifies a file to restore after record initialization.

**Parameters**:
- `save_file`: Name of the save file
- `macroSubstitutions`: Optional macro substitutions

**Returns**: Status (0 for success)

**Example**:
```c
// int set_pass1_restoreFile(char *save_file, char *macroSubstitutions)
set_pass1_restoreFile("auto_settings.sav", "P=xxx:");
```

### reboot_restore

**Purpose**: Restores PV values during iocInit.

**Parameters**:
- `save_file`: Name of the save file
- `init_state`: Current initialization state

**Returns**: Status (0 for success)

**Notes**: Should only be called from initHooks

### fdbrestore

**Purpose**: Restores PV values at runtime from a save set.

**Parameters**:
- `save_file`: Name of the save file

**Returns**: Status (0 for success)

**Notes**:
- Uses channel access, which may cause record processing
- Creates a backup file with ".bu" suffix

**Example**:
```c
// int fdbrestore(char *save_file)
fdbrestore("auto_settings.sav");
```

### fdbrestoreX

**Purpose**: Restores PV values at runtime from any file.

**Parameters**:
- `save_file`: Name of the save file

**Returns**: Status (0 for success)

**Notes**:
- File doesn't need to end with `<END>`
- No backup file is written

**Example**:
```c
// int fdbrestoreX(char *save_file)
fdbrestoreX("custom_settings.dat");
```

### fdbrestoreX (extended)

**Purpose**: Extended version with macro substitution and callback.

**Parameters**:
- `filename`: Name of the save file
- `macrostring`: Optional macro substitutions
- `callbackFunction`: Function to call when restore completes
- `puser`: User pointer passed to callback

**Returns**: Status (0 for success)

**Notes**: Used by configMenu implementation

---

## Status and Debug Functions

### save_restoreSet_Debug

**Purpose**: Sets the debug level for console messages.

**Parameters**:
- `debug_level`: Debug level (0 = minimal output)

**Example**:
```c
// void save_restoreSet_Debug(int debug_level)
save_restoreSet_Debug(2);  // Verbose debugging
```

### save_restoreShow

**Purpose**: Lists save sets being managed by the save_restore task.

**Parameters**:
- `verbose`: If non-zero, lists PVs as well

**Example**:
```c
// void save_restoreShow(int verbose)
save_restoreShow(1);  // Show all save sets and their PVs
```

### getMacroString

**Purpose**: Retrieves the macro string used with a request file.

**Parameters**:
- `request_file`: Name of the request file

**Returns**: Macro string or NULL

**Notes**: Used by configMenu to allow .cfg files to include macros

**Example**:
```c
// char *getMacroString(char *request_file)
char *macros = getMacroString("auto_settings.req");
```

---

## autosaveBuild Functions

### autosaveBuild

**Purpose**: Configures automatic generation of request files.

**Parameters**:
- `requestFileName`: Name of the request file to build
- `suffix`: Suffix to add to database names (e.g., "_settings.req")
- `enable`: If non-zero, enables building for this suffix

**Notes**:
- Requires EPICS 3.14.12.5+ or a patched earlier version
- Requires uncommenting `USR_CFLAGS += -DDBLOADRECORDSHOOKREGISTER` in Makefile

**Example**:
```c
// void autosaveBuild(char *requestFileName, char *suffix, int enable)
autosaveBuild("built_settings.req", "_settings.req", 1);
```

### eraseFile

**Purpose**: Erases the contents of a file by opening it in write mode and immediately closing it.

**Parameters**:
- `filename`: Name of the file to erase. Supports environment variable expansion via `macEnvExpand()`.

**Returns**: Status (0 for success, -1 on error)

**Notes**:
- Useful for clearing a request file before rebuilding it with `appendToFile()` calls
- The file is truncated to zero length but not deleted

**Example**:
```c
// int eraseFile(char *filename)
eraseFile("built_settings.req");
```

### appendToFile

**Purpose**: Adds a line to a file being built by autosaveBuild.

**Parameters**:
- `filename`: Name of the file
- `line`: Line to add

**Example**:
```c
// void appendToFile(char *filename, char *line)
appendToFile("built_settings.req", '$(P)userStringSeqEnable');
```

---

## Verification Functions

### asVerify

**Purpose**: Compares PV values in the IOC with values in a save file.

**Parameters**:
- `fileName`: Name of the save file to verify
- `verbose`: If non-zero, reports all PVs (not just differences)
- `restoreFileName`: If not empty, writes a new restore file

**Returns**: Number of differences found

**Example**:
```c
// int asVerify(char *fileName, int verbose, char *restoreFileName)
asVerify("auto_settings.sav", 1, "");  // Verbose verification
```

---

## Info Node Functions

### makeAutosaveFiles

**Purpose**: Generates request files from info nodes in the database.

**Notes**:
- Searches for info nodes named 'autosaveFields' and 'autosaveFields_pass0'
- Writes PV names to 'info_settings.req' and 'info_positions.req'
- Can be called any time after iocInit

**Example**:
```c
// void makeAutosaveFiles(void)
makeAutosaveFiles();
```

### makeAutosaveFileFromDbInfo

**Purpose**: Generates a request file from specific info nodes.

**Parameters**:
- `fileBaseName`: Base name for the output file
- `info_name`: Name of the info nodes to search for

**Notes**: Can be called any time after iocInit

**Example**:
```c
// void makeAutosaveFileFromDbInfo(char *fileBaseName, char *info_name)
makeAutosaveFileFromDbInfo("custom_settings", "customFields");
```
