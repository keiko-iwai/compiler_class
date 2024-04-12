%{
    #include <cstdio>
    #include "exprAST.h"
    extern int yylex();
    void yyerror(const char *s) { printf("[Bison] ERROR: %s\n", s); }
%}

%union {
    StatementExprAST *stmt;
    ExprAST *expr;
    std::string *string;
    int token;
}

/* Define our terminal symbols (tokens). This should
   match our tokens.l lex file. We also define the node type
   they represent.
 */
%token <string> TIDENTIFIER TINTEGER TDOUBLE
%token <token> TCEQ TCNE TCLT TCLE TCGT TCGE TEQUAL
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TCOMMA TDOT
%token <token> TPLUS TMINUS TMUL TDIV

/* Define the type of node our nonterminal symbols represent.
   The types refer to the %union declaration above. Ex: when
   we call an ident (defined by union type ident) we are really
   calling an (NIdentifier*). It makes the compiler happy.
 */
%type <expr> numeric expr 
%type <block> program stmts
%type <stmt> stmt
%type <token> comparison_op numeric_op

/* Operator precedence for mathematical operators */
%left TPLUS TMINUS
%left TMUL TDIV

%start program

%%

program : stmts {  }
        ;
        
stmts : stmt {  }
      | stmts stmt {  }
      ;

stmt : expr { }
     ;

numeric : TINTEGER { }
        | TDOUBLE  { }
        ;
    
expr : numeric
     | expr comparison_op expr {  }
     | expr numeric_op expr {  }
     | TLPAREN expr TRPAREN {  }
     ;

comparison_op : TCEQ | TCNE | TCLT | TCLE | TCGT | TCGE
    ;

numeric_op : TPLUS | TMINUS | TMUL | TDIV
          ;
%%
