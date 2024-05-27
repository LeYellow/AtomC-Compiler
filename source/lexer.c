#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lexer.h"
#include "utils.h"

Token *tokens;	// single linked list of tokens
Token *lastTk;	// the last token in list

int line=1;		// the current line in the input file

// adds a token to the end of the tokens list and returns it
// sets its code and line
Token *addTk(int code)
{
	Token *tk=safeAlloc(sizeof(Token));
	tk->code=code;
	tk->line=line;
	tk->next=NULL;
	if(lastTk)
	{
		lastTk->next=tk;
	}
	else
	{
		tokens=tk;
	}
	lastTk=tk;
	return tk;
}

char *extract(const char *begin,const char *end)
{
	int length = end-begin;
	char *result = safeAlloc(length+1);
	strncpy(result, begin, length);
	result[length] = '\0';
	return result;
}

Token *tokenize(const char *pch)
{
	const char *start;
	Token *tk;
	for(;;)
	{
		switch(*pch)
		{
			case ' ':
			case '\t':
				pch++;
				break;
			case '\r':		// handles different kinds of newlines (Windows: \r\n, Linux: \n, MacOS, OS X: \r or \n)
				if(pch[1]=='\n')pch++;
				// fallthrough to \n
			case '\n':
				line++;
				pch++;
				break;
			case '\0':
				addTk(END);
				return tokens;
			case ',':
				addTk(COMMA);
				pch++;
				break;
			case ';':
				addTk(SEMICOLON);
				pch++;
				break;
			case '(':
				addTk(LPAR);
				pch++;
				break;
			case ')':
				addTk(RPAR);
				pch++;
				break;
			case '[':
				addTk(LBRACKET);
				pch++;
				break;
			case ']':
				addTk(RBRACKET);
				pch++;
				break;
			case '{':
				addTk(LACC);
				pch++;
				break;
			case '}':
				addTk(RACC);
				pch++;
				break;
			case '+':
				addTk(ADD);
				pch++;
				break;
			case '-':
				addTk(SUB);
				pch++;
				break;
			case '*':
				addTk(MUL);
				pch++;
				break;
			case '.':					//
				addTk(DOT);
				pch++;
				break;
			case '/':
				if(pch[1]=='/')		//single line comment
				{
					for(pch+=2 ; *pch!='\r' && *pch!='\n' && *pch!='\0' ; pch++)
					{}
				}
				else
				{
					addTk(DIV);
					pch++;
				}
				break;
			case '|':
				if(pch[1]=='|')
				{
					addTk(OR);
					pch+=2;
				}
				else
				{
					err("'|' invalid opperand.");
				}
				break;
			case '&':
				if(pch[1]=='&')
				{
					addTk(AND);
					pch+=2;
				}
				else
				{
					err("'&' invalid opperand.");
				}
				break;
			case '=':
				if(pch[1]=='=')
				{
					addTk(EQUAL);
					pch+=2;
				}
				else
				{
					addTk(ASSIGN);
					pch++;
				}
				break;
			case '<':
				if(pch[1]=='=')
				{
					addTk(LESSEQ);
					pch+=2;
				}
				else
				{
					addTk(LESS);
					pch++;
				}
				break;
			case '>':
				if(pch[1]=='=')
				{
					addTk(GREATEREQ);
					pch+=2;
				}
				else
				{
					addTk(GREATER);
					pch++;
				}
				break;
			case '!':
				if(pch[1]=='=')
				{
					addTk(NOTEQ);
					pch+=2;
				}
				else
				{
					addTk(NOT);
					pch++;
				}
				break;
			default:
				if(isalpha(*pch) || *pch=='_')		//normal functioning
				{
					for(start=pch++; isalnum(*pch) || *pch=='_'; pch++)
					{
					}
					char *text=extract(start,pch);
					if(strcmp(text,"char")==0)
					{
						addTk(TYPE_CHAR);
					}
					else if(strcmp(text,"int")==0)
					{
						addTk(TYPE_INT);
					}
					else if(strcmp(text,"double")==0)
					{
						addTk(TYPE_DOUBLE);
					}
					else if(strcmp(text,"struct")==0)
					{
						addTk(STRUCT);
					}
					else if(strcmp(text,"void")==0)
					{
						addTk(VOID);
					}
					else if(strcmp(text,"return")==0)
					{
						addTk(RETURN);
					}
					else if(strcmp(text,"while")==0)
					{
						addTk(WHILE);
					}
					else if(strcmp(text,"if")==0)
					{
						addTk(IF);
					}
					else if(strcmp(text,"else")==0)
					{
						addTk(ELSE);
					}
					else
					{
						tk=addTk(ID);
						tk->text=text;
					}
				}
				else if(isalpha(*pch) || *pch=='"' || *pch=='\'')	//for quoted words //alphanumerice
				{
					int isQuote=0;
					int stringEnd=0;
					pch++;
					for(start=pch++; isalnum(*pch) || *pch=='"' || *pch=='\''; pch++)
					{
						if(*pch=='\'')
						{
							isQuote=1;
						}
						if(*pch == '"')
						{
							stringEnd=1;
							pch++;
							break;
						}

					}

					if(isQuote)
					{
						char *letter=extract(start, pch-1);
						if(*pch=='\'')
						{
							err("quotation did not end correctly");
						}
						tk=addTk(CHAR);
						tk->c=*letter;
					}
					else
					{

						char *str=extract(start, pch-1);
						if((!stringEnd))
						{
							err("string did not end correctly");
						}
						tk=addTk(STRING);
						tk->text=str;
					}
				}
				else if(isdigit(*pch))		//case for numbers
				{
					int dot=0;
					int specialNumber=0;
					int invalidExponent=0;
					const char *ePCH;
					for(start=pch++; isalnum(*pch) || *pch=='.' || *pch=='e' || *pch=='E' || *pch=='-' || *pch=='+'; pch++)
					{
						if(*pch=='.')
						{
							dot=1;
							if((!isalnum(*(pch+1))) && ((*(pch+1)) != 'e' || (*(pch+1)) != 'E'))
							{
								invalidExponent=1;
							}
						}
						if(*pch=='e' || *pch=='E')
						{
							specialNumber=1;
							ePCH=pch;
						}
					}
					char *txt=extract(start, pch);

					if(invalidExponent)
					{
						err("invalid digits after .");
					}
					else if(dot || specialNumber)
					{

						if(specialNumber)
						{
							if(*(ePCH+1)=='-')
							{
								if(!isdigit(*(ePCH+2)))
								{
									err("invalid exponent");
								}
							}
							else if(!isdigit(*(ePCH+1)))
							{
								err("no digit after E");
							}
							
						}

						tk=addTk(DOUBLE);
						char *end_aux;
						tk->d=strtod(txt, &end_aux);
					}
					else
					{
						tk=addTk(INT);
						tk->i=atoi(txt);
					}
				}
				else
				{
					err("invalid char: %c (%d)",*pch,*pch);
				}
		}
	}
}

void showTokens(const Token *tokens, char filepath[])
{
	FILE *out=fopen(filepath, "wb");
    if(!out)
    {
        err("Cannot create output for lexer");
    }

	int lines_number=1;
	for(const Token *tk=tokens;tk;tk=tk->next)
	{
		fprintf(out,"%d\t",tk->line);
		switch (tk->code)
		{
		case TYPE_INT:
			fprintf(out,"TYPE_INT\n");
			break;
		case TYPE_CHAR:
			fprintf(out,"TYPE_CHAR\n");
			break;
		case TYPE_DOUBLE:
			fprintf(out,"TYPE_DOUBLE\n");
			break;
		case ID:
			fprintf(out,"ID:%s\n", tk->text);
			break;
		case LPAR:
			fprintf(out,"LPAR\n");
			break;
		case RPAR:
			fprintf(out,"RPAR\n");
			break;
		case LBRACKET:
			fprintf(out,"LBRACKET\n");
			break;
		case RBRACKET:
			fprintf(out,"RBRACKET\n");
			break;
		case LACC:
			fprintf(out,"LACC\n");
			break;
		case RACC:
			fprintf(out,"RACC\n");
			break;
		case SEMICOLON:
			fprintf(out,"SEMICOLON\n");
			break;
		case INT:
			fprintf(out,"INT:%d\n", tk->i);
			break;
		case DOUBLE:
			fprintf(out,"DOUBLE:%f\n", tk->d);
			break;
		case STRING:
			fprintf(out,"STRING:%s\n", tk->text);
			break;
		case CHAR:
			fprintf(out,"CHAR:%c\n", tk->c);
			break;
		case WHILE:
			fprintf(out,"WHILE\n");
			break;
		case LESS:
			fprintf(out,"LESS\n");
			break;
		case DIV:
			fprintf(out,"DIV\n");
			break;
		case ADD:
			fprintf(out,"ADD\n");
			break;
		case AND:
			fprintf(out,"AND\n");
			break;
		case MUL:
			fprintf(out,"MUL\n");
			break;
		case IF:
			fprintf(out,"IF\n");
			break;
		case ASSIGN:
			fprintf(out,"ASSIGN\n");
			break;
		case EQUAL:
			fprintf(out,"EQUAL\n");
			break;
		case RETURN:
			fprintf(out,"RETURN\n");
			break;
		case END:
			fprintf(out,"END\n");
			break;
		case ELSE:
			fprintf(out,"ELSE\n");
			break;
		case STRUCT:
			fprintf(out,"STRUCT\n");
			break;
		case VOID:
			fprintf(out,"VOID\n");
			break;
		case SUB:
			fprintf(out,"SUB\n");
			break;
		case OR:
			fprintf(out,"OR\n");
			break;
		case NOT:
			fprintf(out,"NOT\n");
			break;
		case NOTEQ:
			fprintf(out,"NOTEQ\n");
			break;
		case LESSEQ:
			fprintf(out,"LESSEQ\n");
			break;
		case GREATER:
			fprintf(out,"GREATER\n");
			break;
		case GREATEREQ:
			fprintf(out,"GREATEREQ\n");
			break;
		case DOT:
			fprintf(out,"DOT\n");
			break;
		default:
			break;
		}
		lines_number+=1;
	}
	fclose(out);
}