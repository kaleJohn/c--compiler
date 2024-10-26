%{
/**
*Scanner that will find c- tokens, stores their properties, and passes them to a parser
*Kaleb Johnson
*11/05/23
*/
#include "LToken.h"  // TokenData Type
#include "parser.tab.h"
#include <ctype.h>
#include <string.h>//c++ strings
#include <vector>

extern int warnings;
extern int errors;
char *lastToken;
std::vector<char*> tokenStore;
int tokNum;
void storeToken(const char *text);
%}
%option yylineno

BOOLCONST true|false
NUMCONST [0-9]+
IDENT [a-zA-Z][_a-zA-Z0-9]*
CHARCONST \'([^\\\'\n]|\\[^\n])*\'
STRINGCONST \"([^\\\"]|\\.)*\" 
%{
    /*
    * Maybe find a better char regex?
    * allow escape characters to work in string
    * fix strlen numbers
    * old versio of charconst: '\\?.'
    */
%}
%%
\/\/[^\n\r]*   /*ignore comments? why does this not work with two in a row*/;

{NUMCONST} {
    yylval.token = new LToken(1, yylineno, yytext, atoi(yytext));
    storeToken(yytext);
    return NUMCONST;
}
     
{BOOLCONST} {
    if(strstr(yytext, "true")!= NULL) {
         yylval.token = new LToken(2, yylineno, yytext, 1);
    } else {
        yylval.token = new LToken(2, yylineno, yytext, 0);
    }
    storeToken(yytext);
    return BOOLCONST;
}

"if" {
    yylval.token = new LToken(6, yylineno, yytext, 21);
    storeToken(yytext);
    return IF;
}
"else" {
    yylval.token = new LToken(6, yylineno, yytext, 22);
    storeToken(yytext);
    return ELSE;
}
"return" {
    yylval.token = new LToken(6, yylineno, yytext, 23);
    storeToken(yytext);
    return RETURN;
}
"int" {
    yylval.token = new LToken(6, yylineno, yytext, 24);
    storeToken(yytext);
    return INT;   
}
"bool" {
    yylval.token = new LToken(6, yylineno, yytext, 25);
    storeToken(yytext);
    return BOOL;
} 
"char" {
    yylval.token = new LToken(6, yylineno, yytext, 26);
    storeToken(yytext);
    return CHAR;
} 
"static" {
    yylval.token = new LToken(6, yylineno, yytext, 27);
    storeToken(yytext);
    return STATIC;
} 
"then" {
    yylval.token = new LToken(6, yylineno, yytext, 28);
    storeToken(yytext);
    return THEN;
} 
"and" {
    yylval.token = new LToken(6, yylineno, yytext, 29);
    storeToken(yytext);
    return AND;
} 
"or" {
    yylval.token = new LToken(6, yylineno, yytext, 30);
    storeToken(yytext);
    return OR;
} 
"not" {
    yylval.token = new LToken(6, yylineno, yytext, 31);
    storeToken(yytext);
    return NOT;
} 
"for" {
    yylval.token = new LToken(6, yylineno, yytext, 32);
    storeToken(yytext);
    return FOR;
} 
"to" {
    yylval.token = new LToken(6, yylineno, yytext, 33);
    storeToken(yytext);
    return TO;
} 
"do" {
    yylval.token = new LToken(6, yylineno, yytext, 34);
    storeToken(yytext);
    return DO;
} 
"by" {
    yylval.token = new LToken(6, yylineno, yytext, 35);
    storeToken(yytext);
    return BY;
} 
"break" {
    yylval.token = new LToken(6, yylineno, yytext, 36);
    storeToken(yytext);
    return BREAK;
} 
"while" {
    yylval.token = new LToken(6, yylineno, yytext, 37);
    storeToken(yytext);
    return WHILE;
}

"--" {
    yylval.token = new LToken(6, yylineno, yytext, 0);
    storeToken(yytext);
    return DEC;
}
"++" {
    yylval.token = new LToken(6, yylineno, yytext, 1);
    storeToken(yytext);
    return INC;
}
":=" {
    yylval.token = new LToken(6, yylineno, yytext, 2);
    storeToken(yytext);
    return ASSIGN;
}
"+=" {
    yylval.token = new LToken(6, yylineno, yytext, 3);
    storeToken(yytext);
    return PLUSEQ;
}
"-=" {
    yylval.token = new LToken(6, yylineno, yytext, 4);
    storeToken(yytext);
    return MINUSEQ;
}
"*=" {
    yylval.token = new LToken(6, yylineno, yytext, 5);
    storeToken(yytext);
    return MULEQ;
}
"/=" {
    yylval.token = new LToken(6, yylineno, yytext, 6);
    storeToken(yytext);
    return DIVEQ;
}
"><" {
    yylval.token = new LToken(6, yylineno, yytext, 7);
    storeToken(yytext);
    return NEQ;
}
">=" {
    yylval.token = new LToken(6, yylineno, yytext, 8);
    storeToken(yytext);
    return GEQ;
}
"<=" {
    yylval.token = new LToken(6, yylineno, yytext, 9);
    storeToken(yytext);
    return LEQ;
}
"<" {
    yylval.token = new LToken(6, yylineno, yytext, 10);
    storeToken(yytext);
    return LTHAN;
}
">" {
    yylval.token = new LToken(6, yylineno, yytext, 11);
    storeToken(yytext);
    return GTHAN;
}
"=" {
    yylval.token = new LToken(6, yylineno, yytext, 12);
    storeToken(yytext);
    return EQ;
}
"+" {
    yylval.token = new LToken(6, yylineno, yytext, 13);
    storeToken(yytext);
    return PLUS;
}
"-" {
    yylval.token = new LToken(6, yylineno, yytext, 14);
    storeToken(yytext);
    return MINUS;
}
"*" {
    yylval.token = new LToken(6, yylineno, yytext, 15);
    storeToken(yytext);
    return STAR;
}
"/" {
    yylval.token = new LToken(6, yylineno, yytext, 16);
    storeToken(yytext);
    return DIV;
}
"%" {
    yylval.token = new LToken(6, yylineno, yytext, 17);
    storeToken(yytext);
    return MODULO;
}
"?" {
    yylval.token = new LToken(6, yylineno, yytext, 18);
    storeToken(yytext);
    return QMARK;
}
"[" {
    yylval.token = new LToken(6, yylineno, yytext, 19);
    storeToken(yytext);
    return LBRACK;
}
"{" {
    yylval.token = new LToken(6, yylineno, yytext, 20);
    storeToken(yytext);
    return LBRACE;
}

{IDENT} {
    yylval.token = new LToken(3, yylineno, yytext);
    storeToken(yytext);
    return ID;
}

{CHARCONST} {
    if(strlen(yytext) > 3) {
        if(yytext[1] == '\\') {
            switch (yytext[2]) {
                case 'n':
                    yylval.token = new LToken(4, yylineno, yytext, 2, '\n');
                    break;
                case '0':
                    yylval.token = new LToken(4, yylineno, yytext, 2, '\0');
                    break;
                case 'r':
                    yylval.token = new LToken(4, yylineno, yytext, 2, '\r');
                    break;
                default:
                    yylval.token = new LToken(4, yylineno, yytext, strlen(yytext)-2, yytext[2]);
            }
        } else {
            warnings++;
            printf("WARNING(%d): character is %d characters long and not a single character: '%s'. The first char will be used.\n", yylineno, strlen(yytext)-2, yytext);
            yylval.token = new LToken(4, yylineno, yytext, strlen(yytext)-2, yytext[1]);
        }
    } else {
        yylval.token = new LToken(4, yylineno, yytext, 1, yytext[1]);//use the numerical value to hold the char length
    }
    storeToken(yytext);
    return CHARCONST;
}

{STRINGCONST} {
    yylval.token = new LToken(5, yylineno, yytext, yyleng-2);//nvalue stores the string length;
    storeToken(yytext);
    return STRINGCONST;
}

"("|")"|"|"|"&"|";"|","|":"|"]"|"}" {
    yylval.token = new LToken(6, yylineno, yytext, -1);
    storeToken(yytext);
    return yytext[0];
}

[ \t]			;

\n              /**/;

.				{ errors++;
    printf("ERROR(%d): Invalid or misplaced input character: '%c'. Character Ignored.\n", yylineno, yytext[0]); }
%%

//exit with success upon reaching EOF
int yywrap() {
    return 1;
}

void storeToken(const char *text){
    lastToken = strdup(text);
    if(lastToken != NULL)
        tokenStore.push_back(lastToken);
    tokNum = tokenStore.size();
}