#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <iconv.h>
#include "header.h"


#define PATH_LEN 1024 
#define FILENAME_LEN 256
#define BUFF_LEN 4096

#define ARGU_INPUT_PATH 1
#define ARGU_OUTPUT_PATH 2

#define CMD_QUERY_TABLE     "SELECT * FROM TSETradeDay;"

// reference to database 
sqlite3 *db = NULL;
// reference to table 
char **table = NULL;

void usage(char *app_name)
{
    DEBUG_OUTPUT( "%s <Folder contains csv fllies download from TSE> <ouptut foloder>\n" , app_name)
}

void csv_date_check(f, date_str)
{
    /*
    try:
        os.stat(f)
    except:     
        print (sys.argv[0] + "[Error] file is not exist : " + f + ", script will insert data directly.")
        return 0

    if 0 == len(date_str) :
        print (sys.argv[0] + "[Warning] argument date is NULL")
        return -1

    f=open(f, 'r', encoding='utf-8')
    with f: 
        for his_data in csv.reader(f, delimiter=';' ):
            print (sys.argv[0] + "[info] " + str(his_data))
            if his_data[0] == str(date_str):
                print (sys.argv[0]+"[Info] date " + str(date_str) + "has in record")
                return -1
    f.close()
    return 0 
    */
}

void closeDB()
{
    /* free */
    if(NULL != table)
        sqlite3_free_table(table);

    /* close database */
    sqlite3_close(db);
}

int main(int argc, char *argv[])
{
    // output compiled date and time 
    DEBUG_OUTPUT("Compiled date : \t[%s  %s] \n", __DATE__, __TIME__);

    int rows=0, cols =0;
    int i = 0, j = 0 ;
    char outputPath[PATH_LEN];
    char inputPath[PATH_LEN];
    struct stat st;
    FILE *traDayFp = NULL;
    char buf[BUFF_LEN];
    char *endptr =NULL;
    long int val = 0;
    int ret =0;
    char *errMsg = NULL;

    if ( 3 > argc) 
    {
        DEBUG_OUTPUT( "Too few arguments\n")
        usage(argv[0]);
        exit(0);
    }

    // check in and out path
    memset(inputPath, 0x0, sizeof(inputPath));
    strcpy(inputPath, argv[ARGU_INPUT_PATH]);
    if(!(0 == stat(inputPath, &st) && S_ISDIR(st.st_mode)))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }

    memset(outputPath, 0x0, sizeof(outputPath));
    strcpy(outputPath, argv[ARGU_OUTPUT_PATH]);
    if(!(0 == stat(outputPath, &st) && S_ISDIR(st.st_mode)))
    {
        DEBUG_OUTPUT("Folder doesn't exist\n");
        exit(0);
    }
    DEBUG_OUTPUT("Output folder  : \t[%s] \n", outputPath);
    DEBUG_OUTPUT("Input folder: \t[%s] \n", inputPath);

    // check trading day list
    memset(buf, 0x0, sizeof(buf));
    sprintf(buf, "%s/%s", inputPath, TSE_DATE_CLASSIFIC);
    DEBUG_OUTPUT("DB file: \t[%s] \n", buf);
    if (sqlite3_open_v2(buf, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL))
    {
        // db doesn't exist
        output_err(DB_NOT_FOUND);
        closeDB();
        exit(0);
    }
    DEBUG_OUTPUT("DB open successfully \n");

    // query all trading days.
    ret = sqlite3_get_table(db , CMD_QUERY_TABLE, &table, &rows, &cols, &errMsg);
    if( SQLITE_OK != ret && SQLITE_ROW != ret && SQLITE_DONE != ret )
    {
        output_err(DB_QUERY_FAIL);
        DEBUG_OUTPUT("Cause: \t[%s]\n", errMsg);
        closeDB();
        exit(0);
    }

    DEBUG_OUTPUT("List all date: \n");

    // get daily data sequencely
    for(i = 0 ; i< rows; i++)
    {
        // in 2-dimension string structure 
        // EX:
        //  date1 1
        //  date2 0
        //  date3 1...
        //  => [data1][1][date2][0][date3][1]...
        if ((val = strtol(table[i*cols+1], &endptr, 10))==1) 
        {
            // trading day
            // sprintf(buf, "%s/%s", outputPath, table[i*cols]);
            DEBUG_OUTPUT("%s\t[%s]\n", table[i*cols], table[i*cols+1]);

        }

    }



    closeDB();
    exit(0);
}
