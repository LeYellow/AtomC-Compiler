#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "parser.h"
#include "ad.h"
#include "at.h"
#include "gc.h"

#include "at.c"
#include "gc.c"

Token *iTk;				// the iterator in the tokens list
Token *consumedTk;		// the last consumed token
Symbol* owner = NULL; 	// the current owner of the symbols
FILE *fout;				// debug file

void tkerr(const char *fmt,...)
{
	fprintf(stderr,"error in line %d: ",iTk->line);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
}

bool consume(int code)
{
	
	fprintf(fout, "Consuming (%s) line %d ", tkCode(code), iTk->line);		//debug
	if(iTk->code==code)
	{
		fprintf(fout," => consumed\n");		//debug
		consumedTk=iTk;
		iTk=iTk->next;
		return true;
	}
	fprintf(fout," => found %s\n", tkCode(iTk->code));		//debug
	return false;
}

//exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR
bool exprPrimary(Ret *r)
{
	fprintf(fout, "exprPrimary %s\n",tkCode(iTk->code));		//debug
	Token* startAtom=iTk;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(consume(ID))
	{
		Token* tkName=consumedTk;
		Symbol *s=findSymbol(tkName->text);
		if(!s)
		{
			tkerr("undefined id: %s",tkName->text);
		}
		if(consume(LPAR))
		{
			if(s->kind!=SK_FN)
			{
				tkerr("only a function can be called");
			}
			Ret rArg;
			Symbol *param=s->fn.params;
			if(consume(RPAR))
			{
				return true;
			}
			if(expr(&rArg))
			{
				if(!param)
				{
					tkerr("too many arguments in function call");
				}
				if(!convTo(&rArg.type,&param->type))
				{
					tkerr("in call, cannot convert the argument type to the parameter type");
				}	
				addRVal(&owner->fn.instr,rArg.lval,&rArg.type);
				insertConvIfNeeded(lastInstr(owner->fn.instr),&rArg.type,&param->type);
				param=param->next;
				while(consume(COMMA))
				{
					if(expr(&rArg))
					{
						if(!param)
						{
							tkerr("too many arguments in function call");
						}
						if(!convTo(&rArg.type,&param->type))
						{
							tkerr("in call, cannot convert the argument type to the parameter type");
						}
						addRVal(&owner->fn.instr,rArg.lval,&rArg.type);
						insertConvIfNeeded(lastInstr(owner->fn.instr),&rArg.type,&param->type);
						param=param->next;
						continue;
					}
					else
					{
						tkerr("Missing function argument after ,");
					}
					iTk=startAtom;
					return false;
				}
				if(consume(RPAR))
				{
					if(param)
					{
						tkerr("too few arguments in function call");
					}
					*r=(Ret){s->type,false,true};
					if(s->fn.extFnPtr)
					{
						addInstr(&owner->fn.instr,OP_CALL_EXT)->arg.extFnPtr=s->fn.extFnPtr;
					}
					else
					{
						addInstr(&owner->fn.instr,OP_CALL)->arg.instr=s->fn.instr;
					}
					return true;
				}
				else
				{
					tkerr("Missing ) to end function call");
				}
			}
		}
		else
		{
			if(s->kind==SK_FN)
			{
				tkerr("a function can only be called");
			}
			*r=(Ret){s->type,true,s->type.n>=0};
			if(s->kind==SK_VAR)
			{
				if(s->owner==NULL) // global variables
				{
					addInstr(&owner->fn.instr,OP_ADDR)->arg.p=s->varMem;
				}
				else // local variables
				{
					switch(s->type.tb)
					{
						case TB_INT:
							addInstrWithInt(&owner->fn.instr,OP_FPADDR_I,s->varIdx+1);
							break;
						case TB_DOUBLE:
							addInstrWithInt(&owner->fn.instr,OP_FPADDR_F,s->varIdx+1);
							break;
						default:
							break;
					}
				}
			}
			if(s->kind==SK_PARAM)
			{
				switch(s->type.tb)
				{
					case TB_INT:
						addInstrWithInt(&owner->fn.instr,OP_FPADDR_I,s->paramIdx-symbolsLen(s->owner->fn.params)-1);
						break;
					case TB_DOUBLE:
						addInstrWithInt(&owner->fn.instr,OP_FPADDR_F,s->paramIdx-symbolsLen(s->owner->fn.params)-1);
						break;
					default:
						break;
				}
			}
		}
		return true;
	}
	
	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	if(consume(INT))
	{
		*r=(Ret){{TB_INT,NULL,-1},false,true};
		addInstrWithInt(&owner->fn.instr,OP_PUSH_I,consumedTk->i);
		return true;
	}
	if(consume(DOUBLE))
	{
		*r=(Ret){{TB_DOUBLE,NULL,-1},false,true};
		addInstrWithDouble(&owner->fn.instr,OP_PUSH_F,consumedTk->d);
		return true;
	}
	if(consume(CHAR))
	{
		*r=(Ret){{TB_CHAR,NULL,-1},false,true};
		return true;
	}
	if(consume(STRING))
	{
		*r=(Ret){{TB_CHAR,NULL,0},false,true};
		return true;
	}
	if(consume(LPAR))
	{
		if(expr(r))
		{
			if(consume(RPAR))
			{
				return true;
			}
			else
			{
				tkerr("Missing ) after expression");
			}
		}
	}
	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	return false;
}

//exprPostfix: exprPostfix LBRACKET expr RBRACKET | exprPostfix DOT ID | exprPrimary
bool exprPostfixPrim(Ret *r)
{
	fprintf(fout, "exprPostfixPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(consume(LBRACKET))
	{
		Ret idx;
		if(expr(&idx))
		{
			if(consume(RBRACKET))
			{
				if(r->type.n<0)
				{
					tkerr("only an array can be indexed");
				}
				Type tInt={TB_INT,NULL,-1};
				if(!convTo(&idx.type,&tInt))
				{
					tkerr("the index is not convertible to int");
				}
				r->type.n=-1;
				r->lval=true;
				r->ct=false;
				if(exprPostfixPrim(r))
				{
					return true;
				}
			}
			else
			{
				tkerr("Missing ] after expression");
			}
		}
	}

	iTk=startAtom;
	if(consume(DOT))
	{
		if(consume(ID))
		{
			Token* tkName=consumedTk;
			if(r->type.tb!=TB_STRUCT)
			{
				tkerr("a field can only be selected from a struct");
			}
			Symbol *s=findSymbolInList(r->type.s->structMembers,tkName->text);
			if(!s)
			{
				tkerr("the structure %s does not have a field%s",r->type.s->name,tkName->text);
			}
			*r=(Ret){s->type,true,s->type.n>=0};
			if(exprPostfixPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("Missing struct attribute after .");
		}
	}
	return true;
}

bool exprPostfix(Ret *r)
{
	fprintf(fout, "exprPostfix %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprPrimary(r))
	{
		if(exprPostfixPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Ret *r)
{
	fprintf(fout, "exprUnary %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(consume(SUB) || consume(NOT))
	{
		if(exprUnary(r))
		{
			if(!canBeScalar(r))
			{
				tkerr("unary - or ! must have a scalar operand");
			}
			r->lval=false;
			r->ct=true;
			return true;
		}
		else
		{
			iTk=startAtom;
			if(consume(NOT))
			{
				tkerr("Missing expression after !");
			}
		}
	}

	iTk=startAtom;
	if(exprPostfix(r))
	{
		return true;
	}

	iTk=startAtom;
	return false;
}

//exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
//exprCast: LPAR {Type t;} typeBase[&t] arrayDecl[&t]? RPAR exprCast | exprUnary
bool exprCast(Ret *r)
{
	fprintf(fout, "exprCast %s\n",tkCode(iTk->code));		//debug
	Type t;
	Token *startAtom=iTk;
	if(consume(LPAR))
	{
		Ret op;
		if(typeBase(&t))
		{
			if(arrayDecl(&t))
			{}
			if(consume(RPAR))
			{
				if(exprCast(&op))
				{
					if(t.tb==TB_STRUCT)
					{
						tkerr("cannot convert to a struct type");
					}
					if(op.type.tb==TB_STRUCT)
					{
						tkerr("cannot convert a struct");
					}
					if(op.type.n>=0&&t.n<0)
					{
						tkerr("an array can be converted only to another array");
					}
					if(op.type.n<0&&t.n>=0)
					{
						tkerr("a scalar can be converted only to another scalar");
					}
					*r=(Ret){t,false,true};
					return true;
				}
				else
				{
					tkerr("Missing expression after cast");
				}
			}
			else
			{
				tkerr("Missing ) after type");
			}
		}
	}

	iTk=startAtom;
	if(exprUnary(r))
	{
		return true;
	}
	iTk=startAtom;
	return false;
}

//exprMul: exprMul ( MUL | DIV ) exprCast | exprCast
bool exprMulPrim(Ret *r)
{
	fprintf(fout, "exprMulPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret right;
	Token *op;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(consume(MUL) || consume(DIV))
	{
		op = startAtom;
		Instr *lastLeft=lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr,r->lval,&r->type);
		if(exprCast(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type,&right.type,&tDst))
			{
				tkerr("invalid operand type for * or /");
			}
			addRVal(&owner->fn.instr,right.lval,&right.type);
			insertConvIfNeeded(lastLeft,&r->type,&tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
			switch(op->code)
			{
				case MUL:
					switch(tDst.tb)
					{
						case TB_INT:
							addInstr(&owner->fn.instr,OP_MUL_I);
							break;
						case TB_DOUBLE:
							addInstr(&owner->fn.instr,OP_MUL_F);
							break;
						default:
							break;
					}
					break;
				case DIV:
					switch(tDst.tb)
					{
						case TB_INT:
							addInstr(&owner->fn.instr,OP_DIV_I);
							break;
						case TB_DOUBLE:
							addInstr(&owner->fn.instr,OP_DIV_F);
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			*r=(Ret){tDst,false,true};
			if(exprMulPrim(r))
			{
				return true;
			}
		}
		else
		{
			if(owner)
				delInstrAfter(startInstr);
			iTk=startAtom;
			if(consume(MUL))
			{
				tkerr("Missing expression after *");
			}
			else if(consume(DIV))
			{
				tkerr("Missing expression after /");
			}
		}
	}
	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	return true;
}

bool exprMul(Ret *r)
{
	fprintf(fout, "exprMul %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprCast(r))
	{
		if(exprMulPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprAdd: exprAdd ( ADD | SUB ) exprMul | exprMul
bool exprAddPrim(Ret *r)
{
	fprintf(fout, "exprAddPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret right;
	Token *op;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(consume(ADD) || consume(SUB))
	{
		op=startAtom;
		Instr *lastLeft=lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr,r->lval,&r->type);
		if(exprMul(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type,&right.type,&tDst))
			{
				tkerr("invalid operand type for + or -");
			}
			addRVal(&owner->fn.instr,right.lval,&right.type);
			insertConvIfNeeded(lastLeft,&r->type,&tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
			switch(op->code)
			{
				case ADD:
					switch(tDst.tb)
					{
						case TB_INT:
							addInstr(&owner->fn.instr,OP_ADD_I);
							break;
						case TB_DOUBLE:
							addInstr(&owner->fn.instr,OP_ADD_F);
							break;
						default:
							break;
					}
					break;
				case SUB:
					switch(tDst.tb)
					{
						case TB_INT:
							addInstr(&owner->fn.instr,OP_SUB_I);
							break;
						case TB_DOUBLE:
							addInstr(&owner->fn.instr,OP_SUB_F);
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			*r=(Ret){tDst,false,true};
			if(exprAddPrim(r))
			{
				return true;
			}
		}
		else
		{
			if(owner)
				delInstrAfter(startInstr);
			iTk=startAtom;
			if(consume(ADD))
			{
				tkerr("Missing expression after +");
			}
			else if(consume(SUB))
			{
				tkerr("Missing expression after -");
			}
		}
	}
	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	return true;
}

bool exprAdd(Ret *r)
{
	fprintf(fout, "exprAdd %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprMul(r))
	{
		if(exprAddPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprRel: exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd
bool exprRelPrim(Ret *r)
{
	fprintf(fout, "exprRelPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret right;
	Token* op;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(consume(LESS) || consume(LESSEQ) || consume(GREATER) || consume(GREATEREQ))
	{
		op=startAtom;
		Instr *lastLeft=lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr,r->lval,&r->type);
		if(exprAdd(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type,&right.type,&tDst))
			{
				tkerr("invalid operand type for <, <=, >,>=");
			}
			addRVal(&owner->fn.instr,right.lval,&right.type);
			insertConvIfNeeded(lastLeft,&r->type,&tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
			switch(op->code)
			{
				case LESS:
					switch(tDst.tb)
					{
						case TB_INT:
							addInstr(&owner->fn.instr,OP_LESS_I);
							break;
						case TB_DOUBLE:
							addInstr(&owner->fn.instr,OP_LESS_F);
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
			*r=(Ret){{TB_INT,NULL,-1},false,true};
			if(exprRelPrim(r))
			{
				return true;
			}
		}
		else
		{
			iTk=startAtom;
			if(consume(LESS))
			{
				tkerr("Missing expression after <");
			}
			if(consume(LESSEQ))
			{
				tkerr("Missing expression after <=");
			}
			if(consume(GREATER))
			{
				tkerr("Missing expression after >");
			}
			if(consume(GREATEREQ))
			{
				tkerr("Missing expression after >=");
			}
		}
	}
	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	return true;
}

bool exprRel(Ret *r)
{
	fprintf(fout, "exprRel %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprAdd(r))
	{
		if(exprRelPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprEq: exprEq ( EQUAL | NOTEQ ) exprRel | exprRel
bool exprEqPrim(Ret *r)
{
	fprintf(fout, "exprEqPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret right;
	if(consume(EQUAL) || consume(NOTEQ))
	{
		if(exprRel(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type,&right.type,&tDst))
				{
				tkerr("invalid operand type for == or!=");
				}
			*r=(Ret){{TB_INT,NULL,-1},false,true};
			if(exprEqPrim(r))
			{
				return true;
			}
		}
		else
		{
			tkerr("Missing expression after == or !=");
		}
	}
	iTk=startAtom;
	return true;
}

bool exprEq(Ret *r)
{
	fprintf(fout, "exprEq %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprRel(r))
	{
		if(exprEqPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprAnd: exprAnd AND exprEq | exprEq
bool exprAndPrim(Ret *r)
{
	fprintf(fout, "exprAndPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret right;
	if(consume(AND))
	{
		if(exprEq(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type,&right.type,&tDst))
			{
				tkerr("invalid operand type for &&");
			}
			*r=(Ret){{TB_INT,NULL,-1},false,true};
			if(exprAndPrim(r))
			{
				return true;
			}	
		}
		else
		{
			tkerr("Missing expression after &&");
		}
	}
	iTk=startAtom;
	return true;
}

bool exprAnd(Ret *r)
{
	fprintf(fout, "exprAnd %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprEq(r))
	{
		if(exprAndPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprOr: exprOr OR exprAnd | exprAnd
bool exprOrPrim(Ret* r)
{
	fprintf(fout, "exprOrPrim %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret right;
	if(consume(OR))
	{
		if(exprAnd(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type, &right.type, &tDst))
			{
				tkerr("Invalid operand type for ||");
			}
			*r=(Ret){{TB_INT, NULL, -1}, false, true};
			exprOrPrim(r);
			return true;	
		}
		else
		{
			tkerr("Missing expression after ||");
		}
	}
	iTk=startAtom;
	return true;
}

bool exprOr(Ret* r)
{
	fprintf(fout, "exprOr %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprAnd(r))
	{
		if(exprOrPrim(r))
		{
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r)
{
	fprintf(fout, "exprAssign %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	Ret rDst;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(exprUnary(&rDst))
	{
		if(consume(ASSIGN))
		{
			if(exprAssign(r))
			{
				if(!rDst.lval)
				{
					tkerr("the assign destination must be a left-value");
				}
				if(rDst.ct)
				{
					tkerr("the assign destination cannot be constant");
				}
				if(!canBeScalar(&rDst))
				{
					tkerr("the assign destination must be scalar");
				}
				if(!canBeScalar(r))
				{
					tkerr("the assign source must be scalar");
				}
				if(!convTo(&r->type,&rDst.type))
				{
					tkerr("the assign source cannot be converted to destination");
				}
				r->lval=false;
				r->ct=true;
				addRVal(&owner->fn.instr,r->lval,&r->type);
				insertConvIfNeeded(lastInstr(owner->fn.instr),&r->type,&rDst.type);
				switch(rDst.type.tb)
				{
					case TB_INT:
						addInstr(&owner->fn.instr,OP_STORE_I);
						break;
					case TB_DOUBLE:
						addInstr(&owner->fn.instr,OP_STORE_F);
						break;
					default:
						break;
				}
				return true;
			}
			else
			{	
				tkerr("Missing expresion after '='\n");
			}
		}
	}

	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	if(exprOr(r))
	{
		return true;
	}
	return false;
}

//expr: exprAssign
bool expr(Ret *r)
{
	fprintf(fout, "expr %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(exprAssign(r))
	{
		return true;
	}
	iTk=startAtom;
	return false;
}

//stmCompound: LACC ( varDef | stm )* RACC
//stmCompound[in bool newDomain]: LACC {if(newDomain)pushDomain();} ( varDef | stm )* RACC {if(newDomain)dropDomain();}
bool stmCompound(bool newDomain)
{
	fprintf(fout, "stmCompound %s\n",tkCode(iTk->code));		//debug
	Token* startAtom=iTk;
	if(consume(LACC))
	{
		if(newDomain)
		{
			pushDomain();
		}
		while(varDef() || stm());
		if(consume(RACC))
		{
			if(newDomain)
			{
				dropDomain();
			}
			return true;
		}
		else
		{
			tkerr("Syntax err in started code block");
		}
	}
	iTk=startAtom;
	return false;
}

//stm: stmCompound | IF LPAR expr RPAR stm ( ELSE stm )? | WHILE LPAR expr RPAR stm | RETURN expr? SEMICOLON | expr? SEMICOLON
bool stm()
{
	fprintf(fout, "stm %s\n", tkCode(iTk->code));		//debug
	Token* startAtom = iTk;
	Ret rCond, rExpr;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(stmCompound(true))
	{
		return true;
	}
	if(consume(IF))
	{
		if(consume(LPAR))
		{
			if(expr(&rCond))
			{
				if(!canBeScalar(&rCond))
				{
					tkerr("the if condition must be a scalar value");
				}	
				if(consume(RPAR))
				{
					addRVal(&owner->fn.instr,rCond.lval,&rCond.type);
					Type intType={TB_INT,NULL,-1};
					insertConvIfNeeded(lastInstr(owner->fn.instr),&rCond.type,&intType);
					Instr *ifJF=addInstr(&owner->fn.instr,OP_JF);
					if(stm())
					{
						if(consume(ELSE))
						{
							Instr *ifJMP=addInstr(&owner->fn.instr,OP_JMP);
							ifJF->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
							if(stm())
							{
								ifJMP->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
								return true;
							}
						}
						else
						{
							ifJF->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
							return true;
						}
					}
					else 
					{
						tkerr("Missing statement in if");
					}
				}
				else
				{
					tkerr("Missing ) after expression in if");
				}
			}
			else
			{
				tkerr("Missing condition in if");
			}
		}
		else
		{
			tkerr("Missing ( after if");
		}
	}

	if(owner)
		delInstrAfter(startInstr);
	iTk = startAtom;
	if(consume(WHILE))
	{
		Instr *beforeWhileCond=lastInstr(owner->fn.instr);
		if(consume(LPAR))
		{
			if(expr(&rCond))
			{
				if(!canBeScalar(&rCond))
				{
					tkerr("the while condition must be a scalar value");
				}
				if(consume(RPAR))
				{
					addRVal(&owner->fn.instr,rCond.lval,&rCond.type);
					Type intType={TB_INT,NULL,-1};
					insertConvIfNeeded(lastInstr(owner->fn.instr),&rCond.type,&intType);
					Instr *whileJF=addInstr(&owner->fn.instr,OP_JF);
					if(stm())
					{
						addInstr(&owner->fn.instr,OP_JMP)->arg.instr=beforeWhileCond->next;
						whileJF->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
						return true;
					}
					else
					{
						tkerr("Missing statement in while");
					}
				}
				else
				{
					tkerr("Missing ) after expression in while");
				}
			}
			else
			{
				tkerr("Missing expression in while");
			}
		}
		else 
		{
			tkerr("Missing ( after while");
		}
	}
	
	if(owner)
		delInstrAfter(startInstr);
	iTk = startAtom;
	if(consume(RETURN))
	{
		if(expr(&rExpr))
		{
			if(owner->type.tb==TB_VOID)
			{
				tkerr("a void function cannot return a value");
			}	
			if(!canBeScalar(&rExpr))
			{
				tkerr("the return value must be a scalar value");
			}
			if(!convTo(&rExpr.type,&owner->type))
			{
				tkerr("cannot convert the return expression type to the function return type");
			}
			addRVal(&owner->fn.instr,rExpr.lval,&rExpr.type);
			insertConvIfNeeded(lastInstr(owner->fn.instr),&rExpr.type,&owner->type);
			addInstrWithInt(&owner->fn.instr,OP_RET,symbolsLen(owner->fn.params));
		}
		else
		{
			if(owner->type.tb!=TB_VOID)
			{
				tkerr("a non-void function must return a value");
			}
			addInstr(&owner->fn.instr,OP_RET_VOID);
		}
		if(consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("Missing ; after return");
		}
	}

	if(owner)
		delInstrAfter(startInstr);
	iTk = startAtom;
	if(expr(&rExpr))
	{
		if(rExpr.type.tb!=TB_VOID)
		{
		addInstr(&owner->fn.instr,OP_DROP);
		}
		if(consume(SEMICOLON))
		{
			return true;
		} 
		else
		{
			tkerr("Missing ; after expression");
		}
	}

	if(owner)
		delInstrAfter(startInstr);
	iTk = startAtom;
	if(consume(SEMICOLON))
	{
		return true;
	} 
	return false;
}

//fnParam: typeBase ID arrayDecl?
bool fnParam()
{
	fprintf(fout, "fnParam %s\n",tkCode(iTk->code));		//debug
	Type t;
	Token* startAtom=iTk;
	if(typeBase(&t))
	{
		if(consume(ID))
		{
			Token* tkName=consumedTk;
			if(arrayDecl(&t))
			{
				t.n=0;
			}
			Symbol *param=findSymbolInDomain(symTable,tkName->text);
			if(param)
				tkerr("symbol redefinition: %s",tkName->text);
			param=newSymbol(tkName->text,SK_PARAM);
			param->type=t;
			param->owner=owner;
			param->paramIdx=symbolsLen(owner->fn.params);
			// parametrul este adaugat atat la domeniul curent, cat si la parametrii fn
			addSymbolToDomain(symTable,param);
			addSymbolToList(&owner->fn.params,dupSymbol(param));
			return true;
		}
	}
	iTk=startAtom;
	return false;
}

//fnDef: ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
bool fnDef()
{
	fprintf(fout, "fnDef %s\n",tkCode(iTk->code));		//debug
	Type t;
	Token *startAtom=iTk;
	Instr *startInstr=owner?lastInstr(owner->fn.instr):NULL;
	if(typeBase(&t) || consume(VOID))
	{
		if(startAtom->code==VOID)
		{
			t.tb=TB_VOID;
		}
		if(consume(ID))
		{
			Token* tkName = consumedTk;
			if(consume(LPAR))
			{
				Symbol *fn=findSymbolInDomain(symTable,tkName->text);
				if(fn)
					tkerr("symbol redefinition: %s",tkName->text);
				fn=newSymbol(tkName->text,SK_FN);
				fn->type=t;
				addSymbolToDomain(symTable,fn);
				owner=fn;
				pushDomain();
				if(fnParam())
				{
					for(;;)
					{
						if(consume(COMMA))
						{
							if(fnParam())
							{
								continue;
							}
							else
							{
								tkerr("Missing function parameter after ,");
							}
						}
						break;
					}
				}
				if(consume(RPAR))
				{
					addInstr(&fn->fn.instr,OP_ENTER);
					if(stmCompound(false))
					{
						fn->fn.instr->arg.i=symbolsLen(fn->fn.locals);
						if(fn->type.tb==TB_VOID)
						{
							addInstrWithInt(&fn->fn.instr,OP_RET_VOID,symbolsLen(fn->fn.params));
						}
						dropDomain();
						owner=NULL;
						return true;
					}
					else
					{
						tkerr("Missing function body");
					}
				}
				else
				{
					tkerr("Missing ) after function parameters");
				}
			}
		}
	}
	if(owner)
		delInstrAfter(startInstr);
	iTk=startAtom;
	return false;
}

//arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t)
{
	fprintf(fout, "arrayDecl %s\n",tkCode(iTk->code));		//debug
	Token *startAtom=iTk;
	if(consume(LBRACKET))
	{
		if(consume(INT))
		{
			Token *tkSize=consumedTk;
			t->n=tkSize->i;
		}
		else
		{
			t->n=0;
		}

		if(consume(RBRACKET))
		{
			return true;
		}
		else
		{
			tkerr("Missing ] after array declaration");
		}
	}
	iTk=startAtom;
	return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t)
{
	fprintf(fout, "typeBase %s\n",tkCode(iTk->code));		//debug
	t->n=-1;
	Token *startAtom=iTk;
	if(consume(TYPE_INT))
	{
		t->tb=TB_INT;
		return true;
	}
	if(consume(TYPE_DOUBLE))
	{
		t->tb=TB_DOUBLE;
		return true;
	}
	if(consume(TYPE_CHAR))
	{
		t->tb=TB_CHAR;
		return true;
	}
	if(consume(STRUCT))
	{
		if(consume(ID))
		{
			Token* tkName=consumedTk;
			t->tb=TB_STRUCT;
			t->s=findSymbol(tkName->text);
			if(!t->s)
				tkerr("structura nedefinita: %s",tkName->text);
			return true;
		}
		else
		{
			tkerr("Invalid struct definition");
		}
	}
	iTk=startAtom;
	return false;
}

//varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef()
{
	fprintf(fout, "varDef %s\n",tkCode(iTk->code));		//debug
	Type t;
	Token *startAtom=iTk;
	if(typeBase(&t))
	{
		if(consume(ID))
		{
			Token* tkName=consumedTk;
			if(arrayDecl(&t))
			{
				if(t.n==0)
					tkerr("a vector variable must have a specified dimension");
			}
			if(consume(SEMICOLON))
			{
				Symbol *var=findSymbolInDomain(symTable,tkName->text);
				if(var)
					tkerr("symbol redefinition: %s",tkName->text);
				var=newSymbol(tkName->text,SK_VAR);
				var->type=t;
				var->owner=owner;
				addSymbolToDomain(symTable,var);
				if(owner){
					switch(owner->kind){
						case SK_FN:
							var->varIdx=symbolsLen(owner->fn.locals);
							addSymbolToList(&owner->fn.locals,dupSymbol(var));
							break;
						case SK_STRUCT:
							var->varIdx=typeSize(&owner->type);
							addSymbolToList(&owner->structMembers,dupSymbol(var));
							break;
						default:							//change here
							break;
					}
				}
				else
				{
					var->varMem=safeAlloc(typeSize(&t));
				}
				return true;	
			}
			else 
			{
				tkerr("Missing ; after variable declaration");
			}
		}
	}
	iTk=startAtom;
	return false;
}

//structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
	fprintf(fout, "structDef %s\n", tkCode(iTk->code));		//debug
	Token* start = iTk;
	if(consume(STRUCT))
	{
		if(consume(ID))
		{
			Token* tkName = consumedTk;
			if(consume(LACC))
			{
				Symbol *s=findSymbolInDomain(symTable,tkName->text);
				if(s)
					tkerr("symbol redefinition: %s",tkName->text);
				s=addSymbolToDomain(symTable,newSymbol(tkName->text,SK_STRUCT));
				s->type.tb=TB_STRUCT;
				s->type.s=s;
				s->type.n=-1;
				pushDomain();
				owner=s;
				for(;;)
				{
					if(varDef())
					{}
					else
					{ 
						break;
					}
				}
				if(consume(RACC))
				{
					if(consume(SEMICOLON))
					{
						owner=NULL;
						dropDomain();
						return true;
					}
					else
					{
						tkerr("Missing ; after struct");
					}
				}
				else
				{
					tkerr("Missing } after struct");
				}
			}
		} 
		else
		{
			tkerr("Missing struct name");
		}
	}
	iTk=start;
	return false;
	}

// unit: ( structDef | fnDef | varDef )* END
bool unit()
{
	fprintf(fout, "unit %s\n",tkCode(iTk->code));		//debug
	for(;;)
	{
		if(structDef()){}
		else if(fnDef()){}
		else if(varDef()){}
		else break;
	}
	if(consume(END))
	{
		return true;
	}
	return false;
}

void parse(Token *tokens, char outpath[])
{
	fout = fopen(outpath, "wb");
	if(!fout)
    {
        err("Cannot create output for parser");
    }

	iTk=tokens;
	if(!unit())
		tkerr("syntax error");
	
	fclose(fout);
}

char* tkCode(int code)
{
	char* s = safeAlloc(10 * sizeof(char));
	if (code == ID)
			strcpy(s, "ID");
	else if (code == INT)
			strcpy(s, "INT");
	else if (code == DOUBLE)
			strcpy(s, "DOUBLE");
	else if (code == CHAR) 
			strcpy(s, "CHAR");
	else if (code == STRING) 
			strcpy(s, "STRING");
	else if (code == STRUCT) 
			strcpy(s, "STRUCT");
	else if (code == TYPE_INT) 
			strcpy(s, "TYPE_INT");
	else if (code == TYPE_DOUBLE) 
			strcpy(s, "TYPE_DOUBLE");
	else if (code == TYPE_CHAR) 
			strcpy(s, "TYPE_CHAR");
	else if (code == LPAR) 
			strcpy(s, "LPAR");
	else if (code == RPAR) 
			strcpy(s, "RPAR");
	else if (code == LACC) 
			strcpy(s, "LACC");
	else if (code == RACC) 
			strcpy(s, "RACC");
	else if (code == LBRACKET) 
			strcpy(s, "LBRACKET");
	else if (code == RBRACKET) 
			strcpy(s, "RBRACKET");
	else if (code == COMMA) 
			strcpy(s, "COMMA");
	else if (code == SEMICOLON) 
			strcpy(s, "SEMICOLON");
	else if (code == DOT) 
			strcpy(s, "DOT");
	else if (code == ASSIGN) 
			strcpy(s, "ASSIGN");
	else if (code == IF) 
			strcpy(s, "IF");
	else if (code == ELSE) 
			strcpy(s, "ELSE");
	else if (code == WHILE) 
			strcpy(s, "WHILE");
	else if (code == RETURN) 
			strcpy(s, "RETURN");
	else if (code == VOID) 
			strcpy(s, "VOID");
	else if (code == END) 
			strcpy(s, "END");
	else if (code == ADD) 
			strcpy(s, "ADD");
	else if (code == SUB) 
			strcpy(s, "SUB");
	else if (code == MUL) 
			strcpy(s, "MUL");
	else if (code == DIV) 
			strcpy(s, "DIV");
	else if (code == NOT) 
			strcpy(s, "NOT");
	else if (code == OR) 
			strcpy(s, "OR");
	else if (code == AND) 
			strcpy(s, "AND");
	else if (code == EQUAL) 
			strcpy(s, "EQUAL");
	else if (code == LESS) 
			strcpy(s, "LESS");
	else if (code == GREATER) 
			strcpy(s, "GREATER");
	else if (code == GREATEREQ) 
			strcpy(s, "GREATEREQ");
	else if (code == LESSEQ) 
			strcpy(s, "LESSEQ");
	else if (code == NOTEQ) 
			strcpy(s, "NOTEQ");
	else 
			strcpy(s, "UNKNOWN");

	return s;
}
