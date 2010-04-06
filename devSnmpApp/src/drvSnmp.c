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
int snmpMaxVarsPerMsg = 30;	/* Max number of variables per request message */

/*********************************************************************/
static int Snmp_Operation(SNMP_AGENT * pSnmpAgent)
{
    int     msgQstatus, status;
    int     loop;

    int     NofReqs;	/* Num of queries we will process in one msg */
    SNMP_REQUEST  *pRequest;
    ELLLIST requestQryList;
    ELLLIST requestCmdList;

    struct snmp_pdu * respQryPdu;
    struct snmp_pdu * respCmdPdu;

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
        msgQstatus = epicsMessageQueueReceive(pSnmpAgent->msgQ_id, &pRequest, sizeof(SNMP_REQUEST *) );
        if(msgQstatus < 0)
        {/* we should never time out, so something wrong */
            errlogPrintf("Operation %s msgQ timeout!\n", pSnmpAgent->pActiveSession->peername);
            epicsThreadSleep(2.0);  /* Avoid super loop to starve CPU */
        }
        else
        {/* some requests come in */
            if(SNMP_DRV_DEBUG) printf("Oper task for agent[%s] gets requests!\n", pSnmpAgent->pActiveSession->peername);

            /* Figure out how many requests in queue */
            NofReqs = epicsMessageQueuePending(pSnmpAgent->msgQ_id);

            /* Deal up to max number of variables per request, don't forget we already got one request */
            /* Since we will do both query and commmand, NofReqs + 1 is the total of both */
            NofReqs = MIN( (NofReqs + 1), (MAX(1, snmpMaxVarsPerMsg)) ) - 1;

            /* We just re-init link list instead of delete, because there is no resource issue */
            ellInit( &requestQryList );
            ellInit( &requestCmdList );
            /* create a PDU for this request */
            pSnmpAgent->reqQryPdu = snmp_pdu_create(SNMP_MSG_GET);
            pSnmpAgent->reqCmdPdu = snmp_pdu_create(SNMP_MSG_SET);

            for(loop=0; loop<=NofReqs; loop++)
            {/* Read out requests. We loop one more time, because we already read out one pRequest */
                if(loop != 0) epicsMessageQueueReceive(pSnmpAgent->msgQ_id, &pRequest, sizeof(SNMP_REQUEST *));

                if(pRequest->cmd != 0 && pSnmpAgent->reqCmdPdu != NULL)
                {/* Command request */
                    /* Add each request into the pdu */
                    if(snmp_add_var(pSnmpAgent->reqCmdPdu, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen, pRequest->type, pRequest->pValStr))
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
                    if(NULL == snmp_add_null_var(pSnmpAgent->reqQryPdu, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen))
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

            if(requestCmdList.count)
            {/* Send out all commands */
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
                                        if(SNMP_DRV_DEBUG > 1) printf("Got response for record [%s]=[%s]\n", pRequest->pRecord->name, pRequest->pValStr);
                                        dbScanLock(pRequest->pRecord);
                                        (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                        dbScanUnlock(pRequest->pRecord);
                                        if(SNMP_DRV_DEBUG > 1) printf("Record [%s] processed\n", pRequest->pRecord->name);
                                    }

                                    /* Remove the request from link list */
                                    epicsMutexLock(pSnmpAgent->mutexLock); 
                                    ellDelete(&requestCmdList, (ELLNODE *) pRequest);
                                    epicsMutexUnlock(pSnmpAgent->mutexLock); 
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
                                    epicsMutexLock(pSnmpAgent->mutexLock);
                                    ellDelete(&requestCmdList, (ELLNODE *) pRequest);
                                    epicsMutexUnlock(pSnmpAgent->mutexLock);
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

                            sprintf(pErrBuf, "snmp_sess_synch_response: errindex %ld, %s %s %s\n",
                                respCmdPdu->errindex, pSnmpAgent->pActiveSession->peername, pRequest->objectId.requestName, pBuf);
                            errlogPrintf("%s", pErrBuf);
                            snmp_sess_perror(pErrBuf, pSnmpAgent->pActiveSession);
                        }

                        /* If still some queries in link list and can't find variables to match */
                        for(pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
                        {/* Go thru the request list to process all records */

                             pRequest->errCode = SNMP_REQUEST_CMD_NOANS; /* snmp request no answer in response */
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

                        for(pRequest = (SNMP_REQUEST *)ellFirst(&requestCmdList); pRequest; pRequest = (SNMP_REQUEST *)ellNext( (ELLNODE *) pRequest ))
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
                /*If error happened before, the reqCmdPdu is already freed by snmp_sess_synch_response, the respCmdPdu is NULL */
                /* If no error happened, respCmdPdu is cloned from reqCmdPdu and reqCmdPdu is freed */
                if(respCmdPdu)
                {
                    snmp_free_pdu(respCmdPdu);
                    respCmdPdu = NULL;
                    if(SNMP_DRV_DEBUG) printf("Oper task for agent[%s] frees PDU!\n", pSnmpAgent->pActiveSession->peername);
                }

            }/* Command PDU */

            if(requestQryList.count)
            {/* Read out and re-organize all requests */
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
                                        if(SNMP_DRV_DEBUG > 1) printf("Got response for record [%s]=[%s]\n", pRequest->pRecord->name, pRequest->pValStr);
                                        dbScanLock(pRequest->pRecord);
                                        (*(pRequest->pRecord->rset->process))(pRequest->pRecord);
                                        dbScanUnlock(pRequest->pRecord);
                                        if(SNMP_DRV_DEBUG > 1) printf("Record [%s] processed\n", pRequest->pRecord->name);
                                    }

                                    /* Remove the request from link list */
                                    epicsMutexLock(pSnmpAgent->mutexLock); 
                                    ellDelete(&requestQryList, (ELLNODE *) pRequest);
                                    epicsMutexUnlock(pSnmpAgent->mutexLock); 
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
                                    epicsMutexLock(pSnmpAgent->mutexLock);
                                    ellDelete(&requestQryList, (ELLNODE *) pRequest);
                                    epicsMutexUnlock(pSnmpAgent->mutexLock);
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

                            sprintf(pErrBuf, "snmp_sess_synch_response: errindex %ld, %s %s %s\n",
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
                    if(SNMP_DRV_DEBUG) printf("Oper task for agent[%s] frees PDU!\n", pSnmpAgent->pActiveSession->peername);
                }

            }/* Query PDU */
        }/* process requests */

    }/* infinite loop */

    snmp_sess_close(pSnmpAgent->pSess);
    /* We should never get here */
    return 0;
}

int snmpRequestInit(dbCommon * pRecord, const char * ioString, long snmpVersion, size_t valStrLen, int cmd, char type)
{
    static int snmpInited = FALSE;

    SNMP_AGENT   * pSnmpAgent;

    SNMP_REQINFO * pSnmpReqInfo;
    SNMP_REQPTR  * pSnmpReqPtr;
    SNMP_REQUEST   * pSnmpRequest;

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
        read_all_mibs();
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
            if(!strcmp((char *)(pSnmpAgent->snmpSession.community), community))
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

        ellInit(&(pSnmpAgent->snmpReqPtrList));

        pSnmpAgent->msgQ_id = epicsMessageQueueCreate(OPTHREAD_MSGQ_CAPACITY, sizeof(SNMP_REQUEST *));
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

        /* we will create PDU before each time we request the agent */
        pSnmpAgent->reqQryPdu = NULL;
        pSnmpAgent->reqCmdPdu = NULL;

        pSnmpAgent->status = 0;

        /* We successfully allocate all resource */
        ellAdd( &snmpAgentList, (ELLNODE *)pSnmpAgent);

        if(SNMP_DRV_DEBUG) printf("Add snmp agent %s\n", peerName);
    }
    epicsMutexUnlock(snmpAgentListLock);
    /* Check if the agent is already in our list, or else add it */

    /* Query info prepare */
    pSnmpReqInfo = (SNMP_REQINFO *)callocMustSucceed(1, sizeof(SNMP_REQINFO), "calloc SNMP_REQINFO");

    pSnmpReqPtr = &(pSnmpReqInfo->reqptr);
    pSnmpReqPtr->pRequest = &(pSnmpReqInfo->request);

    pSnmpRequest = &(pSnmpReqInfo->request);

    pSnmpRequest->cmd = cmd;
    pSnmpRequest->type = type;

    pSnmpRequest->pRecord = pRecord;
    pSnmpRequest->pSnmpAgent = pSnmpAgent;

    pSnmpRequest->objectId.requestName = epicsStrDup(oidStr);
    pSnmpRequest->objectId.requestOidLen = MAX_OID_LEN;

    if (!get_node(pSnmpRequest->objectId.requestName, pSnmpRequest->objectId.requestOid, &pSnmpRequest->objectId.requestOidLen))
    {/* Search traditional MIB2 and find nothing */
        if (!read_objid(pSnmpRequest->objectId.requestName, pSnmpRequest->objectId.requestOid, &pSnmpRequest->objectId.requestOidLen))
        {
            snmp_perror("Parsing objectId"); /* parsing error has nothing to do with session */
            errlogPrintf("Fail to parse the objectId %s.\n", oidStr);
            pRecord->dpvt = NULL;
            free(pSnmpRequest->objectId.requestName);
            free(pSnmpReqInfo);
            return -1;
        }
    }

    /* Set when we get response */
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
    struct snmp_pdu * respQryPdu;

    struct variable_list *pVar;
    char pBuf[1024];
    char pErrBuf[1024];

    if(pRequest == NULL)
    {
        errlogPrintf("snmpQuerySingleVar failed because of no legal pRequest!\n");
        return -1;
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

    /* request the snmp agent */
    status = snmp_sess_synch_response(pRequest->pSnmpAgent->pSess, reqQryPdu, &respQryPdu);
    switch(status)
    {
        case STAT_SUCCESS:
            pVar = respQryPdu->variables;	/* Since we are querying single var, this must be it */
            if (respQryPdu->errstat == SNMP_ERR_NOERROR)
            {/* Successfully got response, and no error in the response */

                /* Check if the request has the matched Oid */
                if(pVar && !memcmp(pVar->name, pRequest->objectId.requestOid, pRequest->objectId.requestOidLen * sizeof(oid)))
                {/* resp matched request */
                    /* snprint_value converts the value into a text string. If the buffer is not big enough, it might return -1 and only  */
                    /* put data type in. So we use a large temp buffer here.                                                              */
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
                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                    errlogPrintf("Get response for %s, but no matched request!\n", pBuf);
                    rtn = -1;
                }
            }
            else
            {/* Successfully got response, and with error in the response, error is defined by errstat and errindex */
             /* Since we are querying single var, error means the query is no good */
                if(pVar)
                    snprint_objid(pBuf, sizeof(pBuf), pVar->name, pVar->name_length);
                else
                    strcpy(pBuf, "(none)");

                sprintf(pErrBuf, "snmp_sess_synch_response: errindex %ld, %s %s %s\n",
                    respQryPdu->errindex, pRequest->pSnmpAgent->pActiveSession->peername, pRequest->objectId.requestName, pBuf);
                errlogPrintf("%s", pErrBuf);
                snmp_sess_perror(pErrBuf, pRequest->pSnmpAgent->pActiveSession);
                rtn = -1;
            }

            break;
        case STAT_TIMEOUT:
            errlogPrintf("%s: Snmp Query Timeout\n", pRequest->pSnmpAgent->pActiveSession->peername);
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
                printf("\tcommunity: %s, version: %s, %d variables\n\n", pSnmpAgent->snmpSession.community, ver_msg[MIN(4,pSnmpAgent->snmpSession.version)], pSnmpAgent->snmpReqPtrList.count);
            }
        }
    }

    return 0;
}

