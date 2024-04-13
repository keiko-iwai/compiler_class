%{
    #include <cstdio>
    #include "exprAST.h"

    BlockExprAST *programBlock;

    extern int yylex();
    void yyerror(const char *s) { printf("[Bison] ERROR: %s\n", s); }
%}

%union {
    ExprAST *expr;
    BlockExprAST *block;
    StatementExprAST *stmt;
    std::string *string;
    int token;
}

/* Define our terminal symbols (tokens). This should
   match our tokens.l lex file. We also define the node type
   they represent.
 */
%token <string> TIDENTIFIER TINTEGER TDOUBLE
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TCOMMA TDOT
%token <string> TCEQ TCNE TCLT TCLE TCGT TCGE TEQUAL
%token <string> TPLUS TMINUS TMUL TDIV

/* Define the type of node our nonterminal symbols represent.
   The types refer to the %union declaration above. Ex: when
   we call an ident (defined by union type ident) we are really
   calling an (NIdentifier*). It makes the compiler happy.
 */
%type <expr> numeric expr 
%type <block> program stmts
%type <stmt> stmt
%type <string> comparison_op numeric_op

/* Operator precedence for mathematical operators */
%left TPLUS TMINUS
%left TMUL TDIV

%start program

%%

program : stmts { programBlock = $1; }
        ;
        
stmts : stmt { $$ = new BlockExprAST(); $$->statements.push_back($<stmt>1); }
      | stmts stmt { $1->statements.push_back($<stmt>2); }
      ;

stmt : expr /* fixme - there are more statements */
     ;

numeric : TINTEGER { $$ = new IntExprAST(atoi($1->c_str())); delete $1; }
        | TDOUBLE  { $$ = new DoubleExprAST(atol($1->c_str())); delete $1;  }
        ;

expr : numeric /*defined*/
     | expr comparison_op expr {  $$ = new BinaryExprAST(*$2, $1, $3); }
     | expr numeric_op expr { $$ = new BinaryExprAST(*$2, $1, $3); }
     | TLPAREN expr TRPAREN { $$ = $2; }
     ;

comparison_op : TCEQ | TCNE | TCLT | TCLE | TCGT | TCGE
    ;

numeric_op : TPLUS | TMINUS | TMUL | TDIV
          ;
%%
