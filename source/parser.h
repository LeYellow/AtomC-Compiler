#pragma once

#include <stdbool.h>
#include "lexer.h"

void parse(Token *tokens, char outpath[]);
void tkerr(const char *fmt,...);
bool consume(int code);

bool unit();
bool structDef();
bool varDef();
bool typeBase();
bool arrayDecl();
bool fnDef();
bool fnParam();
bool stm();
bool stmCompund(bool);
bool expr();
bool exprAssign();
bool exprOrPrim();
bool exprOr();
bool exprAndPrim();
bool exprAND();
bool exprEqPrim();
bool exprEq();
bool exprRelPrim();
bool exprRel();
bool exprAddPrim();
bool exprAdd();
bool exprMulPrim();
bool exprMul();
bool exprCast();
bool exprUnary();
bool exprPostFix();
bool exprPostfixPrim();
bool exprPrimary();

char* tkCode(int code);