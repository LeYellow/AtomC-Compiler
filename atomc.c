#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "source/lexer.h"
#include "source/utils.h"
#include "source/parser.h"
#include "source/ad.h"
#include "source/vm.h"

#include "source/lexer.c"
#include "source/utils.c"
#include "source/parser.c"
#include "source/ad.c"
#include "source/vm.c"

void CheckArgs(int argc)
{
    if(argc!=2)
    {
        printf("Cannot find command, use './atomc <inputFile.c>'\n");
        exit(EXIT_FAILURE);
    }
}

char *inputFileName(char filename[])
{
    int length=strlen(filename)+strlen("tests/")+1;
    char *filepath=(char*)malloc(length);

    if (filepath == NULL) {
        err("Memory allocation failed for input file.\n");
    }

    strcpy(filepath, "tests/");
    strcat(filepath, filename);

    return filepath;
}

char *debugFileName(char filename[], char type[])
{
    int length=strlen("debug/")+strlen(filename)+strlen(type)+1;
    char *filepath=(char*)malloc(length);
    
    if (filepath == NULL) {
        err("Memory allocation failed for '%s' output file.\n", type);
    }
    
    strcpy(filepath, "debug/");
    strcat(filepath, filename);
    strcat(filepath, type);

    return filepath;
}

int main(int argc, char **argv)
{
    CheckArgs(argc);

    char *fileIn=inputFileName(argv[1]);
    char *input=loadFile(fileIn);

    //lexical analizer (lexer)
    char *dfl = debugFileName(argv[1], "_lexer.txt");           //debug
    Token *tokens = tokenize(input);
    showTokens(tokens, dfl);
    pushDomain();                                               //ad
    vmInit();                                                   //vm

    //sintactic analizer (parser)
    char *dfp = debugFileName(argv[1], "_parser.txt");          //debug
    parse(tokens, dfp);

    //domain analizer (ad)
    char *dfd = debugFileName(argv[1], "_domain.txt");          //debug
    showDomain(symTable, "global", dfd);

    //virtual machine (vm) + code generation (gc)
    char *dfv = debugFileName(argv[1], "_vm.txt");              //debug
    Symbol *symMain=findSymbolInDomain(symTable,"main");
    if(!symMain)
    {
        err("missing main function");
    }
    Instr *entryCode=NULL;
    addInstr(&entryCode,OP_CALL)->arg.instr=symMain->fn.instr;
    addInstr(&entryCode,OP_HALT);
    run(entryCode, dfv);
    dropDomain();                                               //ad

    printf("Done executing file %s.\n", argv[1]);

    return 0;
}