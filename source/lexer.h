#pragma once

enum{
	ID, CHAR, INT, DOUBLE, STRING
	// keywords
	,TYPE_CHAR, TYPE_INT, TYPE_DOUBLE, STRUCT, VOID
	// delimiters
	,COMMA, END, SEMICOLON, RETURN
	// operators
	,ASSIGN, EQUAL, ADD, SUB, MUL, DIV, WHILE, IF, ELSE, AND, OR, NOT, NOTEQ, LESS, LESSEQ, GREATER, GREATEREQ, LPAR, RPAR, LACC, RACC, LBRACKET, RBRACKET, DOT
	};

typedef struct Token{
	int code;		// ID, TYPE_CHAR, ...
	int line;		// the line from the input file
	union{
		char *text;		// the chars for ID, STRING (dynamically allocated)
		int i;		// the value for INT
		char c;		// the value for CHAR
		double d;		// the value for DOUBLE
		};
	struct Token *next;		// next token in a simple linked list
	}Token;

Token *tokenize(const char *pch);
void showTokens(const Token *tokens, char filepath[]);
