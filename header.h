
#include <errno.h>
#define FILE_TRADING_DAY    "TradingDay" 
#define FILE_HOLIDAY        "Holiday"
#define TSE_DATE_CLASSIFIC  "TSE.db3"
#define DEBUG_OUTPUT( formats, args...) do{ \
                fprintf(stderr, formats, ##args); \
            }while(0);

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
    DB_CREATE_FAIL,
    DB_INSERT_FAIL,
    DB_QUERY_FAIL,
    LC_SET_FAIL,
    DATA_FORMAT_ERROR,
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
        default:
            DEBUG_OUTPUT("General error : %d\n", err);
            break;
    }
}
