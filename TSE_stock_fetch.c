#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <errno.h>
#include <sys/stat.h>

#define EXECNAME

#define DEBUG_OUTPUT( formats, args...) do{ \
                fprintf(stderr, formats, ##args); \
            }while(0);

#define PATH_LEN 512
#define URL_LEN 4096
#define FILENAME_LEN 256


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

typedef enum{
    NOT_ENOUGH,
    NOT_MATCH,
    FOLDER_NOT_MOUNT,
    FILE_NOT_FOUND,
    ROC_YEAR_NOT_SUPPORT,
    ARGU_NOT_SUPPORT,
}errList;

char *errString[] = 
{
    ""
    "Arguemnt not enough",
    "Arguemnt not match",
    "Folder has not exist",
    "File has not exist",
    "ROC year not support",
    "Specified stock not support now",
};

// 下載的資料天數
static int dayBack=1;
//  目前下載的天數
static int currentDayBack; 
// 指定的股號
int numStock;
//////////////////////////////////////////////////////////////////////////
// Declaration
void usage();
void output_err(unsigned int err)
{
    switch(err)
    {
        case NOT_ENOUGH: 
            DEBUG_OUTPUT("Argument error : %s\n", errString[NOT_ENOUGH]);
            break;
        case NOT_MATCH: 
            DEBUG_OUTPUT("Argument error : %s \n", errString[NOT_MATCH]);
            break;
        case FOLDER_NOT_MOUNT:
            DEBUG_OUTPUT("File operation error : %s => %s\n", errString[FOLDER_NOT_MOUNT], strerror(errno));
            break;
        case FILE_NOT_FOUND:
            DEBUG_OUTPUT("File operation error : %s => %s\n", errString[FILE_NOT_FOUND], strerror(errno));
            break;
        case ROC_YEAR_NOT_SUPPORT:
            DEBUG_OUTPUT("Arguemnt error : %s\n", errString[ROC_YEAR_NOT_SUPPORT]);
            break;
        case ARGU_NOT_SUPPORT:
            DEBUG_OUTPUT("Argument error : %s\n", errString[ARGU_NOT_SUPPORT]);
            break;
        default:            
            DEBUG_OUTPUT("General error : %d\n", err);
            break;
    }
}
size_t write_data(void* ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written = fwrite (ptr, size, nmemb, stream);
    return written ; 
}

int main(int argc, char **argv)
{
    // output compiled date and time 
    DEBUG_OUTPUT("Compiled date : \t[%s  %s] \n", __DATE__, __TIME__);

    time_t t;
    int i=0;
    int ret=0;
    char *ptr = NULL;
    struct tm  time_now;
    struct tm  time_ROC;
    struct stat st;
    char yearROCFormat[4];

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
        case  4:
        {
            numStock = atoi(argv[3]);
            DEBUG_OUTPUT("Fetch target: \t[%d]\n", numStock);
            output_err(ARGU_NOT_SUPPORT);
            exit(0);
            break;
        }
        default:
        {
            output_err(NOT_ENOUGH);
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
        DEBUG_OUTPUT("Folder doesn't exist\n");
        output_err(FOLDER_NOT_MOUNT);
    // 不嘗試產生資料夾，可能沒有掛好
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
        exit(0);
    }

    DEBUG_OUTPUT("-------------------------------Begin-------------------------------\n");
    
    //從當天開始倒數
    for(i = 0; i< dayBack; i++)
    {
        DEBUG_OUTPUT("Try download each data until countdown over \n");
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
        if(stat(outputFile, &st) < 0) // 檔案不存在才抓
        {
            DEBUG_OUTPUT("spell urll\n");
            memset(fetchURL, 0x0, sizeof(outputFile));
            sprintf(fetchURL , "%s%03d%%2F%02d%%2F%02d%s", 
                            strURL3A,
                            time_ROC.tm_year,
                            time_ROC.tm_mon,
                            time_ROC.tm_mday,
                            strURL3B);

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
                            default:
                                DEBUG_OUTPUT("rror while fetching '%s' : err(%d), %s => %s\n", fetchURL, res, curl_easy_strerror(res), cmd);
                                break;
                        }
                    }

                    /* always cleanup */
                    curl_easy_cleanup(curl);
                    fclose(fp);
                }
            }
        }
        else 
            DEBUG_OUTPUT("File exists\n");

        // check the size  of file, small than 400 bytes should be not a trading day.
        DEBUG_OUTPUT("Check file %s's size\n", outputFile);
        ret =0;
        if(0 == (ret = stat(outputFile, &st) ))
        {
                DEBUG_OUTPUT("File size: %d\n", (int)st.st_size);
                if(400 > st.st_size) 
                {
                    DEBUG_OUTPUT("Date : %03d%02d%02d not a trading day, ignore it\n", time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday);
                    // ignore this day 
                    i--;
                }
        }
        else 
        {
            output_err(FILE_NOT_FOUND);
        }

        DEBUG_OUTPUT("-------------------------------Next one-------------------------------\n");
        // decreasing date
        // NOTE: satauday may be a trading day. 
        if((time_ROC.tm_mday--)<=0)
        {
            time_ROC.tm_mday = 31;
            if((time_ROC.tm_mon --)<=0)
            {
                time_ROC.tm_year--; 
                time_ROC.tm_mon =12;
            }
        }
        DEBUG_OUTPUT("Next date : %03d%02d%02d\n", time_ROC.tm_year, time_ROC.tm_mon, time_ROC.tm_mday);

        continue;
        // 存在，下載次一日的
    }

    //日期檢查    
    //每日收盤行情
              //string('證券代號,證券名稱,成交股數,成交筆數,成交金額,開盤價,最高價,最低價,收盤價,漲跌(+/-),漲跌價差,最後揭示買價,最後揭示買量,最後揭示賣價,最後揭示賣量,本益比'+'\n')
}


void usage()
{
    printf("%s <amount of day to download> <output folder> <specified stock number> \n", __FILE__);
}


