#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure asApp
asApp_DEPEND_DIRS   = configure

ifeq ($(BUILD_IOCS), YES)
    DIRS += iocs
    iocs_DEPEND_DIRS = asApp
endif

include $(TOP)/configure/RULES_TOP
