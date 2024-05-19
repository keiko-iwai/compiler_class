/*!
error handling based on the IBM article
by Christian Hagen, chagen@de.ibm.com
https://developer.ibm.com/tutorials/l-flexbison/
 */
#ifndef BISON_ERROR_H_
#define BISON_ERROR_H_

#include <cstdio>
#include <cmath>
#include <cctype>
#include <string>
#include <memory>
#include <cstdlib>
#include <cstdarg>
#include <cfloat>

#include "parser.h"

/* lex & bison external functions */
extern int yylex(void);
extern int yyparse(void);
extern void yyerror(char*);


/* external variables */
extern int debugErrorParser;
extern int lMaxBuffer;
extern char *buffer;
extern int parseError;

extern int GetNextChar(char *b, int maxBuffer);
extern void BeginToken(char*);
extern void PrintError(const char *s, ...);
extern int getNextLine(void);

#endif /* BISON_ERROR_H_ */