/**
 * @file codeGen.h
 * @author Kaleb Johnson
 * @brief 
 * @version 0.1
 * @date 2023-12-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "emitcode.h"
#include "symbolTable.h"
#include "TreeNode.h"//Tree Node Class
#include <string.h>
#include <vector>

#ifndef CODEGEN_H
#define CODEGEN_H


#define rNum 7
#define drNum 10

struct rStore {
    char name[20];
    bool used;
};

struct func {
    std::string name;
    int line;
};

/*
typedef enum {LDC, LD, LDA, JMP, ADD} OpCom;

struct CodeNode{    
    OpCom com;
};*/

//generates the code for a program
void generateCode(TreeNode *root, int globalEnd);
//recursively generates code from the syntax tree
void generateTreeCode(TreeNode *root);
//calls generateTreeCode on each child
void traverseChildren(TreeNode* node);

//emits the IO functions
void genIO();

//functions for tracking the registers
int regAlloc(const char* name);
void freeReg(int index);
const char* regName(int index);
void clearReg();
//functions for tracking the "stack registers"
int dataRegAlloc();
void clearDReg();
void freeDReg(int index);

int storeDReg(int reg, int index = -1);
//pops the data from the stack into a register
void loadDReg(int reg, int dIndex);


//functions for generating each node type
void generateDecl(TreeNode *node);
void generateStmt(TreeNode *node);
void generateExp(TreeNode *node);

//subfunction specifically for the OpKs, since they're so varied.
void generateOp(TreeNode *node);
void callParam(TreeNode* param, int index);
void loadGlobals();
int getArrayAddr(TreeNode* node);
#endif