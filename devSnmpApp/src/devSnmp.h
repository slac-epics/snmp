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
#include <sys/time.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <epicsVersion.h>
#if EPICS_VERSION>=3 && EPICS_REVISION>=14
#include <epicsExport.h>
#include <alarm.h>
#include <dbScan.h>
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
#include <aoRecord.h>
#include <mbbiRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboRecord.h>
#include <mbboDirectRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
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
#define OPTHREAD_MSGQ_CAPACITY  (2000)           /* This means we can support 2000 signal records per coupler */

/******************************************************************************************/
/*********************       EPICS device support return        ***************************/
/******************************************************************************************/
#define CONVERT                 (0)
#define NO_CONVERT              (2)
#define MAX_CA_STRING_SIZE      (40)

#define	TY_REQ_QUERY			0
#define	TY_REQ_COMMAND			1

/* hostname, community name and snmp version define a unique agent */
typedef struct SNMP_AGENT
{
    ELLNODE			node;	/* Link List Node */

    epicsMutexId		mutexLock;

    struct snmp_session		snmpSession;
    void 			*pSess;  /* <-- an opaque pointer, not a struct pointer */
    struct snmp_session		*pActiveSession;

    ELLLIST			snmpReqPtrList;

    epicsMessageQueueId		msgQ_id;        /* Through this message queue, record processing sends request to opthread */
    epicsThreadId		opthread_id;    /* operation thread ID for this snmp agent */
    char			opthread_name[MAX_CA_STRING_SIZE];

    struct snmp_pdu		*reqQryPdu;
    struct snmp_pdu		*reqCmdPdu;

    int				status;		/* not really used, reserved for future */

} SNMP_AGENT;

/* ObjectID to request */
typedef struct OID
{
    char		*requestName;
    oid			requestOid[MAX_OID_LEN];
    size_t		requestOidLen;
} OID;

typedef struct SNMP_WALK
{
    ELLNODE			node;	/* Link List Node */
    SNMP_AGENT                 *agent;
    int                         count;
    struct timeval              delay;
    OID                         objectId;
    struct timeval              nextWalk;
} SNMP_WALK;

typedef struct SNMP_WALK_OID
{
    char                        pBuf[1024];
    int                         errCode;
    IOSCANPVT                   iopvt;
} SNMP_WALK_OID;

typedef struct SNMP_REQUEST
{
    ELLNODE		node;	/* Link List Node */

    int                 cmd;	/* TY_REQ_QUERY, TY_REQ_COMMAND */
    char                type;	/* i: INTEGER, u: unsigned INTEGER, t: TIMETICKS, a: IPADDRESS */
    				/* o: OBJID, s: STRING, x: HEX STRING, d: DECIMAL STRING, b: BITS */
    				/* U: unsigned int64, I: signed int64, F: float, D: double */

    unsigned int	valLength;	/* Value length: 0 = default, non-zero = pad to this length */

    dbCommon		*pRecord;
    SNMP_AGENT		*pSnmpAgent;

    OID			objectId;

    size_t		valStrLen;	/* size of pValStr buf, not strlen of it */
    char		*pValStr;
    int			opDone;
    unsigned int	errCode;

    int			cnt;
    char		*mask;
} SNMP_REQUEST;

#define SNMP_REQUEST_NO_ERR	0
#define SNMP_REQUEST_PDU_ERR	0x00010000
#define SNMP_REQUEST_ADDVAR_ERR	0x00020000
#define SNMP_REQUEST_SNMP_ERR	0x00030000
#define SNMP_REQUEST_QRY_TOUT	0x00040000
#define SNMP_REQUEST_CMD_TOUT	0x00050000
#define SNMP_REQUEST_QRY_NOANS	0x00060000
#define SNMP_REQUEST_CMD_NOANS	0x00070000
#define SNMP_REQUEST_CVT_ERR	0x00080000	/* conversion failed, usually because the buffer is not big enough */

typedef struct SNMP_REQPTR
{
    ELLNODE	node;	/* Link List Node */
    SNMP_REQUEST  *pRequest;
} SNMP_REQPTR;

typedef struct SNMP_REQINFO
{
    SNMP_REQPTR reqptr;
    SNMP_REQUEST  request;
} SNMP_REQINFO;

int snmpRequestInit(dbCommon * pRecord, const char * ioString, long snmpVersion, size_t valStrLen, int cmd, char type);
int snmpQuerySingleVar(SNMP_REQUEST * pRequest);

extern netsnmp_oid_stash_node *stashRoot;

#ifdef __cplusplus
}
#endif

#endif

