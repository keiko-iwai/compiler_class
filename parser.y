%define parse.error detailed
%{
  #include <cstdio>
  #include <algorithm>
  #include <map>
  #include <string>
  #include "AST.h"

  BlockExprAST *programBlock;
  FunctionMap *definedFunctions = new FunctionMap();

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
    ExpressionList *expr_list;
    std::string *string;
    int token;
}

/* Define our terminal symbols (tokens). This should
   match our tokens.l lex file. We also define the node type
   they represent.
 */
%token <string> IDENTIFIER INTEGER DOUBLE STRINGVAL
%token <token> LPAREN RPAREN LBRACE TRBRACE COMMA DOT SEMICOLON
%token <string> EQ NE LT LE GT GE EQUAL
%token <string> PLUS MINUS MUL DIV
%token <string> RETURN IF ELSE FOR

/* Define the type of node our nonterminal symbols represent.
   The types refer to the %union declaration above. Ex: when
   we call an ident (defined by union type ident) we are really
   calling an (NIdentifier*). It makes the compiler happy.
 */
%type <ident> ident
%type <expr> numeric expr add_expr mul_expr comparison_expr factor call_expr string_val
%type <block> program stmts block
%type <func_args> func_decl_args
%type <expr_list> expr_list
%type <stmt> stmt var_decl func_decl return_stmt if_stmt loop_stmt for_stmt
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

stmts : stmt { $$ = new BlockExprAST(); $$->Statements.push_back($<stmt>1); }
      | stmts stmt { $1->Statements.push_back($<stmt>2); }
      ;

stmt : return_stmt SEMICOLON | func_decl | if_stmt | loop_stmt
     | expr SEMICOLON { $$ = new ExpressionStatementAST(*$1); }
     | var_decl SEMICOLON
     ;

return_stmt : RETURN expr { $$ = new ReturnStatementAST($2); }
     | RETURN { $$ = new ReturnStatementAST(); }
     ;

var_decl : ident ident { $$ = new VarDeclExprAST(*$1, *$2); }
         | ident ident EQUAL expr { $$ = new VarDeclExprAST(*$1, *$2, $4); }
         ;

ident : IDENTIFIER { $$ = new IdentifierExprAST(*$1); delete $1; }
      ;

string_val : STRINGVAL
          {
            std::string str = *$1;
            str.erase(std::remove(str.begin(), str.end(), '"'), str.end());
            $$ = new StringExprAST(str); delete $1;
          };

numeric : INTEGER { $$ = new IntExprAST(atoi($1->c_str())); delete $1; }
        | DOUBLE  { $$ = new DoubleExprAST(atol($1->c_str())); delete $1;  }
        ;

if_stmt : IF LPAREN expr RPAREN block { $$ = new IfStatementAST($3, $5); }
        | IF LPAREN expr RPAREN block ELSE block { $$ = new IfStatementAST($3, $5, $7); }
        | IF LPAREN expr RPAREN block ELSE if_stmt { $$ = new IfStatementAST($3, $5, $7); }
        ;

loop_stmt : for_stmt;

for_stmt : FOR LPAREN expr_list SEMICOLON expr SEMICOLON expr_list RPAREN block
            { $$ = new ForStatementAST(*$3, $5, *$7, $9); }
         ;

/* expressions */

expr : comparison_expr | string_val
     | ident EQUAL expr { $$ = new AssignmentAST(*$<ident>1, *$3); }
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
       | call_expr
       | numeric /* MINUS factor too! But it needs a class to support unary expressions */
       | MINUS factor { $$ = new UnaryExprAST(*$1, $2); }
       ;

call_expr : ident LPAREN expr_list RPAREN { $$ = new CallExprAST(*$1, *$3); delete $3; }
          ;

comparison_op : EQ | NE | LT | LE | GT | GE ;

add_op : PLUS | MINUS ;

mul_op : MUL | DIV ;

/* function grammar rules */

func_decl : ident ident LPAREN func_decl_args RPAREN block
          {
              FunctionDeclarationAST *fn = new FunctionDeclarationAST(*$1, *$2, *$4, *$6);
              $$ = fn;
              (*definedFunctions)[$2->Name] = fn;
              delete $4;
          }
          ;

block : LBRACE stmts TRBRACE { $$ = $2; }
      | LBRACE TRBRACE { $$ = new BlockExprAST(); }
      ;

expr_list : /* empty */  { $$ = new ExpressionList(); }
          | expr { $$ = new ExpressionList(); $$->push_back($1); }
          | expr_list COMMA expr  { $1->push_back($3); }
          ;

func_decl_args : /* empty */  { $$ = new VariableList(); }
          | var_decl { $$ = new VariableList(); $$->push_back($<var_decl>1); }
          | func_decl_args COMMA var_decl { $1->push_back($<var_decl>3); }
          ;

%%
