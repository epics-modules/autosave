# vxWorks startup script

cd ""
< ../nfsCommands
< cdCommands

################################################################################
cd topbin

# If the VxWorks kernel was built using the project facility, the following must
# be added before any C++ code is loaded (see SPR #28980).
sysCplusEnable=1

### Load custom EPICS software from user tree and from share
ld < as.munch

### dbrestore setup
# ok to restore a save set that had missing values (no CA connection to PV)?
sr_restore_incomplete_sets_ok = 1
# dbrestore saves a copy of the save file it restored.  File name is, e.g.,
# auto_settings.sav.bu or auto_settings.savYYMMDD-HHMMSS if
# reboot_restoreDatedBU is nonzero.
reboot_restoreDatedBU = 1;
set_savefile_path(startup, "autosave")
set_requestfile_path(startup, "")
set_requestfile_path(startup, "autosave")
set_requestfile_path(autosave, "asApp/Db")
reboot_restoreDebug=0

cd startup
################################################################################
# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in the software we just loaded (as.munch)
dbLoadDatabase("../../dbd/iocas.dbd")
iocas_registerRecordDeviceDriver(pdbbase)

dbLoadRecords("$(TOP)/asApp/Db/sample.db","P=as:")

###############################################################################
# Set shell prompt (otherwise it is left at mv167 or mv162)
shellPromptSet "iocas> "
iocLogDisable=1
iocInit

### Start up the autosave task and tell it what to do.
# The task is actually named "save_restore".
# (See also, 'initHooks' above, which is the means by which the values that
# will be saved by the task we're starting here are going to be restored.
# Note that you can reload these sets after creating them: e.g., 
# reload_monitor_set("auto_settings.req",30,"P=as:")
#
# save positions every five seconds
create_monitor_set("auto_positions.req",5,"P=as:")
# save other things every thirty seconds
create_monitor_set("auto_settings.req",30,"P=as:")
