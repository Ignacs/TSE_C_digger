#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "header.h"

#define ARGU_NUM 3
#define CMD_LEN 1024 
#define PATH_LEN 1024 
#define FILENAME_LEN 256

#define INDICATOR       "indicator"

void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s -f <Folder contains csv fllies download from TSE>\n\t\t-f <file recode stock to query>\n\t\t- i <indicator library path>\n\t\t-o <output file>\n\t\t-x <argument x>\n\t\t-y <argument y>\n\t\t-z <argument 3>\n" , app_name)
}

static   void *fHandle;
static   void (*indicator)();
FILE *fp = NULL;

int open_shared_lib(char *libPath)
{
    fHandle = dlopen(libPath, RTLD_LAZY);
    if(NULL == fHandle)
    {
        DEBUG_OUTPUT( "Open shared library failed\n")
        exit(0);
    }

    dlerror(); // clear all error case

    indicator = (void(*)())dlsym(fHandle, INDICATOR); // load API from shared library

    if(NULL == indicator)
    {
        DEBUG_OUTPUT( "Open shared library failed\n")
        dlclose(fHandle);
        exit(0);
    }
}

void finalize()
{
    if(NULL != fHandle)
        dlclose(fHandle);
    if(NULL != fp)
        fclose(fp);
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


    if ( ARGU_NUM > argc ) 
    {
        DEBUG_OUTPUT( "Too few arguments\n")
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
                DEBUG_OUTPUT( "folder :%s\n", indicator);
                break;
            case 'o':
                memset(outputFile, 0x0, PATH_LEN);
                strcpy(outputFile, optarg);
                DEBUG_OUTPUT( "input file :%s\n", outputFile);
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

   // check folder
    if(!(0 == stat( folderPath, &st) && S_ISDIR( st.st_mode )))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }

    // read roster
    if(!0 == stat(inputFile, &st))
    {
        DEBUG_OUTPUT("File doesn't exist\n");
        exit(0);
    }

    // open indicator
    open_shared_lib(indicator);

    fp = fopen(argv[2], "r");
    if( NULL != fp )
    {
        memset(linebuf, 0x0, CMD_LEN);
        while( NULL != (ptr = fgets(linebuf, CMD_LEN, fp)))
        {
            // specified stock
            // cat database name 
            // open database
            // collect data needed.
            // call the indicator, indicator() 
            // output the string arrary.
        }
    }
    else
    {
        DEBUG_OUTPUT("File doesn't exist\n");
        exit(0);
    }

    finalize();
    return 0;
}
