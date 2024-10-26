/**
 * @file LToken.cpp
 * @author Kaleb Johnson kalebjl@gmail.com
 * @brief 
 * @version 0.3
 * @date 2023-10-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <string>
#include "LToken.h"

LToken::LToken(int tclass, int line, char *input) {
    tokenclass = tclass;
    linenum = line;
    tokenstr = input;
    nvalue = 1;
    isArray = false;
}

LToken::LToken(int tclass, int line, char *input, int nval, char cval){
    tokenclass = tclass;
    linenum = line;
    tokenstr = input;
    cvalue = cval;
    nvalue = nval;
    isArray = false;
}    

LToken::LToken(int tclass, int line, char *input, int nval) {
    tokenclass = tclass;
    linenum = line;
    tokenstr = input;
    nvalue = nval;
    isArray = false;
}

std::string LToken::getText() {
    return tokenstr;
}
   
int LToken::getTClass() {
    return tokenclass;
}

int LToken::getLine() {
    return linenum;
}

char LToken::getChar(){
    return cvalue;
}

int LToken::getNVal() {
    return nvalue;
}