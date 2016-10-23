#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>


int main(int argc, char *argv[])
{
    //argv[1] path to csv file
    //argv[2] number of lines to skip
    //argv[3] length of longest value (in characters)

    fprintf(stderr, "%d\n", __LINE__);
    FILE *pfinput;
    unsigned int nSkipLines = 0, currentLine, lenLongestValue;
    wchar_t pTempValHolder[1024];
    wint_t c;
    unsigned int vcpm; //value character marker
    int QuotationOnOff; //0 - off, 1 - on

    fprintf(stderr, "%d\n", __LINE__);
    // nSkipLines = 0;
    // lenLongestValue = 1024;

    fprintf(stderr, "%d\n", __LINE__);
    // pTempValHolder = (char*)malloc(lenLongestValue);  

    if( pfinput = fopen(argv[1], "r") ) 
    {
        fprintf(stderr, "%d\n", __LINE__);

        if (!setlocale(LC_CTYPE, "zh_TW.Big5")) 
        {
            fprintf(stderr, "Error:Please check LANG, LC_CTYPE, LC_ALL.\n");
            return 1;
        }

        fprintf(stderr, "%d\n", __LINE__);
        printf("%ls,",pTempValHolder);
        rewind(pfinput);
        fprintf(stderr, "%d\n", __LINE__);

        currentLine = 1;
        vcpm = 0;
        QuotationOnOff = 0;

        fprintf(stderr, "%d\n", __LINE__);
        //currentLine > nSkipLines condition skips ignores first argv[2] lines
        while( (c = fgetwc(pfinput)) != WEOF)
        {
            fprintf(stderr, "get : %lc\n",  c);
            switch(c)
            {
                case ',':
                    fprintf(stderr, "%d\n", __LINE__);
                    if(!QuotationOnOff && currentLine > nSkipLines) 
                    {
                        pTempValHolder[vcpm] = '\0';
                        printf("%ls,",pTempValHolder);
                        vcpm = 0;
                    }
                    break;
                case '\n':
                    fprintf(stderr, "%d\n", __LINE__);
                    if(currentLine > nSkipLines)
                    {
                        pTempValHolder[vcpm] = '\0';
                        printf("%ls\n",pTempValHolder);
                        vcpm = 0;
                    }
                    currentLine++;
                    break;
                case '\"':
                    fprintf(stderr, "%d\n", __LINE__);
                    if(currentLine > nSkipLines)
                    {
                        if(!QuotationOnOff) {
                            QuotationOnOff = 1;
                            pTempValHolder[vcpm] = c;
                            vcpm++;
                        } else {
                            QuotationOnOff = 0;
                            pTempValHolder[vcpm] = c;
                            vcpm++;
                        }
                    }
                    break;
                default:
                    fprintf(stderr, "%d\n", __LINE__);
                    if(currentLine > nSkipLines)
                    {
                        pTempValHolder[vcpm] = c;
                        vcpm++;
                    }
                    break;
            }
            fprintf(stderr, "%d\n", __LINE__);
        }
        fprintf(stderr, "Error (%d) => \n", ferror(pfinput));
        fprintf(stderr, "read (%lc)  \n", c);

        fprintf(stderr, "%d\n", __LINE__);
        fclose(pfinput); 
        //free(pTempValHolder);

    }

                    fprintf(stderr, "%d\n", __LINE__);
    return 0;
}
