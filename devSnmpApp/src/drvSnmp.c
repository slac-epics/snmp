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
#include "epicsExport.h"
#include <unistd.h>

ELLLIST snmpAgentList = {{NULL, NULL}, 0};
epicsMutexId snmpAgentListLock;

int SNMP_DRV_DEBUG = 0;
int snmpMaxVarsPerMsg = 60;	/* Max number of variables per request message */

epicsExportAddress( int, SNMP_DRV_DEBUG );
epicsExportAddress( int, snmpMaxVarsPerMsg );

static SNMP_AGENT *snmpGetAgent(char *peerName, char *community, long snmpVersion);

ELLLIST snmpWalkList = {{NULL, NULL}, 0};
epicsMutexId snmpWalkListLock;

static int snmpInited = FALSE;
netsnmp_oid_stash_node *stashRoot = NULL;

static void snmpInit(void)
{
    printf( "init_snmp: ...\n" );
    init_snmp("epicsSnmp");

    /* add_mibdir("/usr/share/snmp/mibs"); */	/* Does not need */
#if 0
    printf( "reading mibs: ...\n" );
    read_all_mibs();
#endif

    init_mib();

    ellInit(&snmpAgentList);
    snmpAgentListLock = epicsMutexMustCreate();

    ellInit(&snmpWalkList);
    snmpWalkListLock = epicsMutexMustCreate();

    stashRoot = netsnmp_oid_stash_create_node();

    SOCK_STARTUP;
    snmpInited = TRUE;
}

static int Snmp2cDoWalk(SNMP_WALK *pSnmpWalk)
{
    netsnmp_pdu *pdu;
    netsnmp_pdu *response;
    netsnmp_variable_list *vars;
    int status, todo = pSnmpWalk->count;
    oid *last = NULL;
    int  lastlen = 0;

    pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
    pdu->non_repeaters = 0;
    pdu->max_repetitions = todo;
    snmp_add_null_var(pdu, pSnmpWalk->objectId.requestOid, pSnmpWalk->objectId.requestOidLen);

    while (todo) {
        status = snmp_sess_synch_response(pSnmpWalk->agent->pSess, pdu, &response);
        if (status != STAT_SUCCESS || !response || response->errstat != SNMP_ERR_NOERROR) {
            if(SNMP_DRV_DEBUG >= 1)
                snmp_perror("Snmp2cWalk: Can't walk?");
            if (response)
                snmp_free_pdu(response);
            return 0;
        }
        for (vars = response->variables; vars; vars = vars->next_variable) {
            SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, vars->name, vars->name_length);
            int cvtSts;

            if (!data) {
                /* We've never seen this before.  Allocate and initialize a new node and add it to the tree. */
                data = callocMustSucceed(1, sizeof(SNMP_WALK_OID), "calloc buffer for SNMP_WALK_OID");
                scanIoInit(&data->iopvt);
                cvtSts = snprint_value(data->pBuf, sizeof(data->pBuf), vars->name, vars->name_length, vars);
                if(cvtSts <= 0)
                    {
                        errlogPrintf("Conversion failed during walk of %s\n", pSnmpWalk->objectId.requestName);
                        data->errCode = SNMP_REQUEST_CVT_ERR;	/* Conversion failure */
                    }
                else
                    data->errCode = SNMP_REQUEST_NO_ERR;
                if (SNMPERR_SUCCESS != netsnmp_oid_stash_add_data(&stashRoot, vars->name, vars->name_length, data)) {
                    printf("Snmp2cWalk: Cannot create node?!?\n");
                }
            } else {
                /* Not our first time at the rodeo.  Print it out, and if successful, scan the record(s). */
                cvtSts = snprint_value(data->pBuf, sizeof(data->pBuf), vars->name, vars->name_length, vars);
                if(cvtSts <= 0)
                    {
                        errlogPrintf("Snmp2cWalk: Conversion failed during walk of %s\n", pSnmpWalk->objectId.requestName);
                        data->errCode = SNMP_REQUEST_CVT_ERR;	/* Conversion failure */
                    }
                else {
                    data->errCode = SNMP_REQUEST_NO_ERR;
                    scanIoRequest(data->iopvt);
                }
            }

            last = vars->name;
            lastlen = vars->name_length;
            todo--;
        }

        if (todo) {
            pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
            pdu->non_repeaters = 0;
            pdu->max_repetitions = todo;
            snmp_add_null_var(pdu, last, lastlen);
        }
        snmp_free_pdu(response);
    }
    return 1;
}

/* Configure an SNMP walk! */
void Snmp2cWalk(char *host, char *community, char *oidname, int count, double delay)
{
    SNMP_WALK *pSnmpWalk;
    int i;

    if (!snmpInited)
        snmpInit();
    pSnmpWalk = callocMustSucceed(1, sizeof(SNMP_WALK), "calloc buffer for SNMP_WALK");
    pSnmpWalk->agent = snmpGetAgent(host, community, SNMP_VERSION_2c);
    pSnmpWalk->count = count;
    pSnmpWalk->delay.tv_sec = (int)delay;
    pSnmpWalk->delay.tv_usec = (delay - (int)delay) * 1000000;
    pSnmpWalk->objectId.requestOidLen = MAX_OID_LEN;

    if (!get_node(oidname, pSnmpWalk->objectId.requestOid, &pSnmpWalk->objectId.requestOidLen)) {
        /* Search traditional MIB2 and find nothing */
        if (!read_objid(oidname, pSnmpWalk->objectId.requestOid, &pSnmpWalk->objectId.requestOidLen)) {
            snmp_perror("Snmp2cWalk: Parsing objectId");
            printf("oidname = %s\n", oidname);
            free(pSnmpWalk);
            return;
        }
    }
    pSnmpWalk->objectId.requestName = epicsStrDup(oidname);

    /* Do the walk immediately! */
    i = 10;
    while (!Snmp2cDoWalk(pSnmpWalk) && --i > 0) {
        printf("WALK %s retry!\n", pSnmpWalk->objectId.requestName);
    }
    if (!i) {
        printf("Giving up on WALK %s!\n", pSnmpWalk->objectId.requestName);
        free(pSnmpWalk->objectId.requestName);
        free(pSnmpWalk);
        return;
    }

    /* Schedule the next walk. */
    gettimeofday(&pSnmpWalk->nextWalk, NULL);
    pSnmpWalk->nextWalk.tv_sec += pSnmpWalk->delay.tv_sec;
    pSnmpWalk->nextWalk.tv_usec += pSnmpWalk->delay.tv_usec;
    if (pSnmpWalk->nextWalk.tv_usec > 1000000) {
        pSnmpWalk->nextWalk.tv_sec++;
        pSnmpWalk->nextWalk.tv_usec -= 1000000;
    }

    /* Add it to the list! */
    epicsMutexLock(snmpWalkListLock);
    ellAdd( &snmpWalkList, (ELLNODE *)pSnmpWalk);
    epicsMutexUnlock(snmpWalkListLock);
}

/*********************************************************************/
static int Snmp_Operation(SNMP_AGENT * pSnmpAgent)
{
    int     msgQstatus, status;
    int     loop;

    int     NofReqs;	/* Num of queries we will process in one msg */
    SNMP_REQUEST  *pRequest;
    ELLLIST requestQryList;
    ELLLIST requestCmdList;

    struct variable_list *pVar;
    int varIndex;
    char pBuf[1024];
    char pErrBuf[1024];
    struct timeval now;
    SNMP_WALK *pSnmpWalk, *pSnmpNextWalk;

    if(pSnmpAgent == NULL)
        {
            errlogPrintf("Operation thread quit because no legal pSnmpAgent!\n");
            return -1;
        }

    while(TRUE)
        {
            gettimeofday(&now, NULL);
            epicsMutexLock(snmpWalkListLock);
            /* Find the earliest scheduled walk. */
            for( pSnmpNextWalk = pSnmpWalk = (SNMP_WALK *)ellFirst(&snmpWalkList); 
                 pSnmpWalk;
                 pSnmpWalk = (SNMP_WALK *)ellNext((ELLNODE *)pSnmpWalk) ) {
                if (pSnmpWalk->nextWalk.tv_sec < pSnmpNextWalk->nextWalk.tv_sec || 
                    (pSnmpWalk->nextWalk.tv_sec < pSnmpNextWalk->nextWalk.tv_sec &&
                     pSnmpWalk->nextWalk.tv_usec < pSnmpNextWalk->nextWalk.tv_usec))
                    pSnmpNextWalk = pSnmpWalk;
            }
            pSnmpWalk = pSnmpNextWalk;
            epicsMutexUnlock(snmpWalkListLock);

            if (!pSnmpWalk) {
                /* We don't have any walks, but let's give this a timeout anyway, because we might get some shortly. */
                msgQstatus = epicsMessageQueueReceiveWithTimeout(pSnmpAgent->msgQ_id, &pRequest, sizeof(SNMP_REQUEST *), 1.0);
                if(msgQstatus < 0)
                    continue;
            } else if (now.tv_sec < pSnmpWalk->nextWalk.tv_sec || 
                (now.tv_sec == pSnmpWalk->nextWalk.tv_sec &&
                 now.tv_usec < pSnmpWalk->nextWalk.tv_usec)) {
                /* The walk is still in the future! */
                double timeout = ((pSnmpWalk->nextWalk.tv_sec - now.tv_sec) + 
                                  (pSnmpWalk->nextWalk.tv_usec - now.tv_usec) / 1000000.);
                msgQstatus = epicsMessageQueueReceiveWithTimeout(pSnmpAgent->msgQ_id, &pRequest, sizeof(SNMP_REQUEST *), timeout);
                if (msgQstatus < 0)
                    continue;
            } else {
                /* We need to do the walk now! */
                Snmp2cDoWalk(pSnmpWalk);

                pSnmpWalk->nextWalk.tv_sec = now.tv_sec + pSnmpWalk->delay.tv_sec;
                pSnmpWalk->nextWalk.tv_usec = now.tv_usec + pSnmpWalk->delay.tv_usec;
                if (pSnmpWalk->nextWalk.tv_usec > 1000000) {
                    pSnmpWalk->nextWalk.tv_sec++;
                    pSnmpWalk->nextWalk.tv_usec -= 1000000;
                }

                /* Try to fit other requests in here! */
                msgQstatus = epicsMessageQueueTryReceive(pSnmpAgent->msgQ_id, &pRequest, sizeof(SNMP_REQUEST *) );
                if (msgQstatus < 0)
                    continue;
            }

            /* Note that pRequest now holds our next SNMP_REQUEST */

            /* Figure out how many requests in queue */
            NofReqs = epicsMessageQueuePending(pSnmpAgent->msgQ_id);

            /* Deal up to max number of variables per request, don't forget we already got one request */
            /* Since we will do both query and commmand, NofReqs + 1 is the total of both */
            NofReqs = MIN( (NofReqs + 1), (MAX(1, snmpMaxVarsPerMsg)) ) - 1;
 
            /* some requests come in */
            if(SNMP_DRV_DEBUG >= 3) printf("Snmp_Operation loop for agent[%s] processing %d requests\n", pSnmpAgent->pActiveSession->peername, NofReqs + 1 );

            if ( requestQryList.node.next || requestQryList.node.previous || requestQryList.count )
                errlogPrintf( "Found %d stale requestQry nodes.?\n", requestQryList.count );
            if ( requestCmdList.node.next || requestCmdList.node.previous || requestCmdList.count )
                errlogPrintf( "Found %d stale requestCmd nodes.\n", requestCmdList.count );
            /* We just re-init link list instead of delete, because there is no resource issue */
            ellInit( &requestQryList );
            ellInit( &requestCmdList );

            /* Create an SNMP Protocol Data Unit (PDU) for this request */
            pSnmpAgent->reqQryPdu = snmp_pdu_create(SNMP_MSG_GET);
            pSnmpAgent->reqCmdPdu = snmp_pdu_create(SNMP_MSG_SET);

            for(loop=0; loop<=NofReqs; loop++)
		{
                    /*
                     * Read out requests.
                     * We loop one more time, because we already read out one pRequest
                     */
                    if(loop != 0)
                        epicsMessageQueueReceive(pSnmpAgent->msgQ_id, &pRequest, sizeof(SNMP_REQUEST *));

                    if(pRequest->cmd != 0 && pSnmpAgent->reqCmdPdu != NULL)
			{/* Command request */

                            /* Add each request into the pdu */
                            if ( snmp_add_var(	pSnmpAgent->reqCmdPdu,
                                                pRequest->objectId.requestOid,
                                                pRequest->objectId.requestOidLen,
                                                pRequest->type, pRequest->pValStr ) )
				{/* failed to add var */
                                    pRequest->errCode = SNMP_REQUEST_ADDVAR_ERR;
                                    pRequest->opDone = 1;
                                    if(pRequest->pRecord)
					{
                                            dbScanLock(pRequest->pRecord);
                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                            dbScanUnlock(pRequest->pRecord);
					}
                                    snmp_perror(pRequest->objectId.requestName);
                                    errlogPrintf("snmp_add_var %s, type %c, value %s\n", pRequest->objectId.requestName, pRequest->type, pRequest->pValStr);
				}
                            else
				{/* succeed to add var */
                                    pRequest->errCode = SNMP_REQUEST_NO_ERR;  /* clean up err_code before we execute it */
                                    pRequest->opDone = 0;   /* We didn't start yet, of cause not done */
                                    /* place each request into the link list */
                                    ellAdd( (ELLLIST *) &requestCmdList, (ELLNODE *) pRequest );
				}
			}
                    else if(pRequest->cmd == 0 && pSnmpAgent->reqQryPdu != NULL)
			{/* Query request */
                            /* Add each request into the pdu */
                            if ( NULL == snmp_add_null_var(	pSnmpAgent->reqQryPdu,
                                                                pRequest->objectId.requestOid,
                                                                pRequest->objectId.requestOidLen ) )
				{/* failed to add var */
                                    pRequest->errCode = SNMP_REQUEST_ADDVAR_ERR;
                                    pRequest->opDone = 1;
                                    if(pRequest->pRecord)
					{
                                            dbScanLock(pRequest->pRecord);
                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                            dbScanUnlock(pRequest->pRecord);
					}
                                    snmp_perror(pRequest->objectId.requestName);
                                    errlogPrintf("snmp_add_null_var %s\n", pRequest->objectId.requestName);
				}
                            else
				{/* succeed to add var */
                                    pRequest->errCode = SNMP_REQUEST_NO_ERR;  /* clean up err_code before we execute it */
                                    pRequest->opDone = 0;   /* We didn't start yet, of cause not done */
                                    /* place each request into the link list */
                                    ellAdd( (ELLLIST *) &requestQryList, (ELLNODE *) pRequest );
				}
			}
                    else
			{/* Failed to create request PDU */
                            pRequest->errCode = SNMP_REQUEST_PDU_ERR;
                            pRequest->opDone = 1;
                            if(pRequest->pRecord)
				{
                                    dbScanLock(pRequest->pRecord);
                                    (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                    dbScanUnlock(pRequest->pRecord);
				}
			}
		}

            /*
             * First handle all requests in the command list
             */
            if ( requestCmdList.count == 0 )
		{
                    /* No commands this time */
                    if ( pSnmpAgent->reqCmdPdu != NULL )
			{
                            snmp_free_pdu( pSnmpAgent->reqCmdPdu );
                            pSnmpAgent->reqCmdPdu = NULL;
                            if(SNMP_DRV_DEBUG >= 5) printf("Snmp_Operation loop for agent[%s] freeing unused Cmd PDU.\n", pSnmpAgent->pActiveSession->peername);
			}
		}
            else
		{
                    /* Send out all commands */
                    struct snmp_pdu * respCmdPdu	= NULL;

                    /* send command to the snmp agent */
                    status = snmp_sess_synch_response(pSnmpAgent->pSess, pSnmpAgent->reqCmdPdu, &respCmdPdu);
                    switch(status)
			{
                        case STAT_SUCCESS:
                            pVar = respCmdPdu->variables;
                            if (respCmdPdu->errstat == SNMP_ERR_NOERROR)
                                {/* Successfully got response, and no error in the response */
                                    while (pVar != NULL)
                                        {
                                            for (	pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList);
                                                        pRequest;
                                                        pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ) )
                                                {	/* Looking for request with matched Oid */
                                                    if ( !memcmp(	pVar->name,	pRequest->objectId.requestOid,
                                                                        pRequest->objectId.requestOidLen * sizeof(oid) ) )
                                                        break;
                                                }

                                            if(pRequest)
                                                {
                                                    /*
                                                     *	Found the matched request for this cmd
                                                     *	snprint_value converts the value into a text string.
                                                     *	If the buffer is not big enough, it might return -1
                                                     *	and only put data type in.
                                                     *	So we use a large temp buffer here.
                                                     */
                                                    int cvtSts;
                                                    cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                                                    if(cvtSts <= 0)
                                                        {
                                                            errlogPrintf("Conversion failed for record [%s]\n", pRequest->pRecord->name);
                                                            pRequest->errCode = SNMP_REQUEST_CVT_ERR;	/* Conversion failure */
                                                        }
                                                    else
                                                        {
                                                            strncpy(pRequest->pValStr, pBuf, pRequest->valStrLen - 1);
                                                            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
                                                            pRequest->errCode = SNMP_REQUEST_NO_ERR;
                                                        }
                                                    pRequest->opDone = 1;

                                                    /* process cmd record */
                                                    if(pRequest->pRecord)
                                                        {
                                                            if(SNMP_DRV_DEBUG >= 4) printf("Got response for record [%s]=[%s]\n", pRequest->pRecord->name, pRequest->pValStr);
                                                            dbScanLock(pRequest->pRecord);
                                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                                            dbScanUnlock(pRequest->pRecord);
                                                            if(SNMP_DRV_DEBUG >= 5) printf("Record [%s] processed\n", pRequest->pRecord->name);
                                                        }

                                                    /* Remove the request from link list */
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexLock(pSnmpAgent->mutexLock);				*/
                                                    ellDelete(&requestCmdList, (ELLNODE *) pRequest);
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexUnlock(pSnmpAgent->mutexLock);				*/
                                                }
                                            else
                                                {
                                                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                                    errlogPrintf("Get response for %s, but no matched request!\n", pBuf);
                                                }

                                            pVar = pVar->next_variable;
                                        }
                                }
                            else
                                {/* Successfully got response, and with error in the response, error is defined by errstat and errindex */
                                    for (varIndex = 1; pVar && varIndex != respCmdPdu->errindex; pVar = pVar->next_variable, varIndex++)
                                        {
                                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                                {/* Looking for request with matched Oid */
                                                    if(!memcmp(pVar->name, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen * sizeof(oid))) break;
                                                }

                                            if(pRequest)
                                                {/* Found the matched request */
                                                    /* snprint_value converts the value into a text string. If the buffer is not big enough, it might return -1 and only  */
                                                    /* put data type in. So we use a large temp buffer here.                                                              */
                                                    int cvtSts;
                                                    cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                                                    if(cvtSts <= 0)
                                                        {
                                                            errlogPrintf("Conversion failed for record [%s]\n", pRequest->pRecord->name);
                                                            pRequest->errCode = SNMP_REQUEST_CVT_ERR;	/* Conversion failure */
                                                        }
                                                    else
                                                        {
                                                            strncpy(pRequest->pValStr, pBuf, pRequest->valStrLen - 1);
                                                            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
                                                            pRequest->errCode = SNMP_REQUEST_NO_ERR;
                                                        }
                                                    pRequest->opDone = 1;

                                                    /* process record */
                                                    if(pRequest->pRecord)
                                                        {
                                                            dbScanLock(pRequest->pRecord);
                                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                                            dbScanUnlock(pRequest->pRecord);
                                                        }

                                                    /* Remove the request from link list */
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexLock(pSnmpAgent->mutexLock);				*/
                                                    ellDelete(&requestCmdList, (ELLNODE *) pRequest);
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexUnlock(pSnmpAgent->mutexLock);				*/
                                                }
                                            else
                                                {
                                                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                                    errlogPrintf("Get response for %s, but no matched request!\n", pBuf);
                                                }
                                        }

                                    if(pVar)
                                        snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                    else
                                        strcpy(pBuf, "(none)");

                                    sprintf(pErrBuf,
                                            "\nrequestCmdList: snmp_sess_synch_response: errindex %ld\n"
                                            "\tdevName: %s\n"
                                            "\treqName: %s\n"
                                            "\tbuffer:  %s\n",
                                            respCmdPdu->errindex,
                                            pSnmpAgent->pActiveSession->peername, pRequest->objectId.requestName, pBuf );
                                    errlogPrintf("%s", pErrBuf);
                                    snmp_sess_perror(pErrBuf, pSnmpAgent->pActiveSession);
                                }

                            /* If still some queries in link list and can't find variables to match */
                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                {	/* Go thru the request list to process all records */

                                    pRequest->errCode = SNMP_REQUEST_CMD_NOANS; /* snmp request no answer in response */
                                    pRequest->opDone = 1;

                                    /* process record */
                                    if(pRequest->pRecord)
                                        {
                                            dbScanLock(pRequest->pRecord);
                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                            dbScanUnlock(pRequest->pRecord);
                                        }
                                    errlogPrintf( "Request %s not handled!\n", pRequest->pRecord->name);
                                }
                            break;

                        case STAT_TIMEOUT:
                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                {/* Go thru the request list to process all records */

                                    pRequest->errCode = SNMP_REQUEST_CMD_TOUT; /* snmp command timeout */
                                    pRequest->opDone = 1;

                                    /* process record */
                                    if(pRequest->pRecord)
                                        {
                                            dbScanLock(pRequest->pRecord);
                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                            dbScanUnlock(pRequest->pRecord);
                                            errlogPrintf(	"%s: Snmp CmdList Timeout on %s\n",
                                                                pRequest->pSnmpAgent->pActiveSession->peername,
                                                                pRequest->pRecord->name	);
                                        }
                                }

                            /* errlogPrintf("%s: Snmp CmdList Timeout\n", pSnmpAgent->pActiveSession->peername); */
                            break;
                        case STAT_ERROR:
                        default:
                            {
                                int liberr, syserr;
                                char * errstr;
                                snmp_sess_perror(pSnmpAgent->pActiveSession->peername, pSnmpAgent->pActiveSession);
                                snmp_sess_error(pSnmpAgent->pSess, &liberr, &syserr, &errstr);

                                for(	pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList);
                                        pRequest != NULL;
                                        pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                    {/* Go thru the request list to process all records */

                                        pRequest->errCode = SNMP_REQUEST_SNMP_ERR |(liberr?liberr:syserr); /* snmp error */
                                        pRequest->opDone = 1;

                                        /* process record */
                                        if(pRequest->pRecord)
                                            {
                                                dbScanLock(pRequest->pRecord);
                                                (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                                dbScanUnlock(pRequest->pRecord);
                                            }
                                    }

                                errlogPrintf("CmdList %s error %s!\n", pSnmpAgent->pActiveSession->peername, errstr);
                                free(errstr);
                            }
                            break;
			}

                    /* Free the response PDU for this request. */
                    /*If error happened before, the reqCmdPdu is already freed by snmp_sess_synch_response, the respCmdPdu is NULL */
                    /* If no error happened, respCmdPdu is cloned from reqCmdPdu and reqCmdPdu is freed */
                    if(respCmdPdu)
			{
                            snmp_free_pdu(respCmdPdu);
                            respCmdPdu = NULL;
                            if(SNMP_DRV_DEBUG >= 5) printf("Snmp_Operation loop for agent[%s] freeing PDU.\n", pSnmpAgent->pActiveSession->peername);
			}

		}/* Command PDU */

            /*
             * Next handle all requests in the query list
             */
            if ( requestQryList.count == 0 )
		{
                    /* No commands this time */
                    if ( pSnmpAgent->reqQryPdu != NULL )
			{
                            snmp_free_pdu( pSnmpAgent->reqQryPdu );
                            pSnmpAgent->reqQryPdu = NULL;
                            if(SNMP_DRV_DEBUG >= 5) printf("Snmp_Operation loop for agent[%s] freeing unused Qry PDU.\n", pSnmpAgent->pActiveSession->peername);
			}
		}
            else
		{
                    /* Read out and re-organize all requests */
                    struct snmp_pdu * respQryPdu	= NULL;

                    /* request the snmp agent */
                    status = snmp_sess_synch_response(pSnmpAgent->pSess, pSnmpAgent->reqQryPdu, &respQryPdu);
                    switch(status)
			{
                        case STAT_SUCCESS:
                            pVar = respQryPdu->variables;
                            if (respQryPdu->errstat == SNMP_ERR_NOERROR)
                                {/* Successfully got response, and no error in the response */
                                    while (pVar != NULL)
                                        {
                                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestQryList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                                {/* Looking for request with matched Oid */
                                                    if(!memcmp(pVar->name, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen * sizeof(oid))) break;
                                                }

                                            if(pRequest)
                                                {/* Found the matched query request */
                                                    /* snprint_value converts the value into a text string. If the buffer is not big enough, it might return -1 and only  */
                                                    /* put data type in. So we use a large temp buffer here.                                                              */
                                                    int cvtSts;
                                                    cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                                                    if(cvtSts <= 0)
                                                        {
                                                            errlogPrintf("Conversion failed for record [%s]\n", pRequest->pRecord->name);
                                                            pRequest->errCode = SNMP_REQUEST_CVT_ERR;	/* Conversion failure */
                                                        }
                                                    else
                                                        {
                                                            strncpy(pRequest->pValStr, pBuf, pRequest->valStrLen - 1);
                                                            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
                                                            pRequest->errCode = SNMP_REQUEST_NO_ERR;
                                                        }
                                                    pRequest->opDone = 1;

                                                    /* process query record */
                                                    if(pRequest->pRecord)
                                                        {
                                                            if(SNMP_DRV_DEBUG >= 4) printf("Got response for record [%s]=[%s]\n", pRequest->pRecord->name, pRequest->pValStr);
                                                            dbScanLock(pRequest->pRecord);
                                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                                            dbScanUnlock(pRequest->pRecord);
                                                            if(SNMP_DRV_DEBUG >= 5) printf("Record [%s] processed\n", pRequest->pRecord->name);
                                                        }

                                                    /* Remove the request from link list */
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexLock(pSnmpAgent->mutexLock);				*/
                                                    ellDelete(&requestQryList, (ELLNODE *) pRequest);
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexUnlock(pSnmpAgent->mutexLock);				*/
                                                }
                                            else
                                                {
                                                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                                    errlogPrintf("Get response for %s, but no matched request!\n", pBuf);
                                                }

                                            pVar = pVar->next_variable;
                                        }
                                }
                            else
                                {/* Successfully got response, and with error in the response, error is defined by errstat and errindex */
                                    for (varIndex = 1; pVar && varIndex != respQryPdu->errindex; pVar = pVar->next_variable, varIndex++)
                                        {
                                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestQryList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                                {/* Looking for request with matched Oid */
                                                    if(!memcmp(pVar->name, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen * sizeof(oid))) break;
                                                }

                                            if(pRequest)
                                                {/* Found the matched request */
                                                    /* snprint_value converts the value into a text string. If the buffer is not big enough, it might return -1 and only  */
                                                    /* put data type in. So we use a large temp buffer here.                                                              */
                                                    int cvtSts;
                                                    cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                                                    if(cvtSts <= 0)
                                                        {
                                                            errlogPrintf("Conversion failed for record [%s]\n", pRequest->pRecord->name);
                                                            pRequest->errCode = SNMP_REQUEST_CVT_ERR;	/* Conversion failure */
                                                        }
                                                    else
                                                        {
                                                            strncpy(pRequest->pValStr, pBuf, pRequest->valStrLen - 1);
                                                            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
                                                            pRequest->errCode = SNMP_REQUEST_NO_ERR;
                                                        }
                                                    pRequest->opDone = 1;

                                                    /* process record */
                                                    if(pRequest->pRecord)
                                                        {
                                                            dbScanLock(pRequest->pRecord);
                                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                                            dbScanUnlock(pRequest->pRecord);
                                                        }

                                                    /* Remove the request from link list */
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexLock(pSnmpAgent->mutexLock);				*/
                                                    ellDelete(&requestQryList, (ELLNODE *) pRequest);
                                                    /* This mutex is only needed to protect snmpReqPtrList	*/
                                                    /* epicsMutexUnlock(pSnmpAgent->mutexLock);				*/
                                                }
                                            else
                                                {
                                                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                                    errlogPrintf("Get response for %s, but no matched request!\n", pBuf);
                                                }
                                        }

                                    if(pVar)
                                        snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                                    else
                                        strcpy(pBuf, "(none)");

                                    sprintf(pErrBuf, "\nrequestQryList: snmp_sess_synch_response: errindex %ld\n\t%s %s %s\n",
                                            respQryPdu->errindex, pSnmpAgent->pActiveSession->peername, pRequest->objectId.requestName, pBuf);
                                    errlogPrintf("%s", pErrBuf);
                                    snmp_sess_perror(pErrBuf, pSnmpAgent->pActiveSession);
                                }

                            /* If still some queries in link list and can't find variables to match */
                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestQryList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                {/* Go thru the request list to process all records */

                                    pRequest->errCode = SNMP_REQUEST_QRY_NOANS; /* snmp request no answer in response */
                                    pRequest->opDone = 1;

                                    /* process record */
                                    if(pRequest->pRecord)
                                        {
                                            dbScanLock(pRequest->pRecord);
                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                            dbScanUnlock(pRequest->pRecord);
                                        }
                                    errlogPrintf("%s: Response doesn't have a match!\n", pRequest->pRecord->name);
                                }

                            break;
                        case STAT_TIMEOUT:
                            for(pRequest = (SNMP_REQUEST *)ellFirst(&requestQryList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                {/* Go thru the request list to process all records */

                                    pRequest->errCode = SNMP_REQUEST_QRY_TOUT; /* snmp request timeout */
                                    pRequest->opDone = 1;

                                    /* process record */
                                    if(pRequest->pRecord)
                                        {
                                            dbScanLock(pRequest->pRecord);
                                            (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                            dbScanUnlock(pRequest->pRecord);
                                            errlogPrintf(	"%s: Snmp QryList Timeout on %s\n",
                                                                pRequest->pSnmpAgent->pActiveSession->peername,
                                                                pRequest->pRecord->name	);
                                        }
                                }

                            /* errlogPrintf("%s: Snmp Query Timeout\n", pSnmpAgent->pActiveSession->peername); */
                            break;
                        case STAT_ERROR:
                        default:
                            {
                                int liberr, syserr;
                                char * errstr;
                                snmp_sess_perror(pSnmpAgent->pActiveSession->peername, pSnmpAgent->pActiveSession);
                                snmp_sess_error(pSnmpAgent->pSess, &liberr, &syserr, &errstr);

                                for(pRequest = (SNMP_REQUEST *)ellFirst(&requestQryList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                                    {/* Go thru the request list to process all records */

                                        pRequest->errCode = SNMP_REQUEST_SNMP_ERR |(liberr?liberr:syserr); /* snmp error */
                                        pRequest->opDone = 1;

                                        /* process record */
                                        if(pRequest->pRecord)
                                            {
                                                dbScanLock(pRequest->pRecord);
                                                (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                                dbScanUnlock(pRequest->pRecord);
                                            }
                                    }

                                errlogPrintf("Query %s error %s!\n", pSnmpAgent->pActiveSession->peername, errstr);
                                free(errstr);
                            }
                            break;
			}

                    /* Free the response PDU for this request. */
                    /*If error happened before, the reqQryPdu is already freed by snmp_sess_synch_response, the respQryPdu is NULL */
                    /* If no error happened, respQryPdu is cloned from reqQryPdu and reqQryPdu is freed */
                    if(respQryPdu)
			{
                            snmp_free_pdu(respQryPdu);
                            respQryPdu = NULL;
                            if(SNMP_DRV_DEBUG >= 5) printf("Snmp_Operation loop for agent[%s] freeing PDU.\n", pSnmpAgent->pActiveSession->peername);
			}

		}	/* Query PDU */
            /* process requests */

        }	/* infinite loop */

    snmp_sess_close(pSnmpAgent->pSess);
    /* We should never get here */
    return 0;
}

static SNMP_AGENT *snmpGetAgent(char *peerName, char *community, long snmpVersion)
{
    SNMP_AGENT		*	pSnmpAgent;
    int liberr, syserr;
    char * errstr;

    /* Check if the agent is already in our list, or else add it */
    epicsMutexLock(snmpAgentListLock); 
    for( pSnmpAgent = (SNMP_AGENT *)ellFirst(&snmpAgentList); pSnmpAgent; pSnmpAgent = (SNMP_AGENT *)ellNext((ELLNODE *)pSnmpAgent) )
   {
        if(!strcmp(pSnmpAgent->snmpSession.peername, peerName))
        {
            if(!strcmp((char *)(pSnmpAgent->snmpSession.community), community))
            {
                if(pSnmpAgent->snmpSession.version == snmpVersion) break;
            }
        }
    }

    if(!pSnmpAgent)
    {/* Did not find any existing matching agent, create one */
    	if(SNMP_DRV_DEBUG >= 1) printf( "\nsnmpRequestInit: Creating Agent[%s], Community[%s]\n", peerName, community );
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
            epicsMutexUnlock(snmpAgentListLock);
            epicsThreadSuspendSelf();
            return NULL;
        }

        /* In case we need to change the content of session */
        pSnmpAgent->pActiveSession = snmp_sess_session(pSnmpAgent->pSess);
        if(SNMP_DRV_DEBUG >= 2) 
        {
            char * ver_msg[5]={"Ver1", "Ver2c", "Ver2u", "V3", "Unknown"};
            printf("Active SNMP agent: %s,", pSnmpAgent->pActiveSession->peername);
        /*	printf("\tcommunity: %s, version: %s\n", pSnmpAgent->pActiveSession->community, ver_msg[MIN(4,pSnmpAgent->pActiveSession->version)]); */
        	printf("\tcommunity: %s, version: %s\n", pSnmpAgent->snmpSession.community, ver_msg[MIN(4,pSnmpAgent->pActiveSession->version)]);
        }

        ellInit(&(pSnmpAgent->snmpReqPtrList));

		/* Does this pre-alloc all SNMP_REQUEST objects? */
        pSnmpAgent->msgQ_id = epicsMessageQueueCreate(OPTHREAD_MSGQ_CAPACITY, sizeof(SNMP_REQUEST *));
        if(pSnmpAgent->msgQ_id == NULL)
        {
            errlogPrintf("Fail to create message queue for agent %s!\n", peerName);
            epicsMutexUnlock(snmpAgentListLock);
            epicsThreadSuspendSelf();
            return NULL;
        }

        /* Create the operation thread */
        bzero(pSnmpAgent->opthread_name,MAX_CA_STRING_SIZE);
        strncpy(pSnmpAgent->opthread_name, peerName, MAX_CA_STRING_SIZE-1);
        pSnmpAgent->opthread_name[MAX_CA_STRING_SIZE-1] = '\0';

        pSnmpAgent->opthread_id = epicsThreadCreate(pSnmpAgent->opthread_name, OPTHREAD_PRIORITY, OPTHREAD_STACK, (EPICSTHREADFUNC)Snmp_Operation, (void *)pSnmpAgent);
        if(pSnmpAgent->opthread_id == (epicsThreadId)(-1))
        {
            /* epicsMutexUnlock(pSnmpAgent->mutexLock); wasn't locked */
            errlogPrintf("Fail to create operation thread for snmp agent %s, Fatal!\n", peerName);
            epicsMutexUnlock(snmpAgentListLock);
            epicsThreadSuspendSelf();
            return NULL;
        }

        /* we will create PDU before each time we request the agent */
        pSnmpAgent->reqQryPdu = NULL;
        pSnmpAgent->reqCmdPdu = NULL;

        pSnmpAgent->status = 0;

        /* We successfully allocate all resource */
        ellAdd( &snmpAgentList, (ELLNODE *)pSnmpAgent);

        if(SNMP_DRV_DEBUG >= 2) printf("Adding snmp agent %s to snmpAgentList\n", peerName);
    }
    epicsMutexUnlock(snmpAgentListLock);
    return pSnmpAgent;
}

int snmpRequestInit(dbCommon * pRecord, const char * ioString, long snmpVersion, size_t valStrLen, int cmd, char type)
{

    SNMP_AGENT		*	pSnmpAgent;

    SNMP_REQINFO	*	pSnmpReqInfo;
    SNMP_REQPTR		*	pSnmpReqPtr;
    SNMP_REQUEST	*	pSnmpRequest;

    char peerName[81];
    char community[81];
    char oidStr[81];
	char			typeFromIO	= '?';
	unsigned int	lenFromIO	= 0;

    int n;

    if (!snmpInited)
    {
        snmpInit();
    }

    /* parameter check */
    if(!pRecord || !ioString) return -1;

    if(snmpVersion != SNMP_VERSION_1 && snmpVersion != SNMP_VERSION_3)
        snmpVersion = SNMP_VERSION_2c;

    n = sscanf(ioString, "%s %s %s %c %u", peerName, community, oidStr, &typeFromIO, &lenFromIO );
    if ( n < 3 )
	{
		errlogPrintf( "snmpRequestInit Error: Invalid INP/OUT string: %s\n", ioString );
		return -1;
	}
	if ( n == 4 && typeFromIO != '?' )
	{
		type	= typeFromIO;
	}
    if(SNMP_DRV_DEBUG >= 2) printf( "\nsnmpRequestInit: Agent[%s], Community[%s], OIDStr[%s], Type %c\n", peerName, community, oidStr, type );

    if (!(pSnmpAgent = snmpGetAgent(peerName, community, snmpVersion)))
        return -1;

    /* Query info prepare */
	if(SNMP_DRV_DEBUG >= 2) printf("Alloc pSnmpReqInfo for %s\n", pRecord->name );
    pSnmpReqInfo = (SNMP_REQINFO *)callocMustSucceed(1, sizeof(SNMP_REQINFO), "calloc SNMP_REQINFO");

    pSnmpReqPtr = &(pSnmpReqInfo->reqptr);
    pSnmpReqPtr->pRequest = &(pSnmpReqInfo->request);

    pSnmpRequest = &(pSnmpReqInfo->request);

    pSnmpRequest->cmd = cmd;
    pSnmpRequest->type = type;
    pSnmpRequest->valLength = lenFromIO;

    pSnmpRequest->pRecord = pRecord;
    pSnmpRequest->pSnmpAgent = pSnmpAgent;

    pSnmpRequest->objectId.requestName = epicsStrDup(oidStr);
    pSnmpRequest->objectId.requestOidLen = MAX_OID_LEN;

    if ( !get_node(	pSnmpRequest->objectId.requestName,
					pSnmpRequest->objectId.requestOid,
					&pSnmpRequest->objectId.requestOidLen ) )
    {/* Search traditional MIB2 and find nothing */
        if ( !read_objid(	pSnmpRequest->objectId.requestName,
							pSnmpRequest->objectId.requestOid,
							&pSnmpRequest->objectId.requestOidLen ) )
        {
            snmp_perror("Parsing objectId"); /* parsing error has nothing to do with session */
            errlogPrintf("Failed to parse the objectId %s.\n", oidStr);
            pRecord->dpvt = NULL;
            free(pSnmpRequest->objectId.requestName);
            free(pSnmpReqInfo);
            return -1;
        }
    }

    /* Set when we get response */
	if(SNMP_DRV_DEBUG >= 2) printf("Alloc pValStr for %s\n", pRecord->name );
    pSnmpRequest->valStrLen = MAX(valStrLen, MAX_CA_STRING_SIZE);
    pSnmpRequest->pValStr = (char *)callocMustSucceed(1, pSnmpRequest->valStrLen, "calloc pValStr");

    pSnmpRequest->opDone = 0;
    pSnmpRequest->errCode = SNMP_REQUEST_NO_ERR;
    
    epicsMutexLock(pSnmpAgent->mutexLock);
    ellAdd( &(pSnmpAgent->snmpReqPtrList), (ELLNODE *)pSnmpReqPtr );
    epicsMutexUnlock(pSnmpAgent->mutexLock);

    pRecord->dpvt = (void *)pSnmpRequest;

    return 0;
}

/*********************************************************************/
int snmpQuerySingleVar(SNMP_REQUEST * pRequest)
{
    int     status, rtn;

    struct snmp_pdu * reqQryPdu;
    struct snmp_pdu * respQryPdu	= NULL;

    struct variable_list *pVar;
    char pBuf[1024];
    char pErrBuf[1024];
    SNMP_WALK_OID *data = NULL;

    if(pRequest == NULL)
        {
            errlogPrintf("snmpQuerySingleVar failed because of no legal pRequest!\n");
            return -1;
        }

    data = netsnmp_oid_stash_get_data(stashRoot, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen);
    if (data && data->errCode == SNMP_REQUEST_NO_ERR) {
        /* We've already done this as part of a walk!  Just copy it and call it a day! */
        strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
        pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        return 0;
    }

    /* create a PDU for this request */
    reqQryPdu = snmp_pdu_create(SNMP_MSG_GET);
    if(!reqQryPdu)
        {
            errlogPrintf("snmp_pdu_create %s for record [%s] failed\n", pRequest->objectId.requestName, pRequest->pRecord->name);
            return -1;
        }

    /* Add the request into the pdu */
    if(NULL == snmp_add_null_var(reqQryPdu, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen))
        {/* failed to add var */
            snmp_free_pdu(reqQryPdu);
            snmp_perror(pRequest->objectId.requestName);
            errlogPrintf("snmp_add_null_var %s for record [%s] failed\n", pRequest->objectId.requestName, pRequest->pRecord->name);
            return -1;
        }

    /*
     * Request the snmp agent
     * The query pdu will be freed via snmp_sess_synch_repsonse()
     * for both error and success.
     * If a response pdu is created, a pointer to it will be returned in respQryPdu.
     * That response pdu must be freed before we return.
     */
    status = snmp_sess_synch_response(pRequest->pSnmpAgent->pSess, reqQryPdu, &respQryPdu);
    switch(status)
        {
        case STAT_SUCCESS:
            pVar = respQryPdu->variables;	/* Since we are querying single var, this must be it */
            if (respQryPdu->errstat == SNMP_ERR_NOERROR)
                {	/* Successfully got response, and no error in the response */

                    /* Check if the request has the matched Oid */
                    if(pVar && !memcmp(	pVar->name, pRequest->objectId.requestOid,
                                        pRequest->objectId.requestOidLen * sizeof(oid) ) )
                        {	/* resp matched request */
                            /*
                             * snprint_value converts the value into a text string.
                             * If the buffer is not big enough, it might return -1 and only put data type in.
                             * So we use a large temp buffer here.
                             */
                            int cvtSts;
                            cvtSts = snprint_value(pBuf, sizeof(pBuf), pVar->name, pVar->name_length, pVar);
                            if(cvtSts <= 0)
                                {
                                    errlogPrintf("Conversion failed for record [%s]\n", pRequest->pRecord->name);
                                    rtn = -1;
                                }
                            else
                                {
                                    strncpy(pRequest->pValStr, pBuf, pRequest->valStrLen - 1);
                                    pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
                                    rtn = 0;
                                }
                        }
                    else
                        {/* resp did not match request */
                            char	reqId[1024];
                            snprint_objid( pBuf,	sizeof(pBuf),	pVar->name, pVar->name_length );
                            snprint_objid( reqId,	sizeof(reqId),	pRequest->objectId.requestOid,
                                           pRequest->objectId.requestOidLen );
                            errlogPrintf("Get response for %s, but requested %s!\n", pBuf, reqId );
                            rtn = -1;
                        }
                }
            else
                {	/*
                         * Successfully got response, and with error in the response,
                         * error is defined by errstat and errindex
                         *
                         * Since we are querying single var,
                         * error means the query is no good
                         */
                    if(pVar)
                        snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                    else
                        strcpy(pBuf, "(none)");

                    sprintf( pErrBuf,
                             "\nsnmpQuerySingleVar: snmp_sess_synch_response: errindex %ld\n"
                             "\tdevName: %s\n"
                             "\treqName: %s\n"
                             "\tbuffer:  %s\n",
                             respQryPdu->errindex,
                             pRequest->pSnmpAgent->pActiveSession->peername,
                             pRequest->objectId.requestName, pBuf);
                    errlogPrintf("%s", pErrBuf);
                    snmp_sess_perror(pErrBuf, pRequest->pSnmpAgent->pActiveSession);
                    rtn = -1;
                }
            break;

        case STAT_TIMEOUT:
            errlogPrintf(	"%s: Snmp QuerySingleVar Timeout on %s\n",
                                pRequest->pSnmpAgent->pActiveSession->peername,
                                pRequest->pRecord->name	);
            rtn = -1;
            break;

        case STAT_ERROR:
        default:
            {
                int liberr, syserr;
                char * errstr;
                snmp_sess_perror(pRequest->pSnmpAgent->pActiveSession->peername, pRequest->pSnmpAgent->pActiveSession);
                snmp_sess_error(pRequest->pSnmpAgent->pSess, &liberr, &syserr, &errstr);

                errlogPrintf("Query %s error %s!\n", pRequest->pSnmpAgent->pActiveSession->peername, errstr);
                free(errstr);
                rtn = -1;
            }
            break;
        }

    /* Free the response PDU for this request. */
    /* If error happened before, the reqQryPdu is already freed by snmp_sess_synch_response, the respQryPdu is NULL */
    /* If no error happened, respQryPdu is cloned from reqQryPdu and reqQryPdu is freed */
    if(respQryPdu)
        snmp_free_pdu(respQryPdu);

    return rtn;
}

/**************************************************************************************************/
/* Here we supply the driver report function for epics                                            */
/**************************************************************************************************/
static  long    Snmp_EPICS_Init();
static  long    Snmp_EPICS_Report(int level);

const struct drvet drvSnmp = {2,                              /*2 Table Entries */
                             (DRVSUPFUN) Snmp_EPICS_Report,  /* Driver Report Routine */
                             (DRVSUPFUN) Snmp_EPICS_Init};   /* Driver Initialization Routine */

#if EPICS_VERSION>3 || (EPICS_VERSION==3 && EPICS_REVISION>=14)
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
        for (	pSnmpAgent = (SNMP_AGENT *)ellFirst(&snmpAgentList);
				pSnmpAgent;
				pSnmpAgent = (SNMP_AGENT *)ellNext((ELLNODE *)pSnmpAgent) )
        {
            printf("\tSNMP agent: %s\n", pSnmpAgent->snmpSession.peername);
            if(level > 1)
            {
                char * ver_msg[5]={"Ver1", "Ver2c", "Ver2u", "V3", "Unknown"};
                /*
				 * Due to snmp_sess_open and snmp_sess_session don't guarantee
				 * string terminator of community but use community_len,
				 * we use original snmpSession here
				 */
                printf(	"\tcommunity: %s, version: %s, %d variables\n\n",
						pSnmpAgent->snmpSession.community,
						ver_msg[MIN(4,pSnmpAgent->snmpSession.version)],
						pSnmpAgent->snmpReqPtrList.count );
            }
        }
    }

    return 0;
}
