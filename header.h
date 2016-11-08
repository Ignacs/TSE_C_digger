
#include <errno.h>
#include <string.h>
#include <wchar.h>
#include <sqlite3.h>

#define BUFF_LEN        4096
#define LEN_DAYS_ORD    256


#define FILE_TRADING_DAY    "TradingDay"
#define FILE_HOLIDAY        "Holiday"
#define TSE_DATE_CLASSIFIC  "TSE.db3"

#define DEBUG_OUTPUT( formats, args...) do{ \
                fprintf(stderr, "%s:%d > ", __FUNCTION__, __LINE__); \
                fprintf(stderr, formats, ##args); \
            }while(0)

#define READONLY    SQLITE_OPEN_READONLY
#define READWRITE   SQLITE_OPEN_READWRITE

#define SIGN_UP     '+'
#define SIGN_DOWN   '-'

typedef struct stock_daily {
//  line data sequence: date      stockNum        tradeNum        volume      open        high        low     close       sign        diff        buy     buyVol      sell        sellVol     epr
    unsigned long int date;
    unsigned long int stockNum;
    unsigned long int tradeNum;
    unsigned long int volume;
    double open;
    double high;
    double low;
    double close;
    char sign;
    double diff;
    double buy;
    unsigned long int buyVol;
    double sell;
    unsigned long int sellVol;
    double epr;
}*pStockHistory;

typedef enum{
    ARG_NOT_ENOUGH,
    ARG_NOT_MATCH,
    FOLDER_NOT_MOUNT,
    FILE_NOT_FOUND,
    FILE_OPEN_FAIL,
    FILE_READ_FAIL,
    ROC_YEAR_NOT_SUPPORT,
    ARGU_NOT_SUPPORT,
    DB_NOT_FOUND,
    DB_OPEN_FAIL,
    DB_CREATE_FAIL,
    DB_INSERT_FAIL,
    DB_QUERY_FAIL,
    LC_SET_FAIL,
    DATA_FORMAT_ERROR,
    STRING_NULL,
}errList;

void output_err(unsigned int err)
{
    switch(err)
    {
        case ARG_NOT_ENOUGH:
            DEBUG_OUTPUT("Argument error : Arguemnt not enough\n");
            break;
        case ARG_NOT_MATCH:
            DEBUG_OUTPUT("Argument error : Arguemnt not match \n" );
            break;
        case FOLDER_NOT_MOUNT:
            DEBUG_OUTPUT("File operation error : Folder has not exist => %s\n",  strerror(errno) );
            break;
        case FILE_NOT_FOUND:
            DEBUG_OUTPUT("File operation error : File has not exist => %s\n",  strerror(errno) );
            break;
        case FILE_OPEN_FAIL:
            DEBUG_OUTPUT("File operation error : File open fail=> %s\n",  strerror(errno) );
            break;
        case FILE_READ_FAIL:
            DEBUG_OUTPUT("File operation error : File read fail=> %s\n",  strerror(errno) );
            break;
        case ROC_YEAR_NOT_SUPPORT:
            DEBUG_OUTPUT("Arguemnt error : ROC year not support\n");
            break;
        case ARGU_NOT_SUPPORT:
            DEBUG_OUTPUT("Argument error : Specified stock not support now\n");
            break;
        case DB_NOT_FOUND:
            DEBUG_OUTPUT("Database error : Database not found\n");
            break;
        case DB_OPEN_FAIL:
            DEBUG_OUTPUT("Database error : Database open fail \n");
            break;
        case DB_CREATE_FAIL:
            DEBUG_OUTPUT("Database error : Database table create fail\n");
            break;
        case DB_INSERT_FAIL:
            DEBUG_OUTPUT("Database error : Database table insert fail\n");
            break;
        case DB_QUERY_FAIL:
            DEBUG_OUTPUT("Database error : Database table query fail\n");
            break;

        case LC_SET_FAIL:
            DEBUG_OUTPUT("Locale error : Set locale failed \n");
            break;

        case DATA_FORMAT_ERROR:
            DEBUG_OUTPUT("Parse error : Error data format \n");
            break;
        case STRING_NULL:
            DEBUG_OUTPUT("Parse error : \n");
            break;
        default:
            DEBUG_OUTPUT("General error : %d\n", err);
            break;
    }
}


int checkFileAbs( char *src, char *dest, unsigned int lenDest)
{
    if( 0 > lenDest )
        return -1;
    if( NULL == src)
        return -1;
    if( NULL == dest )
        return -1;

    if('/'!= *(src) && '~' != *(src))
    {
        memset(dest, 0x0, lenDest);
        sprintf(dest , "./%s", src);
    }
    else
        sprintf(dest , "%s", src);
}

unsigned int wcsConvToU_10b(wchar_t *wcs)
{
    wchar_t *ptr = NULL ;
    int i = 0;
    char buf[BUFF_LEN];

    if( NULL == wcs)
        return 0;

    ptr = wcs;
    memset(buf, 0x0, sizeof(buf));
    for(i = 0; 1; i++)
    {
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
                return strtol( buf, NULL, 10);
                break;
        }
    }

    return 0;
}

//
static int openDB(char *pathDB, int rw, sqlite3 **db)
{
    int ret ;

    if(NULL == pathDB)
    {
        DEBUG_OUTPUT("Database not exist = %s\n", pathDB );
        return -1;
    }

#if 0
    DEBUG_OUTPUT("db pointer address = %p\n", &db );
    DEBUG_OUTPUT("db address = %p\n", db );
#endif
    if(READONLY == rw)
        ret = sqlite3_open_v2(pathDB, db, SQLITE_OPEN_READONLY, NULL);
    else
        ret = sqlite3_open_v2(pathDB, db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    // increasing counter.
    if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
    {
        // countDBOpen ++;
        DEBUG_OUTPUT("open data successfull\n");
    }
    else
        DEBUG_OUTPUT("Cause: \terrno = %d\n", ret );
    return ret;
}

void close_db(sqlite3 *db, char **table)
{
    /* free */
    if(NULL != table)
    {
        DEBUG_OUTPUT("clear table \n" );
        sqlite3_free_table(table);
        table =NULL;
    }

    /* close database */
    if(NULL != db)
    {
        DEBUG_OUTPUT("clear database \n" );
        sqlite3_close(db);
        db = NULL;
    }
}
