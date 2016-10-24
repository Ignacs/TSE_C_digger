#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <iconv.h>
#include <locale.h>
#include <wchar.h>
#include <math.h>
#include "header.h"


// debug flag
#define SQL_DBG 1

#define PATH_LEN 1024 
#define FILENAME_LEN 256
#define BUFF_LEN 4096

#define ARGU_INPUT_PATH 1
#define ARGU_OUTPUT_PATH 2

#define CMD_QUERY_TABLE     "SELECT * FROM TSETradeDay;"

#define STOCK_ID_LEN        64 
#define STOCK_VALUE_LEN     64 
#define STOCK_ATTR_NUM      16
#define STOCK_NAME_LEN      32 

// reference to database 
sqlite3 *db = NULL;
// reference to table 
char **table = NULL;

struct __STOCK__{
    wchar_t id[STOCK_ID_LEN];                         // 靡ㄩ腹
    wchar_t name[STOCK_NAME_LEN];   // "靡ㄩ嘿"
    unsigned long int stock;             // "Θユ计"
    unsigned long int trade;             // "Θユ掸计"
    unsigned long int vol;               // "Θユ肂"
    float open;            // "秨絃基"
    float high;            // "程蔼基"
    float low;            // "程基"
    float close;            // "Μ絃基"
    unsigned char sign;             // "害禴(+/-)"
    float diff;            // "害禴基畉"
    float buy;             // "程处ボ禦基"
    float buy_vol;         // "程处ボ禦秖"
    float sell;            // "程处ボ芥基"
    float sell_vol;        // "程处ボ芥秖"
    float epr;             // "セ痲ゑ"
}stockData, *pstockData;

int csvhandler(char *csvFile);
void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s <Folder contains csv fllies download from TSE> <ouptut foloder>\n" , app_name)
}

// return :     length of value
int searchInDoubleQuotea(wchar_t *bufWC, wchar_t *buf)
{
    // passing string [XXX","YYY", ...]
    wchar_t *pCurr = NULL;
    int i =0;

    if(NULL == bufWC)
    {
        output_err(STRING_NULL);
        return 0;
    }
    pCurr = bufWC;

    i = 0;
    while(L'"' != *(pCurr+i)) 
    {
DEBUG_OUTPUT(">>> %d => %lc\n", __LINE__, *(pCurr+i));
        DEBUG_OUTPUT("??? : >>>%lc\n ", *pCurr);
        *(buf+i) = *(pCurr+i);
        i++;
    }

    return i;
}

unsigned int wcsConvToU_10b(wchar_t *wcs)
{
    wchar_t *ptr = NULL ;
    int i = 0;
    char buf[BUFF_LEN];

    if( NULL == wcs)
        return 0;
    ptr = wcs;
    
DEBUG_OUTPUT(">>> string [%ls] \n", wcs);
    for(i = 0; 1; i++)
    {
DEBUG_OUTPUT(">>> %lc \n",  *(ptr+i));
        switch(*(ptr+i))
        {
            case L'0':
                buf[i] = '0';
                break;
            case L'1':
                buf[i] = '1';
                break;
            case L'2':
                buf[i] = '2';
                break;
            case L'3':
                buf[i] = '3';
                break;
            case L'4':
                buf[i] = '4';
                break;
            case L'5':
                buf[i] = '5';
                break;
            case L'6':
                buf[i] = '6';
                break;
            case L'7':
                buf[i] = '7';
                break;
            case L'8':
                buf[i] = '8';
                break;
            case L'9':
                buf[i] = '9';
                break;
            default : 
DEBUG_OUTPUT(">>> %ld \n",  strtol( buf, NULL, 10));
                return strtol( buf, NULL, 10);
                break;
        }
    }

DEBUG_OUTPUT(">>> %ld \n",  strtol( buf, NULL, 10));
    return 0;
}

struct __STOCK__ * data_pasrer(wchar_t *bufWC)
{
    int itemCount = 0, i = 0;
    unsigned char encounterFirstItem = 0;
    wchar_t value[STOCK_VALUE_LEN];
    wchar_t *pVal = NULL;
    unsigned int len = 0;

    if(NULL == bufWC)
    {
        output_err(STRING_NULL);
        return NULL;
    }

    // "靡ㄩ腹","靡ㄩ嘿","Θユ计","Θユ掸计","Θユ肂","秨絃基","程蔼基","程基","Μ絃基","害禴(+/-)","害禴基畉","程处ボ禦基","程处ボ禦秖","程处ボ芥基","程处ボ芥秖","セ痲ゑ"
    memset(&stockData, 0x0, sizeof(stockData));

    DEBUG_OUTPUT("parsing string: >>>%ls ", bufWC);

    // initlal 
    i = 0;
    do
    {
#if DEBUG
        // DEBUG_OUTPUT("get %lc\n", c);
#endif
        switch(bufWC[i])
        {
            case L'=': 
            {
                // ETF prefix, dont care
                break;
            }
            case L'\n':
            {
                DEBUG_OUTPUT("Read the end of line : Last charater : %lc \n", bufWC[i]);
                return NULL;
                break;
            }
            case L'\"':
            {
                if( 0 == encounterFirstItem )
                {
                    DEBUG_OUTPUT("Set flag to search value. \n");
                    encounterFirstItem = 1;
                }

DEBUG_OUTPUT(">>> check data, first = %lc ,  found items = %d\n", bufWC[i], itemCount);
                i++;
                while ((L'\"' != (bufWC[i])) && (STOCK_ATTR_NUM > itemCount))
                {
DEBUG_OUTPUT(">============================================<\n");
                    // wchar_t id[];                         // 靡ㄩ腹
                    pVal = stockData.id;
                    memset(pVal, 0x0, sizeof(pVal));
                    len = searchInDoubleQuotea(&bufWC[i], pVal);
DEBUG_OUTPUT(">>> get string [%d](%ls)\n",  len, pVal);
#if 0
                    memset(pVal, 0x0, sizeof(pVal));
                    //    wchar_t name[STOCK_NAME_LEN];   // "靡ㄩ嘿"
                    pVal = value;
                    memset(pVal, 0x0, sizeof(pVal));
                    //    unsigned int stock;             // "Θユ计"
                    memset(pVal, 0x0, sizeof(pVal));
                    stockData.id = wcsConvToU_10b( pVal);
                    //    unsigned int trade;             // "Θユ掸计"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    unsigned int vol;               // "Θユ肂"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float open;            // "秨絃基"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float high;            // "程蔼基"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float low;            // "程基"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float close;            // "Μ絃基"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    unsigned char sign;             // "害禴(+/-)"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float diff;            // "害禴基畉"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float buy;             // "程处ボ禦基"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float buy_vol;         // "程处ボ禦秖"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float sell;            // "程处ボ芥基"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float sell_vol;        // "程处ボ芥秖"
                    memset(pVal, 0x0, sizeof(pVal));
                    //    float epr;             // "セ痲ゑ"
#endif
                    i=i+len+1; // '\"'+<data>+'\"'
                }
                itemCount++;
                break;
            }
            case L',': 
            default:
            {
                if( 0 == encounterFirstItem) // If first item still not found, give up to search..
                {
                    output_err(DATA_FORMAT_ERROR);
                    DEBUG_OUTPUT("Last charater : %lc \n", bufWC[i]);
                    return NULL;
                }
                break;
            }
        }
    } while ( (L'\0' != bufWC[i++]) && (STOCK_ATTR_NUM > itemCount)) ; // 

    return &stockData;
}


int store_stock_to_db(struct __STOCK__ * pData)
{

    return 0;
}

void close_db()
{
    /* free */
    if(NULL != table)
        sqlite3_free_table(table);

    /* close database */
    sqlite3_close(db);
}

int main(int argc, char *argv[])
{
    // output compiled date and time 
    DEBUG_OUTPUT("Compiled date : \t[%s  %s] \n", __DATE__, __TIME__);

    int rows=0, cols =0;
    int i = 0, j = 0 ;
    char outputPath[PATH_LEN];
    char inputPath[PATH_LEN];
    struct stat st;
    FILE *traDayFp = NULL;
    char buf[BUFF_LEN];
    char *endptr =NULL;
    long int val = 0;
    int ret =0;
    char *errMsg = NULL;

    if ( 3 > argc) 
    {
        DEBUG_OUTPUT( "Too few arguments\n")
        usage(argv[0]);
        exit(0);
    }

    // change locale to Big5
    if (!setlocale(LC_CTYPE, "zh_TW.Big5")) 
    {
        output_err(LC_SET_FAIL);
        return 1;
    }

    // check in and out path
    memset(inputPath, 0x0, sizeof(inputPath));
    strcpy(inputPath, argv[ARGU_INPUT_PATH]);
    if(!(0 == stat(inputPath, &st) && S_ISDIR(st.st_mode)))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }

    memset(outputPath, 0x0, sizeof(outputPath));
    strcpy(outputPath, argv[ARGU_OUTPUT_PATH]);
    if(!(0 == stat(outputPath, &st) && S_ISDIR(st.st_mode)))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }
    DEBUG_OUTPUT("Output folder  : \t[%s] \n", outputPath);
    DEBUG_OUTPUT("Input folder: \t[%s] \n", inputPath);

    // check trading day list
    memset(buf, 0x0, sizeof(buf));
    sprintf(buf, "%s/%s", inputPath, TSE_DATE_CLASSIFIC);
    DEBUG_OUTPUT("DB file: \t[%s] \n", buf);
    if (sqlite3_open_v2(buf, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
    {
        // db doesn't exist
        output_err(DB_NOT_FOUND);
        close_db();
        exit(0);
    }

#if SQL_DBG
    DEBUG_OUTPUT("DB open successfully \n");
#endif //SQL_DBG

    // query all trading days.
    ret = sqlite3_get_table(db , CMD_QUERY_TABLE, &table, &rows, &cols, &errMsg);
    if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
    {
        output_err(DB_QUERY_FAIL);
        DEBUG_OUTPUT("Cause: \t[%s]\n", errMsg);
        close_db();
        exit(0);
    }

    DEBUG_OUTPUT("List all date: \n");

    // get daily data sequencely
    for(i = 0 ; i< rows; i++)
    {
        // in 2-dimension string structure 
        // EX:
        //  date1 1
        //  date2 0
        //  date3 1...
        //  => [data1][1][date2][0][date3][1]...
        if ((val = strtol(table[i*cols+1], &endptr, 10))==1) 
        {
            // trading day
            // sprintf(buf, "%s/%s", outputPath, table[i*cols]);
#if SQL_DBG
            DEBUG_OUTPUT("Date: %s\t[%s]\n", table[i*cols], table[i*cols+1]);
#endif //SQL_DBG

            // check trading day csv file
            memset(buf, 0x0, sizeof(buf));
            sprintf(buf, "%s/%s.csv", inputPath, table[i*cols]);

            ret = csvhandler(buf);
        }
    }



    close_db();
    exit(0);
}

int csvhandler(char *csvFile)
{
    //argv[1] path to csv file
    //argv[2] number of lines to skip
    //argv[3] length of longest value (in characters)

    FILE *pf = NULL;
    struct stat st;
    wint_t c ;
    wchar_t bufWC[BUFF_LEN];
    unsigned int vcpm; //value character marker
    int i = 0 ;
    int ret = 0 ;

    if(0 != stat(csvFile, &st))
    {
        output_err(FILE_NOT_FOUND);
        DEBUG_OUTPUT("%s doesn't exist\n", csvFile);
        return -1;
    }

    if( pf = fopen(csvFile,"r") )
    {
        DEBUG_OUTPUT("open %s successfully\n", csvFile);
        rewind(pf);

        while( (c = fgetwc(pf)) != WEOF)
        {
#if DEBUG
            // DEBUG_OUTPUT("get %lc\n", c);
#endif
            switch(c)
            {
                case ',':
                {
                    break;
                }
                case '\n':
                {
                    break;
                }
                case '\"':
                {
                    break;
                }
                case L'琩':
                {
                    // 琩礚戈 => not a trading day.
                    c = fgetwc(pf);
                    if( L'礚' == c )
                    {
                        DEBUG_OUTPUT("%s not a trading day\n", csvFile);
                        fclose(pf);
                        return 0;
                    }
                    break;
                }
#if 0 // parser example
                case L'':
                {
                    DEBUG_OUTPUT("get Keyword\n");
                    //絃参璸戈癟 => a trading day
                    memset(bufWC, 0x0, sizeof(bufWC));
                    bufWC[0] = c ;
                    for(i = 1 ; i< 6; i++)
                    {
                        if((c = fgetwc(pf)) == WEOF)
                        {
                            output_err(FILE_READ_FAIL);
                            fclose(pf);
                            return 0;
                        }
                        DEBUG_OUTPUT("%lc,", c);
                        bufWC[i] = c ;
                    }

                    DEBUG_OUTPUT("get string %ls\n", bufWC);
                    if( 0 ==  wcscmp(L"絃参璸戈癟", bufWC))
                    {
                        DEBUG_OUTPUT(">>>> %s is a trading day\n", csvFile);
                        fclose(pf);
                        return 0;
                    }
                    else 
                        DEBUG_OUTPUT("... not keystring\n");
                    break;
                }
#endif
#if 1 // т碝ETF, パ'='秨﹍﹃
                case '=':
                {
                    DEBUG_OUTPUT("get ETF, try to get whole stock line by line\n");
                    //絃参璸戈癟 => a trading day
                    memset(bufWC, 0x0, sizeof(bufWC));

                    // It should be the first ETF 
                    // parse all stocks to last one
                    while(fgetws(bufWC, sizeof(bufWC), pf) != NULL)
                    {
                        // if "弧" appears, it means no more data, so stop read file.
                        if(!wcsncmp(L"\"弧\"", bufWC, (int)wcslen(L"\"弧\"")))
                        {
                            DEBUG_OUTPUT("No more data, stop read file\n");
                            fclose(pf);
                            return 0;
                        }
                        // pass to ETF_wcs_handler and return a struct store specified ETF's data.
                        if((pstockData = data_pasrer(bufWC) ) == NULL)
                            continue;
                        else // pass to function to store in DB.
                        {
                            ret = store_stock_to_db(pstockData);
                        }
                    }
                    break;
                }
#endif

                default:
                {
                    break;
                }
            }
        }
        fclose(pf);
    }
    else
    {
        output_err(FILE_OPEN_FAIL);
    }

    DEBUG_OUTPUT("%s has no related string...\n", csvFile);

    return 0;
}


