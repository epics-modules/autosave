TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_HOST += asVerify
asVerify_SRCS	+= asVerify.c verify.c
asVerify_LIBS	+= ca
asVerify_LIBS	+= Com


#=============================
# xxxRecord.h will be created from xxxRecord.dbd

#DBDINC += xxxRecord

# To enable autosaveBuild, use dbLoadRecordsHookRegister.  (But it doesn't
# appear in EPICS base until 3.14.12.5, so disable by default for now.)
#USR_CFLAGS += -DDBLOADRECORDSHOOKREGISTER

# <name>.dbd will be created from <name>Include.dbd
DBD += as.dbd
DBD += asSupport.dbd

#include files
INC += osdNfs.h

#=============================

LIBRARY_IOC += autosave

#os independent code
autosave_SRCS += dbrestore.c save_restore.c
autosave_SRCS += initHooks.c fGetDateStr.c
autosave_SRCS += configMenuSub.c verify.c

#os dependent code
autosave_SRCS += osdNfs.c

autosave_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application for testing

PROD_IOC = asApp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
asApp_SRCS_DEFAULT += as_registerRecordDeviceDriver.cpp
asApp_SRCS_DEFAULT += asMain.cpp
asApp_SRCS_vxWorks += -nil-
#The following adds support from base/src/vxWorks
asApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

asApp_LIBS += autosave
asApp_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

dbrestore.o:   ../Makefile
