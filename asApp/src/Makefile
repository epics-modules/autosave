TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CFLAGS += -DUSE_TYPED_RSET

PROD_HOST += asVerify
asVerify_SRCS	+= asVerify.c verify.c
asVerify_LIBS	+= ca
asVerify_LIBS	+= Com

#=============================
# xxxRecord.h will be created from xxxRecord.dbd

#DBDINC += xxxRecord

# To enable autosaveBuild, use dbLoadRecordsHookRegister().  (Requires EPICS
# base 3.14.12.5 or later.)

USR_CFLAGS += -DDBLOADRECORDSHOOKREGISTER

DBD += asSupport.dbd
DBD += asSupportNfs.dbd

#include files
INC += osdNfs.h
INC += autosave_release.h

#=============================

LIBRARY_IOC += autosave

#os independent code
autosave_SRCS += dbrestore.c save_restore.c nfs_utils.c
autosave_SRCS += initHooks.c fGetDateStr.c
autosave_SRCS += configMenuSub.c verify.c

#os dependent code
autosave_SRCS += osdNfs.c

autosave_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

dbrestore.o:   ../Makefile

$(COMMON_DIR)/autosave_release.h: $(TOP)/configure/CONFIG ../autosave_release.pl
	$(PERL) ../autosave_release.pl $(AUTOSAVE_RELEASE) > $@
