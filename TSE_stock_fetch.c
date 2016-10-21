#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define EXECNAME

#define DEBUG_OUTPUT( formats, args...) do{ \
                fprintf(stderr, formats, ##args); \
            }while(0);

//////////////////////////////////////////////////////////////// 
// Target web site string  
char *strURL1A="http://www.twse.com.tw/ch/trading/exchange/MI_INDEX/genpage/Report";
char *strURL1B="ALLBUT0999_1.php?select2=ALLBUT0999&chk_date=";

char *strURL2A="http://www.twse.com.tw/ch/trading/exchange/MI_INDEX/MI_INDEX3_print.php?genpage=genpage/Report";
char *strURL2B="ALLBUT0999_1.php&type=csv";

typedef enum{
    NOT_ENOUGH,
    NOT_MATCH,
    FOLDER_NOT_MOUNT,
    FILE_NOT_MOUNT,
}errList;

char *errString[] = 
{
    ""
    "Arguemnt not enough",
    "Arguemnt not match",
    "Folder has not exist",
    "File has not exist",
};

// 下載的資料天數
static int dayBack=1;
//  目前下載的天數
static int intDayBack; 
// 指定的股號
int numStock;
//////////////////////////////////////////////////////////////////////////
// Declaration
void usage();
void output_err(unsigned int err);

int main(int argc, char **argv)
{
    // output compiled date and time 
    DEBUG_OUTPUT("Compiled date : \t[%s  %s] \n", __DATE__, __TIME__);

    time_t t;
    struct tm  time_now;

    char *ptr = NULL;

    // char outputPATH[(unsigned int)strlen(strURL1A) + (unsigned int)strlen(strURL2A)+256];
    char outputPATH[4096];

    // check arguments, 
    switch(argc)
    {
        case 3:
        {
            output_err(NOT_ENOUGH);
            usage();
            exit(0);
            break;
        }
        case  4:
        {
            numStock = atoi(argv[3]);
            DEBUG_OUTPUT("Fetch target: \t[%d]\n", numStock);
            break;
        }
        default:
        {
            break;
        }
    }

    dayBack = atoi(argv[1]);  // 下載資料日數[以今日起算]
    DEBUG_OUTPUT("Fetch days: \t[%d]\n", dayBack);

    ptr = strcpy((char*)outputPATH, (const char *)argv[2]);  // 檔案輸出目錄
    DEBUG_OUTPUT("Output folder: \t[%s]\n", outputPATH);

    // 取得系統時間
    t = time(NULL);
    time_now = *localtime(&t);
    DEBUG_OUTPUT("now is %d-%d-%d %d:%d:%d\n", time_now.tm_year+1900, time_now.tm_mon+1, time_now.tm_mday, time_now.tm_hour, time_now.tm_min, time_now.tm_sec);

    //檢查輸出目錄是否存在
    if(0>stat(outputPATH))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        output_err(FOLDER_NOT_MOUNT);
        exit(0);
    }



    //確認輸出目錄建立成功，才進行
    //檢查輸出目錄是否存在         
    // 不嘗試產生資料夾，可能沒有掛好
    //開始下載
    //日期檢查    
    //每日收盤行情
              //string('證券代號,證券名稱,成交股數,成交筆數,成交金額,開盤價,最高價,最低價,收盤價,漲跌(+/-),漲跌價差,最後揭示買價,最後揭示買量,最後揭示賣價,最後揭示賣量,本益比'+'\n')
}

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
            DEBUG_OUTPUT("File operation error : %s\n", errString[FOLDER_NOT_MOUNT]);
            break;
        default:            
            DEBUG_OUTPUT("General error : \n");
            break;
    }
}

void usage()
{
    printf("%s <amount of day to download> <output folder> <specified stock number> \n", __FILE__);
}
