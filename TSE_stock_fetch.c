#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include "header.h"

#define EXECNAME

// debug flag
#define SQL_DBG 1

#define PATH_LEN 512
#define URL_LEN 4096
#define FILENAME_LEN 256

#define ABNORMAL_FILE_SIZE      32
#define FILESIZE_TRADEDAY       400

// database command
#define CMD_CREATE_TABLE    "CREATE TABLE IF NOT EXISTS TSETradeDay( date int PRIMARY KEY NOT NULL UNIQUE, tradingDay bit);"
// UPDATE TSETradeDay set tradingDay = 0 where date=1051109;
#define CMD_UPDATE_TABLE    "UPDATE TSETradeDay set tradingDay ="
#define CMD_DATE            " where date="

#define CMD_INSERT_TABLE    "INSERT INTO TSETradeDay VALUES"
#define CMD_QUERY_TABLE     "SELECT * FROM TSETradeDay;"

////////////////////////////////////////////////////////////////
// Target web site string
// older and not workable URL
// char *strURL1A="http://www.twse.com.tw/ch/trading/exchange/MI_INDEX/genpage/Report";
// char *strURL1B="ALLBUT0999_1.php?select2=ALLBUT0999&chk_date=";

// char *strURL2A="http://www.twse.com.tw/ch/trading/exchange/MI_INDEX/MI_INDEX3_print.php?genpage=genpage/Report";
// char *strURL2B="ALLBUT0999_1.php&type=csv";


//"http://www.twse.com.tw/ch/trading/exchange/MI_INDEX/MI_INDEX.php?download=csv&qdate=105%2F10%2F21&selectType=ALLBUT0999
// 105%2F10%2F21
char *strURL3A="http://www.twse.com.tw/ch/trading/exchange/MI_INDEX/MI_INDEX.php?download=csv&qdate=";
char *strURL3B="&selectType=ALLBUT0999";



// 下載的資料天數
static int dayBack=1;
//  目前下載的天數
static int currentDayBack;
// 指定的股號
int numStock;

// reference to database
sqlite3 *db = NULL;
// reference to table
char **table = NULL;
//////////////////////////////////////////////////////////////////////////
// Declaration
void usage();

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite (ptr, size, nmemb, stream);
    return written ;
}

void closeDB()
{
    /* free */
    if(NULL != table)
        sqlite3_free_table(table);

    /* close database */
    sqlite3_close(db);
}

int main(int argc, char **argv)
{
    // output compiled date and time
    DEBUG_OUTPUT("Compiled date : \t[%s  %s] \n", __DATE__, __TIME__);

    int rows=0, cols=0;
    char *errMsg = NULL;

    time_t t;
    int i=0, j=0;
    int ret=0;
    char *ptr = NULL;
    struct tm  time_now;
    struct tm  time_ROC;
    struct stat st;
    char yearROCFormat[4];
#define INVAILD 0
#define VAILD 1
    int redownload = INVAILD;

    // char outputPATH[(unsigned int)strlen(strURL1A) + (unsigned int)strlen(strURL2A)+256];
    char outputPath[PATH_LEN];
    char fetchURL[URL_LEN];
    char outputFile[FILENAME_LEN];
    char cmd[32+URL_LEN+PATH_LEN];

    FILE *fp=NULL;
    CURL *curl=NULL;
    CURLcode res;

    // check arguments,
    switch(argc)
    {
        case 3:
        {
            break;
        }
#if 0 // not supported
        case  4:
        {
            numStock = atoi(argv[3]);
            DEBUG_OUTPUT("Fetch target: \t[%d]\n", numStock);
            output_err(ARGU_NOT_SUPPORT);
            exit(0);
            break;
        }
#endif
        default:
        {
            output_err(ARG_NOT_ENOUGH);
            usage();
            exit(0);
            break;
        }
    }

    dayBack = atoi(argv[1]);  // 下載資料日數[以今日起算]
    DEBUG_OUTPUT("Fetch days: \t[%d]\n", dayBack);

    ptr = strcpy((char*)outputPath, (const char *)argv[2]);  // 檔案輸出目錄
    DEBUG_OUTPUT("Output folder: \t[%s]\n", outputPath);

    // 取得系統時間
    t = time(NULL);
    time_now = *localtime(&t);
    DEBUG_OUTPUT("now is %d-%d-%d %d:%d:%d\n", time_now.tm_year+1900, time_now.tm_mon+1, time_now.tm_mday, time_now.tm_hour, time_now.tm_min, time_now.tm_sec);

    //檢查輸出目錄是否存在
    if(!(0 == stat(outputPath, &st) && S_ISDIR(st.st_mode)))
    {
        output_err(FOLDER_NOT_MOUNT);
        DEBUG_OUTPUT("%s\n", strerror(errno));
        // 不嘗試產生資料夾，可能沒有掛好partition
        exit(0);
    }

    memset(cmd, 0x0, sizeof(cmd));
    sprintf(cmd, "%s/%s", outputPath, TSE_DATE_CLASSIFIC);

    /* open sqlite3 database */
    if (sqlite3_open_v2(cmd, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
    {
        output_err(DB_NOT_FOUND);
        closeDB();
        exit(0);
    }

    /* create table */
    ret = sqlite3_exec(db, CMD_CREATE_TABLE, 0, 0, &errMsg);
    if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
    {
        output_err(DB_CREATE_FAIL);
        DEBUG_OUTPUT("Cause: \t[%s]\n", errMsg);
        closeDB();
        exit(0);
    }

    time_ROC.tm_year = time_now.tm_year-11;
    time_ROC.tm_mon = time_now.tm_mon+1;
    time_ROC.tm_mday = time_now.tm_mday;
    // Dont support before ROC  100
    if( time_ROC.tm_year < 100 )
    {
        DEBUG_OUTPUT("ROC year 100 not support (%d)\n", time_ROC.tm_year);
        output_err(ROC_YEAR_NOT_SUPPORT);
        closeDB();
        exit(0);
    }

    DEBUG_OUTPUT("-------------------------------Begin-------------------------------\n");

    //從當天開始倒數
    for(i = 0; i< dayBack; i++)
    {
        redownload = INVAILD;
        DEBUG_OUTPUT("[%d]th\n", i+1);
        //其中下載的url格式牽扯到時間，尤其是民國的格式

        //TODO:西元2000年前後的格式不一樣，目前實作到2000之後的，不包含2000年
        //      剩下的可以看python 版本的：TSE_CLOSE.py

        // 其中民國100年前格式為XX
        // 100之後為XXX
        // 也許1000年之後是XXXX
        if( time_ROC.tm_year>100 )
        {
            // mark 2016/10/22
            // memset(yearROCFormat, 0x0, sizeof(outputFile));
            // sprintf(yearROCFormat  ,"%d", time_ROC.tm_year);
        }
        else
        {
            DEBUG_OUTPUT("ROC year not support\n");
            output_err(ROC_YEAR_NOT_SUPPORT);
            break;
        }

        // 下載每日收盤行情
        // 先拼出下載的檔案名稱 民國
        memset(outputFile, 0x0, sizeof(outputFile));
        sprintf(outputFile, "%s/%03d%02d%02d.csv", outputPath, time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday);
        DEBUG_OUTPUT("output file %s\n", outputFile);
        if(stat(outputFile, &st) < 0 || (ABNORMAL_FILE_SIZE > (int)st.st_size)) // 檔案不存在才抓 or file's size is abnormal
        {
            // check the size of file, download again if too small.
            DEBUG_OUTPUT("Check file size: %d\n", (int)st.st_size);
            if(ABNORMAL_FILE_SIZE > (int)st.st_size)
                redownload = VAILD;

            DEBUG_OUTPUT("spell urll\n");
            memset(fetchURL, 0x0, sizeof(outputFile));
            sprintf(fetchURL , "%s%03d%%2F%02d%%2F%02d%s",
                            strURL3A,
                            time_ROC.tm_year,
                            time_ROC.tm_mon,
                            time_ROC.tm_mday,
                            strURL3B);
            // download this day's data.
            ret = 1;
        }
        else
        {
            DEBUG_OUTPUT("File exists\n");
        }

        // download if necessary.
        if( 1 == ret )
        {
            DEBUG_OUTPUT("Download url [%s]\n", fetchURL);
            // sprintf(cmd, "curl -O %s -o %s", outputFile
            //
            // init curl
            curl = curl_easy_init();
            if(curl)
            {
                DEBUG_OUTPUT("Curl init succ\n");
                fp = fopen (outputFile, "w+");
                if(NULL != fp)
                {
                    DEBUG_OUTPUT("File open succ\n");
                    curl_easy_setopt(curl, CURLOPT_URL, fetchURL);
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, cmd);
                    res = curl_easy_perform(curl);
                    if(CURLE_OK != res )
                    {
                        switch(res)
                        {
                            case CURLE_COULDNT_RESOLVE_HOST :
                            case CURLE_COULDNT_CONNECT :
                                DEBUG_OUTPUT("Error while fetching '%s' : err(%d), %s => %s\n", fetchURL, res, curl_easy_strerror(res), cmd);
                                // stop  to download.
                                i = dayBack ;
                                break;
                            default:
                                DEBUG_OUTPUT("Error while fetching '%s' : err(%d), %s => %s\n", fetchURL, res, curl_easy_strerror(res), cmd);
                                break;
                        }
                    }

                    /* always cleanup */
                    curl_easy_cleanup(curl);
                    fclose(fp);
                }
            }
        }

        // check the size  of file, small than 400 bytes should be not a trading day.
        DEBUG_OUTPUT("Check file %s's size\n", outputFile);
        ret =0;
        if(0 == (ret = stat(outputFile, &st) ))
        {
            DEBUG_OUTPUT("File size: %d\n", (int)st.st_size);
            memset(cmd, 0x0, sizeof(cmd));
            // This day not a trading day.
            if( FILESIZE_TRADEDAY > st.st_size)
            {
                DEBUG_OUTPUT("Date : %03d%02d%02d not a trading day, ignore it\n", time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday);
                // ignore this day
                i--;
                // reocde holiday
                // sprintf(cmd , "echo %03d%02d%02d>> %s/%s", time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday, outputPath,FILE_HOLIDAY);
                // output to sqlite db
                sprintf(cmd, "%s( %03d%02d%02d , 0 )", CMD_INSERT_TABLE, time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday );
            }
            else
            {
                // reocde trading day.
                // sprintf(cmd , "echo %03d%02d%02d>> %s/%s", time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday, outputPath, FILE_TRADING_DAY);
                // output to sqlite db
                sprintf(cmd, "%s( %03d%02d%02d , 1 )", CMD_INSERT_TABLE, time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday );
            }

#if SQL_DBG
            DEBUG_OUTPUT("Execute cmd [%s]\n", cmd);
#endif //SQL_DBG

            // system(cmd);
            // insert to Database
            ret = sqlite3_exec(db, cmd, NULL, NULL, &errMsg);
            if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
            {
                output_err(DB_INSERT_FAIL);
                DEBUG_OUTPUT("Cause: \t%d :[%s]\n", ret, errMsg);
                //19 :[UNIQUE constraint failed: TSETradeDay.date]↵ and the record is from redownload.
                if(19 == ret && redownload)
                {
                    memset(cmd, 0x0, sizeof(cmd));
                    if( FILESIZE_TRADEDAY < st.st_size)
                    {
                        sprintf(cmd, "%s 1 %s%03d%02d%02d;", CMD_UPDATE_TABLE, CMD_DATE, time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday );
                    }
                    else
                    {
                        sprintf(cmd, "%s 0 %s%03d%02d%02d;", CMD_UPDATE_TABLE, CMD_DATE, time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday );
                    }
#if SQL_DBG
                    DEBUG_OUTPUT("Execute cmd [%s]\n", cmd);
#endif //SQL_DBG
                    ret = sqlite3_exec(db, cmd, NULL, NULL, &errMsg);
                    if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
                    {
                        DEBUG_OUTPUT("Cause: \t%d :[%s]\n", ret, errMsg);
                    }
                }
            }
        }
        else
        {
            output_err(FILE_NOT_FOUND);
            DEBUG_OUTPUT("%s\n", strerror(errno));
        }

        DEBUG_OUTPUT("-------------------------------Next one-------------------------------\n");
        // decreasing date
        // NOTE: satauday may be a trading day.
        time_ROC.tm_mday--;
        if(0 >= time_ROC.tm_mday)
        {
            time_ROC.tm_mday = 31;
            time_ROC.tm_mon --;
            DEBUG_OUTPUT("Previous month\n");
            if(0 >= time_ROC.tm_mon)
            {
                time_ROC.tm_year--;
                time_ROC.tm_mon =12;
                DEBUG_OUTPUT("Previous year\n");
            }
        }
        DEBUG_OUTPUT("Next date : %03d%02d%02d\n", time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday);

        continue;
        // 存在，下載次一日的
    }

    //每日收盤行情
    //string('證券代號,證券名稱,成交股數,成交筆數,成交金額,開盤價,最高價,最低價,收盤價,漲跌(+/-),漲跌價差,最後揭示買價,最後揭示買量,最後揭示賣價,最後揭示賣量,本益比'+'\n')
    //

#if SQL_DBG // query Database
    ret = sqlite3_get_table(db , CMD_QUERY_TABLE, &table, &rows, &cols, &errMsg);
    if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
    {
        output_err(DB_QUERY_FAIL);
        DEBUG_OUTPUT("Cause: \t[%s]\n", errMsg);
    }
    else
    {
        for (i=0;i<rows;i++)
        {
            for (j=0;j<cols;j++)
                 DEBUG_OUTPUT("%s\t", table[i*cols+j]);
            fprintf(stderr, "\n");
        }
    }
#endif

    closeDB();
}

void usage()
{
    printf("%s <amount of day to download> <output folder>  \n", __FILE__);
}
