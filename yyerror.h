#include <vector>

#ifndef _YYERROR_H_
#define _YYERROR_H_

// write a nice error message
#ifndef YYERROR_VERBOSE
#define YYERROR_VERBOSE
#endif

// NOTE: make sure these variables interface with your code!!!
//just hooking these up to the scanner numbers. yytext could change before the error is processed
extern int yylineno;        // line number of last token scanned in your scanner (.l) 
extern std::vector<char*> tokenStore; // last token scanned in your scanner (connect to your .l) 
extern int errors;   // number of errors
extern int tokNum;

void initErrorProcessing();    // WARNING: MUST be called before any errors occur (near top of main)!
void yyerror(const char *msg); // error routine called by Bison

#endif
