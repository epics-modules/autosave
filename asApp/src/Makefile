TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#PROD_HOST += caExample
#caExample_SRCS	+= caExample.c
#caExample_LIBS	+= ca
#caExample_LIBS	+= Com
#ca_DIR           = $(EPICS_BASE_LIB)
#Com_DIR          = $(EPICS_BASE_LIB)


#=============================
# xxxRecord.h will be created from xxxRecord.dbd

#DBDINC += xxxRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += as.dbd
DBD += asSupport.dbd

#=============================

LIBRARY_IOC += autosave

autosave_SRCS += dbrestore.c save_restore.c
autosave_SRCS += initHooks.c fGetDateStr.c

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
autosave_SRCS += as_registerRecordDeviceDriver.cpp
#autosave_SRCS_DEFAULT += asMain.cpp #maybe only for win32
#autosave_SRCS_vxWorks += -nil- #?

#The following adds support from base/src/vxWorks
#autosave_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

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

