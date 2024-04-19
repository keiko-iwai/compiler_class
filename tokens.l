%option noinput nounput noyywrap nodefault yylineno
%{
#include <string>
#include "exprAST.h"
#include "parser.hpp"

static int next_column = 1;
int column = 1;
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
#define HANDLE_COLUMN column = next_column; next_column += strlen(yytext)
%}

%%

[ \t\n]                    ;
"//"[^\n]*                 ; /* comment */
"/*"([^*]|\*+[^*/])*\*+"/" ; /* multi-line comment */

"return"                HANDLE_COLUMN; return RETURN;
"if"                    HANDLE_COLUMN; return IF;
"else"                  HANDLE_COLUMN; return ELSE;
[a-zA-Z_][a-zA-Z0-9_]*  HANDLE_COLUMN; SAVE_TOKEN; return IDENTIFIER;
-?[0-9]+\.[0-9]*        HANDLE_COLUMN; SAVE_TOKEN; return DOUBLE;
-?[0-9]+                HANDLE_COLUMN; SAVE_TOKEN; return INTEGER;
"="                     HANDLE_COLUMN; return TOKEN(EQUAL);
";"                     HANDLE_COLUMN; return TOKEN(SEMICOLON);
"("                     HANDLE_COLUMN; return TOKEN(LPAREN);
")"                     HANDLE_COLUMN; return TOKEN(RPAREN);
"{"                     HANDLE_COLUMN; return TOKEN(LBRACE);
"}"                     HANDLE_COLUMN; return TOKEN(TRBRACE);
"."                     HANDLE_COLUMN; return TOKEN(DOT);
","                     HANDLE_COLUMN; return TOKEN(COMMA);
"=="                    HANDLE_COLUMN; SAVE_TOKEN; return EQ;
"!="                    HANDLE_COLUMN; SAVE_TOKEN; return NE;
"<"                     HANDLE_COLUMN; SAVE_TOKEN; return LT;
"<="                    HANDLE_COLUMN; SAVE_TOKEN; return LE;
">"                     HANDLE_COLUMN; SAVE_TOKEN; return GT;
">="                    HANDLE_COLUMN; SAVE_TOKEN; return GE;
"+"                     HANDLE_COLUMN; SAVE_TOKEN; return PLUS;
"-"                     HANDLE_COLUMN; SAVE_TOKEN; return MINUS;
"*"                     HANDLE_COLUMN; SAVE_TOKEN; return MUL;
"/"                     HANDLE_COLUMN; SAVE_TOKEN; return DIV;
.                       printf("[Lex] ERROR: Unknown token!\n"); yyterminate();

%%
