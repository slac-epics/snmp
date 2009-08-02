TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# build an ioc application

PROD_IOC = snmp
# <name>.dbd will be created from <name>Include.dbd
DBD += snmp.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
snmp_SRCS += snmp_registerRecordDeviceDriver.cpp
snmp_SRCS_DEFAULT += snmpMain.cpp
snmp_SRCS_vxWorks += -nil-

# Add locally compiled object code
# snmp_SRCS += dbSubExample.c

# The following adds support from base/src/vxWorks
snmp_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

snmp_LIBS += devSnmp

# NOTES:
#      1)It is not possible to build sncExample both as a component of snmp
#        and standalone. You must choose only one.
#      2)To build sncExample SNCSEQ must be defined in <top>/configure/RELEASE

# The following builds sncExample as a component of snmp
# Also in snmpInclude.dbd uncomment #registrar(sncExampleRegistrar)
#snmp_SRCS += sncExample.stt
#snmp_LIBS += seq pv

snmp_LIBS += $(EPICS_BASE_IOC_LIBS)

# The following builds sncExample as a standalone application
#PROD_HOST += sncExample
#sncExample_SNCFLAGS += +m
#sncExample_SRCS += sncExample.stt
#sncExample_LIBS += seq pv
#sncExample_LIBS += $(EPICS_BASE_HOST_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

