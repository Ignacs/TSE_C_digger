#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "header.h"

#define ARGU_NUM 11
#define NECESSARY_ARGC 5
#define CMD_LEN 1024 
#define PATH_LEN 1024 
#define FILENAME_LEN 256
#define BUF_LEN 4096

#define WRITE_SIDE 1
#define READ_SIDE 0

#define INDICATOR       "indicator"
#define CMD_QUERY_STOCK     "SELECT * FROM DailyData ORDER by date;"
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
static char **pDailyData= NULL;
static sqlite3 *db = NULL;
FILE    *inputFp = NULL;
unsigned int memCount = 0;
int pipefd[2] ;

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

    if(NULL != pDailyData)
    {
#if !DBG
        free(pDailyData);
#endif
        pDailyData = NULL;
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

    // check path is absolute.
    if('/'!= *(libPath) && '~' != *(libPath))
    {
        memset(buf, 0x0, BUF_LEN);
        sprintf(buf , "./%s", libPath);
DEBUG_OUTPUT( "Open shared library :%s\n", buf);
    }

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

// return 1 : get data
// return 0 : fail to get dbata
int get_stock_daily_data(char *dbName, char **dailyData)
{
    char *delim = ",";
    pid_t pid,w; 
    // int cntFork = 0;
    int status = 0;
    int ret = 0;
    int rows = 0, cols = 0, size = 0;
    char *errMsg = NULL;
    int i = 0, j = 0, k = 0;
    char buf[BUF_LEN] ;
    char *ptr=NULL, *saveptr;
    int len = 0;

    // create pipe 
    if(0 > (ret = pipe (pipefd)))
    {
        DEBUG_OUTPUT( "pipe create fail = %d, %s\n", errno, strerror(errno));
        finalize(__FUNCTION__, __LINE__);
        return -1;
    }

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
            ret = sqlite3_get_table(db , CMD_QUERY_STOCK, &ptable, &rows, &cols, &errMsg);
            if( SQLITE_OK == ret || SQLITE_ROW == ret || SQLITE_DONE == ret )
            {
fprintf(stderr, "rows = %d\tcols =%d\n", rows, cols);

                // find the maximum element to set the size arguemnt 
                size = 0;
                for(i = 1 ; i< rows ; i++)
                {
                    for(j = 0 ; j< cols; j++)
                    {
                        if(size < strlen(ptable[i*cols+j]))
                            size = strlen(ptable[i*cols+j]) + 1;
                    }
                }

DEBUG_OUTPUT( "the maximum size of element = %u\n", size);

                // notify parent allocte memory
                memset(buf, 0x0, BUF_LEN);

                // row , columen, size
                sprintf(buf, "%d,%d,%d"END_SYMBOL, rows, cols, size);
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
        rows --;  //remopve the line includes title.
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
        pDailyData = getMemSpace(rows * cols, size);
        if(NULL != pDailyData)
        {
            for(i = 0 ; i < rows; i++)
            {
                for(j = 0 ; j < cols; j++)
                {
                    // read length = (string) + (,)
                    memset(buf, 0x0, sizeof(buf));
                    k = 0;
                    do
                    {
                        len += ret = read ( pipefd[READ_SIDE], &buf[k], 1 );
                        if(ret >  0 && ',' == buf[k])
                        {
                            strncpy( (char *)pDailyData+(i*cols)+j, (const char*)buf, k);
                            //*( pDailyData+(i*cols)+j) = '\0';
fprintf( stderr, "[%d][%d] %s\t", i, j, (char *)pDailyData+(i*cols)+j);
                            k = 0;
                            break;
                        } //if 
                        k++;
                        if(k >= BUF_LEN)
                        {
                            DEBUG_OUTPUT( "[parent] : buf overflow !!!! \n");
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
DEBUG_OUTPUT( "[parent] : close pipe ret = %d\n", ret);
    if(ret >= 0)
        return 1;
    else
        return 0;
}

void initialize()
{
    memCount = 0;
}

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
    unsigned int x, y, z; 
    char buf[BUF_LEN];
    unsigned char c = 0x0; 
    char **pstockData = NULL;
    int i = 0;
    int day = 0;


    initialize();
    memset(pathDynamicLib, 0x0, FILENAME_LEN);
    memset(outputFile, 0x0, PATH_LEN);
    memset(inputFile, 0x0, PATH_LEN);
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
DEBUG_OUTPUT( "%p\n", pathDynamicLib);
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
                x = atoi( optarg);
                break;
            case 'y':
                y = atoi( optarg);
                break;
            case 'z':
                z = atoi( optarg);
                break;
            default: /* '?' */
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
DEBUG_OUTPUT( "folder :%s\n", folderPath);
DEBUG_OUTPUT( "shared library :%s\n", pathDynamicLib);
DEBUG_OUTPUT( "%p\n", pathDynamicLib);
DEBUG_OUTPUT( "day :%d\n", day);
DEBUG_OUTPUT( "input file :%s\n", inputFile);
DEBUG_OUTPUT( "output file :%s\n", outputFile);
    if( NECESSARY_ARGC > i)
    {
        DEBUG_OUTPUT("Necessary arguments is not enought > -f, -d, -i, -o and -n\n");
        exit(EXIT_FAILURE);
    }
DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);

    inputFp = fopen( inputFile, "r" );
    if( NULL != inputFp )
    {
#if DBG
        DEBUG_OUTPUT("------------------- Check %s -----------------------------\n", inputFile);
#endif
        memset(linebuf, 0x0, CMD_LEN);
        // specified stock
DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);
        while( NULL != (ptr = fgets(linebuf, CMD_LEN, inputFp)))
        {
DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);
            // cat database name 
            memset(buf, 0x0, BUF_LEN);
DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);

            for(i = 0 ; i< strlen(linebuf); i++)
            {
                if( !isalnum( linebuf[i]) )
                    break;
            }
DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);
            // add postfix ".sl3" 
            linebuf[i] = '.';
            linebuf[i+1] = 's';
            linebuf[i+2] = 'l';
            linebuf[i+3] = '3';
            linebuf[i+4] = '\0';

DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);
            sprintf( buf,"%s/%s", folderPath, linebuf );

DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);
            // open specified stock's db 
            if(get_stock_daily_data(buf, pstockData))
            {
DEBUG_OUTPUT("Open shared library : %s\n", pathDynamicLib);
                // open indicator
                if(0 < open_shared_lib( pathDynamicLib))
                {
DEBUG_OUTPUT("Calculate\n");
                    // call the indicator, indicator() 
                    // pass stock data, day parameter, output file name
                    // indicator(pstockData, day, outputFile);
                }
            }
        }

        finalize( __FUNCTION__ , __LINE__);
    }
    else
    {
        DEBUG_OUTPUT("Input File doesn't exist\n");
    }

    exit(EXIT_SUCCESS);
}
