/***************************************************************************/
/*   $Id: drvSnmp.c,v 1.1.1.1 2008/02/22 19:50:56 ernesto Exp $                   */
/***************************************************************************\
 *   File:              drvSNMP.c
 *   Author:            Sheng Peng
 *   Email:             pengsh2003@yahoo.com
 *   Phone:             408-660-7762
 *   Company:           RTESYS, Inc.
 *   Date:              07/2007
 *   Version:           1.0
 *
 *   EPICS driver support for net-snmp
 *
\***************************************************************************/

#include "devSnmp.h"

ELLLIST snmpAgentList = {{NULL, NULL}, 0};
epicsMutexId snmpAgentListLock;

int SNMP_DRV_DEBUG = 0;
int snmpMaxVarsPerMsg = 30;	/* Max number of variables per query message */

/*********************************************************************/
static int Snmp_Operation(SNMP_AGENT * pSnmpAgent)
{
    int     msgQstatus, status;
    int     loop;

    int     NofQrys;	/* Num of queries we will process in one msg */
    SNMP_QUERY  *pQuery;
    ELLLIST queryList;

    struct snmp_pdu * respPdu;

    struct variable_list *pVar;
    int varIndex;
    char pBuf[1024];
    char pErrBuf[1024];

    if(pSnmpAgent == NULL)
    {
        errlogPrintf("Operation thread quit because no legal pSnmpAgent!\n");
        return -1;
    }

    while(TRUE)
    {
        msgQstatus = epicsMessageQueueReceive(pSnmpAgent->msgQ_id, &pQuery, sizeof(SNMP_QUERY *) );
        if(msgQstatus < 0)
        {/* we should never time out, so something wrong */
            errlogPrintf("Operation %s msgQ timeout!\n", pSnmpAgent->pActiveSession->peername);
            epicsThreadSleep(2.0);  /* Avoid super loop to starve CPU */
        }
        else
        {/* some requests come in */
            if(SNMP_DRV_DEBUG) printf("Oper task for agent[%s] gets requests!\n", pSnmpAgent->pActiveSession->peername);

            /* Figure out how many requests in queue */
            NofQrys = epicsMessageQueuePending(pSnmpAgent->msgQ_id);

            /* Deal up to max number of variables per query, don't forget we already got one query */
            NofQrys = MIN( (NofQrys + 1), (MAX(1, snmpMaxVarsPerMsg)) ) - 1;

            /* We just re-init link list instead of delete, because there is no resource issue */
            ellInit( &queryList );
            /* create a PDU for this query */
            pSnmpAgent->reqPdu = snmp_pdu_create(SNMP_MSG_GET);

            if(pSnmpAgent->reqPdu == NULL)
            {/* Failed to create query PDU */
                /* Read out requests. We loop one more time, because we already read out one pQuery */
                for(loop=0; loop<=NofQrys; loop++)
                {
                    if(loop != 0) epicsMessageQueueReceive(pSnmpAgent->msgQ_id, &pQuery, sizeof(SNMP_QUERY *));

                    pQuery->errCode = SNMP_QUERY_PDU_ERR;
                    pQuery->opDone = 1;
                    if(pQuery->pRecord)
                    {
                        dbScanLock(pQuery->pRecord);
                        (*(pQuery->pRecord->rset->process))(pQuery->pRecord);
                        dbScanUnlock(pQuery->pRecord);
                    }
                }
            }
            else
            {/* Read out and re-organize all requests */
                /* Read out requests. We loop one more time, because we already read out one pQuery */
                for(loop=0; loop<=NofQrys; loop++)
                {
                    if(loop != 0) epicsMessageQueueReceive(pSnmpAgent->msgQ_id, &pQuery, sizeof(SNMP_QUERY *));
                    pQuery->errCode = SNMP_QUERY_NO_ERR;  /* clean up err_code before we execute it */
                    pQuery->opDone = 0;   /* We didn't start yet, of cause not done */
                    /* place each query into the link list */
                    ellAdd( (ELLLIST *) &queryList, (ELLNODE *) pQuery );
                    /* Add each query into the pdu */
                    snmp_add_null_var(pSnmpAgent->reqPdu, pQuery->objectId.queryOid, pQuery->objectId.queryOidLen);
                }

                /* query the snmp agent */
                status = snmp_sess_synch_response(pSnmpAgent->pSess, pSnmpAgent->reqPdu, &respPdu);
                switch(status)
                {
                    case STAT_SUCCESS:
                        pVar = respPdu->variables;
                        if (respPdu->errstat == SNMP_ERR_NOERROR)
                        {/* Successfully got response, and no error in the response */
                            while (pVar != NULL)
                            {
                                for(pQuery = (SNMP_QUERY *)ellFirst(&queryList); pQuery; pQuery = (SNMP_QUERY *)ellNext( (ELLNODE *) pQuery ))
                                {/* Looking for query with matched Oid */
                                    if(!memcmp(pVar->name, pQuery->objectId.queryOid, pQuery->objectId.queryOidLen * sizeof(oid))) break;
                                }

                                if(pQuery)
                                {/* Found the matched query */
                                    /* snprint_value converts the value into a text string. If the buffer is not big enough, it might return -1 and only  */
                                    /* put data type in. So we use a large temp buffer here.                                                              */
                                    int cvtSts;
                                    cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                                    if(cvtSts <= 0)
                                    {
                                        errlogPrintf("Conversion failed for record [%s]\n", pQuery->pRecord->name);
                                        pQuery->errCode = SNMP_QUERY_CVT_ERR;	/* Conversion failure */
                                    }
                                    else
                                    {
                                        strncpy(pQuery->pValStr, pBuf, pQuery->valStrLen - 1);
                                        pQuery->pValStr[pQuery->valStrLen - 1] = '\0';
                                        pQuery->errCode = SNMP_QUERY_NO_ERR;
                                    }
                                    pQuery->opDone = 1;

                                    /* process record */
                                    if(pQuery->pRecord)
                                    {
                                        if(SNMP_DRV_DEBUG > 1) printf("Got response for record [%s]=[%s]\n", pQuery->pRecord->name, pQuery->pValStr);
                                        dbScanLock(pQuery->pRecord);
                                        (*(pQuery->pRecord->rset->process))(pQuery->pRecord);
                                        dbScanUnlock(pQuery->pRecord);
                                        if(SNMP_DRV_DEBUG > 1) printf("Record [%s] processed\n", pQuery->pRecord->name);
                                    }

                                    /* Remove the query from link list */
                                    epicsMutexLock(pSnmpAgent->mutexLock); 
                                    ellDelete(&queryList, (ELLNODE *) pQuery);
                                    epicsMutexUnlock(pSnmpAgent->mutexLock); 
                                }
                                else
                                {
                                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                    errlogPrintf("Get response for %s, but no matched query!\n", pBuf);
                                }

                                pVar = pVar->next_variable;
                            }
                        }
                        else
                        {/* Successfully got response, and with error in the response, error is defined by errstat and errindex */
                            for (varIndex = 1; pVar && varIndex != respPdu->errindex; pVar = pVar->next_variable, varIndex++)
                            {
                                for(pQuery = (SNMP_QUERY *)ellFirst(&queryList); pQuery; pQuery = (SNMP_QUERY *)ellNext( (ELLNODE *) pQuery ))
                                {/* Looking for query with matched Oid */
                                    if(!memcmp(pVar->name, pQuery->objectId.queryOid, pQuery->objectId.queryOidLen * sizeof(oid))) break;
                                }

                                if(pQuery)
                                {/* Found the matched query */
                                    /* snprint_value converts the value into a text string. If the buffer is not big enough, it might return -1 and only  */
                                    /* put data type in. So we use a large temp buffer here.                                                              */
                                    int cvtSts;
                                    cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                                    if(cvtSts <= 0)
                                    {
                                        errlogPrintf("Conversion failed for record [%s]\n", pQuery->pRecord->name);
                                        pQuery->errCode = SNMP_QUERY_CVT_ERR;	/* Conversion failure */
                                    }
                                    else
                                    {
                                        strncpy(pQuery->pValStr, pBuf, pQuery->valStrLen - 1);
                                        pQuery->pValStr[pQuery->valStrLen - 1] = '\0';
                                        pQuery->errCode = SNMP_QUERY_NO_ERR;
                                    }
                                    pQuery->opDone = 1;

                                    /* process record */
                                    if(pQuery->pRecord)
                                    {
                                        dbScanLock(pQuery->pRecord);
                                        (*(pQuery->pRecord->rset->process))(pQuery->pRecord);
                                        dbScanUnlock(pQuery->pRecord);
                                    }

                                    /* Remove the query from link list */
                                    epicsMutexLock(pSnmpAgent->mutexLock);
                                    ellDelete(&queryList, (ELLNODE *) pQuery);
                                    epicsMutexUnlock(pSnmpAgent->mutexLock);
                                }
                                else
                                {
                                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                    errlogPrintf("Get response for %s, but no matched query!\n", pBuf);
                                }
                            }

                            if(pVar)
                                snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                            else
                                strcpy(pBuf, "(none)");

                            sprintf(pErrBuf, "snmp_sess_synch_response: errindex %ld, %s %s %s\n",
                                respPdu->errindex, pSnmpAgent->pActiveSession->peername, pQuery->objectId.queryName, pBuf);
                            errlogPrintf("%s", pErrBuf);
                            snmp_sess_perror(pErrBuf, pSnmpAgent->pActiveSession);
                        }

                        /* If still some queries in link list and can't find variables to match */
                        for(pQuery = (SNMP_QUERY *)ellFirst(&queryList); pQuery; pQuery = (SNMP_QUERY *)ellNext( (ELLNODE *) pQuery ))
                        {/* Go thru the query list to process all records */

                             pQuery->errCode = SNMP_QUERY_QRY_NOANS; /* snmp query no answer in response */
                             pQuery->opDone = 1;

                            /* process record */
                            if(pQuery->pRecord)
                            {
                                dbScanLock(pQuery->pRecord);
                                (*(pQuery->pRecord->rset->process))(pQuery->pRecord);
                                dbScanUnlock(pQuery->pRecord);
                            }
                            errlogPrintf("%s: Response doesn't have a match!\n", pQuery->pRecord->name);
                        }

                        break;
                    case STAT_TIMEOUT:
                        for(pQuery = (SNMP_QUERY *)ellFirst(&queryList); pQuery; pQuery = (SNMP_QUERY *)ellNext( (ELLNODE *) pQuery ))
                        {/* Go thru the query list to process all records */

                             pQuery->errCode = SNMP_QUERY_QRY_TOUT; /* snmp query timeout */
                             pQuery->opDone = 1;

                            /* process record */
                            if(pQuery->pRecord)
                            {
                                dbScanLock(pQuery->pRecord);
                                (*(pQuery->pRecord->rset->process))(pQuery->pRecord);
                                dbScanUnlock(pQuery->pRecord);
                            }
                        }

                        errlogPrintf("%s: Snmp Query Timeout\n", pSnmpAgent->pActiveSession->peername);
                        break;
                    case STAT_ERROR:
                    default:
                    {
                        int liberr, syserr;
                        char * errstr;
                        snmp_sess_perror(pSnmpAgent->pActiveSession->peername, pSnmpAgent->pActiveSession);
                        snmp_sess_error(pSnmpAgent->pSess, &liberr, &syserr, &errstr);

                        for(pQuery = (SNMP_QUERY *)ellFirst(&queryList); pQuery; pQuery = (SNMP_QUERY *)ellNext( (ELLNODE *) pQuery ))
                        {/* Go thru the query list to process all records */

                             pQuery->errCode = SNMP_QUERY_SNMP_ERR |(liberr?liberr:syserr); /* snmp error */
                             pQuery->opDone = 1;

                            /* process record */
                            if(pQuery->pRecord)
                            {
                                dbScanLock(pQuery->pRecord);
                                (*(pQuery->pRecord->rset->process))(pQuery->pRecord);
                                dbScanUnlock(pQuery->pRecord);
                            }
                        }

                        errlogPrintf("Query %s error %s!\n", pSnmpAgent->pActiveSession->peername, errstr);
                        free(errstr);
                    }
                        break;
                }

                /* Free the response PDU for this query. */
                /*If error happened before, the reqPdu is already freed by snmp_sess_synch_response, the respPdu is NULL */
                /* If no error happened, respPdu is cloned from reqPdu and reqPdu is freed */
                if(respPdu)
                {
                    snmp_free_pdu(respPdu);
                    respPdu = NULL;
                    if(SNMP_DRV_DEBUG) printf("Oper task for agent[%s] frees PDU!\n", pSnmpAgent->pActiveSession->peername);
                }

            }/* PDU ok */
        }/* process requests */

    }/* infinite loop */

    snmp_sess_close(pSnmpAgent->pSess);
    /* We should never get here */
    return 0;
}

int snmpQueryInit(dbCommon * pRecord, const char * ioString, long snmpVersion, size_t valStrLen)
{
    static int snmpInited = FALSE;

    SNMP_AGENT   * pSnmpAgent;

    SNMP_QRYINFO * pSnmpQryInfo;
    SNMP_QRYPTR  * pSnmpQryPtr;
    SNMP_QUERY   * pSnmpQuery;

    char peerName[81];
    char community[81];
    char oidStr[81];

    int n;

    int liberr, syserr;
    char * errstr;

    if (!snmpInited)
    {
        init_snmp("epicsSnmp");
        /* add_mibdir("/usr/share/snmp/mibs"); */	/* Does not need */
        init_mib();

        ellInit(&snmpAgentList);
        snmpAgentListLock = epicsMutexMustCreate();

        SOCK_STARTUP;
        snmpInited = TRUE;
    }

    /* parameter check */
    if(!pRecord || !ioString) return -1;

    if(snmpVersion != SNMP_VERSION_1 && snmpVersion != SNMP_VERSION_3)
        snmpVersion = SNMP_VERSION_2c;

    n = sscanf(ioString, "%s %s %s", peerName, community, oidStr);
    if(n != 3) return -1;
    if(SNMP_DRV_DEBUG) printf("Query: Agent[%s], Community[%s], OIDStr[%s]\n", peerName, community, oidStr);

    /* Check if the agent is already in our list, or else add it */
    epicsMutexLock(snmpAgentListLock);
    for( pSnmpAgent = (SNMP_AGENT *)ellFirst(&snmpAgentList); pSnmpAgent; pSnmpAgent = (SNMP_AGENT *)ellNext((ELLNODE *)pSnmpAgent) )
    {
        if(!strcmp(pSnmpAgent->snmpSession.peername, peerName))
        {
            if(!strcmp(pSnmpAgent->snmpSession.community, community))
            {
                if(pSnmpAgent->snmpSession.version == snmpVersion) break;
            }
        }
    }

    if(!pSnmpAgent)
    {/* Did not find any existing matching agent, create one */
        pSnmpAgent = callocMustSucceed(1, sizeof(SNMP_AGENT), "calloc buffer for SNMP_AGENT");

        pSnmpAgent->mutexLock = epicsMutexMustCreate();

        snmp_sess_init(&(pSnmpAgent->snmpSession));
        /* http://www.net-snmp.org/support/irc/net-snmp.log.2007-7-4.html */
        /* According the above URL, the port is part of peerName now */
        pSnmpAgent->snmpSession.peername = epicsStrDup(peerName);
        pSnmpAgent->snmpSession.community = (unsigned char*)epicsStrDup(community);
        pSnmpAgent->snmpSession.community_len = strlen(community);
        pSnmpAgent->snmpSession.version = snmpVersion;

        pSnmpAgent->pSess = snmp_sess_open(&(pSnmpAgent->snmpSession)); /* <-- an opaque pointer, not a struct pointer */
        if(pSnmpAgent->pSess == NULL)
        {
            snmp_error(&(pSnmpAgent->snmpSession), &liberr, &syserr, &errstr);
            errlogPrintf("snmp_session_open for %s error %s!\n", peerName, errstr);
            free(errstr);
            epicsThreadSuspendSelf();
            return -1;
        }

        /* In case we need to change the content of session */
        pSnmpAgent->pActiveSession = snmp_sess_session(pSnmpAgent->pSess);
        if(SNMP_DRV_DEBUG) 
        {
            char * ver_msg[5]={"Ver1", "Ver2c", "Ver2u", "V3", "Unknown"};
            printf("Active SNMP agent: %s,", pSnmpAgent->pActiveSession->peername);
            printf("\tcommunity: %s, version: %s\n", pSnmpAgent->pActiveSession->community, ver_msg[MIN(4,pSnmpAgent->pActiveSession->version)]);
        }

        ellInit(&(pSnmpAgent->snmpQryPtrList));

        pSnmpAgent->msgQ_id = epicsMessageQueueCreate(OPTHREAD_MSGQ_CAPACITY, sizeof(SNMP_QUERY *));
        if(pSnmpAgent->msgQ_id == NULL)
        {
            errlogPrintf("Fail to create message queue for agent %s!\n", peerName);
            epicsThreadSuspendSelf();
            return -1;
        }

        /* Create the operation thread */
        bzero(pSnmpAgent->opthread_name,MAX_CA_STRING_SIZE);
        strncpy(pSnmpAgent->opthread_name, peerName, MAX_CA_STRING_SIZE-1);
        pSnmpAgent->opthread_name[MAX_CA_STRING_SIZE-1] = '\0';

        pSnmpAgent->opthread_id = epicsThreadCreate(pSnmpAgent->opthread_name, OPTHREAD_PRIORITY, OPTHREAD_STACK, (EPICSTHREADFUNC)Snmp_Operation, (void *)pSnmpAgent);
        if(pSnmpAgent->opthread_id == (epicsThreadId)(-1))
        {
            epicsMutexUnlock(pSnmpAgent->mutexLock);
            errlogPrintf("Fail to create operation thread for snmp agent %s, Fatal!\n", peerName);
            epicsThreadSuspendSelf();
            return -1;
        }

        /* we will create PDU before each time we query the agent */
        pSnmpAgent->reqPdu = NULL;

        pSnmpAgent->status = 0;

        /* We successfully allocate all resource */
        ellAdd( &snmpAgentList, (ELLNODE *)pSnmpAgent);

        if(SNMP_DRV_DEBUG) printf("Add snmp agent %s\n", peerName);
    }
    epicsMutexUnlock(snmpAgentListLock);
    /* Check if the agent is already in our list, or else add it */

    /* Query info prepare */
    pSnmpQryInfo = (SNMP_QRYINFO *)callocMustSucceed(1, sizeof(SNMP_QRYINFO), "calloc SNMP_QRYINFO");

    pSnmpQryPtr = &(pSnmpQryInfo->qryptr);
    pSnmpQryPtr->pQuery = &(pSnmpQryInfo->query);

    pSnmpQuery = &(pSnmpQryInfo->query);

    pSnmpQuery->pRecord = pRecord;
    pSnmpQuery->pSnmpAgent = pSnmpAgent;

    pSnmpQuery->objectId.queryName = epicsStrDup(oidStr);
    pSnmpQuery->objectId.queryOidLen = MAX_OID_LEN;

    if (!get_node(pSnmpQuery->objectId.queryName, pSnmpQuery->objectId.queryOid, &pSnmpQuery->objectId.queryOidLen))
    {/* Search traditional MIB2 and find nothing */
        if (!read_objid(pSnmpQuery->objectId.queryName, pSnmpQuery->objectId.queryOid, &pSnmpQuery->objectId.queryOidLen))
        {
            snmp_perror("Parsing objectId"); /* parsing error has nothing to do with session */
            errlogPrintf("Fail to parse the objectId %s.\n", oidStr);
            pRecord->dpvt = NULL;
            free(pSnmpQuery->objectId.queryName);
            free(pSnmpQryInfo);
            return -1;
        }
    }

    /* Set when we get response */
    pSnmpQuery->valStrLen = MAX(valStrLen, MAX_CA_STRING_SIZE);
    pSnmpQuery->pValStr = (char *)callocMustSucceed(1, pSnmpQuery->valStrLen, "calloc pValStr");

    pSnmpQuery->opDone = 0;
    pSnmpQuery->errCode = SNMP_QUERY_NO_ERR;
    
    epicsMutexLock(pSnmpAgent->mutexLock);
    ellAdd( &(pSnmpAgent->snmpQryPtrList), (ELLNODE *)pSnmpQryPtr );
    epicsMutexUnlock(pSnmpAgent->mutexLock);

    pRecord->dpvt = (void *)pSnmpQuery;

    return 0;
}

/**************************************************************************************************/
/* Here we supply the driver report function for epics                                            */
/**************************************************************************************************/
static  long    Snmp_EPICS_Init();
static  long    Snmp_EPICS_Report(int level);

const struct drvet drvSnmp = {2,                              /*2 Table Entries */
                             (DRVSUPFUN) Snmp_EPICS_Report,  /* Driver Report Routine */
                             (DRVSUPFUN) Snmp_EPICS_Init};   /* Driver Initialization Routine */

#if EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(drvet,drvSnmp);
#endif

/* implementation */
static long Snmp_EPICS_Init()
{
    return 0;
}

static long Snmp_EPICS_Report(int level)
{
    SNMP_AGENT  * pSnmpAgent;

    printf("\n"SNMP_DRV_VER_STRING"\n\n");

    if(level > 0)   /* we only get into link list for detail when user wants */
    {
        for(pSnmpAgent=(SNMP_AGENT *)ellFirst(&snmpAgentList); pSnmpAgent; pSnmpAgent = (SNMP_AGENT *)ellNext((ELLNODE *)pSnmpAgent))
        {
            printf("\tSNMP agent: %s\n", pSnmpAgent->snmpSession.peername);
            if(level > 1)
            {
                char * ver_msg[5]={"Ver1", "Ver2c", "Ver2u", "V3", "Unknown"};
                /* Due to snmp_sess_open and snmp_sess_session don't guarantee string terminator of community but use community_len, we use original snmpSession here */
                printf("\tcommunity: %s, version: %s, %d variables\n\n", pSnmpAgent->snmpSession.community, ver_msg[MIN(4,pSnmpAgent->snmpSession.version)], pSnmpAgent->snmpQryPtrList.count);
            }
        }
    }

    return 0;
}

