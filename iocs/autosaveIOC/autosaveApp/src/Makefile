TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# build an ioc application for testing

PROD_IOC = asApp

DBD += as.dbd

as_DBD += base.dbd
as_DBD += $(PROD_DBD_DEPENDS)

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
asApp_SRCS_DEFAULT += as_registerRecordDeviceDriver.cpp
asApp_SRCS_DEFAULT += asMain.cpp
asApp_SRCS_vxWorks += -nil-
#The following adds support from base/src/vxWorks
asApp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary


asApp_LIBS := $(PROD_LIBS_DEPENDS) $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
