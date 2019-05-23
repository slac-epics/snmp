/***************************************************************************\
 *   File:              devSNMP.c
 *   Author:            Sheng Peng
 *   Email:             pengsh2003@yahoo.com
 *   Phone:             408-660-7762
 *   Company:           RTESYS, Inc.
 *   Date:              07/2007
 *   Version:           1.0
 *
 *   EPICS device support for net-snmp
 *
\***************************************************************************/

#include "devSnmp.h"
#include "epicsExport.h"
#include "iocsh.h"

int SNMP_DEV_DEBUG = 0;

epicsExportAddress( int, SNMP_DEV_DEBUG );

/* function prototypes */
static long init_ai_snmp(struct aiRecord *pai, long snmpVersion);
static long init_ai_snmpV1(struct aiRecord *pai);
static long init_ai_snmpV2c(struct aiRecord *pai);
static long read_ai_snmp(struct aiRecord *pai);

static long init_ao_snmp(struct aoRecord *pao, long snmpVersion);
static long init_ao_snmpV1(struct aoRecord *pao);
static long init_ao_snmpV2c(struct aoRecord *pao);
static long write_ao_snmp(struct aoRecord *pao);

static long init_li_snmp(struct longinRecord *pli, long snmpVersion);
static long init_li_snmpV1(struct longinRecord *pli);
static long init_li_snmpV2c(struct longinRecord *pli);
static long read_li_snmp(struct longinRecord *pli);

static long init_mbbi_snmp(struct mbbiRecord *pmbbi, long snmpVersion);
static long init_mbbi_snmpV1(struct mbbiRecord *pmbbi);
static long init_mbbi_snmpV2c(struct mbbiRecord *pmbbi);
static long read_mbbi_snmp(struct mbbiRecord *pmbbi);

static long init_mbbiDirect_snmp(struct mbbiDirectRecord *pmbbiDirect, long snmpVersion);
static long init_mbbiDirect_snmpV1(struct mbbiDirectRecord *pmbbiDirect);
static long init_mbbiDirect_snmpV2c(struct mbbiDirectRecord *pmbbiDirect);
static long read_mbbiDirect_snmp(struct mbbiDirectRecord *pmbbiDirect);

static long init_mbbo_snmp(struct mbboRecord *pmbbo, long snmpVersion);
static long init_mbbo_snmpV1(struct mbboRecord *pmbbo);
static long init_mbbo_snmpV2c(struct mbboRecord *pmbbo);
static long write_mbbo_snmp(struct mbboRecord *pmbbo);

static long init_mbboDirect_snmp(struct mbboDirectRecord *pmbboDirect, long snmpVersion);
static long init_mbboDirect_snmpV1(struct mbboDirectRecord *pmbboDirect);
static long init_mbboDirect_snmpV2c(struct mbboDirectRecord *pmbboDirect);
static long write_mbboDirect_snmp(struct mbboDirectRecord *pmbboDirect);

static long init_bi_snmp(struct biRecord *pbi, long snmpVersion);
static long init_bi_snmpV1(struct biRecord *pbi);
static long init_bi_snmpV2c(struct biRecord *pbi);
static long read_bi_snmp(struct biRecord *pbi);

static long init_bo_snmp(struct boRecord *pbi, long snmpVersion);
static long init_bo_snmpV1(struct boRecord *pbi);
static long init_bo_snmpV2c(struct boRecord *pbi);
static long write_bo_snmp(struct boRecord *pbi);

static long init_lo_snmp(struct longoutRecord *plo, long snmpVersion);
static long init_lo_snmpV1(struct longoutRecord *plo);
static long init_lo_snmpV2c(struct longoutRecord *plo);
static long write_lo_snmp(struct longoutRecord *plo);

static long init_si_snmp(struct stringinRecord *psi, long snmpVersion);
static long init_si_snmpV1(struct stringinRecord *psi);
static long init_si_snmpV2c(struct stringinRecord *psi);
static long read_si_snmp(struct stringinRecord *psi);

static long init_so_snmp(struct stringoutRecord *pso, long snmpVersion);
static long init_so_snmpV1(struct stringoutRecord *pso);
static long init_so_snmpV2c(struct stringoutRecord *pso);
static long write_so_snmp(struct stringoutRecord *pso);

static long init_wf_snmp(struct waveformRecord *pwf, long snmpVersion);
static long init_wf_snmpV1(struct waveformRecord *pwf);
static long init_wf_snmpV2c(struct waveformRecord *pwf);
static long read_wf_snmp(struct waveformRecord *pwf);

static long get_ioint_info(int cmd, struct dbCommon *prec, IOSCANPVT *iopvt);

/* global struct for devSup */
typedef struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_write;
        DEVSUPFUN       special_linconv;
} SNMP_DEV_SUP_SET;

SNMP_DEV_SUP_SET devAiSnmpV1 = {6, NULL, NULL, init_ai_snmpV1,  get_ioint_info, read_ai_snmp, NULL};
SNMP_DEV_SUP_SET devAoSnmpV1 = {6, NULL, NULL, init_ao_snmpV1,  NULL, write_ao_snmp, NULL};
SNMP_DEV_SUP_SET devLiSnmpV1 = {6, NULL, NULL, init_li_snmpV1,  get_ioint_info, read_li_snmp, NULL};
SNMP_DEV_SUP_SET devMbbiSnmpV1={6, NULL, NULL, init_mbbi_snmpV1,  get_ioint_info, read_mbbi_snmp, NULL};
SNMP_DEV_SUP_SET devMbbiDirectSnmpV1={6, NULL, NULL, init_mbbiDirect_snmpV1,  get_ioint_info, read_mbbiDirect_snmp, NULL};
SNMP_DEV_SUP_SET devMbboSnmpV1={6, NULL, NULL, init_mbbo_snmpV1,  NULL, write_mbbo_snmp, NULL};
SNMP_DEV_SUP_SET devMbboDirectSnmpV1={6, NULL, NULL, init_mbboDirect_snmpV1,  NULL, write_mbboDirect_snmp, NULL};
SNMP_DEV_SUP_SET devBiSnmpV1 = {6, NULL, NULL, init_bi_snmpV1,  get_ioint_info, read_bi_snmp, NULL};
SNMP_DEV_SUP_SET devBoSnmpV1 = {6, NULL, NULL, init_bo_snmpV1,  NULL, write_bo_snmp, NULL};
SNMP_DEV_SUP_SET devLoSnmpV1 = {6, NULL, NULL, init_lo_snmpV1,  NULL, write_lo_snmp, NULL};
SNMP_DEV_SUP_SET devSiSnmpV1 = {6, NULL, NULL, init_si_snmpV1,  get_ioint_info, read_si_snmp, NULL};
SNMP_DEV_SUP_SET devSoSnmpV1 = {6, NULL, NULL, init_so_snmpV1,  NULL, write_so_snmp, NULL};
SNMP_DEV_SUP_SET devWfSnmpV1 = {6, NULL, NULL, init_wf_snmpV1,  get_ioint_info, read_wf_snmp, NULL};

SNMP_DEV_SUP_SET devAiSnmpV2c = {6, NULL, NULL, init_ai_snmpV2c,  get_ioint_info, read_ai_snmp, NULL};
SNMP_DEV_SUP_SET devAoSnmpV2c = {6, NULL, NULL, init_ao_snmpV2c,  NULL, write_ao_snmp, NULL};
SNMP_DEV_SUP_SET devLiSnmpV2c = {6, NULL, NULL, init_li_snmpV2c,  get_ioint_info, read_li_snmp, NULL};
SNMP_DEV_SUP_SET devMbbiSnmpV2c={6, NULL, NULL, init_mbbi_snmpV2c,  get_ioint_info, read_mbbi_snmp, NULL};
SNMP_DEV_SUP_SET devMbbiDirectSnmpV2c={6, NULL, NULL, init_mbbiDirect_snmpV2c,  get_ioint_info, read_mbbiDirect_snmp, NULL};
SNMP_DEV_SUP_SET devMbboSnmpV2c={6, NULL, NULL, init_mbbo_snmpV2c,  NULL, write_mbbo_snmp, NULL};
SNMP_DEV_SUP_SET devMbboDirectSnmpV2c={6, NULL, NULL, init_mbboDirect_snmpV2c,  NULL, write_mbboDirect_snmp, NULL};
SNMP_DEV_SUP_SET devBiSnmpV2c={6, NULL, NULL, init_bi_snmpV2c,  get_ioint_info, read_bi_snmp, NULL};
SNMP_DEV_SUP_SET devBoSnmpV2c={6, NULL, NULL, init_bo_snmpV2c,  NULL, write_bo_snmp, NULL};
SNMP_DEV_SUP_SET devLoSnmpV2c = {6, NULL, NULL, init_lo_snmpV2c,  NULL, write_lo_snmp, NULL};
SNMP_DEV_SUP_SET devSiSnmpV2c = {6, NULL, NULL, init_si_snmpV2c,  get_ioint_info, read_si_snmp, NULL};
SNMP_DEV_SUP_SET devSoSnmpV2c = {6, NULL, NULL, init_so_snmpV2c,  NULL, write_so_snmp, NULL};
SNMP_DEV_SUP_SET devWfSnmpV2c = {6, NULL, NULL, init_wf_snmpV2c,  get_ioint_info, read_wf_snmp, NULL};

#if EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(dset, devAiSnmpV1);
epicsExportAddress(dset, devAoSnmpV1);
epicsExportAddress(dset, devLiSnmpV1);
epicsExportAddress(dset, devMbbiSnmpV1);
epicsExportAddress(dset, devMbbiDirectSnmpV1);
epicsExportAddress(dset, devMbboSnmpV1);
epicsExportAddress(dset, devMbboDirectSnmpV1);
epicsExportAddress(dset, devBiSnmpV1);
epicsExportAddress(dset, devBoSnmpV1);
epicsExportAddress(dset, devLoSnmpV1);
epicsExportAddress(dset, devSiSnmpV1);
epicsExportAddress(dset, devSoSnmpV1);
epicsExportAddress(dset, devWfSnmpV1);

epicsExportAddress(dset, devAiSnmpV2c);
epicsExportAddress(dset, devAoSnmpV2c);
epicsExportAddress(dset, devLiSnmpV2c);
epicsExportAddress(dset, devMbbiSnmpV2c);
epicsExportAddress(dset, devMbbiDirectSnmpV2c);
epicsExportAddress(dset, devMbboSnmpV2c);
epicsExportAddress(dset, devMbboDirectSnmpV2c);
epicsExportAddress(dset, devBiSnmpV2c);
epicsExportAddress(dset, devBoSnmpV2c);
epicsExportAddress(dset, devLoSnmpV2c);
epicsExportAddress(dset, devSiSnmpV2c);
epicsExportAddress(dset, devSoSnmpV2c);
epicsExportAddress(dset, devWfSnmpV2c);
#endif

static long get_ioint_info(int cmd, struct dbCommon *prec, IOSCANPVT *iopvt)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(prec->dpvt);
    SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                     pRequest->objectId.requestOid,
                                                     pRequest->objectId.requestOidLen);
    if (!data) {
        static IOSCANPVT badscan;
        static int first = 1;
        if (first) {
            first = 0;
            scanIoInit(&badscan);
        }
        *iopvt = badscan;
        printf("ERROR: record %s, MIB %s is not covered by walk!\n",
               prec->name, pRequest->objectId.requestName);
    } else {
        *iopvt = data->iopvt;
    }
    return 0;
}

static long init_ai_snmp(struct aiRecord *pai, long snmpVersion)
{
    int status = 0;

    /* ai.inp must be INST_IO */
    if(pai->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pai, "devAiSnmp (init_record) Illegal INP field");
        pai->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pai, pai->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 0, 'D');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pai,"devAiSnmp (init_record) bad parameters");
        pai->pact = TRUE;
        return(S_db_badField);
    }

    return(status);
}

static long init_ai_snmpV1(struct aiRecord *pai)
{
    return init_ai_snmp(pai, SNMP_VERSION_1);
}

static long init_ai_snmpV2c(struct aiRecord *pai)
{
    return init_ai_snmp(pai, SNMP_VERSION_2c);
}

static long read_ai_snmp(struct aiRecord *pai)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pai->dpvt);
    int rtn = -1;
    char *pValStr;

    if(!pRequest) return(-1);

    if (pai->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!pai->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_ai_snmp: epicsMessageQueueTrySend Error [%s]\n", pai->name);
            rtn = -1;
        }
        else
        {
            pai->pact = TRUE;
            rtn = NO_CONVERT;
        }
        return rtn;
    }/* pre-process */

    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pai->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] received string [%s]\n", pai->name, pRequest->pValStr);

            pValStr = strrchr(pRequest->pValStr, ':');
            if(pValStr == NULL) pValStr = pRequest->pValStr;
            else pValStr++;

            if (sscanf(pValStr, "%lf", &pai->val))
            {
                pai->udf = FALSE;
                rtn = NO_CONVERT;
            }
            else
            {
                recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pai->name, pValStr);
                rtn = -1;
            }
        }
    }/* post-process */
    return (rtn);
}

static long init_ao_snmp(struct aoRecord *pao, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;

    /* ao.out must be INST_IO */
    if(pao->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pao, "devAoSnmp (init_record) Illegal INP field");
        pao->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pao, pao->out.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 1, 'D');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pao,"devAoSnmp (init_record) bad parameters");
        pao->pact = TRUE;
        return(S_db_badField);
    }

    pRequest = (SNMP_REQUEST *)(pao->dpvt);
    if(0 == snmpQuerySingleVar(pRequest))
    {
        if(SNMP_DEV_DEBUG)   printf("Record [%s] received string [%s] during init.\n", pao->name, pRequest->pValStr);
        pValStr = strrchr(pRequest->pValStr, ':');
        if(pValStr == NULL) pValStr = pRequest->pValStr;
        else pValStr++;

        if (sscanf(pValStr, "%lf", &pao->val))
        {
            pao->udf = FALSE;
            pao->stat=pao->sevr=NO_ALARM;
        }
        else
        {
            errlogPrintf("Record [%s] parsing response [%s] error!\n", pao->name, pValStr);
        }
    }

    return NO_CONVERT;
}

static long init_ao_snmpV1(struct aoRecord *pao)
{
    return init_ao_snmp(pao, SNMP_VERSION_1);
}

static long init_ao_snmpV2c(struct aoRecord *pao)
{
    return init_ao_snmp(pao, SNMP_VERSION_2c);
}

static long write_ao_snmp(struct aoRecord *pao)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pao->dpvt);

    if(!pRequest) return(-1);

    if(!pao->pact)
    {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;
        /* Give the value */
        /* snprintf(pRequest->pValStr, MAX_CA_STRING_SIZE-1, "%lf", pao->val); */
        sprintf(pRequest->pValStr, "%lf", pao->val);

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pao, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("write_ao_snmp: epicsMessageQueueTrySend Error [%s]\n", pao->name);
            return -1;
        }
        else
        {
            pao->pact = TRUE;
        }

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pao, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pao->name, pRequest->errCode);
            return -1;
        }
    }/* post-process */
    return 0;
}

static long init_li_snmp(struct longinRecord *pli, long snmpVersion)
{
    int status = 0;

    /* longin.inp must be INST_IO */
    if(pli->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pli, "devLiSnmp (init_record) Illegal INP field");
        pli->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pli, pli->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 0, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pli,"devLiSnmp (init_record) bad parameters");
        pli->pact = TRUE;
        return(S_db_badField);
    }

    return(status);
}

static long init_li_snmpV1(struct longinRecord *pli)
{
    return init_li_snmp(pli, SNMP_VERSION_1);
}

static long init_li_snmpV2c(struct longinRecord *pli)
{
    return init_li_snmp(pli, SNMP_VERSION_2c);
}

static long read_li_snmp(struct longinRecord *pli)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pli->dpvt);
    int rtn = -1;
    char *pValStr;
    int i32temp	= -1;

    if(!pRequest) return(-1);

    if (pli->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!pli->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_li_snmp: epicsMessageQueueTrySend Error on [%s]\n", pli->name);
            rtn = -1;
        }
        else
        {
            pli->pact = TRUE;
            rtn = 0;
        }
        return rtn;
    }/* pre-process */

    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pli->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] received string [%s]\n", pli->name, pRequest->pValStr);

            pValStr = strrchr(pRequest->pValStr, ':');
            if(pValStr == NULL) pValStr = pRequest->pValStr;
            else pValStr++;

            /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
            for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; )
				++pValStr;

            /* Scan for an integer */
            if (pValStr && sscanf(pValStr, "%d", &i32temp))
            {
                pli->val = i32temp;
                pli->udf = FALSE;
                rtn = 0;
            }
            else
            {
                pli->val = -1;
                recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pli->name, pValStr);
                rtn = -1;
            }
        }
    }/* post-process */

    return(rtn);
}

static long init_mbbi_snmp(struct mbbiRecord *pmbbi, long snmpVersion)
{
    int status = 0;

    /* mbbi.inp must be INST_IO */
    if(pmbbi->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pmbbi, "devMbbiSnmp (init_record) Illegal INP field");
        pmbbi->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pmbbi, pmbbi->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 0, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pmbbi,"devMbbiSnmp (init_record) bad parameters");
        pmbbi->pact = TRUE;
        return(S_db_badField);
    }

    return(status);
}

static long init_mbbi_snmpV1(struct mbbiRecord *pmbbi)
{
    return init_mbbi_snmp(pmbbi, SNMP_VERSION_1);
}

static long init_mbbi_snmpV2c(struct mbbiRecord *pmbbi)
{
    return init_mbbi_snmp(pmbbi, SNMP_VERSION_2c);
}

static long read_mbbi_snmp(struct mbbiRecord *pmbbi)
{
    SNMP_REQUEST	*	pRequest	= (SNMP_REQUEST *)(pmbbi->dpvt);
    int					rtn 		= -1;
    char			*	pValStr;
    int					i32temp		= -1;

    if(!pRequest) return(-1);

    if (pmbbi->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!pmbbi->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pmbbi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_mbbi_snmp: epicsMessageQueueTrySend Error on [%s]\n", pmbbi->name);
            rtn = -1;
        }
        else
        {
			if ( pmbbi->tpro )
				printf( "read_mbbi_snmp [%s]: Val %d, Sent req, set pact TRUE\n", pmbbi->name, pmbbi->val );
            pmbbi->pact = TRUE;
            rtn = 0;
        }
        return rtn;
    }	/* pre-process */


    {	/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pmbbi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pmbbi->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if ( pmbbi->tpro )
                printf( "read_mbbi_snmp [%s]: Val %d, received string [%s]\n", pmbbi->name, pmbbi->val, pRequest->pValStr );
            if(SNMP_DEV_DEBUG > 1)
                printf("Record [%s] received string [%s]\n", pmbbi->name, pRequest->pValStr);

            pValStr = strrchr(pRequest->pValStr, ':');
            if ( pValStr == NULL )
                pValStr = pRequest->pValStr;
            else
                pValStr++;

            /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
            for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; )
                ++pValStr;

            /* Scan for an integer */
            if ( pValStr && sscanf( pValStr, "%d", &i32temp ) ) {
                pmbbi->rval	= i32temp;
                pmbbi->udf	= FALSE;
                rtn = 0;
            } else {
                pmbbi->rval	= -1;
                recGblSetSevr(pmbbi, READ_ALARM, INVALID_ALARM);
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pmbbi->name, pValStr);
                rtn = -1;
            }
        }
    }	/* post-process */

    return(rtn);
}

static long init_mbbiDirect_snmp(struct mbbiDirectRecord *pmbbiDirect, long snmpVersion)
{
    int status = 0;

    /* mbbiDirect.inp must be INST_IO */
    if(pmbbiDirect->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pmbbiDirect, "devMbbiDirectSnmp (init_record) Illegal INP field");
        pmbbiDirect->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pmbbiDirect, pmbbiDirect->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 0, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pmbbiDirect,"devMbbiDirectSnmp (init_record) bad parameters");
        pmbbiDirect->pact = TRUE;
        return(S_db_badField);
    }

    return(status);
}

static long init_mbbiDirect_snmpV1(struct mbbiDirectRecord *pmbbiDirect)
{
    return init_mbbiDirect_snmp(pmbbiDirect, SNMP_VERSION_1);
}

static long init_mbbiDirect_snmpV2c(struct mbbiDirectRecord *pmbbiDirect)
{
    return init_mbbiDirect_snmp(pmbbiDirect, SNMP_VERSION_2c);
}


static long read_mbbiDirect_snmp(struct mbbiDirectRecord *pmbbiDirect)
{
    SNMP_REQUEST	*	pRequest	= (SNMP_REQUEST *)(pmbbiDirect->dpvt);
    int					rtn 		= -1;
    char			*	pValStr;
    int					i32temp		= -1;

    if(!pRequest) return(-1);

    if (pmbbiDirect->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!pmbbiDirect->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1) {
            recGblSetSevr(pmbbiDirect, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_mbbiDirect_snmp: epicsMessageQueueTrySend Error on [%s]\n", pmbbiDirect->name);
            rtn = -1;
        } else {
            if ( pmbbiDirect->tpro )
                printf( "read_mbbiDirect_snmp [%s]: Val %d, Sent req, set pact TRUE\n", pmbbiDirect->name, pmbbiDirect->val );
            pmbbiDirect->pact = TRUE;
            rtn = 0;
        }
        return rtn;
    }	/* pre-process */


    {	/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode ) {
            recGblSetSevr(pmbbiDirect, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pmbbiDirect->name, pRequest->errCode);
            rtn = -1;
        } else {
            if ( pmbbiDirect->tpro )
                printf( "read_mbbiDirect_snmp [%s]: Val %d, received string [%s]\n", pmbbiDirect->name, pmbbiDirect->val, pRequest->pValStr );
            if(SNMP_DEV_DEBUG > 1)
                printf("Record [%s] received string [%s]\n", pmbbiDirect->name, pRequest->pValStr);

            pValStr = strrchr(pRequest->pValStr, ':');
            if ( pValStr == NULL )
                pValStr = pRequest->pValStr;
            else
                pValStr++;

            /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
            for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; )
                ++pValStr;

            /* Scan for an integer */
            if ( pValStr && sscanf( pValStr, "%d", &i32temp ) ) {
                pmbbiDirect->rval	= i32temp;
                pmbbiDirect->udf	= FALSE;
                rtn = 0;
            } else {
                pmbbiDirect->rval	= -1;
                recGblSetSevr(pmbbiDirect, READ_ALARM, INVALID_ALARM);
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pmbbiDirect->name, pValStr);
                rtn = -1;
            }
        }
    }	/* post-process */

    return(rtn);
}

static long init_mbbo_snmp(struct mbboRecord *pmbbo, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;
    int i32temp;

    /* type must be INST_IO */
    if(pmbbo->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pmbbo, "devMbboSnmp (init_record) Illegal INP field");
        pmbbo->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pmbbo, pmbbo->out.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 1, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pmbbo,"devMbboSnmp (init_record) bad parameters");
        pmbbo->pact = TRUE;
        return(S_db_badField);
    }

    pRequest = (SNMP_REQUEST *)(pmbbo->dpvt);
    if(0 == snmpQuerySingleVar(pRequest))
    {
        if(SNMP_DEV_DEBUG)   printf("Record [%s] received string [%s] during init.\n", pmbbo->name, pRequest->pValStr);
        pValStr = strrchr(pRequest->pValStr, ':');
        if(pValStr == NULL) pValStr = pRequest->pValStr;
        else pValStr++;

        /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
        for (; isdigit(*pValStr) == 0 && *pValStr != '-' && *pValStr != '\0'; ) ++pValStr;
        /* Scan for an integer */
        if (pValStr && sscanf(pValStr, "%d", &i32temp))
        {
            pmbbo->rval = i32temp;
            pmbbo->udf = FALSE;
            pmbbo->stat=pmbbo->sevr=NO_ALARM;
        }
        else
        {
            pmbbo->rval = -1;
            errlogPrintf("Record [%s] parsing response [%s] error!\n", pmbbo->name, pValStr);
    		return 1;	/* no convert */
        }
    }
    return 0;	/* convert */
}

static long init_mbbo_snmpV1(struct mbboRecord *pmbbo)
{
    return init_mbbo_snmp(pmbbo, SNMP_VERSION_1);
}

static long init_mbbo_snmpV2c(struct mbboRecord *pmbbo)
{
    return init_mbbo_snmp(pmbbo, SNMP_VERSION_2c);
}

static long write_mbbo_snmp(struct mbboRecord *pmbbo)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pmbbo->dpvt);

    if(!pRequest) return(-1);

    if(!pmbbo->pact)
    {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;
        /* Give the value */
        /* sprintf(pRequest->pValStr, "%*d", MAX_CA_STRING_SIZE-1, pmbbo->rval); */
        sprintf(pRequest->pValStr, "%d", pmbbo->rval);

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pmbbo, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("write_mbbo_snmp: epicsMessageQueueTrySend Error [%s]\n", pmbbo->name);
            return -1;
        }
        else
        {
            pmbbo->pact = TRUE;
        }

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pmbbo, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pmbbo->name, pRequest->errCode);
            return -1;
        }
    }/* post-process */

    return 0;
}

static long init_mbboDirect_snmp(struct mbboDirectRecord *pmbboDirect, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;

    /* type must be INST_IO */
    if(pmbboDirect->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pmbboDirect, "devMbboDirectSnmp (init_record) Illegal INP field");
        pmbboDirect->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pmbboDirect, pmbboDirect->out.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 1, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pmbboDirect,"devMbboDirectSnmp (init_record) bad parameters");
        pmbboDirect->pact = TRUE;
        return(S_db_badField);
    }

    pRequest = (SNMP_REQUEST *)(pmbboDirect->dpvt);
    if(0 == snmpQuerySingleVar(pRequest))
    {
        if(SNMP_DEV_DEBUG)   printf("Record [%s] received string [%s] during init.\n", pmbboDirect->name, pRequest->pValStr);
        pValStr = strrchr(pRequest->pValStr, ':');
        if(pValStr == NULL) pValStr = pRequest->pValStr;
        else pValStr++;
    }
    return 0;	/* convert */
}

static long init_mbboDirect_snmpV1(struct mbboDirectRecord *pmbboDirect)
{
    return init_mbboDirect_snmp(pmbboDirect, SNMP_VERSION_1);
}

static long init_mbboDirect_snmpV2c(struct mbboDirectRecord *pmbboDirect)
{
    return init_mbboDirect_snmp(pmbboDirect, SNMP_VERSION_2c);
}

static long write_mbboDirect_snmp(struct mbboDirectRecord *pmbboDirect)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pmbboDirect->dpvt);

    if(!pRequest) return(-1);

    if(!pmbboDirect->pact)
    {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;
        /* Give the value */
        /* sprintf(pRequest->pValStr, "%*d", MAX_CA_STRING_SIZE-1, pmbboDirect->rval); */
        sprintf(pRequest->pValStr, "%d", pmbboDirect->rval);

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pmbboDirect, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("write_mbboDirect_snmp: epicsMessageQueueTrySend Error [%s]\n", pmbboDirect->name);
            return -1;
        }
        else
        {
            pmbboDirect->pact = TRUE;
        }

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pmbboDirect, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pmbboDirect->name, pRequest->errCode);
            return -1;
        }
    }/* post-process */

    return 0;
}

static long init_bi_snmp(struct biRecord *pbi, long snmpVersion)
{
    int status = 0;

    /* bi.inp must be INST_IO */
    if(pbi->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pbi, "devBiSnmp (init_record) Illegal INP field");
        pbi->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pbi, pbi->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 0, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pbi,"devBiSnmp (init_record) bad parameters");
        pbi->pact = TRUE;
        return(S_db_badField);
    }

    return(status);
}

static long init_bi_snmpV1(struct biRecord *pbi)
{
    return init_bi_snmp(pbi, SNMP_VERSION_1);
}

static long init_bi_snmpV2c(struct biRecord *pbi)
{
    return init_bi_snmp(pbi, SNMP_VERSION_2c);
}

static long read_bi_snmp(struct biRecord *pbi)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pbi->dpvt);
    int rtn = -1;
    char *pValStr;
    unsigned int u32temp;

    if(!pRequest) return(-1);

    if (pbi->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!pbi->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pbi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_bi_snmp: epicsMessageQueueTrySend Error on [%s]\n", pbi->name);
            rtn = -1;
        }
        else
        {
            pbi->pact = TRUE;
            rtn = 0;
        }
        return rtn;
    }	/* pre-process */

    {	/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pbi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pbi->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] received string [%s]\n", pbi->name, pRequest->pValStr);

            pValStr = strrchr(pRequest->pValStr, ':');
            if ( pValStr == NULL )
				pValStr = pRequest->pValStr;
            else
				pValStr++;

            /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
            for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; )
				++pValStr;

            /* The bi record for snmp only handles unsigned int */
            if ( pValStr && sscanf( pValStr, "%u", &u32temp ) )
            {
                pbi->rval	= u32temp;
                /* pbi->udf	= FALSE; */
				rtn = 0;
			}
            else
            {
                recGblSetSevr(pbi, READ_ALARM, INVALID_ALARM);
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pbi->name, pValStr);
				rtn = -1;
			}
        }
    }	/* post-process */

    return(rtn);
}

static long init_lo_snmp(struct longoutRecord *plo, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;
    int i32temp;

    /* longout.out must be INST_IO */
    if(plo->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)plo, "devLoSnmp (init_record) Illegal INP field");
        plo->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)plo, plo->out.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 1, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)plo,"devLoSnmp (init_record) bad parameters");
        plo->pact = TRUE;
        return(S_db_badField);
    }

    pRequest = (SNMP_REQUEST *)(plo->dpvt);
    if(0 == snmpQuerySingleVar(pRequest))
    {
        if(SNMP_DEV_DEBUG)   printf("Record [%s] received string [%s] during init.\n", plo->name, pRequest->pValStr);
        pValStr = strrchr(pRequest->pValStr, ':');
        if(pValStr == NULL) pValStr = pRequest->pValStr;
        else pValStr++;

        /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
        for (; isdigit(*pValStr) == 0 && *pValStr != '-' && *pValStr != '\0'; ) ++pValStr;
        /* Scan for an integer */
        if (pValStr && sscanf(pValStr, "%d", &i32temp))
        {
            plo->val = i32temp;
            plo->udf = FALSE;
            plo->stat=plo->sevr=NO_ALARM;
        }
        else
        {
            plo->val = -1;
            errlogPrintf("Record [%s] parsing response [%s] error!\n", plo->name, pValStr);
        }
    }
    return 0;
}

static long init_lo_snmpV1(struct longoutRecord *plo)
{
    return init_lo_snmp(plo, SNMP_VERSION_1);
}

static long init_lo_snmpV2c(struct longoutRecord *plo)
{
    return init_lo_snmp(plo, SNMP_VERSION_2c);
}

static long write_lo_snmp(struct longoutRecord *plo)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(plo->dpvt);

    if(!pRequest) return(-1);

    if(!plo->pact)
    {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;
        /* Give the value */
        /* sprintf(pRequest->pValStr, "%*d", MAX_CA_STRING_SIZE-1, plo->val); */
        sprintf(pRequest->pValStr, "%d", plo->val);

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(plo, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("write_lo_snmp: epicsMessageQueueTrySend Error [%s]\n", plo->name);
            return -1;
        }
        else
        {
            plo->pact = TRUE;
        }

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(plo, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", plo->name, pRequest->errCode);
            return -1;
        }
    }/* post-process */

    return 0;
}

static long init_bo_snmp(struct boRecord *plo, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;
    unsigned int u32temp;

    /* bout.out must be INST_IO */
    if(plo->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)plo, "devBoSnmp (init_record) Illegal INP field");
        plo->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)plo, plo->out.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 1, 'i');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)plo,"devBoSnmp (init_record) bad parameters");
        plo->pact = TRUE;
        return(S_db_badField);
    }

    pRequest = (SNMP_REQUEST *)(plo->dpvt);
    if(0 == snmpQuerySingleVar(pRequest))
    {
        if(SNMP_DEV_DEBUG)   printf("Record [%s] received string [%s] during init.\n", plo->name, pRequest->pValStr);
        pValStr = strrchr(pRequest->pValStr, ':');
        if(pValStr == NULL) pValStr = pRequest->pValStr;
        else pValStr++;

        /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
        for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; ) ++pValStr;
        /* The bout record for snmp only handles unsigned int */
        if (pValStr && sscanf(pValStr, "%u", &u32temp))
        {
            plo->val = u32temp;
            plo->udf = FALSE;
            plo->stat=plo->sevr=NO_ALARM;
        }
        else
        {
            plo->val = 0;
            errlogPrintf("Record [%s] parsing response [%s] error!\n", plo->name, pValStr);
        }
    }
    return 0;
}

static long init_bo_snmpV1(struct boRecord *plo)
{
    return init_bo_snmp(plo, SNMP_VERSION_1);
}

static long init_bo_snmpV2c(struct boRecord *plo)
{
    return init_bo_snmp(plo, SNMP_VERSION_2c);
}

static long write_bo_snmp(struct boRecord *plo)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(plo->dpvt);

    if(!pRequest) return(-1);

    if(!plo->pact)
    {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;
        /* Give the value */
        /* sprintf(pRequest->pValStr, "%*d", MAX_CA_STRING_SIZE-1, plo->val); */
        sprintf(pRequest->pValStr, "%d", plo->val);

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(plo, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("write_bo_snmp: epicsMessageQueueTrySend Error [%s]\n", plo->name);
            return -1;
        }
        else
        {
            plo->pact = TRUE;
        }

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(plo, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", plo->name, pRequest->errCode);
            return -1;
        }
    }/* post-process */

    return 0;
}

static long init_si_snmp(struct stringinRecord *psi, long snmpVersion)
{
    int status = 0;

    /* stringin.inp must be INST_IO */
    if(psi->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)psi, "devSiSnmp (init_record) Illegal INP field");
        psi->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)psi, psi->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 0, 's');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)psi,"devSiSnmp (init_record) bad parameters");
        psi->pact = TRUE;
        return(S_db_badField);
    }

    return(status);
}

static long init_si_snmpV1(struct stringinRecord *psi)
{
    return init_si_snmp(psi, SNMP_VERSION_1);
}

static long init_si_snmpV2c(struct stringinRecord *psi)
{
    return init_si_snmp(psi, SNMP_VERSION_2c);
}

static long read_si_snmp(struct stringinRecord *psi)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(psi->dpvt);
    int rtn = -1;
    char *pValStr;

    if(!pRequest) return(-1);

    if (psi->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!psi->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(psi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_si_snmp: epicsMessageQueueTrySend Error [%s]\n", psi->name);
            rtn = -1;
        }
        else
        {
            psi->pact = TRUE;
            rtn = 0;
        }
        return rtn;
    }/* pre-process */

    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(psi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", psi->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] received string [%s]\n", psi->name, pRequest->pValStr);

            if((pValStr = strrchr(pRequest->pValStr, ':')) != NULL)
            {
                pValStr++;	/* skip ':' */
                /* skip whitespace */
                for (; isspace(*pValStr) && *pValStr != '\0'; ) ++pValStr;

                if(pValStr)
                {
                    strcpy(psi->val, pValStr);
                    psi->udf = FALSE;
                    rtn = 0;
                }
                else
                {
                    recGblSetSevr(psi, READ_ALARM, INVALID_ALARM);
                    errlogPrintf("Record [%s] parsing response [%s] error!\n", psi->name, pRequest->pValStr);
                    rtn = -1;
                }
            }
            else
            {
                /* When we init pValStr, we made sure valStrLen is not longer than MAX_CA_STRING_SIZE */
                strcpy(psi->val, pRequest->pValStr);
                rtn = 0;
            }
        }
    }/* post-process */

    return(rtn);
}

static long init_so_snmp(struct stringoutRecord *pso, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;

    /* stringout.out must be INST_IO */
    if(pso->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pso, "devSoSnmp (init_record) Illegal INP field");
        pso->pact = TRUE;
        return(S_db_badField);
    }

    status = snmpRequestInit((dbCommon *)pso, pso->out.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE, 1, 's');

    if (status)
    {
        recGblRecordError(S_db_badField, (void *)pso,"devSoSnmp (init_record) bad parameters");
        pso->pact = TRUE;
        return(S_db_badField);
    }

    pRequest = (SNMP_REQUEST *)(pso->dpvt);
    if(0 == snmpQuerySingleVar(pRequest))
    {
        if(SNMP_DEV_DEBUG)   printf("Record [%s] received string [%s] during init.\n", pso->name, pRequest->pValStr);

        if((pValStr = strrchr(pRequest->pValStr, ':')) != NULL)
        {
            pValStr++;	/* skip ':' */
            /* skip whitespace */
            for (; isspace(*pValStr) && *pValStr != '\0'; ) ++pValStr;

            if(pValStr)
            {
                strcpy(pso->val, pValStr);
                pso->udf = FALSE;
                pso->stat=pso->sevr=NO_ALARM;
            }
            else
            {
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pso->name, pRequest->pValStr);
            }
        }
        else
        {
            /* When we init pValStr, we made sure valStrLen is not longer than MAX_CA_STRING_SIZE */
            strcpy(pso->val, pRequest->pValStr);
        }
    }
    return 0;
}

static long init_so_snmpV1(struct stringoutRecord *pso)
{
    return init_so_snmp(pso, SNMP_VERSION_1);
}

static long init_so_snmpV2c(struct stringoutRecord *pso)
{
    return init_so_snmp(pso, SNMP_VERSION_2c);
}

static long write_so_snmp(struct stringoutRecord *pso)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pso->dpvt);

    if(!pRequest) return(-1);

    if(!pso->pact)
    {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;
        /* Give the value */
		if ( pRequest->valLength != 0 )
		{
        	snprintf( &pRequest->pValStr[0], MAX_CA_STRING_SIZE-1, "%-*.*s", pRequest->valLength, pRequest->valLength, pso->val );
		}
		else
        	strncpy(pRequest->pValStr, pso->val, MAX_CA_STRING_SIZE-1);
        pRequest->pValStr[MAX_CA_STRING_SIZE-1] = '\0';

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pso, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("write_so_snmp: epicsMessageQueueTrySend Error [%s]\n", pso->name);
            return -1;
        }
        else
        {
            pso->pact = TRUE;
        }

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pso, WRITE_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pso->name, pRequest->errCode);
            return -1;
        }
    }/* post-process */

    return 0;
}

static long init_wf_snmp(struct waveformRecord *pwf, long snmpVersion)
{
    int status = 0;

    /* waveform.inp must be an INST_IO */
    if(pwf->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField,(void *)pwf, "devWfSnmp (init_record) Illegal INP field");
        pwf->pact = TRUE;
        return(S_db_badField);
    }

    switch(pwf->ftvl)
    {
        case DBF_STRING:
        case DBF_CHAR:
        case DBF_UCHAR:
            status = snmpRequestInit((dbCommon *)pwf, pwf->inp.value.instio.string, snmpVersion, sizeof(char) * pwf->nelm, 0, 's');
            if (status)
            {
                recGblRecordError(S_db_badField, (void *)pwf,"devWfSnmp (init_record) bad parameters");
                pwf->pact = TRUE;
                return(S_db_badField);
            }
            break;
        default:
            recGblRecordError(S_db_badField,(void *)pwf, "devWfSnmp (init_record) Illegal FTVL field");
            pwf->pact = TRUE;
            return(S_db_badField);
    }

    return(status);
}

static long init_wf_snmpV1(struct waveformRecord *pwf)
{
    return init_wf_snmp(pwf, SNMP_VERSION_1);
}

static long init_wf_snmpV2c(struct waveformRecord *pwf)
{
    return init_wf_snmp(pwf, SNMP_VERSION_2c);
}

static long read_wf_snmp(struct waveformRecord *pwf)
{
    SNMP_REQUEST  *pRequest = (SNMP_REQUEST *)(pwf->dpvt);
    int rtn = -1;

    if(!pRequest) return(-1);

    if (pwf->scan == SCAN_IO_EVENT) {
        SNMP_WALK_OID *data = netsnmp_oid_stash_get_data(stashRoot, 
                                                         pRequest->objectId.requestOid,
                                                         pRequest->objectId.requestOidLen);
        if (!data)
            return -1;
        pRequest->opDone = 1;
        pRequest->errCode = data->errCode;
        if (data->errCode == SNMP_REQUEST_NO_ERR) {
            strncpy(pRequest->pValStr, data->pBuf, pRequest->valStrLen - 1);
            pRequest->pValStr[pRequest->valStrLen - 1] = '\0';
        }
    } else if (!pwf->pact) {/* pre-process */
        /* Clean up the request */
        pRequest->errCode = SNMP_REQUEST_NO_ERR;
        pRequest->opDone = 0;

        if(epicsMessageQueueTrySend(pRequest->pSnmpAgent->msgQ_id, (void *)&pRequest, sizeof(SNMP_REQUEST *)) == -1)
        {
            recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
            errlogPrintf("read_wf_snmp: epicsMessageQueueTrySend Error [%s]\n", pwf->name);
            rtn = -1;
        }
        else
        {
            pwf->pact = TRUE;
            rtn = 0;
        }
        return rtn;
    }/* pre-process */

    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] received error code [0x%08x]!\n", pwf->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] received string [%s]\n", pwf->name, pRequest->pValStr);

            switch(pwf->ftvl)
            {
                case DBF_CHAR:
                case DBF_UCHAR:
                case DBF_STRING:
                    /* When we init pValStr, we made sure valStrLen is not longer than nelm */
                    strcpy(pwf->bptr, pRequest->pValStr);
                    pwf->nord = strlen(pwf->bptr);
                    rtn = 0;
                    break;
                default:
                    pwf->nord = 1;
                    rtn = 0;
                    break;
            }
        }
    }/* post-process */


    return(rtn);
}

/*
 * IOC shell command registration
 */
static const iocshArg Snmp2cWalkArg0 = {"host", iocshArgString};
static const iocshArg Snmp2cWalkArg1 = {"community", iocshArgString};
static const iocshArg Snmp2cWalkArg2 = {"OIDname", iocshArgString};
static const iocshArg Snmp2cWalkArg3 = {"count",iocshArgInt};
static const iocshArg Snmp2cWalkArg4 = {"delay",iocshArgDouble};
static const iocshArg *Snmp2cWalkArgs[] =
{
    &Snmp2cWalkArg0, &Snmp2cWalkArg1, &Snmp2cWalkArg2, &Snmp2cWalkArg3, &Snmp2cWalkArg4,
};
static const iocshFuncDef Snmp2cWalkFuncDef = { "Snmp2cWalk", 5, Snmp2cWalkArgs };

extern void Snmp2cWalk(char *host, char *community, char *oidname, int count, double delay);

static void Snmp2cWalkCallFunc( const iocshArgBuf * args )
{
    Snmp2cWalk(args[0].sval, args[1].sval, args[2].sval, args[3].ival, args[4].dval);
}

static void
snmpRegisterCommands(void)
{
    static int firstTime = 1;
    if (firstTime)
	{
        iocshRegister(&Snmp2cWalkFuncDef,Snmp2cWalkCallFunc);
        firstTime = 0;
    }
}
epicsExportRegistrar(snmpRegisterCommands);
