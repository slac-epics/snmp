/***************************************************************************\
 *   File:              snmpRegister.cpp
 *   Author:            Sheng Peng
 *   Email:             pengsh2003@yahoo.com
 *   Phone:             408-660-7762
 *   Company:           RTESYS, Inc.
 *   Date:              07/2007
 *   Version:           1.0
 *
 *   EPICS OSI shell function registration for net-snmp
 *
\***************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/mib_api.h>

#include "iocsh.h"
#include "registryFunction.h"
#include "epicsExport.h"


#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

extern int SNMP_DRV_DEBUG;
extern int SNMP_DEV_DEBUG;
extern int snmpMaxVarsPerMsg;

static const iocshArg SNMP_DRV_DEBUGArg0 = {"value", iocshArgInt};
static const iocshArg *const SNMP_DRV_DEBUGArgs[1] = {&SNMP_DRV_DEBUGArg0};
static const iocshFuncDef SNMP_DRV_DEBUGDef = {"SNMP_DRV_DEBUG", 1, SNMP_DRV_DEBUGArgs};
static void SNMP_DRV_DEBUGCall(const iocshArgBuf * args) {
        SNMP_DRV_DEBUG = args[0].ival;
}

static const iocshArg SNMP_DEV_DEBUGArg0 = {"value", iocshArgInt};
static const iocshArg *const SNMP_DEV_DEBUGArgs[1] = {&SNMP_DEV_DEBUGArg0};
static const iocshFuncDef SNMP_DEV_DEBUGDef = {"SNMP_DEV_DEBUG", 1, SNMP_DEV_DEBUGArgs};
static void SNMP_DEV_DEBUGCall(const iocshArgBuf * args) {
        SNMP_DEV_DEBUG = args[0].ival;
}

static const iocshArg snmpMaxVarsPerMsgArg0 = {"value", iocshArgInt};
static const iocshArg *const snmpMaxVarsPerMsgArgs[1] = {&snmpMaxVarsPerMsgArg0};
static const iocshFuncDef snmpMaxVarsPerMsgDef = {"snmpMaxVarsPerMsg", 1, snmpMaxVarsPerMsgArgs};
static void snmpMaxVarsPerMsgCall(const iocshArgBuf * args) {
        snmpMaxVarsPerMsg = args[0].ival;
}

static const iocshArg epicsSnmpInitArg0 = {"value", iocshArgInt};
static const iocshArg *const epicsSnmpInitArgs[1] = {&epicsSnmpInitArg0};
static const iocshFuncDef epicsSnmpInitDef = {"epicsSnmpInit", 1, epicsSnmpInitArgs};
static void epicsSnmpInitCall(const iocshArgBuf * args) {
	printf("Function epicsSnmpInit is obsoleted\n");
};

static const iocshArg add_mibdirArg0 = {"dirpath", iocshArgString};
static const iocshArg *const add_mibdirArgs[1] = {&add_mibdirArg0};
static const iocshFuncDef add_mibdirDef = {"add_mibdir", 1, add_mibdirArgs};
static void add_mibdirCall(const iocshArgBuf * args)
{
        add_mibdir( args[0].sval );
}

static const iocshArg read_mibArg0 = {"mib-filename", iocshArgString};
static const iocshArg *const read_mibArgs[1] = {&read_mibArg0};
static const iocshFuncDef read_mibDef = {"read_mib", 1, read_mibArgs};
static void read_mibCall(const iocshArgBuf * args)
{
        read_mib( args[0].sval );
}

int write_mib_tree( const char * pszFileName )
{
	if ( pszFileName == NULL )
	{
		fprintf( stderr, "write_mib_tree Error: NULL ptr to filename\n" );
		printf( "write_mib_tree Usage: write_mib_tree( \"filename_to_open\"\n" );
		return -1;
	}

	FILE		*	pFile	= fopen( pszFileName, "w" );
	if ( pFile == NULL )
	{
		fprintf( stderr, "write_mib_tree Error: Unable to open %s\n", pszFileName );
		return -1;
	}

	print_mib( pFile );
	print_oid_report( pFile );
	fclose( pFile );
	return 0;
}


static const iocshArg write_mib_treeArg0 = {"output-filename", iocshArgString};
static const iocshArg *const write_mib_treeArgs[1] = {&write_mib_treeArg0};
static const iocshFuncDef write_mib_treeDef = {"write_mib_tree", 1, write_mib_treeArgs};
static void write_mib_treeCall(const iocshArgBuf * args)
{
	write_mib_tree( args[0].sval );
}

void snmp_Register()
{
   static int firstTime = 1;

   if  (!firstTime)
      return;
   firstTime = 0;
   iocshRegister(&SNMP_DRV_DEBUGDef, SNMP_DRV_DEBUGCall);
   iocshRegister(&SNMP_DEV_DEBUGDef, SNMP_DEV_DEBUGCall);
   iocshRegister(&snmpMaxVarsPerMsgDef, snmpMaxVarsPerMsgCall);
   iocshRegister(&epicsSnmpInitDef, epicsSnmpInitCall);
   iocshRegister(&add_mibdirDef, add_mibdirCall);
   iocshRegister(&read_mibDef, read_mibCall);
   iocshRegister(&write_mib_treeDef, write_mib_treeCall);
}

epicsRegisterFunction( add_mibdir );
epicsRegisterFunction( read_mib );
epicsRegisterFunction( write_mib_tree );

#ifdef __cplusplus
}
#endif	/* __cplusplus */

class snmp_CommonInit {
    public:
    snmp_CommonInit() {
	snmp_Register();
    }
};
static snmp_CommonInit snmp_CommonInitObj;
