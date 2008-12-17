#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure asApp iocBoot
asApp_DEPEND_DIRS   = configure
iocBoot_DEPEND_DIRS = asApp
include $(TOP)/configure/RULES_TOP
