/**
 * @file semantic.h
 * @author Kaleb Johnson
 * @brief 
 * @version 0.1
 * @date 2023-10-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H
#include "TreeNode.h"
#include "symbolTable.h"

/*the analysis function. returns a pointer to a manually allocated pair of integers, 
*return[0] = warnings, return[1] = errors
*/
void semanticAnalysis(TreeNode *root, SymbolTable *global);
//the recursive traversal function
void treeTraverse(TreeNode *node, SymbolTable *sTable, bool regErrors);

//adds the node's declarations to the symbolTable, if it works
void addNode(TreeNode *node, SymbolTable *sTable);
//checks the node's types for errors
void checkStmtNode(TreeNode *node, SymbolTable *sTable, int init_foffset);
void checkDeclNode(TreeNode *node, SymbolTable *sTable);
void checkExpNode(TreeNode *node, SymbolTable *sTable);

TreeNode * generateIOTree();

std::string printType(ExpType type);
int opFlag(OpKind o);

void warnIfNotUsed(std::string , void *);
int enterScope(TreeNode *node, SymbolTable *sTable, bool updateErrors);

#endif //SEMANTIC_H