#!../../bin/linux-x86/snmp

## You may have to change snmp to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase("dbd/snmp.dbd")
snmp_registerRecordDeviceDriver(pdbbase)

## Load record instances
dbLoadRecords("db/snmpDemo.db","HOST=192.168.2.19,COMMUNITY=public,VER=V2c")

## Set this to see messages from mySub
#var mySubDebug 1
SNMP_DRV_DEBUG(1)
SNMP_DEV_DEBUG(1)

# Each SNMP query message could query multi variables.
# This number needs to be the minimum one of all your agents
snmpMaxVarsPerMsg(30)

cd ${TOP}/iocBoot/${IOC}
iocInit()
epicsSnmpInit()

