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
#define STOCK_NAME_LEN 32 

#define ARGU_INPUT_PATH 1
#define ARGU_OUTPUT_PATH 2

#define CMD_QUERY_TABLE     "SELECT * FROM TSETradeDay;"

#define STOCK_ATTR_NUM      16

// reference to database 
sqlite3 *db = NULL;
// reference to table 
char **table = NULL;

struct __STOCK__{
    unsigned long int id;                         // "�Ҩ�N��"
    wchar_t name[STOCK_NAME_LEN];   // "�Ҩ�W��"
    unsigned long int stock;             // "����Ѽ�"
    unsigned long int trade;             // "���浧��"
    unsigned long int vol;               // "������B"
    float open;            // "�}�L��"
    float high;            // "�̰���"
    float low;            // "�̧C��"
    float close;            // "���L��"
    unsigned char sign;             // "���^(+/-)"
    float diff;            // "���^���t"
    float buy;             // "�̫ᴦ�ܶR��"
    float buy_vol;         // "�̫ᴦ�ܶR�q"
    float sell;            // "�̫ᴦ�ܽ��"
    float sell_vol;        // "�̫ᴦ�ܽ�q"
    float epr;             // "���q��"
}stockData, *pstockData;

int csvhandler(char *csvFile);
void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s <Folder contains csv fllies download from TSE> <ouptut foloder>\n" , app_name)
}

struct __STOCK__ * data_pasrer(wchar_t *bufWC)
{
    int itemCount = 0, i = 0;
    unsigned char encounterFirstItem = 0;
    unsigned char encounterETFPrefix = 0;


    // "�Ҩ�N��","�Ҩ�W��","����Ѽ�","���浧��","������B","�}�L��","�̰���","�̧C��","���L��","���^(+/-)","���^���t","�̫ᴦ�ܶR��","�̫ᴦ�ܶR�q","�̫ᴦ�ܽ��","�̫ᴦ�ܽ�q","���q��"
    memset(&stockData, 0x0, sizeof(stockData));

    DEBUG_OUTPUT("Target string: >>>%ls ", bufWC);

    do
    {
#if DEBUG
        // DEBUG_OUTPUT("get %lc\n", c);
#endif
        switch(bufWC[i])
        {
            case L'=': // ETF prefix, dont care
            {
                if( 0 == encounterETFPrefix )
                {
                    DEBUG_OUTPUT("Another ETF found\n");
                    encounterETFPrefix = 1;
                }
                else 
                {
                    output_err(DATA_FORMAT_ERROR);
                    DEBUG_OUTPUT("Last charater : %lc \n", bufWC[i]);
                    return NULL;
                }
                break;
            }
            case L'\n':
            {
                DEBUG_OUTPUT("Read the end of line : Last charater : %lc \n", bufWC[i]);
                return NULL;
                break;
            }
            case '\"':
            {
                if( 0 == encounterFirstItem )
                    encounterFirstItem = 1;

                /*
                while ((L'\"' != bufWC[i++]) && (STOCK_ATTR_NUM > itemCount))
                {

                    searchPairDoubleQuotea();
                    //    int id;                         // "�Ҩ�N��"
                    stockData.id = wcstold();
                    wcstoul ( __nptr,  __endptr, __base)
                    //    wchar_t name[STOCK_NAME_LEN];   // "�Ҩ�W��"
                    //    unsigned int stock;             // "����Ѽ�"
                    //    unsigned int trade;             // "���浧��"
                    //    unsigned int vol;               // "������B"
                    //                    wcstol
                    //    float open;            // "�}�L��"
                    //    float high;            // "�̰���"
                    //    float low;            // "�̧C��"
                    //    float close;            // "���L��"
                    //    unsigned char sign;             // "���^(+/-)"
                    //    float diff;            // "���^���t"
                    //    float buy;             // "�̫ᴦ�ܶR��"
                    //    float buy_vol;         // "�̫ᴦ�ܶR�q"
                    //    float sell;            // "�̫ᴦ�ܽ��"
                    //    float sell_vol;        // "�̫ᴦ�ܽ�q"
                    //    float epr;             // "���q��"
                    itemCount++;
                    */


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

    FILE *pf;
    struct stat st;
    char pTempValHolder[BUFF_LEN];
    wint_t c;
    wchar_t bufWC[BUFF_LEN];
    unsigned int vcpm; //value character marker
    int QuotationOnOff; //0 - off, 1 - on
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

        vcpm = 0;
        QuotationOnOff = 0;

        while( (c = fgetwc(pf)) != WEOF)
        {
#if DEBUG
            // DEBUG_OUTPUT("get %lc\n", c);
#endif
            switch(c)
            {
                case ',':
                {
                    if(!QuotationOnOff) 
                    {
                        pTempValHolder[vcpm] = '\0';
                        vcpm = 0;
                    }
                    break;
                }
                case '\n':
                {
                    pTempValHolder[vcpm] = '\0';
                    vcpm = 0;
                    break;
                }
                case '\"':
                {
                    if(!QuotationOnOff) 
                    {
                        QuotationOnOff = 1;
                        pTempValHolder[vcpm] = c;
                        vcpm++;
                    }
                    else 
                    {
                        QuotationOnOff = 0;
                        pTempValHolder[vcpm] = c;
                        vcpm++;
                    }
                    break;
                }
                case L'�d':
                {
                    // �d�L��� => not a trading day.
                    c = fgetwc(pf);
                    if( L'�L' == c )
                    {
                        DEBUG_OUTPUT("%s not a trading day\n", csvFile);
                        fclose(pf);
                        return 0;
                    }
                    break;
                }
#if 0 // parser example
                case L'�j':
                {
                    DEBUG_OUTPUT("get Keyword\n");
                    //�j�L�έp��T => a trading day
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
                    if( 0 ==  wcscmp(L"�j�L�έp��T", bufWC))
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
#if 1 // ��METF, ��'='�}�l���r��
                case '=':
                {
                    DEBUG_OUTPUT("get ETF, try to get whole stock line by line\n");
                    //�j�L�έp��T => a trading day
                    memset(bufWC, 0x0, sizeof(bufWC));

                    // It should be the first ETF 
                    // parse all stocks to last one
                    while(fgetws(bufWC, sizeof(bufWC), pf) != NULL)
                    {
                        // if "�����G" appears, it means no more data, so stop read file.
                        if(!wcsncmp(L"\"�����G\"", bufWC, (int)wcslen(L"\"�����G\"")))
                        {
                            DEBUG_OUTPUT("No more data, stop read file\n");
                            fclose(pf);
                            return 0;
                        }
                        // pass to ETF_wcs_handler and return a struct store specified ETF's data.
                        if((pstockData = data_pasrer(bufWC) )== NULL)
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
                    pTempValHolder[vcpm] = c;
                    vcpm++;
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


