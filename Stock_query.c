#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include "header.h"

#define ARGU_NUM 9
#define CMD_LEN 1024 
#define PATH_LEN 1024 
#define FILENAME_LEN 256
#define BUF_LEN 4096

#define INDICATOR       "indicator"
#define CMD_QUERY_STOCK     "SELECT * FROM DailyData ORDER by date;"
// #define CMD_QUERY_STOCK     "SELECT * FROM sqlite_master;"

#define DBG     1

void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s -d <Folder contains stock database>\n\t\t-f <file recode stock to query>\n\t\t-i <indicator library path>\n\t\t-o <output file>\n\t\t-x <argument x>\n\t\t-y <argument y>\n\t\t-z <argument 3>\n" , app_name);
}

static void *fHandle = NULL;
static void (*indicator)();
static char **ptable = NULL;
static char **pDailyData= NULL;
static sqlite3 *db = NULL;
FILE    *inputFp = NULL;

void finalize(const char *func, int line)
{
    DEBUG_OUTPUT( "Free via %s, %d\n", func, line);
    if(NULL != fHandle)
    {
        DEBUG_OUTPUT( "clear indicator pointer \n");
        dlclose(fHandle);
    }

    if(NULL != inputFp)
    {
        DEBUG_OUTPUT( "clear input file pointer \n");
        fclose(inputFp);
    }

    if(NULL != pDailyData)
    {
        free(pDailyData);
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
    if(! fHandle )
    {
        DEBUG_OUTPUT( "Open shared library failed = %s\n", dlerror());
        dlclose(fHandle);
        exit(0);
    }

    dlerror(); // clear all error case

    indicator = (void(*)())dlsym(fHandle, INDICATOR); // load API from shared library

    if(NULL == indicator)
    {
        DEBUG_OUTPUT( "Get indicator failed = %s\n", dlerror());
        dlclose(fHandle);
        exit(0);
    }
    return 1;
}

int get_stock_daily_data(char *dbName, char **dailyData)
{
#define WRITE_SIDE 1
#define READ_SIDE 0
    char *delim = ",";
    pid_t pid,w; 
    int cntFork = 0;
    int status = 0;
    int ret = 0;
    int rows = 0, cols = 0, size = 0;
    char *errMsg = NULL;
    int i = 0, j = 0;
    int pipefd[2] ;
    char buf[BUF_LEN] ;
    char *ptr=NULL, *saveptr;
    int len = 0;

    // create pipe 
    if(0 > (ret = pipe (pipefd)))
    {
        DEBUG_OUTPUT( "pipe create fail = %d, %s\n", errno, strerror(errno));
        finalize(__FUNCTION__, __LINE__);
        exit(EXIT_FAILURE);
    }


    // Warning : child process
    if( 0 == (pid = fork()) )
    {
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
                sprintf(buf, "%d,%d,%d<", rows, cols, size);
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
            exit(EXIT_SUCCESS);
        else 
            exit(EXIT_FAILURE);
    }

    // parent 
DEBUG_OUTPUT( "=================== parent continue:  ==================================\n");

    cntFork = 0;

DEBUG_OUTPUT("[Parent] : close write pipe \n");
    close(pipefd[WRITE_SIDE]);

    // sleep(1);
    do{
        // read pipe to make sure the size of memory to create
        memset(buf, 0x0, BUF_LEN);
        i = 0;
        do 
        {
             len += ret = read ( pipefd[READ_SIDE], &buf[i], 1 );
             if( 0 > ret )
                DEBUG_OUTPUT("[Parent] : error when read, err = %d\n", errno);
            else
            {
                len += ret ; 
                if('<' == buf[i])
                {
                    break;
                }
                i++;
            }
        }while (1); 
        DEBUG_OUTPUT("[Parent] : read %d bytes = %s\n", len, buf);

        ret =0;

        // rows 
        ptr = strtok_r(buf, (const char *)delim, &saveptr);
        if(NULL != ptr)
        {
DEBUG_OUTPUT("[Parent] :  %s\n", ptr);
            rows = strtol(ptr, NULL, 10);
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
            /*
            = (char **)calloc( rows * cols, size);
            if(NULL != ptable)
            {

            }
            */
        }

        status = 0;
        // w = waitpid(pid, &status, WUNTRACED );
        w = wait(&status);

        if (w == -1) {
            DEBUG_OUTPUT( "no more child process \n");
        }

        if (WIFEXITED(status)) 
        {
        //    DEBUG_OUTPUT("[parent] : exited, status=%d\n", WEXITSTATUS(status));

            // child exit
            cntFork --;
        }
    } while (0 < cntFork);

    // close pipe 
    close(pipefd[READ_SIDE]);
    return ret ;
}

int main(int argc, char **argv)
{
    struct stat st;
    char linebuf[CMD_LEN];
    char folderPath[PATH_LEN];
    char inputFile[FILENAME_LEN];
    char outputFile[FILENAME_LEN];
    char *ptr = NULL;
    char opt = '\0';
    char indicator[FILENAME_LEN];
    unsigned int x, y, z; 
    char buf[BUF_LEN];
    unsigned char c = 0x0; 
    char **pstockData = NULL;
    int i = 0;


    if ( ARGU_NUM > argc ) 
    {
        DEBUG_OUTPUT( "Too few arguments\n");
        usage(argv[0]);
        exit(0);
    }

    while ((opt = getopt(argc, argv, "d:f:i:o:x:y:z:")) != -1) 
    {
        switch (opt) 
        {
            case 'd':
                memset(folderPath, 0x0, PATH_LEN);
                strcpy(folderPath, optarg);
                DEBUG_OUTPUT( "folder :%s\n", folderPath);
                break;
            case 'f':
                memset(inputFile, 0x0, PATH_LEN);
                strcpy(inputFile, optarg);
                DEBUG_OUTPUT( "input file :%s\n", inputFile);
                break;
            case 'i':
                memset(indicator, 0x0, PATH_LEN);
                strcpy(indicator, optarg);
                DEBUG_OUTPUT( "shared library :%s\n", indicator);
                break;
            case 'o':
                memset(outputFile, 0x0, PATH_LEN);
                strcpy(outputFile, optarg);
                DEBUG_OUTPUT( "output file :%s\n", outputFile);
                break;
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

   // check database folder
    if(!(0 == stat( folderPath, &st) && S_ISDIR( st.st_mode )))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }

    // read roster
    memset(buf, 0x0, BUF_LEN);
    if(!0 == stat( inputFile, &st))
    {
        DEBUG_OUTPUT("File doesn't exist\n");
        exit(0);
    }

    inputFp = fopen( inputFile, "r" );
    if( NULL != inputFp )
    {
#if DBG
        DEBUG_OUTPUT("------------------- Check %s -----------------------------\n", inputFile);
#endif

        memset(linebuf, 0x0, CMD_LEN);
        // specified stock
        while( NULL != (ptr = fgets(linebuf, CMD_LEN, inputFp)))
        {
            // cat database name 
            memset(buf, 0x0, BUF_LEN);

            for(i = 0 ; i< strlen(linebuf); i++)
            {
                if( !isalnum( linebuf[i]) )
                    break;
            }
            // add postfix ".sl3" 
            linebuf[i] = '.';
            linebuf[i+1] = 's';
            linebuf[i+2] = 'l';
            linebuf[i+3] = '3';
            linebuf[i+4] = '\0';

            sprintf( buf,"%s/%s", folderPath, linebuf );

            // open specified stock's db 
            get_stock_daily_data(buf, pstockData);

            // collect data needed.

            // call the indicator, indicator() 
            //
            // output the string arrary.
        }

        // open indicator
        // open_shared_lib(indicator);
    }
    else
    {
        DEBUG_OUTPUT("Input File doesn't exist\n");
    }


    finalize( __FUNCTION__ , __LINE__);
    exit(EXIT_SUCCESS);
}
