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

cd startup
################################################################################
# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in the software we just loaded (as.munch)
dbLoadDatabase("../../dbd/iocas.dbd")
iocas_registerRecordDeviceDriver(pdbbase)

# save_restore setup
< save_restore.cmd

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

# The following command makes the autosave request files 'info_settings.req',
# and 'info_positions.req', from information (info nodes) contained in all of
# the EPICS databases that have been loaded into this IOC.
makeAutosaveFiles()
create_monitor_set("info_positions.req",5,"P=as:")
create_monitor_set("info_settings.req",30,"P=as:")

# configMenu example
# Note that the request file MUST be named $(CONFIG)Menu.req
# If the macro CONFIGMENU is defined with any value, backup (".savB") and
# sequence files (".savN") will not be written.  We don't want these for
# configMenu.
create_manual_set("scan1Menu.req","P=xxxL:,CONFIG=scan1,CONFIGMENU=1")
