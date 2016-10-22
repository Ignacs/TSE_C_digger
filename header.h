
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
    ROC_YEAR_NOT_SUPPORT,
    ARGU_NOT_SUPPORT,
    DB_NOT_FOUND,
    DB_CREATE_FAIL,
    DB_INSERT_FAIL,
    DB_QUERY_FAIL,
}errList;

char *errString[] = 
{
    "Null string",
    "Arguemnt not enough",
    "Arguemnt not match",
    "Folder has not exist",
    "File has not exist",
    "ROC year not support",
    "Specified stock not support now",
    "Database not found",
    "Database table create fail",
    "Database table insert fail",
    "Database table query fail",
};
void output_err(unsigned int err)
{
    switch(err)
    {
        case ARG_NOT_ENOUGH: 
            DEBUG_OUTPUT("Argument error : %s\n", errString[ARG_NOT_ENOUGH]);
            break;
        case ARG_NOT_MATCH: 
            DEBUG_OUTPUT("Argument error : %s \n", errString[ARG_NOT_MATCH]);
            break;
        case FOLDER_NOT_MOUNT:
            DEBUG_OUTPUT("File operation error : %s => %s\n", errString[FOLDER_NOT_MOUNT], strerror(errno) );
            break;
        case FILE_NOT_FOUND:
            DEBUG_OUTPUT("File operation error : %s => %s\n", errString[FILE_NOT_FOUND], strerror(errno) );
            break;
        case ROC_YEAR_NOT_SUPPORT:
            DEBUG_OUTPUT("Arguemnt error : %s\n", errString[ROC_YEAR_NOT_SUPPORT]);
            break;
        case ARGU_NOT_SUPPORT:
            DEBUG_OUTPUT("Argument error : %s\n", errString[ARGU_NOT_SUPPORT]);
            break;
        case DB_NOT_FOUND:
            DEBUG_OUTPUT("Database error : %s\n", errString[DB_NOT_FOUND]);
            break;
        case DB_CREATE_FAIL:
            DEBUG_OUTPUT("Database error : %s\n", errString[DB_CREATE_FAIL]);
            break;
        case DB_INSERT_FAIL:
            DEBUG_OUTPUT("Database error : %s\n", errString[DB_INSERT_FAIL]);
            break;
        case DB_QUERY_FAIL:
            DEBUG_OUTPUT("Database error : %s\n", errString[DB_QUERY_FAIL]);
            break;
        default:
            DEBUG_OUTPUT("General error : %d\n", err);
            break;
    }
}
