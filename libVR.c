#include <stdio.h>
#include "header.h"
#define LAST_DATA_PRIV      2 // the lastday privriledge weigth

//  line data sequence: date      stockNum        tradeNum        volume      open        high        low     close       sign        diff        buy     buyVol      sell        sellVol     epr
//
//
typedef struct dataCal
{
    char sign;
    unsigned long vol;
}dataUnit;
//struct dataUnit dataToCalVR[LEN_DAYS_ORD];
//
//
// Volume Ratio
// In N days, <Total stock volumn that close price upper than yesterday> / <Total stock volume that close price lower than yesterday>
//
//
// This library will calculate stock's VREMA
//
//
double calculate_VR(struct stock_daily *pDataCal, unsigned int day)
{
    int i = 0;
    unsigned long volSumPosDay = 0;
    unsigned long volSumNegDay = 0;
    double ret =0.0;

    if(NULL == pDataCal)
    {
        DEBUG_OUTPUT("Error when calculate data.\n");
        return -1;
    }

    if(0 > day)
    {
        DEBUG_OUTPUT("Argument day is too few.\n");
        return -1;
    }

    for( i = 0; i< day; i++ )
    {
        // 在這裡是要將股數換成張數所以除以1000
        if(SIGN_UP == (pDataCal+i)->sign)
        {
            volSumPosDay += (pDataCal+i)->stockNum;
            DEBUG_OUTPUT("%d day is + = %lu, total = %lu\n", i, (pDataCal+i)->stockNum, volSumPosDay);
        }
        else if(SIGN_DOWN == (pDataCal+i)->sign)
        {
            volSumNegDay += (pDataCal+i)->stockNum;
            DEBUG_OUTPUT("%d day is - = %lu, total = %lu\n", i, (pDataCal+i)->stockNum, volSumNegDay);
        }
    }
    DEBUG_OUTPUT("postive day %lu.\n", volSumPosDay);
    DEBUG_OUTPUT("negative day %lu .\n", volSumNegDay);

    ret = 0.0;
    if(0 < volSumNegDay)
    {
        ret = (double )((volSumPosDay*1.0) / (volSumNegDay*1.0));
        DEBUG_OUTPUT("result %f .\n", ret);
    }
    return ret;
}

//
//
// Volume Ratio Moving Average
// In N days, ((N VR)*2() + (N-1)VREMA*(N-2))/ (N+1)
// For more reliablity,  result must compare (N) to (N-VREMA) days.
int indicator(char *stockID, struct stock_daily **pStockData, int length, int dayVR, int dayVREMA, char *outputFile)
{
    FILE *pOoutFp = NULL;
    char buf[BUFF_LEN];
    unsigned int i  = 0, j = 0;
    int ret = 0;
    struct stock_daily *pData=*pStockData;
    double aVR[LEN_DAYS_ORD] = {0.0};
    double dVREMA1 = 0.0, dVREMA2 = 0.0;
    double dVRSMA=0;
    unsigned int pastPri = 0;
    unsigned int currOffsetDay = 0;

    if(NULL == pStockData)
    {
        DEBUG_OUTPUT("NULL data\n");
        return -1;
    }

    DEBUG_OUTPUT( "==============Call library  = %s===============\n", __FILE__);
    DEBUG_OUTPUT( "dayMA = %d\n",dayVREMA);
    if( LAST_DATA_PRIV >= dayVREMA )
    {
        DEBUG_OUTPUT( "ERR: Too short than priority :%d\n", LAST_DATA_PRIV);
        return 0;
    }

    DEBUG_OUTPUT( "dayVR = %d\n", dayVR);
    // if VR is too short to calculate VREMA.
    if(dayVREMA >= dayVR && LEN_DAYS_ORD < dayVR)
    {
        DEBUG_OUTPUT( "ERR: Too short than VREMA\n");
        return 0;
    }

    DEBUG_OUTPUT( "length = %d\n", length);
    // if history data is too short to calculate.
    // for precesion , the result must compare (dayVR+dayVREMA+1) days.
    if( (dayVR+dayVREMA+1) >= length )
    {
        DEBUG_OUTPUT( "ERR: history too short to calculate!!\n");
        return 0;
    }

    checkFileAbs(outputFile, (char *)buf, BUFF_LEN);

    DEBUG_OUTPUT( "outputFile = %s\n", buf);
    pOoutFp = fopen(buf, "a+");
    if( pOoutFp )
    {
        DEBUG_OUTPUT( "File open successfully\n");

        // calculate the VR of each day.
        // VR needn (dayVR+dayVREMA+1) days to calculate
        for(i = 0; i <= (dayVR+dayVREMA); i++)
        {
            aVR[i] = calculate_VR((*pStockData)+i, dayVR);
            DEBUG_OUTPUT( "date %lu (%d) days ago's VR= %f\n", ((*pStockData)+i)->date, i, aVR[i]);
        }
        DEBUG_OUTPUT( "========================================\n");

        // For accuracy, Indicator must all history VREMA, but it is impossible
        // Just get apporiate value for initial value. Here use the SMA of (dayVREMA+1)th day
        // First, calculate the SMA of (dayVREMA+1)th day ago
        // But the EMA of yesterday still need the EMA of (dayVREMA) days from 1 to (dayVREMA+1) and the (dayVREMA+2)th day's SMA
        for( currOffsetDay = i = dayVREMA + 2, dVRSMA = 0.0; i < (dayVREMA+2+dayVR) ; i++ )
        {
            DEBUG_OUTPUT( "date %lu (%d)th days ago's VR= %f\n", ((*pStockData)+i)->date, i+1, aVR[i]);
            dVRSMA += aVR[i];
        }
        dVRSMA = (double)dVRSMA/(dayVR);
        DEBUG_OUTPUT( "SMA = %f\n", dVRSMA);

        // the pervious day's prioriorty.
        pastPri = (dayVREMA-LAST_DATA_PRIV+1);

        // the EMA initialize from SMA
        currOffsetDay--;
        dVREMA2 = (( aVR[currOffsetDay] * LAST_DATA_PRIV ) + ( dVRSMA * pastPri )) / ( dayVREMA + 1 );
        DEBUG_OUTPUT( "(%d)th days =%f\n", currOffsetDay+1, dVREMA2);
        DEBUG_OUTPUT( "date %lu (%d)th days ago's VR= %f\n", ((*pStockData)+currOffsetDay)->date, currOffsetDay+1, aVR[currOffsetDay]);

        // VRMA of yesterday
        currOffsetDay--;
        for(i = (currOffsetDay); i > 0; i--)
        {
            //  calculate VREMA
            dVREMA2 = ( (aVR[i] * LAST_DATA_PRIV) + (dVREMA2 * pastPri) ) / (dayVREMA+1);
            DEBUG_OUTPUT( "(%d)th days ago =%f\n", i+1, dVREMA2);
            DEBUG_OUTPUT( "date %lu (%d)th days ago's VR= %f\n", ((*pStockData)+i)->date, i+1, aVR[i]);
        }
        DEBUG_OUTPUT( "yesterday:  days = %f\n",  dVREMA2);

        // VRMA of today
        // calculate VREMA from (dayVREMA-1) to today
        // the EMA of day(dayVREMA)
        dVREMA1 = (( aVR[0] * LAST_DATA_PRIV) + ( dVREMA2 * pastPri)) / ( dayVREMA + 1 );
        DEBUG_OUTPUT( " EMA of Today= %f\n",  dVREMA1);
        DEBUG_OUTPUT( "today's VR= %f\n",  aVR[0]);

        if(dVREMA1 >= dVREMA2)
            fprintf(pOoutFp, "%s", stockID);


        fclose(pOoutFp);
    }
    else
    {
        output_err(FILE_OPEN_FAIL);
    }

    return 0;
}

#if 0
int main( int argc, char **argv)
{
    indicator();

    return 0;
}
#endif
