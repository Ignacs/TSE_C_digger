#include <stdio.h>
#include "header.h"


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
// This library will calculate stock's VRMA
//
//
double calculate_VR(struct stock_daily *pDataCal, unsigned int day)
{
    int i = 0;
    unsigned long volSumPosDay = 0;
    unsigned long volSumNegDay = 0;

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
            DEBUG_OUTPUT("%d day is pos\n", i);
            volSumPosDay += (unsigned long)((pDataCal+i)->stockNum/1000);
        }
        else if(SIGN_DOWN == (pDataCal+i)->sign)
        {
            DEBUG_OUTPUT("%d day is neg\n", i);
            volSumNegDay += (unsigned long)((pDataCal+i)->stockNum/1000);
        }
    }
    DEBUG_OUTPUT("postive day %lu.\n", volSumPosDay);
    DEBUG_OUTPUT("negative day %lu .\n", volSumNegDay);

    if(0 < volSumNegDay)
    {
        DEBUG_OUTPUT("result %f .\n", (double )((volSumPosDay*1.0) / (volSumNegDay*1.0)));
        return (volSumPosDay / volSumNegDay);
    }
    else
    {
        DEBUG_OUTPUT("negative day is ZERO .\n");
        return 0.0;
    }
}

//
//
// Volume Ratio Moving Average
// In N days, ((N VR)*2() + (N-1)VRMA*(N-2))/ (N+1)
// For more reliablity,  result must compare (N) to (N-VRMA) days.
int indicator(struct stock_daily **pStockData, int length, int dayVR, int dayVRMA, char *outputFile)
{
    FILE *pOoutFp = NULL;
    char buf[BUFF_LEN];
    unsigned int i  = 0, j = 0;
    int ret = 0;
    struct stock_daily *pData=*pStockData;

    if(NULL == pStockData)
    {
        DEBUG_OUTPUT("NULL data\n");
        return -1;
    }

    DEBUG_OUTPUT( "==============Call library  = %s===============\n", __FILE__);
    DEBUG_OUTPUT( "dayMA = %d\n",dayVRMA);
    if(0 >= dayVRMA)
    {
        DEBUG_OUTPUT( "ERR: Too short \n");
        return 0;
    }

    DEBUG_OUTPUT( "dayVR = %d\n", dayVR);
    // if VR is too short to calculate VRMA.
    if(dayVRMA >= dayVR)
    {
        DEBUG_OUTPUT( "ERR: Too short than VRMA\n");
        return 0;
    }

    DEBUG_OUTPUT( "length = %d\n", length);
    // if history data is too short to calculate.
    // for precesion , the result must compare (dayVR+dayVRMA) days.
    if( (dayVR+dayVRMA) >= length )
    {
        DEBUG_OUTPUT( "ERR: history too short to calculate!!\n");
        return 0;
    }

    checkFileAbs(outputFile, (char *)buf, BUFF_LEN);

    DEBUG_OUTPUT( "outputFile = %s\n", buf);
    pOoutFp = fopen(buf, "w+");
    if( pOoutFp )
    {
        DEBUG_OUTPUT( "File open successfully\n");

        for(i = 0 ; i< dayVRMA; i++)
        {
            calculate_VR((*pStockData)+i, dayVR);
        }

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
