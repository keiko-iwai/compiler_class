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
    StatementAST *stmt;
    IdentifierExprAST *ident;
    VarDeclExprAST *var_decl;
    VariableList *func_args;
    ExpressionList *call_args;
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
%type <ident> ident
%type <expr> numeric expr add_expr mul_expr comparison_expr factor
%type <block> program stmts block
%type <func_args> func_decl_args
%type <call_args> call_args
%type <stmt> stmt var_decl func_decl
%type <string> comparison_op add_op mul_op

/* Operator precedence for mathematical operators */
%left TPLUS TMINUS
%left TMUL TDIV
%left TCEQ TCNE TCLT TCLE TCGT TCGE
%right TEQUAL

%start program

%%

program : stmts { programBlock = $1; }
        ;

stmts : stmt { $$ = new BlockExprAST(); $$->statements.push_back($<stmt>1); }
      | stmts stmt { $1->statements.push_back($<stmt>2); }
      ;

stmt : var_decl | func_decl
     | expr { $$ = new ExpressionStatementAST(*$1); }
     ;

var_decl : ident ident { $$ = new VarDeclExprAST(*$1, *$2); }
         | ident ident TEQUAL expr { $$ = new VarDeclExprAST(*$1, *$2, $4); }
         ;

ident : TIDENTIFIER { $$ = new IdentifierExprAST(*$1); delete $1; }
      ;

numeric : TINTEGER { $$ = new IntExprAST(atoi($1->c_str())); delete $1; }
        | TDOUBLE  { $$ = new DoubleExprAST(atol($1->c_str())); delete $1;  }
        ;

/* expressions */

expr : TLPAREN expr TRPAREN { $$ = $2; }
     | ident TEQUAL expr { $$ = new AssignmentAST(*$<ident>1, *$3); }
     | ident TLPAREN call_args TRPAREN { $$ = new CallExprAST(*$1, *$3); delete $3; }
     | comparison_expr
     ;

comparison_expr : comparison_expr comparison_op add_expr { $$ = new BinaryExprAST(*$2, $1, $3); }
     | add_expr
     ;

add_expr : add_expr add_op mul_expr { $$ = new BinaryExprAST(*$2, $1, $3); }
         | mul_expr;

mul_expr : mul_expr mul_op factor { $$ = new BinaryExprAST(*$2, $1, $3); }
         | factor;

factor : TLPAREN expr TRPAREN { $$ = $2; }
       | ident { $<ident>$ = $1; }
       | numeric; /* TMINUS factor too! But it needs a class to support unary expressions */


comparison_op : TCEQ | TCNE | TCLT | TCLE | TCGT | TCGE ;

add_op : TPLUS | TMINUS ;

mul_op : TMUL | TDIV ;

/* function grammar rules */

func_decl : ident ident TLPAREN func_decl_args TRPAREN block
            { $$ = new FunctionDeclarationAST(*$1, *$2, *$4, *$6); delete $4; }
          ;

block : TLBRACE stmts TRBRACE { $$ = $2; }
      | TLBRACE TRBRACE { $$ = new BlockExprAST(); }
      ;

call_args : /* empty */  { $$ = new ExpressionList(); }
          | expr { $$ = new ExpressionList(); $$->push_back($1); }
          | call_args TCOMMA expr  { $1->push_back($3); }
          ;

func_decl_args : /* empty */  { $$ = new VariableList(); }
          | var_decl { $$ = new VariableList(); $$->push_back($<var_decl>1); }
          | func_decl_args TCOMMA var_decl { $1->push_back($<var_decl>3); }
          ;

%%
