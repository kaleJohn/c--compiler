/**
 * @file LToken.h
 * @author Kaleb Johnson kalebjl@gmail.com
 * @brief 
 * @version 0.3
 * @date 2023-10-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <string>
extern int warnings;
extern int errors;

#ifndef TOKNCLASS_H
#define TOKNCLASS_H

class LToken {
public:
    LToken(int tclass, int line, char *input);
    LToken(int tclass, int line, char *input, int nval, char cval);
    LToken(int tclass, int line, char *input, int nval);

    std::string getText();
    int getTClass();
    int getLine();
    char getChar();
    int getNVal();

    int tokenclass;
    int linenum;
    std::string tokenstr;
    char cvalue;
    int nvalue;
    bool isArray;
};
#endif /*TOKNCLASS_H*/