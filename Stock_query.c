#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>

#include "header.h"

#define ARGU_NUM 11
#define NECESSARY_ARGC 5
#define CMD_LEN 1024
#define PATH_LEN 1024
#define FILENAME_LEN 256
#define BUF_LEN 4096
#define STOCK_ELE 14

#define WRITE_SIDE 1
#define READ_SIDE 0

#define INDICATOR       "indicator"
// EX: select * from DailyData order by date desc limit 12
#define CMD_QUERY_STOCK     "SELECT * FROM DailyData ORDER by date desc limit "
// #define CMD_QUERY_STOCK     "SELECT * FROM sqlite_master;"
#define END_SYMBOL "<"

#define DBG     1
#if DBG
    char largeBuf[BUF_LEN][BUF_LEN];
#endif


void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s -d <Folder contains stock database>\n\t\t-f <file recode stock to query>\n\t\t-i <indicator library path>\n\t\t-o <output file>\n\t\t-n <the amount of day>\n\t\t-x <argument x>\n\t\t-y <argument y>\n\t\t-z <argument 3>\n" , app_name);
}

static void *fHandle = NULL;
static void (*indicator)();
static char **ptable = NULL;
static char **pStorckStrData= NULL;
static sqlite3 *db = NULL;
FILE    *inputFp = NULL;
unsigned int memCount = 0;
int pipefd[2] ;
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

    close_db(db, ptable);

}

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

    indicator = (void(*)())dlsym(fHandle, INDICATOR); // load API from shared library

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
            close(pipefd[WRITE_SIDE]);
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

// return 1 : (rows-1)  // decrease the row of title
// return 0 : fail to get dbata
int get_stock_daily_data(char *dbName, char ***dailyData, int *pRows , int *pCols , int *pSize )
{
    char *delim = ",";
    pid_t pid,w;
    // int cntFork = 0;
    int status = 0;
    int ret = 0;
    char *errMsg = NULL;
    int i = 0, j = 0, k = 0;
    char buf[BUF_LEN] ;
    char *ptr=NULL, *saveptr;
    int len = 0;
    char **pCurr = NULL;
    int rows = 0, cols = 0, size = 0;

DEBUG_OUTPUT(" \n ");
    if(*pRows < 0)
    {
        output_err(ARGU_NOT_MATCH);
        DEBUG_OUTPUT( "Arugment error \n");
        return -1;
    }

DEBUG_OUTPUT(" \n ");
    // create pipe
    if(0 > (ret = pipe (pipefd)))
    {
        output_err(PIPE_CREATE_ERROR);
        DEBUG_OUTPUT( "pipe create fail = %d, %s\n", errno, strerror(errno));
        finalize(__FUNCTION__, __LINE__);
        return -1;
    }
DEBUG_OUTPUT(" \n ");

    // Warning : child process
    if( 0 == (pid = fork()) )
    {
        if( signal( SIGUSR1, signal_handler) == SIG_ERR  )
        {
            printf("Pærent: Unable to create handler for SIGUSR1\n");
        }

DEBUG_OUTPUT( "=================== child start : open database : %s ==================================\n", dbName);
DEBUG_OUTPUT( " open database : %s \n", dbName);

        close(pipefd[READ_SIDE]);

        // open stock databasee
        ret = openDB(dbName, READONLY, &db);

        // open successfully
        if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
        {
DEBUG_OUTPUT( "get data=> %s\n", dbName);
            memset(buf, 0x0, BUF_LEN);

            // limit output in (*pRows x 2)
            // but api will output 1 title line
            // get (*pRows x 2) + 1 lines. and dont send the first line
            sprintf(buf, "%s %d;", CMD_QUERY_STOCK, (unsigned )(*pRows * 2 +1));
DEBUG_OUTPUT( "Input cmd: %s\n", buf);
            ret = sqlite3_get_table(db , buf, &ptable, &rows, &cols, &errMsg);
            if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
            {
fprintf(stderr, "rows = %d\tcols =%d\n", rows, cols);

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
DEBUG_OUTPUT( "the maximum size of element = %u\n", size);

                // notify parent allocte memory
                memset(buf, 0x0, BUF_LEN);

                // row , columen, size and ignore titile line
                sprintf(buf, "%d,%d,%d"END_SYMBOL, rows-1, cols, size);
                if(0 > (ret = write( pipefd[WRITE_SIDE], (const void *)buf, strlen(buf) )))
                {
                    DEBUG_OUTPUT( "Write error occurs = %d, err = %d, %s\n", ret, errno, strerror(errno));
DEBUG_OUTPUT( "pipe create = %d, %d\n", pipefd[0], pipefd[1]);
                }
                else
                {
                    DEBUG_OUTPUT( "Write header %s\n", buf);

                    // skip the first line  : date      stockNum        tradeNum        volume      open        high        low     close       sign        diff        buy     buyVol      sell        sellVol     epr
                    for(i = 1 ; i< rows ; i++)
                    {
DEBUG_OUTPUT( "rows 1st is %d= %s\n", i*cols, ptable[i*cols]);
                        for(j = 0 ; j< cols; j++)
                        {
                            len += ret = write(pipefd[WRITE_SIDE], ptable[i*cols+j], strlen(ptable[i*cols+j]));
                            if( 0 > ret )
                                DEBUG_OUTPUT( "Write error occurs = %d, err = %d, %s\n", ret, errno, strerror(errno));
                            len += ret = write(pipefd[WRITE_SIDE], (const void *)",", 1);
                            if( 0 > ret )
                                DEBUG_OUTPUT( "Write error occurs = %d, err = %d, %s\n", ret, errno, strerror(errno));
                        }
                    }
                    DEBUG_OUTPUT( "Write to the end %d\n", len);
                    // ret = 0;
                }
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
            output_err(DB_OPEN_FAIL);
            DEBUG_OUTPUT("Cause: \terrno = %d, [%s]\n", ret , errMsg);
            ret = -1;
        }

        DEBUG_OUTPUT("Child exit, free resource \n");
        /* recycle resource */
        finalize(__FUNCTION__, __LINE__);

        // close pipe
        close(pipefd[WRITE_SIDE]);

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

DEBUG_OUTPUT("[Parent] : close write pipe \n");
    close(pipefd[WRITE_SIDE]);

    // read pipe to make sure the size of memory to create
    // pipe is blocked, just
    memset(buf, 0x0, BUF_LEN);
    i = 0;
    do
    {
        ret = read ( pipefd[READ_SIDE], &buf[i], 1 );
        if( 0 > ret )
        {
            DEBUG_OUTPUT("[Parent] : error when read, err = %d\n", errno);
            w = wait(&status);
            if (w == -1)
            {
                DEBUG_OUTPUT( "no more child process \n");
                break;
            }
            if(WIFEXITED(status))
            {
                DEBUG_OUTPUT( "child process exit\n");
                break;
            }
        }
        else
        {
            len += ret ;
            if(0 == strncmp(END_SYMBOL, &buf[i], 1))
            {
                break;
            }
            i++;
        }
    }while ((EBADF != errno ));
    DEBUG_OUTPUT("[Parent] : read %d bytes = %s\n", len, buf);

    ret = 0;

    // rows
    ptr = strtok_r(buf, (const char *)delim, &saveptr);
    if(NULL != ptr)
    {
DEBUG_OUTPUT("[Parent] :  %s\n", ptr);
        rows = strtol(ptr, NULL, 10);
       // rows --;  //remopve the line includes title.
DEBUG_OUTPUT("[Parent] :  rows = %d\n", rows);
        if(0 < rows)
        {
            // cols
            ptr = strtok_r(NULL, (const char *)delim, &saveptr);
DEBUG_OUTPUT("[Parent] :  %s\n", ptr);
            if( NULL != ptr )
            {
                cols = strtol(ptr, NULL, 10);
                if( 0 < cols)
                {
DEBUG_OUTPUT("[Parent] :  cols = %d\n", cols);
                    // size
                    ptr = strtok_r(NULL, (const char *)delim, &saveptr);
DEBUG_OUTPUT("[Parent] :  %s\n", ptr);
                    size = strtol(ptr, NULL, 10);
DEBUG_OUTPUT("[Parent] :  size = %d\n", size);
                    if(0 >= size) ret = -5;
                } else ret = -4;
            } else ret = -3;
        } else ret = -2;
    } else ret = -1;

    DEBUG_OUTPUT( "[parent] : receive ret = %d, row = %d, cols =%d , size =%d \n", ret , rows, cols, size);

    // check how many items to store;
    if(0 == ret)
    {
        // create string array according return value , each string  has CMD_LEN bytes;
        // data structure
        // [date] [stockNum] [tradeNum] [volume] [open] [high] [low] [close] [sign] [diff] [buy] [buyVol] [sell] [sellVol] [epr]
        pCurr = getMemSpace(rows * cols, size);
        if(NULL != pCurr)
        {
            for(i = 0 ; i < rows; i++)
            {
                for(j = 0 ; j < cols; j++)
                {
                    // read length = (string) + (,)
                    memset(buf, 0x0, BUF_LEN);
                    k = 0;
                    do
                    {
                        len += ret = read ( pipefd[READ_SIDE], &buf[k], 1 );
                        if(ret >  0 && ',' == buf[k])
                        {
                            strncpy( (char *)(pCurr+(i*cols*size)+j*size), (const char*)buf, k);
                            //*( pCurr+(i*cols)+j) = '\0';
fprintf( stderr, "[%d][%d] %s\t", i, j, (char *)(pCurr+(i*cols*size)+j*size));
                            k = 0;
                            break;
                        } //if
                        k++;
                        if(k >= BUF_LEN)
                        {

                            DEBUG_OUTPUT( "[parent] : buf overflow !!!! dump>> %s\n", buf);
                            i = rows+1;
                            j = cols+1;
                            ret = -1;
                        }
                    } while(k < BUF_LEN);
fprintf( stderr, "\n");
                }// for j
                DEBUG_OUTPUT( "\n");
            } // for i

            DEBUG_OUTPUT( "[parent] : total=%d \n", len);
        }
    }
    else
    {
        DEBUG_OUTPUT( "[parent] : Header error, stop to receive from child \n");
    }

#if 1
    //
    do
    {
        status = 0;
        // w = waitpid(pid, &status, WUNTRACED );
DEBUG_OUTPUT( "[parent] : w= it\n");
        w = wait(&status);

DEBUG_OUTPUT( "[parent] : w=%d \n", w);
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
DEBUG_OUTPUT( "[parent] : total=%d \n", len);

        if (WIFEXITED(status))
        {
            DEBUG_OUTPUT("[parent] : exited, status=%d\n", WEXITSTATUS(status));

            break;
        }
        else
        {
            DEBUG_OUTPUT( "[parent] : wait child stop \n");
            //kill(w, SIGUSR1);
        }
DEBUG_OUTPUT( "[parent] : next time \n");
#endif
    } while (1);

    // close pipe
    close(pipefd[READ_SIDE]);

    *pRows = rows ;
    *pCols = cols ;
    *pSize = size ;
DEBUG_OUTPUT( "[parent] : close pipe pRows = %d, pCols = %d, pSize = %d\n",* pRows, *pCols, *pSize);
DEBUG_OUTPUT( "[parent] : pdailyDat = %p \n", *dailyData);
    *dailyData =  pCurr ;
DEBUG_OUTPUT( "[parent] : pdailyDat = %p \n", *dailyData);


    if(ret >= 1)
        return 1;
    else
        return 0;
}

// create memory space to store data after convtered
//  line data sequence: date      stockNum        tradeNum        volume      open        high        low     close       sign        diff        buy     buyVol      sell        sellVol     epr
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

void initialize()
{
    memCount = 0;
}

int main(int argc, char **argv)
{
    struct stat st;
    char linebuf[CMD_LEN], stockID[CMD_LEN];
    char folderPath[PATH_LEN];
    char inputFile[FILENAME_LEN];
    char outputFile[FILENAME_LEN];
    char pathDynamicLib[FILENAME_LEN];
    char *ptr = NULL;
    char opt = '\0';
    unsigned int days1, days2, days3;
    char buf[BUF_LEN];
    unsigned char c = 0x0;
    int i = 0;
    int day = 0;
    int ret = 0;
    unsigned int rows=0, cols=0, size=0;


    initialize();
    memset(pathDynamicLib, 0x0, FILENAME_LEN);
    memset(outputFile, 0x0, FILENAME_LEN);
    memset(inputFile, 0x0, FILENAME_LEN);
    memset(folderPath, 0x0, PATH_LEN);
    if ( ARGU_NUM > argc )
    {
        DEBUG_OUTPUT( "Too few arguments\n");
        usage(argv[0]);
        exit(0);
    }

    // count nessary argument
    i = 0;
    while ((opt = getopt(argc, argv, "d:f:i:o:n:x:y:z:")) != -1)
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
    /*
DEBUG_OUTPUT( "folder :%s\n", folderPath);
DEBUG_OUTPUT( "shared library :%s\n", pathDynamicLib);
DEBUG_OUTPUT( "%p\n", pathDynamicLib);
DEBUG_OUTPUT( "day :%d\n", day);
DEBUG_OUTPUT( "input file :%s\n", inputFile);
DEBUG_OUTPUT( "output file :%s\n", outputFile);
*/
    if( NECESSARY_ARGC > i)
    {
        DEBUG_OUTPUT("Necessary arguments is not enought > -f, -d, -i, -o and -n\n");
        exit(EXIT_FAILURE);
    }

    inputFp = fopen( inputFile, "r" );
    if( NULL != inputFp )
    {
#if DBG
        DEBUG_OUTPUT("------------------- Check %s -----------------------------\n", inputFile);
#endif
        memset(linebuf, 0x0, CMD_LEN);
        memset(stockID, 0x0, CMD_LEN);
        // specified stock
        while( NULL != (ptr = fgets(linebuf, CMD_LEN, inputFp)))
        {
            // cat database name
            memset(buf, 0x0, BUF_LEN);

            for(i = 0 ; i< strlen(linebuf); i++)
            {
                if( !isalnum( linebuf[i]) )
                {
DEBUG_OUTPUT(" \n ");
                    break;
                }
            }
DEBUG_OUTPUT(" \n ");
            // add postfix ".sl3"
            strcpy(stockID, linebuf);

            linebuf[i] = '.';
            linebuf[i+1] = 's';
            linebuf[i+2] = 'l';
            linebuf[i+3] = '3';
            linebuf[i+4] = '\0';

DEBUG_OUTPUT(" \n ");

            sprintf( buf,"%s/%s", folderPath, linebuf );

DEBUG_OUTPUT(" \n ");
            // open specified stock's db
            // limit database output be (day*2) days.
            rows = day;
DEBUG_OUTPUT(" \n ");
            ret = get_stock_daily_data(buf, &pStorckStrData, &rows, &cols, &size);
            if(ret > 0)
            {
DEBUG_OUTPUT("total %d rows , %d cols , size %d\n", rows, cols, size);
                convType(pStorckStrData, rows, cols, size, &pStockDigitData );
DEBUG_OUTPUT(" \n ");
                if(NULL != pStockDigitData)
                {
                    // open indicator
                    if(0 < open_shared_lib( pathDynamicLib ))
                    {
DEBUG_OUTPUT("pass pStockDigitData %p\n", pStockDigitData);
DEBUG_OUTPUT("open %f\n", pStockDigitData->open);
                        // call the indicator, indicator()
                        // pass stock data, day parameter, output file name
                        indicator(stockID, &pStockDigitData, rows, day, days1, outputFile);
DEBUG_OUTPUT(" \n ");
                    }
DEBUG_OUTPUT(" \n ");
                }
DEBUG_OUTPUT(" \n ");
            }
            else
DEBUG_OUTPUT(" \n ");
        }

        finalize( __FUNCTION__ , __LINE__);
    }
    else
    {
        output_err(FILE_NOT_FOUND);
        DEBUG_OUTPUT("Input File doesn't exist\n");
    }

    exit(EXIT_SUCCESS);
}
