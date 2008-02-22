/***************************************************************************/
/*   $Id: snmpRegister.cpp,v 1.0 2007/07/26 05:38:30 peng Exp $            */
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

#include "iocsh.h"


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
}

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
