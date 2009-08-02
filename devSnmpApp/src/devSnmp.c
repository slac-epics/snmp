/***************************************************************************/
/*   $Id: devSnmp.c,v 1.1.1.1 2008/02/22 19:50:56 ernesto Exp $                   */
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

static long init_li_snmp(struct longinRecord *pli, long snmpVersion);
static long init_li_snmpV1(struct longinRecord *pli);
static long init_li_snmpV2c(struct longinRecord *pli);
static long read_li_snmp(struct longinRecord *pli);

static long init_si_snmp(struct stringinRecord *psi, long snmpVersion);
static long init_si_snmpV1(struct stringinRecord *psi);
static long init_si_snmpV2c(struct stringinRecord *psi);
static long read_si_snmp(struct stringinRecord *psi);

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
SNMP_DEV_SUP_SET devLiSnmpV1 = {6, NULL, NULL, init_li_snmpV1,  NULL, read_li_snmp, NULL};
SNMP_DEV_SUP_SET devSiSnmpV1 = {6, NULL, NULL, init_si_snmpV1,  NULL, read_si_snmp, NULL};
SNMP_DEV_SUP_SET devWfSnmpV1 = {6, NULL, NULL, init_wf_snmpV1,  NULL, read_wf_snmp, NULL};

SNMP_DEV_SUP_SET devAiSnmpV2c = {6, NULL, NULL, init_ai_snmpV2c,  NULL, read_ai_snmp, NULL};
SNMP_DEV_SUP_SET devLiSnmpV2c = {6, NULL, NULL, init_li_snmpV2c,  NULL, read_li_snmp, NULL};
SNMP_DEV_SUP_SET devSiSnmpV2c = {6, NULL, NULL, init_si_snmpV2c,  NULL, read_si_snmp, NULL};
SNMP_DEV_SUP_SET devWfSnmpV2c = {6, NULL, NULL, init_wf_snmpV2c,  NULL, read_wf_snmp, NULL};

#if EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(dset, devAiSnmpV1);
epicsExportAddress(dset, devLiSnmpV1);
epicsExportAddress(dset, devSiSnmpV1);
epicsExportAddress(dset, devWfSnmpV1);

epicsExportAddress(dset, devAiSnmpV2c);
epicsExportAddress(dset, devLiSnmpV2c);
epicsExportAddress(dset, devSiSnmpV2c);
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

    status = snmpQueryInit((dbCommon *)pai, pai->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE);

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
    SNMP_QUERY  *pQuery = (SNMP_QUERY *)(pai->dpvt);
    int rtn = -1;
    char *pValStr;

    if(!pQuery) return(-1);

    if(!pai->pact)
    {/* pre-process */
        /* Clean up the request */
        pQuery->errCode = SNMP_QUERY_NO_ERR;
        pQuery->opDone = 0;

        if(epicsMessageQueueTrySend(pQuery->pSnmpAgent->msgQ_id, (void *)&pQuery, sizeof(SNMP_QUERY *)) == -1)
        {
            recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Send Message to Snmp Operation Thread Error [%s]", pai->name);
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
        if( (!pQuery->opDone) || pQuery->errCode )
        {
            recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pai->name, pQuery->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG)   printf("Record [%s] receives string [%s]!\n", pai->name, pQuery->pValStr);

            pValStr = strchr(pQuery->pValStr, ':');
            if(pValStr == NULL) pValStr = pQuery->pValStr;
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

    status = snmpQueryInit((dbCommon *)pli, pli->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE);

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
    SNMP_QUERY  *pQuery = (SNMP_QUERY *)(pli->dpvt);
    int rtn = -1;
    char *pValStr;
    unsigned int u32temp;

    if(!pQuery) return(-1);

    if(!pli->pact)
    {/* pre-process */
        /* Clean up the request */
        pQuery->errCode = SNMP_QUERY_NO_ERR;
        pQuery->opDone = 0;

        if(epicsMessageQueueTrySend(pQuery->pSnmpAgent->msgQ_id, (void *)&pQuery, sizeof(SNMP_QUERY *)) == -1)
        {
            recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Send Message to Snmp Operation Thread Error [%s]", pli->name);
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
        if( (!pQuery->opDone) || pQuery->errCode )
        {
            recGblSetSevr(pli, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pli->name, pQuery->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG)   printf("Record [%s] receives string [%s]!\n", pli->name, pQuery->pValStr);

            pValStr = strchr(pQuery->pValStr, ':');
            if(pValStr == NULL) pValStr = pQuery->pValStr;
            else pValStr++;

            /* The longin record for snmp only handles unsigned int */
            if (sscanf(pValStr, "%u", &u32temp))
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

    status = snmpQueryInit((dbCommon *)psi, psi->inp.value.instio.string, snmpVersion, MAX_CA_STRING_SIZE);

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
    SNMP_QUERY  *pQuery = (SNMP_QUERY *)(psi->dpvt);
    int rtn = -1;
    char *pValStr;

    if(!pQuery) return(-1);

    if(!psi->pact)
    {/* pre-process */
        /* Clean up the request */
        pQuery->errCode = SNMP_QUERY_NO_ERR;
        pQuery->opDone = 0;

        if(epicsMessageQueueTrySend(pQuery->pSnmpAgent->msgQ_id, (void *)&pQuery, sizeof(SNMP_QUERY *)) == -1)
        {
            recGblSetSevr(psi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Send Message to Snmp Operation Thread Error [%s]", psi->name);
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
        if( (!pQuery->opDone) || pQuery->errCode )
        {
            recGblSetSevr(psi, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", psi->name, pQuery->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG)   printf("Record [%s] receives string [%s]!\n", psi->name, pQuery->pValStr);

            if((pValStr = strchr(pQuery->pValStr, ':')) != NULL)
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
                    errlogPrintf("Record [%s] parsing response [%s] error!\n", psi->name, pQuery->pValStr);
                    rtn = -1;
                }
            }
            else
            {
                /* When we init pValStr, we made sure valStrLen is not longer than MAX_CA_STRING_SIZE */
                strcpy(psi->val, pQuery->pValStr);
                rtn = 0;
            }
        }
    }/* post-process */

    return(rtn);
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
            status = snmpQueryInit((dbCommon *)pwf, pwf->inp.value.instio.string, snmpVersion, sizeof(char) * pwf->nelm);
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
    SNMP_QUERY  *pQuery = (SNMP_QUERY *)(pwf->dpvt);
    int rtn = -1;

    if(!pQuery) return(-1);

    if(!pwf->pact)
    {/* pre-process */
        /* Clean up the request */
        pQuery->errCode = SNMP_QUERY_NO_ERR;
        pQuery->opDone = 0;

        if(epicsMessageQueueTrySend(pQuery->pSnmpAgent->msgQ_id, (void *)&pQuery, sizeof(SNMP_QUERY *)) == -1)
        {
            recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Send Message to Snmp Operation Thread Error [%s]", pwf->name);
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
        if( (!pQuery->opDone) || pQuery->errCode )
        {
            recGblSetSevr(pwf, READ_ALARM, INVALID_ALARM);
            errlogPrintf("Record [%s] receive error code [0x%08x]!\n", pwf->name, pQuery->errCode);
            rtn = -1;
        }
        else
        {
            if(SNMP_DEV_DEBUG)   printf("Record [%s] receives string [%s]!\n", pwf->name, pQuery->pValStr);

            switch(pwf->ftvl)
            {
                case DBF_CHAR:
                case DBF_UCHAR:
                case DBF_STRING:
                    /* When we init pValStr, we made sure valStrLen is not longer than nelm */
                    strcpy(pwf->bptr, pQuery->pValStr);
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

