#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>

#include "header.h"

#define ARGU_NUM        7
#define CMD_LEN         1024
#define PATH_LEN        1024
#define FILENAME_LEN    256
#define BUF_LEN         4096
#define STOCK_ELE       14

#define WRITE_SIDE      1
#define READ_SIDE       0

#define INDICATOR                   "indicator"
#define SQLCMD_GOT_INDICATOR        "get_indicator_data" // the function to decide SQL command to get specified data for indicator .
#define SQLCMD_MAKE_TABLE           "create_indicator"
// EX: select * from DailyData order by date desc limit 12
#define CMD_QUERY_STOCK     "SELECT * FROM DailyData ORDER by date desc;"
// #define CMD_QUERY_STOCK     "SELECT * FROM sqlite_master;"
#define END_SYMBOL      "<"

#define DBG             1
#if DBG
    char largeBuf[BUF_LEN][BUF_LEN];
#endif


// open stock database and calculate all days's indicator , then store to stock database
// indicator's parameter should be stable or all stock need re-calculate again. It is terrible...

void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s -d <Folder contains stock database>\n\t\t-f <file recode stock to calculate>\n\t\t-i <indicator library path>\n" , app_name);
}

static void *fHandle = NULL;

static int (*indicator)() = NULL;
static char* (*cmdSQL_get_indicator)() = NULL;
static char* (*cmdSQL_create_indicator)() = NULL;

static char **ptable = NULL;
static char **indicList = NULL;
static char **pStorckStrData= NULL;
static sqlite3 *db = NULL;
FILE    *inputFp = NULL;
unsigned int memCount = 0;
// int pipefd[2] ;
struct stock_daily *pStockDigitData = NULL;


void finalize(const char *func, int line)
{
    DEBUG_OUTPUT( "Free via %s, %d\n", func, line);
    printf( "Free via %s, %d\n", func, line);
    if(NULL != fHandle)
    {
        DEBUG_OUTPUT( "clear indicator pointer \n");
        dlclose(fHandle);
        fHandle = NULL;

        // pointer func to NULL;
        indicator = NULL;
        cmdSQL_get_indicator = NULL;
        cmdSQL_create_indicator = NULL;
    }


    if(NULL != inputFp)
    {
        DEBUG_OUTPUT( "clear input file pointer \n");
        fclose(inputFp);
        inputFp = NULL;
    }

    if(NULL != pStorckStrData)
    {
#if !DBG
        free(pStorckStrData);
#endif
        pStorckStrData = NULL;
        memCount -- ;
    }

    if(NULL != pStockDigitData)
    {
#if !DBG
        free(pStockDigitData);
#endif
        pStockDigitData = NULL;
        memCount -- ;
    }

    // if there is still memory that not be free before exit.
    if( memCount > 0)
    {
        DEBUG_OUTPUT( "clear input file pointer \n");
    }

    close_db(db, ptable, indicList);

}
// return 1 : success
// return -1 : faile
int open_shared_lib(char *libPath)
{
    char buf[BUF_LEN];
DEBUG_OUTPUT( "Open shared library :%s\n", libPath);

    // check path if absolute.
    checkFileAbs(libPath, (char *)buf, BUF_LEN);
DEBUG_OUTPUT( "Open shared library :%s\n", buf);

    fHandle = dlopen((const char*)buf, RTLD_LAZY);
    //fHandle = dlopen("./libVR.so", RTLD_LAZY);
    if(NULL == fHandle )
    {
        DEBUG_OUTPUT( "Open shared library failed = %s\n", dlerror());
        return -1;
    }

    dlerror(); // clear all error case
    *(void **) (& cmdSQL_create_indicator) = dlsym(fHandle, SQLCMD_MAKE_TABLE); // load API from shared library
    if(NULL == cmdSQL_create_indicator)
    {
        DEBUG_OUTPUT( "Get indicator data create command  failed = %s\n", dlerror());
        dlclose(fHandle);
        return -1;
    }
    *(void **) (& cmdSQL_get_indicator )= dlsym(fHandle, SQLCMD_GOT_INDICATOR); // load API from shared library
    if(NULL == cmdSQL_get_indicator)
    {
        DEBUG_OUTPUT( "Get indicaotr data get command failed = %s\n", dlerror());
        dlclose(fHandle);
        return -1;
    }

    *(void **) ( &indicator ) = dlsym(fHandle, INDICATOR); // load API from shared library
    if(NULL == indicator)
    {
        DEBUG_OUTPUT( "Get indicator failed = %s\n", dlerror());
        dlclose(fHandle);
        return -1;
    }
    return 1;
}

char **getMemSpace( unsigned int memBlks, unsigned int memSize)
{
    char **buf = NULL;

#if DBG
    buf = (char **)&largeBuf;
    DEBUG_OUTPUT( "&largeBuf addr = %p \n", &largeBuf );
#else
//    if((buf = (char **)calloc( )) != NULL)
#endif
    {
        memCount++;
    }
    return buf;
}

static void signal_handler(int signo)
{
    /* signo contains the signal number that was received */
    switch( signo )
    {
        /* Signal is a SIGUSR1 */
        case SIGUSR1:
        {
            DEBUG_OUTPUT( "Process %d: received SIGUSR1 \n", getpid() );
            finalize(__FUNCTION__, __LINE__);
            // close(pipefd[WRITE_SIDE]);
            exit(EXIT_FAILURE);
            break;
        }

        /*  It's a SIGUSR2 */
#if 0
        case SIGUSR2:
        printf( "Process %d: received SIGUSR2 \n", getpid() );
        if(pid1==getpid())
        {
            printf( "Process %d is passing SIGUSR2 to %d...\n", getpid(),pid2 );
            kill( pid2, SIGUSR2 );
        }
        else /* it is the child */
        {
            printf( "Process %d will terminate itself using SIGINT\n", getpid());
            kill(getpid(), SIGINT);
        }
        break;
#endif

        default:
            break;
    }

    return;
}

// get stock data in char **format
// return 1 : (rows-1)  // decrease the row of title
// return <1: fail to get dbata
int gen_stock_indicaotr(char *stockID, char *folderPath,  char* pathDynamicLib )
{
    char *delim = ",";
    pid_t pid,w;
    // int cntFork = 0;
    int status = 0;
    int ret = 0;
    char *errMsg = NULL;
    int i = 0, j = 0, k = 0;
    char buf[BUF_LEN] ;
    char dbName[BUF_LEN] ;
    char *ptr=NULL, *saveptr;
    int len = 0;
    char **pCurr = NULL;
    int rows = 0, cols = 0, size = 0;

DEBUG_OUTPUT(" folderPath = %s, stock = %s\n ", folderPath, stockID);
    memset(dbName, 0x0, BUF_LEN);
    sprintf( dbName,"%s/%s.sl3", folderPath, stockID );
DEBUG_OUTPUT(" \n ");

    // Warning : child process
    if( 0 == (pid = fork()) )
    {
DEBUG_OUTPUT( "=================== child start : open database : %s ==================================\n", dbName);
        // open indicator
        if(0 < open_shared_lib( pathDynamicLib ))
        {
            // open stock databasee
            ret = openDB(dbName, READWRITE, &db);

            // open successfully
            if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
            {
                // make sure table exist
                memset(buf, 0x0, BUF_LEN);
                sprintf(buf, "%s", (char *)(*cmdSQL_create_indicator)());
DEBUG_OUTPUT( "sql cmd[%s]\n", buf);
                /* create table if not exist */
                ret = sqlite3_exec(db, buf, 0, 0, &errMsg);
                if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
                {
                    output_err(DB_CREATE_FAIL);
                    DEBUG_OUTPUT("Cause: \t[%s]\n", errMsg);
                    ret = -1;
                }
                else
                {
                    // get indicator data
                    memset(buf, 0x0, BUF_LEN);
                    sprintf(buf, "%s", (*cmdSQL_get_indicator)());
DEBUG_OUTPUT( "sql cmd[%s]\n", buf);

                    ret = sqlite3_get_table(db , buf, &indicList, &rows, &cols, &errMsg);
                    if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
                    {
                        // but api will output 1 title line
                        // get pRows lines. and dont send the first line
                        // get all data instead of specified by indicator , because get mothod is too fast to discern
                        memset(buf, 0x0, BUF_LEN);
                        sprintf(buf, "%s", CMD_QUERY_STOCK);
DEBUG_OUTPUT( "sql cmd: %s\n", buf);

                        ret = sqlite3_get_table(db , buf, &ptable, &rows, &cols, &errMsg);
                        if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
                        {
                            // find the maximum element to set the size arguemnt
                            size = 0;
                            // the first line is title, ignopre it
                            for(i = 1 ; i< rows ; i++)
                            {
                                for(j = 0 ; j< cols; j++)
                                {
                                    if(size < strlen(ptable[i*cols+j]))
                                        size = strlen(ptable[i*cols+j]) + 1; // 1 for '\0'
                                }
                            }
DEBUG_OUTPUT("total %d rows , %d cols , size %d\n", rows, cols, size);

                            indicator(stockID, &indicList, &ptable, rows, cols, size);
                            // open specified stock's db
                        }
                        else
                        {
                            output_err(DB_QUERY_FAIL);
                            DEBUG_OUTPUT("Cause: \terrno = %d, [%s]\n", ret , errMsg);
                            ret = -1;
                        }
                    }
                    else
                    {
                        output_err(DB_QUERY_FAIL);
                        DEBUG_OUTPUT("Cause: \terrno = %d, [%s]\n", ret , errMsg);
                        ret = -1;
                    }
                }
            }
            else
            {
                output_err(DB_OPEN_FAIL);
                DEBUG_OUTPUT("Cause: \terrno = %d, [%s]\n", ret , errMsg);
                ret = -1;
            }
        }
        else
        {
            output_err(DYNLIB_OPEN_FAIL);
DEBUG_OUTPUT(" \n ");
        }

        DEBUG_OUTPUT("Child exit, free resource \n");
        /* recycle resource */
        finalize(__FUNCTION__, __LINE__);

        // child process exit.
        if(-1 < ret)
            return 0;
        else
            return -1;
    }
    else if(pid<0)
    {
        output_err(FORK_ERROR);
        return -1;
    }

    // parent
DEBUG_OUTPUT( "=================== parent continue:  ==================================\n");

    do
    {
        status = 0;
        w = wait(&status);

        if (w == -1)
        {
            DEBUG_OUTPUT( "no more child process \n");
            break;
        }
        else if (ret < 0) // any error occurs!!!
        {
            DEBUG_OUTPUT( "[parent] : kill process that get db = error %d \n", ret);
            kill(w, SIGUSR1);
        }

        if (WIFEXITED(status))
        {
            DEBUG_OUTPUT("[parent] : exited, status=%d\n", WEXITSTATUS(status));

            break;
        }
    } while (1);

    if(ret >= 1)
        return 1;
    else
        return 0;
}

// create memory space to store data after convtered
//  line data sequence: date      stockNum        tradeNum        volume      open        high        low     close       sign        diff        buy     buyVol      sell        sellVol     epr
#if 0
int convType(char **pStrData, unsigned int rows,unsigned int cols, unsigned int size, struct stock_daily** pRet)
{
    struct stock_daily* pResult = NULL;
    char *pDay =NULL;
#if !DBG
    struct stock_daily* pMem = NULL;
#else
    struct stock_daily pMem[BUF_LEN];
#endif
    int i = 0 , j = 0;
    if(NULL == pStrData || NULL==*pStrData)
    {
        DEBUG_OUTPUT( "NULL data\n");
        return -1;
    }
    if(1 > size)
    {
        DEBUG_OUTPUT( "no data to create\n");
        return -1;
    }

#if !DBG
    pMem = (struct stock_daily*)malloc(sizeof(struct stock_daily)*size);
DEBUG_OUTPUT("create memory\n" );
#endif

/*
for(i = 0 ; i < cols*size; i++)
{
pDay = (char *)(pStrData);
    fprintf(stderr, "[%s]",  pDay);
pDay = (char *)(pStrData+size);
    fprintf(stderr, "[%s]",  pDay);
}
fprintf(stderr, "\n");
*/

    if(pMem)
    {
        for(i = 0; i < rows; i++)
        {

            pResult = (pMem+i);
            pDay = (char*)(pStrData+(i*cols*size));
//                            strncpy( (char *)(pCurr+(i*cols*size)+j*size), (const char*)buf, k);
DEBUG_OUTPUT("-------------------------------------------\n" );
//DEBUG_OUTPUT("%d rows = %s \n", i , pDay);
            DEBUG_OUTPUT("%d rows =%s\n", i , (char *)(pStrData+(i*cols*size)));
            (pResult)->date = strtoul((char *)(pStrData+(i*cols*size)), NULL, 10);
DEBUG_OUTPUT("%p %lu\n", pResult ,(pResult)->date );

            (pResult)->stockNum= strtoul((const char*)(pStrData+size+(i*cols*size)), NULL, 10);
DEBUG_OUTPUT("%lu\n",(pResult)->stockNum);

            (pResult)->tradeNum= strtoul((const char*)(pStrData+(2*size)+(i*cols*size)), NULL, 10) ;
DEBUG_OUTPUT("%lu\n",(pResult)->tradeNum);

            (pResult)->volume=  strtoul((char *)(pStrData+(3*size)+(i*cols*size)), NULL, 10);
DEBUG_OUTPUT("%lu\n",(pResult)->volume);

            (pResult)->open=  strtof((const char*)(pStrData+(4*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->open);
            (pResult)->high=  strtof((const char*)(pStrData+(5*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->high*100/100.0);
            (pResult)->low=  strtof((const char*)(pStrData+(6*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->low);
            (pResult)->close=  strtof((const char*)(pStrData+(7*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->close);
            (pResult)->sign=  *(char *)(pStrData+(8*size)+(i*cols*size));
DEBUG_OUTPUT("%c\n",(pResult)->sign);
            (pResult)->diff= strtof((const char*)(pStrData+(9*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->diff);
            (pResult)->buy=  strtof((const char*)(pStrData+(10*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->buy);
            (pResult)->buyVol=  strtoul((const char*)(pStrData+(11*size)+(i*cols*size)), NULL, 10) ;
DEBUG_OUTPUT("%lu\n",(pResult)->buyVol);
            (pResult)->sell=  strtof((const char*)(pStrData+(12*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->sell);
            (pResult)->sellVol=  strtoul((const char*)(pStrData+(13*size)+(i*cols*size)), NULL, 10) ;
DEBUG_OUTPUT("%lu\n",(pResult)->sellVol);
            (pResult)->epr=  strtof((const char*)(pStrData+(14*size)+(i*cols*size)), NULL);
DEBUG_OUTPUT("%f\n",(pResult)->epr);
        }
    }
    else
    {
        DEBUG_OUTPUT( "Memory create failed\n");
        return -1;
    }
    *pRet = (struct stock_daily*)&pMem;
    return 0;
}
#endif // if 0


int main(int argc, char **argv)
{
    struct stat st;
    char linebuf[CMD_LEN];
    char folderPath[PATH_LEN];
    char inputFile[FILENAME_LEN];
    char outputFile[FILENAME_LEN];
    char pathDynamicLib[FILENAME_LEN];
    char *ptr = NULL;
    char opt = '\0';
    unsigned int days1, days2, days3;
    unsigned char c = 0x0;
    int i = 0;
    int day = 0;
    int ret = 0;
    unsigned int rows=0, cols=0, size=0;

    memset(pathDynamicLib, 0x0, FILENAME_LEN);
    memset(outputFile, 0x0, FILENAME_LEN);
    memset(inputFile, 0x0, FILENAME_LEN);
    memset(folderPath, 0x0, PATH_LEN);
    memCount = 0;

    if ( ARGU_NUM > argc )
    {
        DEBUG_OUTPUT( "Too few arguments\n");
        usage(argv[0]);
        exit(0);
    }

    // count nessary argument
    i = 0;
    while ((opt = getopt(argc, argv, "d:f:i:")) != -1)
    {
        switch (opt)
        {
            case 'd':
            {
                strcpy(folderPath, optarg);
                DEBUG_OUTPUT( "folder :%s\n", folderPath);
                // check database folder
                if(!(0 == stat( folderPath, &st) && S_ISDIR( st.st_mode )))
                {
                    DEBUG_OUTPUT("Folder doesn't exist\n");
                    exit(0);
                }
                i++;
                break;
            }
            case 'f':
            {
                strcpy(inputFile, optarg);
                DEBUG_OUTPUT( "input file :%s\n", inputFile);
                // check stock roster to calculate
                if(!0 == stat( inputFile, &st ))
                {
                    DEBUG_OUTPUT("Stock roster doesn't exist\n");
                    exit(0);
                }
                i++;
                break;
            }
            case 'i':
            {
                strcpy(pathDynamicLib, optarg);
                DEBUG_OUTPUT( "shared library :%s\n", pathDynamicLib);
                // check indicator library
                if(!0 == stat( pathDynamicLib, &st))
                {
                    DEBUG_OUTPUT("Dynamic library doesn't exist\n");
                    exit(0);
                }
                i++;
                break;
            }
            case 'o':
            {
                strcpy(outputFile, optarg);
                DEBUG_OUTPUT( "output file :%s\n", outputFile);
                i++;
                break;
            }
            case 'n':
            {
                day = atoi( optarg);
                if(day <= 0)
                {
                    usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                DEBUG_OUTPUT( "day :%d\n", day);
                i++;
                break;
            }
            case 'x':
                days1 = atoi( optarg);
                DEBUG_OUTPUT( "days1 :%d\n", days1);
                break;
            case 'y':
                days2 = atoi( optarg);
                DEBUG_OUTPUT( "days2 :%d\n", days2);
                break;
            case 'z':
                days2 = atoi( optarg);
                DEBUG_OUTPUT( "days2 :%d\n", days2);
                break;
            default: /* '?' */
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // register signal
    if( signal( SIGUSR1, signal_handler) == SIG_ERR  )
    {
        printf("Pærent: Unable to create handler for SIGUSR1\n");
    }
    /*
DEBUG_OUTPUT( "folder :%s\n", folderPath);
DEBUG_OUTPUT( "shared library :%s\n", pathDynamicLib);
DEBUG_OUTPUT( "%p\n", pathDynamicLib);
DEBUG_OUTPUT( "day :%d\n", day);
DEBUG_OUTPUT( "input file :%s\n", inputFile);
DEBUG_OUTPUT( "output file :%s\n", outputFile);
*/
    inputFp = fopen( inputFile, "r" );
    if( NULL != inputFp )
    {
#if DBG
        DEBUG_OUTPUT("------------------- Check %s -----------------------------\n", inputFile);
#endif
        // get stock list
        memset(linebuf, 0x0, CMD_LEN);
        while( NULL != (ptr = fgets(linebuf, CMD_LEN, inputFp)))
        {
DEBUG_OUTPUT("  line read = %s\n " ,linebuf);
            for(i = 0 ; i< strlen(linebuf); i++)
            {
                if( !isalnum( linebuf[i]) )
                {
DEBUG_OUTPUT(" \n ");
                    break;
                }
            }
            // remove '\n'
            linebuf[i] = '\0';
DEBUG_OUTPUT(" stock id = %s\n " ,linebuf);
            // cat database name
            ret = gen_stock_indicaotr( linebuf, folderPath, pathDynamicLib);
            memset(linebuf, 0x0, CMD_LEN);
        }
        finalize( __FUNCTION__ , __LINE__);
    }
    else // fopen
    {
        output_err(FILE_NOT_FOUND);
        DEBUG_OUTPUT("Input File doesn't exist\n");
    }

    exit(EXIT_SUCCESS);
}
