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

int SNMP_DEV_DEBUG = 0;

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

SNMP_DEV_SUP_SET devAiSnmpV1 = {6, NULL, NULL, init_ai_snmpV1,  NULL, read_ai_snmp, NULL};
SNMP_DEV_SUP_SET devAoSnmpV1 = {6, NULL, NULL, init_ao_snmpV1,  NULL, write_ao_snmp, NULL};
SNMP_DEV_SUP_SET devLiSnmpV1 = {6, NULL, NULL, init_li_snmpV1,  NULL, read_li_snmp, NULL};
SNMP_DEV_SUP_SET devLoSnmpV1 = {6, NULL, NULL, init_lo_snmpV1,  NULL, write_lo_snmp, NULL};
SNMP_DEV_SUP_SET devSiSnmpV1 = {6, NULL, NULL, init_si_snmpV1,  NULL, read_si_snmp, NULL};
SNMP_DEV_SUP_SET devSoSnmpV1 = {6, NULL, NULL, init_so_snmpV1,  NULL, write_so_snmp, NULL};
SNMP_DEV_SUP_SET devWfSnmpV1 = {6, NULL, NULL, init_wf_snmpV1,  NULL, read_wf_snmp, NULL};

SNMP_DEV_SUP_SET devAiSnmpV2c = {6, NULL, NULL, init_ai_snmpV2c,  NULL, read_ai_snmp, NULL};
SNMP_DEV_SUP_SET devAoSnmpV2c = {6, NULL, NULL, init_ao_snmpV2c,  NULL, write_ao_snmp, NULL};
SNMP_DEV_SUP_SET devLiSnmpV2c = {6, NULL, NULL, init_li_snmpV2c,  NULL, read_li_snmp, NULL};
SNMP_DEV_SUP_SET devLoSnmpV2c = {6, NULL, NULL, init_lo_snmpV2c,  NULL, write_lo_snmp, NULL};
SNMP_DEV_SUP_SET devSiSnmpV2c = {6, NULL, NULL, init_si_snmpV2c,  NULL, read_si_snmp, NULL};
SNMP_DEV_SUP_SET devSoSnmpV2c = {6, NULL, NULL, init_so_snmpV2c,  NULL, write_so_snmp, NULL};
SNMP_DEV_SUP_SET devWfSnmpV2c = {6, NULL, NULL, init_wf_snmpV2c,  NULL, read_wf_snmp, NULL};

#if EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(dset, devAiSnmpV1);
epicsExportAddress(dset, devAoSnmpV1);
epicsExportAddress(dset, devLiSnmpV1);
epicsExportAddress(dset, devLoSnmpV1);
epicsExportAddress(dset, devSiSnmpV1);
epicsExportAddress(dset, devSoSnmpV1);
epicsExportAddress(dset, devWfSnmpV1);

epicsExportAddress(dset, devAiSnmpV2c);
epicsExportAddress(dset, devAoSnmpV2c);
epicsExportAddress(dset, devLiSnmpV2c);
epicsExportAddress(dset, devLoSnmpV2c);
epicsExportAddress(dset, devSiSnmpV2c);
epicsExportAddress(dset, devSoSnmpV2c);
epicsExportAddress(dset, devWfSnmpV2c);
#endif

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

    if(!pai->pact)
    {/* pre-process */
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

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pai->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] receives string [%s]!\n", pai->name, pRequest->pValStr);

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
        /* sprintf(pRequest->pValStr, "%*.8lf", MAX_CA_STRING_SIZE-1, pao->val); */
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
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pao->name, pRequest->errCode);
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
    unsigned int u32temp;

    if(!pRequest) return(-1);

    if(!pli->pact)
    {/* pre-process */
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

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pli->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] receives string [%s]!\n", pli->name, pRequest->pValStr);

            pValStr = strrchr(pRequest->pValStr, ':');
            if(pValStr == NULL) pValStr = pRequest->pValStr;
            else pValStr++;

            /* skip non-digit, particularly because of WIENER crate has ON(1), OFF(0) */
            for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; )
				++pValStr;

            /* The longin record for snmp only handles unsigned int */
            if (pValStr && sscanf(pValStr, "%u", &u32temp))
            {
                pli->val = u32temp&0x7FFFFFFF;	/* Get rid of MSB since pli->val is signed */
                pli->udf = FALSE;
				rtn = 0;
			}
            else
            {
                recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
                errlogPrintf("Record [%s] parsing response [%s] error!\n", pli->name, pValStr);
				rtn = -1;
			}
        }
    }/* post-process */

    return(rtn);
}

static long init_lo_snmp(struct longoutRecord *plo, long snmpVersion)
{
    int status = 0;
    SNMP_REQUEST  *pRequest;
    char *pValStr;
    unsigned int u32temp;

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
        for (; isdigit(*pValStr) == 0 && *pValStr != '\0'; ) ++pValStr;
        /* The longin record for snmp only handles unsigned int */
        if (pValStr && sscanf(pValStr, "%u", &u32temp))
        {
            plo->val = u32temp&0x7FFFFFFF;	/* Get rid of MSB since pli->val is signed */
            plo->udf = FALSE;
            plo->stat=plo->sevr=NO_ALARM;
        }
        else
        {
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
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", plo->name, pRequest->errCode);
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

    if(!psi->pact)
    {/* pre-process */
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

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(psi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", psi->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] receives string [%s]!\n", psi->name, pRequest->pValStr);

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
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pso->name, pRequest->errCode);
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

    if(!pwf->pact)
    {/* pre-process */
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

    }/* pre-process */
    else
    {/* post-process */
        if( (!pRequest->opDone) || pRequest->errCode )
        {
            recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pwf->name, pRequest->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG > 1)   printf("Record [%s] receives string [%s]!\n", pwf->name, pRequest->pValStr);

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

