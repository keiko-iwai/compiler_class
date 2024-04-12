%option noinput nounput noyywrap nodefault
%{
#include <string>
#include "exprAST.h"
#include "parser.hpp"
#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
%}

%%

[ \t\n]                 ;
[a-zA-Z_][a-zA-Z0-9_]*  SAVE_TOKEN; return TIDENTIFIER;
[0-9]+\.[0-9]*          SAVE_TOKEN; return TDOUBLE;
[0-9]+                  SAVE_TOKEN; return TINTEGER;
"="                     return TOKEN(TEQUAL);
"("                     return TOKEN(TLPAREN);
")"                     return TOKEN(TRPAREN);
"{"                     return TOKEN(TLBRACE);
"}"                     return TOKEN(TRBRACE);
"."                     return TOKEN(TDOT);
","                     return TOKEN(TCOMMA);
"=="                    SAVE_TOKEN; return TCEQ;
"!="                    SAVE_TOKEN; return TCNE;
"<"                     SAVE_TOKEN; return TCLT;
"<="                    SAVE_TOKEN; return TCLE;
">"                     SAVE_TOKEN; return TCGT;
">="                    SAVE_TOKEN; return TCGE;
"+"                     SAVE_TOKEN; return TPLUS;
"-"                     SAVE_TOKEN; return TMINUS;
"*"                     SAVE_TOKEN; return TMUL;
"/"                     SAVE_TOKEN; return TDIV;
.                       printf("[Lex] ERROR: Unknown token!\n"); yyterminate();

%%
