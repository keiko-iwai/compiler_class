%option noinput nounput noyywrap nodefault yylineno
%{
  #include <string>
  #include "AST.h"
  #include "parser.h"
  #include "error.h"

  #define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
  #define BEGIN_TOKEN BeginToken(yytext);
  #define TOKEN(t) (yylval.token = t)

  /* error reporting */
  #define YY_INPUT(buf,result,max_size)  {\
    result = GetNextChar(buf, max_size); \
    if (  result <= 0  ) \
      result = YY_NULL; \
    }

%}

%%

[ \t\n]                    ;
"//"[^\n]*                 ; /* comment */
"/*"([^*]|\*+[^*/])*\*+"/" ; /* multi-line comment */

"return"                BEGIN_TOKEN; return RETURN;
"if"                    BEGIN_TOKEN; return IF;
"else"                  BEGIN_TOKEN; return ELSE;
"for"                   BEGIN_TOKEN; return FOR;
[a-zA-Z_][a-zA-Z0-9_]*  BEGIN_TOKEN; SAVE_TOKEN; return IDENTIFIER;
[0-9]+\.[0-9]*          BEGIN_TOKEN; SAVE_TOKEN; return DOUBLE;
[0-9]+                  BEGIN_TOKEN; SAVE_TOKEN; return INTEGER;
\"(\\.|[^"\\])*\"       BEGIN_TOKEN; SAVE_TOKEN; return STRINGVAL;
"="                     BEGIN_TOKEN; return TOKEN(EQUAL);
";"                     BEGIN_TOKEN; return TOKEN(SEMICOLON);
"("                     BEGIN_TOKEN; return TOKEN(LPAREN);
")"                     BEGIN_TOKEN; return TOKEN(RPAREN);
"{"                     BEGIN_TOKEN; return TOKEN(LBRACE);
"}"                     BEGIN_TOKEN; return TOKEN(TBRACE);
"."                     BEGIN_TOKEN; return TOKEN(DOT);
","                     BEGIN_TOKEN; return TOKEN(COMMA);
"=="                    BEGIN_TOKEN; SAVE_TOKEN; return EQ;
"!="                    BEGIN_TOKEN; SAVE_TOKEN; return NE;
"<"                     BEGIN_TOKEN; SAVE_TOKEN; return LT;
"<="                    BEGIN_TOKEN; SAVE_TOKEN; return LE;
">"                     BEGIN_TOKEN; SAVE_TOKEN; return GT;
">="                    BEGIN_TOKEN; SAVE_TOKEN; return GE;
"+"                     BEGIN_TOKEN; SAVE_TOKEN; return PLUS;
"-"                     BEGIN_TOKEN; SAVE_TOKEN; return MINUS;
"*"                     BEGIN_TOKEN; SAVE_TOKEN; return MUL;
"/"                     BEGIN_TOKEN; SAVE_TOKEN; return DIV;
.                       printf("[Lex] ERROR: Unknown token!\n"); yyterminate();

%%
