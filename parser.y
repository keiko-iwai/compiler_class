%define parse.error detailed
%{
  #include <cstdio>
  #include <map>
  #include <string>
  #include "exprAST.h"

  BlockExprAST *programBlock;

  extern int yylineno;
  extern int column;
  extern int yylex();
  void yyerror(const char *str) {
    printf("[Bison] Line %d, column %d: %s\n", yylineno, column, str);
  }
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
%token <string> IDENTIFIER INTEGER DOUBLE
%token <token> LPAREN RPAREN LBRACE TRBRACE COMMA DOT SEMICOLON
%token <string> EQ NE LT LE GT GE EQUAL
%token <string> PLUS MINUS MUL DIV

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
%left PLUS MINUS
%left MUL DIV
%left EQ NE LT LE GT GE
%right EQUAL

%start program

%%

program : stmts { programBlock = $1; }
        ;

stmts : stmt { $$ = new BlockExprAST(); $$->statements.push_back($<stmt>1); }
      | stmts stmt { $1->statements.push_back($<stmt>2); }
      ;

stmt : var_decl SEMICOLON | func_decl
     | expr SEMICOLON { $$ = new ExpressionStatementAST(*$1); }
     ;

var_decl : ident ident { $$ = new VarDeclExprAST(*$1, *$2); }
         | ident ident EQUAL expr { $$ = new VarDeclExprAST(*$1, *$2, $4); }
         ;

ident : IDENTIFIER { $$ = new IdentifierExprAST(*$1); delete $1; }
      ;

numeric : INTEGER { $$ = new IntExprAST(atoi($1->c_str())); delete $1; }
        | DOUBLE  { $$ = new DoubleExprAST(atol($1->c_str())); delete $1;  }
        ;

/* expressions */

expr : comparison_expr
     | ident EQUAL expr { $$ = new AssignmentAST(*$<ident>1, *$3); }
     | ident LPAREN call_args RPAREN { $$ = new CallExprAST(*$1, *$3); delete $3; }
     ;

comparison_expr : comparison_expr comparison_op add_expr { $$ = new BinaryExprAST(*$2, $1, $3); }
     | add_expr
     ;

add_expr : add_expr add_op mul_expr { $$ = new BinaryExprAST(*$2, $1, $3); }
         | mul_expr
         ;

mul_expr : mul_expr mul_op factor { $$ = new BinaryExprAST(*$2, $1, $3); }
         | factor;

factor : LPAREN expr RPAREN { $$ = $2; }
       | ident { $<ident>$ = $1; }
       | numeric /* MINUS factor too! But it needs a class to support unary expressions */
       ;

comparison_op : EQ | NE | LT | LE | GT | GE ;

add_op : PLUS | MINUS ;

mul_op : MUL | DIV ;

/* function grammar rules */

func_decl : ident ident LPAREN func_decl_args RPAREN block
          { $$ = new FunctionDeclarationAST(*$1, *$2, *$4, *$6); delete $4; }
          ;

block : LBRACE stmts TRBRACE { $$ = $2; }
      | LBRACE TRBRACE { $$ = new BlockExprAST(); }
      ;

call_args : /* empty */  { $$ = new ExpressionList(); }
          | expr { $$ = new ExpressionList(); $$->push_back($1); }
          | call_args COMMA expr  { $1->push_back($3); }
          ;

func_decl_args : /* empty */  { $$ = new VariableList(); }
          | var_decl { $$ = new VariableList(); $$->push_back($<var_decl>1); }
          | func_decl_args COMMA var_decl { $1->push_back($<var_decl>3); }
          ;

%%
