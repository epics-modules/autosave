#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure autosaveApp
autosaveApp_DEPEND_DIRS   = configure


DIRS += iocBoot
iocBoot_DEPEND_DIRS = autosaveApp


include $(TOP)/configure/RULES_TOP
