#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "source/lexer.h"
#include "source/utils.h"
#include "source/lexer.c"
#include "source/utils.c"

void CheckArgs(int argc)
{
    if(argc!=2)
    {
        printf("cannot find command, use './exe <inputFile.c>'\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    CheckArgs(argc);

    char *input=loadFile(argv[1]);
    Token *tokens = tokenize(input);
    showTokens(tokens);

    return 0;
}