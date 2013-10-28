
# executable path is below...
# ../../bin/linux-x86/snmp

< envPaths
cd "${TOP}"
epicsEnvSet("MIBDIRS", "+$(TOP)/mibs")
epicsEnvSet("W", "WIENER-CRATE-MIB::")

dbLoadDatabase("dbd/snmp.dbd")
snmp_registerRecordDeviceDriver(pdbbase)

dbLoadRecords("db/wienerCrate.db","CRATE=adv-trigger-crate,SYS=MON,SUBSYS=CRATE")
dbLoadRecords("db/wienerCrate.db","CRATE=taps-vme-0-crate,SYS=MON,SUBSYS=CRATE")
dbLoadRecords("db/wienerCrate.db","CRATE=taps-vme-10-crate,SYS=MON,SUBSYS=CRATE")
dbLoadRecords("db/wienerCrate.db","CRATE=cb-adc-1-crate,SYS=MON,SUBSYS=CRATE")
dbLoadRecords("db/wienerCrate.db","CRATE=cb-adc-2-crate,SYS=MON,SUBSYS=CRATE")
dbLoadRecords("db/wienerCrate.db","CRATE=mwpc-adc-crate,SYS=MON,SUBSYS=CRATE")

cd "${TOP}/iocBoot/${IOC}"
iocInit()
dbl > "/tmp/ioc/snmp"
