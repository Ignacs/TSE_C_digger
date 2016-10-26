#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "header.h"

#define ARGU_NUM 3
#define CMD_LEN 1024 

void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s <Folder contains csv fllies download from TSE> <file recode stock to query>\n" , app_name)
}

int main()
{
    FILE *fp = NULL;
    struct stat st;
    char linebuf[CMD_LEN];
    char *ptr = NULL;
    void *fHandle;
    void (*func)();

    fHandle = dlopen("./libVR.so",RTLD_LAZY);
    if(!fHandle)
    {
        DEBUG_OUTPUT( "Open shared library failed\n")
        exit(0);
    }

    dlerror(); // clear all error case

    func = (void(*)())dlsym(fHandle,"indicator"); // load API from shared library

    if(func)
    {
        func();
    }
    dlclose(fHandle);

#if 0
    if ( ARGU_NUM > argc ) 
    {
        DEBUG_OUTPUT( "Too few arguments\n")
        usage(argv[0]);
        exit(0);
    }

   // check folder
    if(!(0 = stat( argv[1], &st ) && S_ISDIR( st.st_mode )))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }

    // read roster
    if(!0 = stat(argv[2], &st))
    {
        DEBUG_OUTPUT("File doesn't exist\n");
        exit(0);
    }

    fp = fopen(argv[2], "r");
    if( NULL != fp )
    {
        memset(linebuf, 0x0, CMD_LEN);
        while( NULL != (ptr = fgets(linebuf, CMD_LEN, fp)) && EOF != ptr )
        {
            // specified stock 
        }
    }
    else
    {
        DEBUG_OUTPUT("File doesn't exist\n");
        exit(0);
    }
#endif

    return 0;
}
