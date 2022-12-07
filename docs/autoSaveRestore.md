---
layout: default
title: Autosave Restore
nav_order: 4
---


autosaveRestore
===============

Table of Contents
-----------------

- [Overview](#overview)
- [Important nuances](#important-nuances)
- [How to use autosave](#how-to-use-autosave)
- [autosaveBuild (automatic request-file generation)](#autosavebuild-automatic-request-file-generation)
- [asVerify](#asverify)
- [configMenu](#configmenu)
- [About save files](#about-save-files)
- [Module contents](#module-contents)
- [User-callable functions](#user-callable-functions)
- [Example of use](#example-of-use)

- - - - - -

Overview
--------

Autosave automatically saves the values of EPICS process variables (PVs) to files on a server, and restores those values when the IOC (Input-Output Controller — the business end of EPICS) is rebooted. The original author is Bob Dalesio; I made some improvements; Frank Lenkszus made some more improvements, which I folded into the version I've been maintaining. A bunch of people contributed to getting the software running on PPC hardware, including Ron Sluiter, Andrew Johnson, and Pete Jemian (APS), Markus Janousch and David Maden (SLS), and I'm not sure who else. Zheqiao Geng (SLAC) extended the NFS-mount management to RTEMS, and made other improvements, such as connecting to PVs that weren't live when autosave was initialized. I folded those changes back into a version that preserved the previous API. Michael Davidsaver (BNL) extended support for info nodes to nodes of arbitrary length. Lots of other EPICS developers and users contributed by reporting problems and suggesting solutions. Autosave is a two-part operation: run-time save, and boot-time restore. The run-time part (the save\_restore task or thread) is started by commands in the IOC startup file, and persists while the IOC is running. Its primary job is to save PV values to files on a server, but it also supports manual restore and other management operations. The boot-time part (dbrestore.c) is invoked during iocInit, via an EPICS initHook. It restores PV values from files written by the run-time part, and does not persist after iocInit() returns.

In addition to the autosave software, the autosave module contains a client program, *asVerify*, to compare written autosave files with current PV values. This program can also write an autosave file from which PV values can be restored. Because one use of asVerify is to retrieve PV values from an IOC that is having trouble (the save\_restore task has crashed, for example), asVerify connects with only one PV at a time. This makes asVerify rather slow. In one trial, it took two minutes to verify a .sav file containing 5000 PVs.

Autosave also contains a facility, called *configMenu*, for creating, saving, finding, restoring, and verifying IOC configurations (that is, sets of PV values). configMenu is roughly comparable to the EPICS Backup and Restore Tool, BURT, but it uses autosave files, and is driven by EPICS PVs, so it can be used manually, by software clients, and by other IOC code.

Autosave also contains a facility, called *autosaveBuild*, to generate autosave-request files as part of the operation of the EPICS functions dbLoadRecords() and dbLoadTemplate(). This facility requires EPICS 3.14.12.5 or later, or an earlier version of 3.14 with a patch. <a name="Important nuances"></a>

- - - - - -

Important nuances
-----------------

- At boot time, autosave uses dbStaticLib to write PVs, just like dbLoadRecords(). Thus, boot-time restore does not cause record processing to occur, and it does not make a record's value defined (doesn't set UDF to 0). *Exception: arrays cannot be written by dbStaticLib, so autosave writes arrays at boot time using database access. This still doesn't cause processing to occur, though it should cause the record's UDF field to be set to 0.*
- At run time (after iocInit has completed), autosave reads and writes using channel-access. Restoring at run time can produce a different result than restoring at boot time, because record processing may occur. Prior to autosave 5.6, arrays were always read and written using database access, which doesn't cause records to process. Beginning with autosave 5.6, run time restore of arrays is done with channel access.
- Boot-time restore can occur at either or both of two times: before record initialization ("pass 0"); and after record initialization ("pass 1"). (Record initialization includes device-support initialization.) For most PV's, you don't need to think about this; just tell autosave to restore at both times, and autosave will do whatever is required to get the value written. You cannot make this restore-time choice on a PV-by-PV basis; you can only specify when an entire file of PVs will be restored. If you want the restored value available for use by device support during its initialization, you must cause the value to be restored during pass 0. (Note that arrays cannot be restored at this time, because storage normally will not have been allocated for them yet.) If you want the restored value to overwrite the value written by record/device initialization, you must cause the value to be restored during pass 1.
    
    Motor-record positions (VAL or DVAL) should be restored only before record initialization (that is, during pass 0), because this is the assumption under which motor-record device support was written.
    
    In the lore that has built up around autosave, PV's that should be restored only before record initialization have been termed "positions". All other PV's have been termed "settings". Thus, you might run across a file called 'auto\_positions.req', and now you'll know that the PVs in this file are intended to be restored only during autosave's pass-0.
- You can save PVs without also restoring them, and restore PVs without also saving them, but you can't make these choices for individual PVs, only for entire files of PVs.
- You can enable and disable automated saves for a user specified time. (If the user neglects to re-enable saving, you can arrange for the enable to happen after a specified time.) Manual saves (notably, configMenu) are not disabled when automated saves are disabled. (This is controlled by PVs. See the save\_restore display files.)

How to use autosave
-------------------

This software can be used in many different ways. I'll describe what you have to do to use it as it's commonly used at APS beamlines to save PV values periodically, and restore them on reboot. A complete example of how autosave is used at APS can be found in the synApps xxx module. The relevant files in that module are the following: xxx/configure/RELEASE look for "AUTOSAVE" xxx/xxxApp/src/Makefile look for "autosave" and "asSupport.dbd" xxx/iocBoot/iocvxWorks/save\_restore.cmd the whole file xxx/iocBoot/iocvxWorks/auto\_positions.req the whole file xxx/iocBoot/iocvxWorks/auto\_settings.req the whole file xxx/iocBoot/iocvxWorks/st.cmd look for `save_restore.cmd` and `create_monitor_set`. xxx/iocBoot/iocvxWorks/autosave a directory to hold autosave .sav files
Here's a step-by-step program for deploying autosave. Some of the steps are optional:

#### 1. Build (required)

Build the module and include the resulting library, libautosave.a, and database-definition file, asSupport.dbd, in an IOC's build. For example, add

```
	AUTOSAVE=<path to the autosave module>
```

to xxx/configure/RELEASE, add

```
	xxx_LIBS += autosave
```

to xxxApp/src/Makefile, and add

```
	include "asSupport.dbd"
```

to iocxxxInclude.dbd.

#### 2. Write request files (optional, though this is the intended and most common use)

Create "request" files (e.g., auto\_settings.req, auto\_positions.req) specifying the PVs whose values you want to save and restore. The save files corresponding to these request files will have the ".req" suffix replaced by ".sav". Here's a sample request file:

```
	xxx:m1.VAL
	xxx:m2.VAL
```

Request files can also contain macro variables, whose values will be defined in the call that causes the request file to be processed. For example, the above request file could also be written as shown below, with the macro `P`defined by the command `create_monitor_set("auto_settings.req", 30,"P=xxx:")`:

```
	$(P)m1.VAL
	$(P)m2.VAL
```

Request files can include other request files (nested includes are allowed) and macro substitution can be performed on the included files (using William Lupton's macro library), with the following syntax:

```
	file <request_file> <macro-substitution_string>
```

e.g.,

```
	file motor_settings.req P=xxx:,M=m1
```

I've tried to defend against forseeable variations in syntax, so that "file" commands with embedded whitespace and/or quotes, macro strings without commas, and empty macro strings will be parsed as one would want. Generally, quotes are ignored, whitespace implies a comma but otherwise is ignored, and everything after the second sequence of non-whitespace characters (that is, after the file name) is taken as the macro-substitution string. Macro substitution is performed on the entire line, so it's possible to parameterize names of included files, as well as PV names. It is also possible to define a macro that replaces its target with nothing.

__NOTE:__ Beginning with autosave 5.1, it's no longer legal to have a comment on the same line as a file command. This change was made to permit the use of the '#' character in a macro definition.

Most synApps modules contain autosave-request files that are intended to be included in an ioc's autosave-request file(s). For example, calc/calcApp/Db/scalcout\_settings.req contains a list of the fields one might want to autosave for a single scalcout record. This file is *include*d in calc/calcApp/Db/userStringCalcs10\_settings.req, which, in turn, is included in xxx/iocBoot/iocvxWorks/auto\_settings.req.

Beginning with version 4.3, autosave can generate request files from *info* nodes contained in an EPICS database. See the function `makeAutosaveFiles()`, below, for more information on this topic.

 *Note: Beginning with synApps version 5.2.1, synApps contains software to generate autosave-request files from information contained in command and database files. See synApps/support/utils/makeAutosaveFiles.py.*

 Beginning with autosave version 5.5, autosave contains software to generate autosave-request files from dbLoadRecords() and dbLoadTemplate() calls. See [autosaveBuild](#autosaveBuild).

#### 3. Set request-file path (optional, recommended)

Specify one or more directories to be searched for request files, using one or more invocations of set\_requestfile\_path() For systems using `cdCommands`:

```
	set_requestfile_path(startup, "")
	set_requestfile_path(startup, "autosave")
	set_requestfile_path(area_detector, "ADApp/Db")
	...
```

For systems using `envPaths`:

```
	set_requestfile_path("$(TOP)/iocBoot/$(IOC)", "")
	set_requestfile_path("$(TOP)/iocBoot/$(IOC)", "autosave")
	set_requestfile_path("$(AREA_DETECTOR)", "ADApp/Db")
	...
```

#### 4. Set NFS host (optional, only available on vxWorks and RTEMS)

Specify the NFS host from which save files will be read at restore time, and to which they will be written at save time, by calling the function 

```
	save_restoreSet_NFSHost("oxygen", "164.54.49.4")
```

When autosave manages its own NFS mount, as this command directs it to do, it can fix a stale file handle by dismounting and remounting the file system.

#### 5. Use NFS (required on vxWorks) 

Use NFS, preferably as described above, or by including an `nfsMount()` command in your startup script. Save\_restore is only tested with NFS, and probably will not work with vxWorks' netDrv. When autosave runs under operating systems other than vxWorks and RTEMS, it simply uses whatever mount the operating system, or a system administrator, has provided.

#### 6. Set save/restore file path (optional, though this is the intended and most common use)

Specify the directory in which you want .sav files to be written, by calling the function

```
 set_savefile_path("/full/path") 
```

in your startup script, before the `create_xxx_set()` commands, to specify the path to the directory. If you are using NFS (strongly recommended), ensure that the path does not contain symbolic links. In my experience, VxWorks cannot write through a symbolic link. (I don't understand all the ins and outs of this limitation. It may be that symbolic links are OK if they contain absolute path names.) Beginning with autosave v4.3, you can also specify the path and/or filename, to which an autosave .sav file is to be written, with an EPICS PV. See the documentation of create\_xxx\_set() for details.

#### 7. Give the ioc write permission to the autosave directory (required)

Give the IOC write permission to the directory in which the save files are to be written. If you forget this step, save\_restore may be able to write save files, but the files will be corrupted because save\_restore will not be permitted to change their lengths. Save\_restore attempts to detect this condition, but cannot work around it if the file length must increase. If you're using autosaveBuild, you must also give autosave write permission to the directory that will contain the request file(s) autosave will write.

#### 8. Specify restore files (optional, though this is the intended and most common use)

Specify which save files are to be restored before record initialization (pass 0) and which are to be restored after record initialization (pass 1), using the commands set\_pass&lt;N&gt;\_restoreFile(), as in this example:

```
	set_pass0_restoreFile("auto_settings.sav", "P=xxx:")
	set_pass1_restoreFile("auto_settings.sav", "P=xxx:")
```

(Note the macrostring is optional, and a new feature of autosave 5.4.) Place these commands in the startup file before `iocInit`. In versions earlier than 4.4, autosave would attempt to restore "auto\_positions.sav" and "auto\_settings.sav", if no restore files had been specified. Beginning with version 4.5, only files specified in calls to set\_pass*n*\_restoreFile() are restored.

If the first argument to set\_pass*n*\_restoreFile() is a string that doesn't begin with '/', autosave will prepend the path specified in set\_savefile\_path(). The most common way to specify a restore file is simply to specify its name, without any path information.

Beginning with autosave 5.4, if you specify a full path (a string that begins with '/'), autosave will not prepend anything to it. (Also, autosave will not write a restore-time backup of the file in this case.)

To disable boot-time restore completely, delete or comment out all `set_pass<i>n</i>_restoreFile()` calls.

Notes on restore passes:

1. Link fields cannot be restored (by dbStatic calls) after record initialization. If you want save/restore to work for link fields you must specify them in a pass-0 file.
2. Device support code for the motor record uses the value of the field DVAL, restored during pass 0, only if the value read from the hardware is zero. If the value from hardware is nonzero, it is used instead of the restored value.
3. Arrays cannot be restored during pass 0.
4. Scalar PV's that have type DBF\_NOACCESS in the .dbd file, and are set to some other DBF type by record support, cannot be restored during pass 0.
5. It is not an error to attempt to restore PV's during the wrong pass. The default strategy, implemented with auto\_settings.req and auto\_positions.req, is to use both passes for everything except PV's that shouldn't be restored in pass 1. (Motor positions aren't restored in pass 1 because doing so would overwrite any values read from the hardware.)

#### 9. Load initHook routine (required for boot-time restore)

Load a copy of initHooks that calls reboot\_restore() to restore saved PV values. The copy of initHooks included in this distribution is recommended. This will happen automatically if the ioc's executable is built as described above.

#### 10. Select save-file options (optional, recommended)

- Tell save\_restore to writed dated backup files. At boot time, the restore software writes a backup copy of the ".sav" file from which it restored PV's. This file can either be named xxx.sav.bu, and be rewritten every reboot, or be named xxx.sav\_YYMMDD-HHMMSS, where "YY..." is a date. Dated backups are not overwritten. If you want dated backup files, put the following line in your st.cmd file before the call to iocInit():

    ``` 
    	save_restoreSet_DatedBackupFiles(1)
    ```
    
    Note: If a save file is restored in both pass 0 and pass 1, the boot-backup file will be written only during pass 0.
- Tell save\_restore to save sequence files. The commands:

    ```
    	save_restoreSet_NumSeqFiles(3)
    	save_restoreSet_SeqPeriodInSeconds(600)
    ```
    
    will cause save\_restore to maintain three copies of each .sav file, at ten-minute intervals. Note: if autosave fails to write the .sav file, it will stop making sequence copies until it again succeeds.
- Specify the time delay between a failed .sav-file write and the retry of that write. The default delay is 60 seconds. If list-PV's change during the delay, the new values will be written.

    ```
    	save_restoreSet_RetrySeconds(60)
    ```
- Specify whether autosave should periodically retry connecting to PVs whose initial connection attempt failed. Currently, the connection-retry interval is hard-wired at 60 seconds.

    ```
    	save_restoreSet_CAReconnect(1)
    ```
- Specify the time interval in seconds between forced save-file writes. (-1 means forever). This is intended to get save files written even if the normal trigger mechanism is broken. 

    ``` 
    	save_restoreSet_CallbackTimeout(-1)
    ```

#### 11. Start the save task (required to save files)

Invoke the "save" part of this software as part of the EPICS startup sequence, by calling create\_XXX\_set() — e.g., adding lines of the form

```
	create_monitor_set("auto_positions.req", 5, "P=xxx:")
	create_monitor_set("auto_settings.req", 30, "P=xxx:")
```

to your EPICS startup file after iocInit. The third argument to `create_monitor_set()` is a macro-substitution string, as described above in the discussion of request files (step 2). If supplied, this macro-substitution string supplements any macro strings supplied in include-file directives of request files read for this save set. 

> __Note:__ if you want to start the save task without also creating a save set, you can call `create_monitor_set()` with no arguments.*

The macro string can also be used to override the default path and name to which the .sav file will be written. If the macro SAVEPATHPV=&lt;pv-name&gt; is included, autosave will write the .sav file to the path read from the EPICS PV &lt;pv-name&gt;, instead of writing to the path specified in set\_savefile\_path(). If the macro SAVENAMEPV=&lt;pv-name&gt; is included, autosave will write the .sav file to the filename read from the EPICS PV &lt;pv-name&gt;. (By default, the .sav file name is the request-file name, with .reqreplaced by .sav.)

> __NOTE:__ If either SAVEPATHPV or SAVENAMEPV occurs in the macro string, autosave will not attempt to save backup or sequence files for the save set.

For each `create_monitor_set(<name>.req, <time>, <macro>)` command, the save\_restore process will write the files &lt;name&gt;.sav and &lt;name&gt;.savB every &lt;time&gt; seconds, if any of the PVs named in the file &lt;name&gt;.req have changed value since the last write. Other `create_xxx_set()` commands do the same thing, but with different conditions triggering the save operation.

Note that in versions prior to 2.7, `create_monitor_set()` used an argument of type double to specify the period (in seconds). This doesn't work on PowerPC processors, under vWorks, so the arguments for this and similar functions were changed to int.

If your IOC takes a really long time to boot, it's possible the PVs you want to save will not have the correct values when the save\_restore task first looks at them. (If you are restoring lots of long arrays, this is even more likely.) Under vxWorks, you can avoid this by putting a

```
	taskDelay(<number_of_60_Hz_clock_ticks>)
```

before `create_monitor_set()`.

- - - - - -

autosaveBuild (automatic request-file generation)
-------------------------------------------------

 Note: this facility requires an EPICS base version higher than 3.14.12.5, or a patch to an earlier version of EPICS base 3.14. To enable the code in autosave, you must edit asApp/src/Makefile, and uncomment the line

```
#USR_CFLAGS += -DDBLOADRECORDSHOOKREGISTER
```

Many of the databases in synApps have associated autosave-request files. For example, the calc module contains `editSseq.db` and `editSseq_settings.req`. When adding a new database to an IOC, it's common practice to add the associated request file to `auto_settings.req` and/or `auto_positions.req`. For clarity, `st.cmd` contains this:

> ```
> dbLoadRecords("$(CALC)/calcApp/Db/editSseq.db", "P=xxxL:,Q=ES:")
> ```

and `auto_settings.req` contains this: 

> ```
> file editSseq_settings.req P=xxxL:,Q=ES:
> ```

It's tedious and error prone to have these entries separately maintained, so autosave can do the request-file part for you. To do this, you tell autosave to arrange to be called whenever `dbLoadRecords()` is called (note that `dbLoadTemplate()` calls `dbLoadRecords()`), you tell it how to make a request-file name from a database-file name, and you give it the name of the request file you want it to build. You can do this with the following command:

> ```
> autosaveBuild("built_settings.req", "_settings.req", 1)
> ```

This tells autosave to do the following: 1. Begin building the file `built_settings.req`. If this is the first call that mentions `built_settings.req`, erase the file.
2. Generate request-file names by stripping ".db", or ".vdb", or ".template" from database-file names, and adding the suffix "\_settings.req".
3. Enable (disable) automated building if the third argument is 1 (0).

While automated building is enabled, autosave will generate request-file names and search for those files in its request-file path. If it finds a request file, it will add the appropriate line to `built_settings.req`. All this does is get the file `built_settings.req` written. If you want it to be used, you must add the following line to `auto_settings.req`:

> ```
> file built_settings.req P=$(P)
> ```

### Options

- You can specify more than one request-file suffix by making multiple calls to `autosaveBuild()`: 

    ``` 
    autosaveBuild("built_settings.req", "_settings.req", 1)
    autosaveBuild("built_settings.req", ".req", 1)
    ```
    
    After these calls, the request-file name for `abc.db` can be either `abc_settings.req` or `abc.req`. If both are found, lines for both will be added to `built_settings.req`.
- You can disable file/suffix combinations separately:
    
    ```
    autosaveBuild("built_settings.req", "_settings.req", 0)
    ```
    
    disables searching for request files ending in "\_settings.req" for `built_settings.req`. 
    
    ```
    autosaveBuild("built_settings.req", "*", 0)
    ```
    
    disables searching for all request files for `built_settings.req`.
- You can also add a line to `built_settings.req` yourself:
    
    ```
    appendToFile("built_settings.req", '$(P)userStringSeqEnable')
    ```
    
    Note the use of single quotes. This is for iocsh, because it will not accept failure to expand the macro `$(P)`, and single quotes tells it to not do macro expansion.

### Patch for EPICS 3.14.12.4

> ```
> 
> --- src/db/dbAccess.c.ORIG	2014-10-16 16:51:21.778507000 -0500
> +++ src/db/dbAccess.c	2014-11-03 12:16:51.394148000 -0600
> @@ -815,9 +815,16 @@
>      return dbReadDatabase(&pdbbase, file, path, subs);
>  }
>  
> +/* dbLoadRecordsHook from base-3.15 */
> +epicsShareDef DB_LOAD_RECORDS_HOOK_ROUTINE dbLoadRecordsHook = NULL;
> +
>  int epicsShareAPI dbLoadRecords(const char* file, const char* subs)
>  {
> -    return dbReadDatabase(&pdbbase, file, 0, subs);
> +    int status = dbReadDatabase(&pdbbase, file, 0, subs);
> +
> +    if (!status && dbLoadRecordsHook)
> +        dbLoadRecordsHook(file, subs);
> +    return status;
>  }
>  
> 
> 
> --- src/db/dbAccessDefs.h.ORIG	2014-11-03 12:06:29.118353000 -0600
> +++ src/db/dbAccessDefs.h	2014-11-03 12:08:27.229610000 -0600
> @@ -276,6 +276,12 @@
>      short dbrType,long options,long nRequest);
>  epicsShareFunc long epicsShareAPI dbValueSize(short dbrType);
>  
> +/* Hook Routine */
> +
> +typedef void (*DB_LOAD_RECORDS_HOOK_ROUTINE)(const char* filename,
> +    const char* substitutions);
> +epicsShareExtern DB_LOAD_RECORDS_HOOK_ROUTINE dbLoadRecordsHook;
> +
>  epicsShareFunc int epicsShareAPI  dbLoadDatabase(
>      const char *filename, const char *path, const char *substitutions);
> ```

- - - - - -

asVerify
--------

Autosave is not completely bulletproof. Most APS beamlines have at least one autosave related problem every year. If autosave fails, you might be able to detect it, or work around it, using asVerify. This client-side program reads an autosave .sav file and independently verifies that the values it contains agree with current PV values. The program can also be used to write a .sav file, given a list of PVs. The list of PVs is normally an autosave .sav file, but a file containing nothing but PV names, one per line, would also work. Here's asVerify's command line: 

```

usage: asVerify [-vrd] <autosave_file>
         -v (verbose) causes all PV's to be printed out
             Otherwise, only PV's whose values differ are printed.
         -r (restore_file) causes a restore file named
            '<autosave_file>.asVerify' to be written.
         -d (debug) increment debug level by one.
         -rv (or -vr) does both
examples:

    asVerify auto_settings.sav
        (reports only PVs whose values differ from saved values)

    asVerify -v auto_settings.sav
        (reports all PVs, marking differences with '***'.)

    asVerify -vr auto_settings.sav
        (reports all PVs, and writes a restore file.)

    asVerify auto_settings.sav
    caput <myStatusPV> $?
        (writes number of differences found to a PV.)

```

Note that asVerify cannot read an autosave request file; it will understand any PV names contained in the file, but it cannot parse the "file" command, perform macro substitutions, or include other request files.

Beginning with autosave R5.6.1, asVerify can be called from the IOC console:

```
asVerify(char *fileName, int verbose, char *restoreFileName)
```

Examples: 

```
asVerify("auto_settings.sav")
```

This will print out differences.

```
asVerify("auto_settings.sav",0,"junk.sav")
```

This will print out differences and write "junk.sav" in the IOC's current directory.

- - - - - -

configMenu
----------

*configMenu* is a facility for creating, saving, finding, restoring, and verifying sets of EPICS PVs ("configurations") on a running IOC. It is roughly comparable to the EPICS Backup and Restore Tool, BURT, but it saves configurations as autosave files, and is driven by and reports back to EPICS PVs, so it can be used manually, driven by any CA client, or driven by IOC-resident code. configMenu operations can be driven by a ca\_put\_callback to an EPICS PV, so the loading and saving of a configuration can usefully be driven as part of a larger sequence of operations. For example, any EPICS record that can do a ca\_put\_callback can load or save a configuration and wait for the operation to complete before perhaps triggering additional execution. __New in autosave R5-7:__ The number of configurations that a configMenu instance can handle used to be 10, but is now unlimited. If more than ten configurations are found, you can page through the list, ten configurations at a time.

### Example

Suppose we want to configure a set of three sscan records to perform one of many different types of scans. Here are the steps needed to implement a menu of scan types, and to give the user a GUI display for creating scan types and loading them. (In the following, scan1 is the name of this instance of configMenu. The files it loads and saves will be named "scan1\_&lt;*config Name*&gt;.cfg".)

> 1. Create an autosave request file, which I'll call "scan1Menu.req", with the following content: ```
>
>     file configMenu.req P=$(P),CONFIG=$(CONFIG)
>     file scan_settings.req P=$(P),S=scan2
>     file scan_settings.req P=$(P),S=scan1
>     file scan_settings.req P=$(P),S=scanH
>     ```
>     
>     > This is required only if scan1 config files are to be written at run time.
> 2. Add the following lines to `st.cmd`: `dbLoadRecords("$(AUTOSAVE)/asApp/Db/configMenu.db","P=xxx:,CONFIG=<font color="blue">scan1</font>")`
>
>     > This goes before `iocInit`. You can disable the saving of scan1 config files by specifying the macro `ENABLE_SAVE=0`.
>
>     `create_manual_set("<font color="blue">scan1</font>Menu.req","P=xxx:,CONFIG=<font color="blue">scan1</font>,CONFIGMENU=1")`
>
>     > This goes after `iocInit`, and is required only if you intend for scan1 config files to be written at run time, or if you need to have macro substitution performed on a scan1 config file to be loaded. The macro `CONFIGMENU` tells autosave to refrain from writing backup (.savB) and sequence (.sav1, .sav2, etc.) files for this save set.
> 3. Add an MEDM related-display entry to bring up a configMenu\*.adl display. 
>
>     ```
>     label="scan1Menu"
>     name="configMenu.adl"
>     args="P=xxx:,CONFIG=<font color="blue">scan1</font>"
>     ```
> 4. If all of the PVs in a configuration are being autosaved, and you want the current configuration name and description, and the `enableSave`selection also to be autosaved, add the following line to auto\_settings.req: 
>
>     ```
>     file configMenu_settings.req P=$(P),CONFIG=<font color="blue">scan1</font>
>     ```
>
>     > I'm not sure this is really a great idea, because the autosaved values aren't guaranteed to be the same as the values in the .cfg file. (The user might have loaded a .cfg file and then made some changes, for example.) But it's disconcerting for a user to reboot the ioc and not have everything come back just as it was, so I normally do this.

Here an example of what the user might see: 

![](configMenu_small.adl.jpg) ![](configMenu.adl.jpg) ![](configMenu_more.adl.jpg)

In __configMenu\_small.adl__, the menu of configurations is displayed by and selected from the *enum* PV, `$(P)$(CONFIG)Menu`, (e.g., `xxx:<font color="blue">scan1</font>Menu`). This display cannot cause a configuration to be written. When the menu is repopulated, or a new page is selected, MEDM will not automatically retrieve the new names for display by `$(P)$(CONFIG)Menu`. This must be done manually, by closing and reopening the display, which is what the "Refresh menu choices" button does.

Previously, `$(P)$(CONFIG)Menu` was a convenient PV for driving configMenu from CA-client software, because it both selected a configuration and caused the configuration to load. This no longer works if the configuration name is not on the displayed page. Beginning with R5-7, CA clients can safely load a configuration by writing its name to `$(P)$(CONFIG)Menu:name`.

__configMenu.adl__ and __configMenu\_more.adl__ show the menu choices as separate PVs, for which no manual refresh is needed, and provide buttons for loading and saving configurations, and for paging through the list of available configuration files.

__configMenu\_more.adl__ also shows description PVs. When the menu is populated by searching for config files, descriptions are extracted from those files. Otherwise, if configMenuNames.req is included in an autosave-request file, names and descriptions will be restored at boot time. If the `enableSave` PV (labelled "permit save?") has the value "No" (0), the "Save" buttons will not be displayed, and users will not be able to save configuration files.

### Details

1. Configuration names in the display, configMenu.adl, will correspond with autosave ".cfg" files whose names are similar, but with non-alphanumeric characters replaced by '\_' (e.g., "scan1\_align\_entrance\_slit.cfg"). A ".cfg" file is exactly like a ".sav" file; the ".cfg" extension is purely to make them easier to find and distinguish from ".sav" files. > You don't want to make two configurations whose names differ only in non-alphanumeric characters; configMenu will gleefully treat them as the same configuration.
2. configMenu\_small.adl has a problem when the menu of config files changes: MEDM doesn't monitor the menu (enum) strings, so the display must be closed and reopened when they change. That's what the "Refresh menu choices" button is for. (Channel access clients that specify the event-type flag `DBE_PROPERTY` when they subscribe to an enum PV will be notified when the enum strings change.)
3. Beginning with R5-7, configMenu can save/restore all kinds of PVs from/to other IOCs. If you use configMenu for remote PVs, you should tell autosave to retry connections periodically, by including the following line in save\_restore.cmd: 

    ```
    save_restoreSet_CAReconnect(1)
    ```
4. When configMenu overwrites an existing .cfg file, it makes a backup copy of the current version, named *filename*\_YYMMDD-HHMMSS. For example: scan1\_blank.cfg\_130401-140546 was written at 2:05:46 PM on April 1, 2013.
5. You can load .cfg files that contain macros. For example, softGlue standard example circuits can be loaded without modification into any softGlue instance by specifying macros as follows:   
    `create_manual_set("SGMenu.req","P=xxx:,CONFIG=SG,CONFIGMENU=1,H=softGlue:")`  
    where the macros "P" and "H" agree with their definitions in softGlue.cmd.
6. configMenu needs to get a directory listing to search the autosave directory for .cfg files. At APS, we've encountered a problem using nfs3Drv with vxWorks 5.5.2 to talk to a linux-hosted file server. The source and nature of the problem are not thoroughly understood, but one symptom is that directory listings don't work. For example, typing "ls" at the ioc's console prompt yields the following error message: 

    ```
    error reading dir <mydirectoryname> errno: 0x300016
    ```
    
    One solution is to modify the board-support package to use nfs2Drv.

- - - - - -

About save files
----------------

PV values in a save file have been converted to strings, in most cases simply by having been read as strings — e.g., `ca_get(DBR_STRING,...)`. Most data types are read using channel access and written using dbStaticLib calls. Four data types get special attention:  double read as double, converted to string with the format "%.14g". (Otherwise, the record's .PREC field would limit the precision.) float Same as double, but the format is "%.7g". menu and enum Menu and enum are integer data types with values restricted to the set (0,1,...N), where 'N' depends on the PV, but is usually 15 or smaller. EPICS associates a string with each number in the set, and permits clients to use either the number or the string. For menu PV's, the strings are specified in a .dbd file; for enum PV's, the strings can be specified in a database file, and they can be modified at run time. Channel access does not distinguish these types, so neither does autosave. Autosave cannot write enums as strings, because enum strings may not have been defined at the time they must be restored.

Int64 and UInt64 Saving of 64bit integer data is done through channel access which interprets the value as a double. This means that there are certain values that will not restore the same as they are saved. Values up until 2^52 should be completely fine, but after that point, restored values may lose out on the lower 12 bits of data. arrays of any kind Arrays are read using database access. Channel access cannot read only the defined portion of an array, dbStaticLib cannot write an array. (However, asVerify uses channel access to read arrays.) At boot time, autosave uses database access to write arrays, but at runtime, it uses channel access (beginning with autosave 5.6). Here is a sample save file. Characters in blue are documentation comments, and are not part of the file:

```

# save/restore V4.9	Automatically generated - DO NOT MODIFY - 060720-154526
! 1 channel(s) not connected - or not all gets were successful
xxx:SR_ao.DISP 0 <font color="blue">(uchar)</font>
xxx:SR_ao.PREC 1 <font color="blue">(short)</font>
xxx:SR_bo.IVOV 2 <font color="blue">(ushort)</font>
xxx:SR_ao.SCAN 3 <font color="blue">(enum - saved/restored as a short)</font>
xxx:SR_ao.VAL 4.1234567890123 <font color="blue">(double, printed with format "%.14g")</font>
xxx:SR_scaler.RATE 1.234568 <font color="blue">(float, printed with format "%.7g")</font>
xxx:SR_ao.DESC description <font color="blue">(string)</font>
xxx:myCalc.CALC$ 123456789+123456789+123456789+123456789+123456789 <font color="blue">(long string)</font>
xxx:SR_ao.OUT xxx:SR_bo.VAL NPP NMS <font color="blue">(link)</font>
xxx:SR_ao.RVAL 4 <font color="blue">(long)</font>
xxx:SR_bi.SVAL 2 <font color="blue">(ulong)</font>
#i_dont_exist.VAL Search Issued <font color="blue">(no such PV)</font>
xxx:SR_char_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_double_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_float_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_long_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_short_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_string_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_uchar_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_ulong_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
xxx:SR_ushort_array @array@ { "1" "2" "3" "4" "5" "6" "7" "8" "9" "10" }
<END>

```

Save files are not intended to be edited manually. If you, nevertheless, do edit a save file, you must end it with the text

```
<END>
```

followed by one or two arbitrary characters (normally '\\n' or '\\r\\n'). If the file does not end with this text, reboot\_restore() will assume the crate crashed while the file was being written, or that some other bad thing happened, and will not use the file. Once a save file has been created successfully, save\_restore will not overwrite the file unless a good ".savB" backup file exists. Similarly, it will not overwrite the ".savB" file unless the save file was successfully written. You can comment out lines in a .sav file by beginning them with '#'. 

- - - - - -

Module contents
---------------

### asApp/src

save\_restore.c saves PV values in files on a file server according to preset rules. dbrestore.c restore PV values at boot time, using dbStaticLib initHooks.c call restore routines at the correct time during boot. fGetDateStr.c Frank Lenkszus' date-string routines save\_restore.h, fGetDateStr.h, configMenuClient.h headers verify.c compare an autosave save file with current values of PVs. Used by asVeryify and configMenu. asVerify.c Client-side tool to compare autosaved file with current PV values. Can also write an autosave file. configMenuSub.c aSub routines for use by the configMenu database. ### asApp/Db

auto\_settings.req, auto\_positions.req Sample request files save\_restoreStatus.db database containing records save\_restore uses to report status. infoExample.db database containing a record with info nodes specifying fields to be autosaved. SR\_test.db Test database for autosave and asVerify. configMenu.db, configMenu\*.req Support for managing/configuring a collection of PVs. ### asApp/op/adl *(also ../ui, ../edl, ../opi)*

![](save_restoreStatus.adl.jpg)  ![](save_restoreStatus_more.adl.jpg)

save\_restoreStatus\*.adl, save\_restoreStatusLegend.adl, save\_restoreStatus\_more.adl, save\_restoreStatus\_tiny.adl, SR\_X\_Status.adl MEDM displays of save\_restore status. configMenu\*.adl Support for managing/configuring a collection of PVs.

- - - - - -

User-callable functions
-----------------------

`int asVerify(char *fileName, int verbose, char *restoreFileName)` Compare PV values in the IOC with values written in `filename` (which should be an autosave restore file, or at least look like one). If restoreFileName is not empty, write a new restore file. This function can be called at any time after iocInit.

`int create_manual_set(char *request_file, char *macrostring)` Create a save set for the request file. The save file will be written when the function `manual_save()` is called with the same request-file name.  See "Start the save task", above for information about the macro string.

This function can be called at any time after iocInit.

`int create_monitor_set(char *request_file, int period, char *macrostring)` Create a save set for the request file. The save file will be written every `period` seconds, if any PV in the save set was posted (changed value) since the last write.  See "Start the save task", above for information about the macro string.

This function can be called at any time after iocInit.

`int create_periodic_set(char *request_file, int period, char *macrostring)` Create a save set for the request file. The save file will be written every `period` seconds.  See "Start the save task", above for information about the macro string.

This function can be called at any time after iocInit.

`int create_triggered_set(char *request_file, char *trigger_channel, char *macrostring)` Create a save set for the request file. The save file will be written whenever the PV specified by `trigger_channel` is posted. Normally this occurs when the PV's value changes.  See "Start the save task", above for information about the macro string.

This function can be called at any time after iocInit.

`int fdbrestore(char *save_file)` If `save_file` refers to a save set that exists in memory, then PV's in the save set will be restored from values in memory. Otherwise, this functions restores the PV's in &lt;saveRestorePath&gt;/&lt;save\_file&gt; and creates a new backup file "&lt;saveRestorePath&gt;/&lt;save\_file&amp;gt.bu". The effect probably will not be the same as a boot-time restore, because caput() calls are used instead of static database access dbPutX() calls. Record processing will result from caput()'s to inherently process- passive fields. This function can be called at any time after one of the create\_\*\_set() functions have been called. If you want to call this function before creating any save sets, you can call create\_\*\_set() with an empty request-file name. Autosave will complain about this, but it won't think you're a bad person.

`int fdbrestoreX(char *save_file)` (iocsh version) This function restores from the file &lt;saveRestorePath&gt;/&lt;save\_file&amp;gt, which can look just like a save file, but which needn't end with `<END>`. No backup file will be written. The effect probably will not be the same as a boot-time restore, because caput() calls are used instead of static database access dbPut\*() calls. Record processing will result from caput()'s to inherently process-passive fields. This function can be called at any time after one of the create\_\*\_set() functions have been called. If you want to call this function before creating any save sets, you can call create\_\*\_set() with an empty request-file name. Autosave will not hate you for doing this, though it will complain.

`int fdbrestoreX(char *filename, char *macrostring, callbackFunc callbackFunction, void *puser)` (version for c-code clients) This function does the same job as the iocsh version above. If `macrostring` is not NULL, the macro definitions it contains will be applied to the contents of `filename`. If `callbackFunction` is not NULL, it specifies a function of type `void f(int status, void *puser)` that will be called when the save operation is done. This is part of the implementation of *configMenu*.

`char *getMacroString(char *request_file)` If `create_*_set()` was ever called for `request_file`, then the macro-substitution string supplied in that call was recorded by autosave, and can be recovered with this function. This is part of the implementation of *configMenu*, and it allows .cfg files to include macros.

`void makeAutosaveFiles(void)` Search through the EPICS database (that is, all EPICS records loaded into an IOC) for *info* nodes named 'autosaveFields' and 'autosaveFields\_pass0'; construct lists of PV names from the associated info values, and write the PV names to the files 'info\_settings.req' and 'info\_positions.req', respectively. An info node, in an EPICS database, is similar to a field specification, but it has the word `info` instead of `field`; and it has an arbitrary name, instead of the name of a field in the record. Here's an EPICS database containing a single record with two info nodes:

```
record(ao, "$(P)test1") {
  field(DTYP, "Soft Channel")
  <font color="blue">info(autosaveFields, "PREC EGU DESC")
  info(autosaveFields_pass0, "VAL")</font>
}
```

From this information, `makeAutosaveFiles()` will write the following two files:

```
<b><font color="blue">info_settings.req</font></b>
$(P)test1.PREC
$(P)test1.EGU
$(P)test1.DESC

<b><font color="blue">info_positions.req</font></b>
$(P)test1.VAL
```

This function can be called at any time after iocInit().

See also:

- Chapter 14, "Static Database Access", in the *EPICS Application Developer's Guide*.
- `makeAutosaveFileFromDbInfo()`.

`void makeAutosaveFileFromDbInfo(char *fileBaseName, char *info_name)` Search through the EPICS database (that is, all EPICS records loaded into an IOC) for 'info' nodes named `info_name`; construct a list of PV names from the associated info\_values found, and write the PV names to the file `fileBaseName`. If `fileBaseName` does not contain the string '.req', this string will be appended to it. See `makeAutosaveFiles()` for more information. This function can be called at any time after iocInit().

`int manual_save(char *request_file)` (iocsh version) Cause current PV values for the request file to be saved. Any request file named in a create\_xxx\_set() command can be saved manually.

`int manual_save(char *request_file, char *save_file, callbackFunc callbackFunction,void *puser);` (version for c-code clients) Cause current PV values for the request file to be saved. Any request file named in a create\_xxx\_set() command can be saved manually. If `save_file` is not NULL and not empty, it specifies the name of the file that will be written. If `callbackFunction` is not NULL, it specifies a function of type `void f(int status, void *puser)` that will be called when the save operation is done. This is part of the implementation of *configMenu*.

`int reboot_restore(char *save_file, initHookState init_state)` This should only be called from initHooks because it can only function correctly if called at particular times during iocInit.

`int reload_manual_set(char * request_file, char *macrostring)` This function allows you to change the PV's associated with a save set created by `create_manual_set()`. Note: Don't get too ambitious with the remove/reload functions. You have to wait for one to finish completely (the save\_restore task must get through its service loop) before executing another. If you call one before the previous function is completely finished, I don't know what will happen.

`int reload_monitor_set(char * request_file, int period, char *macrostring)` This function allows you to change the PV's and the period associated with a save set created by `create_monitor_set()`.

`int reload_periodic_set(char *request_file, int period, char *macrostring)` This function allows you to change the PV's and the period associated with a save set created by `create_periodic_set()`.

`int reload_triggered_set(char *request_file, char *trigger_channel, char *macrostring)` This function allows you to change the PV's and the trigger channel associated with a save set created by `create_triggered_set()`.

`int remove_data_set(char *request_file)` If a save set has been created for `request_file`, this function will delete it.

`void save_restoreSet_DatedBackupFiles(int ok)` Sets the value of `(int) save_restoreDatedBackupFiles` (initially 1). If zero, the backup file written at reboot time (a copy of the file from which PV values are restored) will have the suffix '.bu', and will be overwritten every reboot. If nonzero, each reboot will leave behind its own backup file. This function can be called at any time.

`void save_restoreSet_periodicDatedBackups(int periodInMinutes)` Enables periodic dated backups, and sets the period. If periodInMinutes This function can be called at any time. `void save_restoreSet_Debug(int debug_level)`Sets the value `(int) save_restoreDebug` (initially 0). Increase to get more informational messages printed to the console. This function can be called at any time.

`void save_restoreSet_FilePermissions(int permissions)` Specify the file permissions used to create new .sav files. This integer value will be supplied, exactly as given, to the system call, open(), and to the call fchmod(). Typically, file permissions are set with an octal number, such as 0640, and save\_restoreSet\_FilePermissions() will confirm any number given to it by echoing it to the console as an octal number. This function can be called at any time after iocInit.

`void save_restoreSet_IncompleteSetsOk(int ok)` Sets the value of `(int) save_restoreIncompleteSetsOk` (initially 1). If set to zero, save files will not be restored at boot time unless they are perfect, and they will not be overwritten at save time unless a valid CA connection and value exists for every PV in the list. This function can be called at any time.

`void save_restoreSet_NFSHost(char *hostname, char *address)` Specifies the name and IP address of the NFS host. If both have been specified, and `set_savefile_path()` has been called to specify the file path, save\_restore will manage its own NFS mount. This allows save\_restore to recover from a reboot of the NFS host (that is, a stale file handle) and from some kinds of tampering with the save\_restore directory. `void save_restoreSet_NumSeqFiles(int numSeqFiles)`Sets the value of `(int) save_restoreNumSeqFiles` (initially 3). This is the number of sequenced backup files to be maintained. `numSeqFiles` must be between 0 and 10 inclusive. This function can be called at any time.

`void save_restoreSet_RetrySeconds(int seconds)` Sets the value of `(int) save_restoreRetrySeconds` (initially 60; minimum 10). If the .sav-file write fails, it will be retried after this interval.. This function can be called at any time.

`void save_restoreSet_SeqPeriodInSeconds(int period)` Sets the value of `(int) save_restoreSeqPeriodInSeconds` (initially 60). Sequenced backup files will be written with this period. `period` must be 10 or greater. This function can be called at any time.

`void save_restoreSet_status_prefix(char *prefix)` Specifies the prefix to be used to construct the names of PV's with which save\_restore reports its status. If you want autosave to update status PVs as it operates, you must call this function and load the database save\_restoreStatus.db, specifying the same prefix in both commands. This function must be called before the first call to `create_xxx_set()`.

`void save_restoreSet_UseStatusPVs(int ok)` Specifies whether save\_restore should report its status to a preloaded set of EPICS PV's (contained in the database save\_restoreStatus.db). If the argument is '0', then status PV's will not be used. This function should be called before the first call to `create_xxx_set()`.

`void save_restoreShow(int verbose)` List all the save sets currently being managed by the save\_restore task. If (verbose != 0), lists the PV's as well. This function can be called at any time after iocInit.

`int set_requestfile_path(char *path, char *pathsub)` Called before create\_xxx\_set(), this function specifies the path to be prepended to request-file names. `pathsub`, if present, will be appended to `path`, if present, with a separating '/', whether or not `path` ends or `pathsub` begins with '/'. If the result does not end in '/', one will be appended to it. You can specify several directories to be searched for request files by calling this routine several times. Directories will be searched in the order in which the `set_requestfile_path()` calls were made. If you never call the routine, the crate's current working directory will be searched. If you ever call it, the current directory ("./") will be searched only if you've asked for it explicitly.

`int set_pass0_restoreFile(char *save_file, char *macroSubstitutions)` This function specifies a save file to be restored during iocInit, before record initialization. An unlimited number of files can be specified using calls to this function. If the file name begins with "/", autosave will use it as specified; otherwise, autosave will prepend the file path specified to `set_savefile_path()`. The second argument is optional. Example: `set_pass0_restoreFile("auto_settings.sav", "P=xxx:")`

`int set_pass1_restoreFile(char *save_file, char *macroSubstitutions)` This function specifies a save file to be restored during iocInit, after record initialization. An unlimited number of files can be specified using calls to this function. If the file name begins with "/", autosave will use it as specified; otherwise, autosave will prepend the file path specified to `set_savefile_path()`. The second argument is optional. Example: `set_pass1_restoreFile("auto_settings.sav", "P=xxx:")`

`int set_savefile_name(char *request_file, char *save_file)` If a save set has already been created for the request file, this function will change the save file name.

`int set_savefile_path(char *path, char *pathsub)`Called before iocInit(), this function specifies the path to be prepended to save-file and restore-file names. `pathsub`, if present, will be appended to `path`, if present, with a separating '/', whether or not `path` ends or `pathsub` begins with '/'. If the result does not end in '/', one will be appended to it. If save\_restore is managing its own NFS mount, this function specifies the mount point, and calling it will result in an NFS mount if all other requirements have already been met. If a valid NFS mount already exists, the file system will be dismounted and then mounted with the new path name. This function can be called at any time.

`int set_saveTask_priority(int priority)`Set the priority of the save\_restore task.

- - - - - -

Example of use
--------------

---------- begin excerpt from st.cmd ----------------------

```

.
.
.
dbLoadDatabase("$(TOP)/dbd/iocxxxVX.dbd")
iocxxxVX_registerRecordDeviceDriver(pdbbase)
.
.
.
<font color="blue">### autoSaveRestore setup
save_restoreSet_Debug(0)

# status-PV prefix, so save_restore can find its status PV's.
save_restoreSet_status_prefix("xxx:")

# ok to restore a save set that had missing values (no CA connection to PV)?
# ok to save a file if some CA connections are bad?
save_restoreSet_IncompleteSetsOk(1)

# In the restore operation, a copy of the save file will be written.  The
# file name can look like "auto_settings.sav.bu", and be overwritten every
# reboot, or it can look like "auto_settings.sav_020306-083522" (this is what
# is meant by a dated backup file) and every reboot will write a new copy.
save_restoreSet_DatedBackupFiles(1)

# specify where save files should go
set_savefile_path(startup, "autosave");

## specify where request files can be found
# current directory
set_requestfile_path(startup, "")
# We want to include request files that are stored with the databases they
# support — e.g., in stdApp/Db, mcaApp/Db, etc.  The variables std and mca
# are defined in cdCommands.  The path is searched in the order in which
# directories are specified. 
set_requestfile_path(startup)
set_requestfile_path(std, "stdApp/Db")
set_requestfile_path(motor, "motorApp/Db")
set_requestfile_path(mca, "mcaApp/Db")
set_requestfile_path(ip, "ipApp/Db")
set_requestfile_path(ip330, "ip330App/Db")
# [...]

# Specify what save files should be restored when.
# Up to eight files can be specified for each pass.
set_pass0_restoreFile("auto_positions.sav")
set_pass0_restoreFile("auto_settings.sav")
set_pass1_restoreFile("auto_settings.sav")

set_pass0_restoreFile("info_positions.sav")
set_pass0_restoreFile("info_settings.sav")
set_pass1_restoreFile("info_settings.sav")
# [...]

# Number of sequenced backup files (e.g., 'auto_settings.sav0') to write
save_restoreSet_NumSeqFiles(3)

# Time interval between sequenced backups
save_restoreSet_SeqPeriodInSeconds(600)

# Time between failed .sav-file write and the retry.
save_restoreSet_RetrySeconds(60)

# Ok to retry connecting to PVs whose initial connection attempt failed?
save_restoreSet_CAReconnect(1)

# Time interval in seconds between forced save-file writes.  (-1 means forever).
# This is intended to get save files written even if the normal trigger mechanism is broken.
save_restoreSet_CallbackTimeout(-1)

# NFS host name and IP address
save_restoreSet_NFSHost("oxygen", "164.54.52.4")</font>

<font color="blue">dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=xxx:")</font>
.
.
.
iocInit
.
.
.
<font color="blue">### Start up the save_restore task and tell it what to do.
# The task is actually named "save_restore".
#
# save positions every five seconds
create_monitor_set("auto_positions.req", 5, "P=xxx:")
# save other things every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=xxx:")

# Handle autosave 'commands' contained in loaded databases.
makeAutosaveFiles()
create_monitor_set("info_positions.req", 5, "P=xxx:")
create_monitor_set("info_settings.req", 30, "P=xxx:")

</font>
.
.
.
```

---------- end excerpt from st.cmd ----------------------

- - - - - -

 Suggestions and Comments to:
 [Tim Mooney](mailto:mooney@aps.anl.gov): (mooney@aps.anl.gov)
