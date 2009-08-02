/***************************************************************************/
/*   $Id: devSnmp.h,v 1.1.1.1 2008/02/22 19:50:56 ernesto Exp $                   */
/***************************************************************************\
 *   File:              devSNMP.h
 *   Author:            Sheng Peng
 *   Email:             pengsh2003@yahoo.com
 *   Phone:             408-660-7762
 *   Company:           RTESYS, Inc.
 *   Date:              07/2007
 *   Version:           1.0
 *
 *   EPICS device support header file for net-snmp
 *
\***************************************************************************/

#ifndef	_DEVSNMP_H_
#define	_DEVSNMP_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <epicsVersion.h>
#if EPICS_VERSION>=3 && EPICS_REVISION>=14
#include <epicsExport.h>
#include <alarm.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <recSup.h>
#include <recGbl.h>
#include <devSup.h>
#include <drvSup.h>
#include <link.h>
#include <ellLib.h>
#include <errlog.h>
#include <special.h>
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsMutex.h>
#include <epicsInterrupt.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <cantProceed.h>
#include <osiSock.h>
#include <devLib.h>

#include <aiRecord.h>
#include <stringinRecord.h>
#include <longinRecord.h>
#include <waveformRecord.h>

#else
#error "We need EPICS 3.14 or above to support OSI calls!"
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SNMP_DRV_VER_STRING "epics SNMP support V2.0"

/******************************************************************************************/
/* We have an opThread running for each snmp agent,                                       */
/* it picks up request from msgQ and line them up in link list and execute it             */
/******************************************************************************************/
#define OPTHREAD_PRIORITY       (50)            /* opthread Priority, make it a little lower than scan task to finish all request once */
#define OPTHREAD_STACK          (0x20000)       /* opthread Stack Size */
#define OPTHREAD_MSGQ_CAPACITY  (200)           /* This means we can support 200 signal records per coupler */

/******************************************************************************************/
/*********************       EPICS device support return        ***************************/
/******************************************************************************************/
#define CONVERT                 (0)
#define NO_CONVERT              (2)
#define MAX_CA_STRING_SIZE      (40)

/* hostname, community name and snmp version define a unique agent */
typedef struct SNMP_AGENT
{
    ELLNODE			node;	/* Link List Node */

    epicsMutexId		mutexLock;

    struct snmp_session		snmpSession;
    void 			*pSess;  /* <-- an opaque pointer, not a struct pointer */
    struct snmp_session		*pActiveSession;

    ELLLIST			snmpQryPtrList;

    epicsMessageQueueId		msgQ_id;        /* Through this message queue, record processing sends request to opthread */
    epicsThreadId		opthread_id;    /* operation thread ID for this snmp agent */
    char			opthread_name[MAX_CA_STRING_SIZE];

    struct snmp_pdu		*reqPdu;

    int				status;		/* not really used, reserved for future */

} SNMP_AGENT;

/* ObjectID to query */
typedef struct OID
{
    char		*queryName;
    oid			queryOid[MAX_OID_LEN];
    unsigned int	queryOidLen;
} OID;

typedef struct SNMP_QUERY
{
    ELLNODE		node;	/* Link List Node */

    dbCommon		*pRecord;
    SNMP_AGENT		*pSnmpAgent;

    OID			objectId;

    size_t		valStrLen;	/* size of pValStr buf, not strlen of it */
    char		*pValStr;
    int			opDone;
    unsigned int	errCode;

    int			cnt;
    char		*mask;
} SNMP_QUERY;

#define SNMP_QUERY_NO_ERR 0
#define SNMP_QUERY_PDU_ERR 0x00010000
#define SNMP_QUERY_SNMP_ERR 0x00020000
#define SNMP_QUERY_QRY_TOUT 0x00030000
#define SNMP_QUERY_QRY_NOANS 0x00040000
#define SNMP_QUERY_CVT_ERR 0x00050000	/* conversion failed, usually because the buffer is not big enough */

typedef struct SNMP_QRYPTR
{
    ELLNODE	node;	/* Link List Node */
    SNMP_QUERY  *pQuery;
} SNMP_QRYPTR;

typedef struct SNMP_QRYINFO
{
    SNMP_QRYPTR qryptr;
    SNMP_QUERY  query;
} SNMP_QRYINFO;

int snmpQueryInit(dbCommon * pRecord, const char * ioString, long snmpVersion, size_t valStrLen);

#ifdef __cplusplus
}
#endif

#endif

