/**
 * @file TreeNode.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.3
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <string>
#include "LToken.h"

#ifndef TREECLASS_H
#define TREECLASS_H

#define MAXCHILDREN 3
extern int errors;//for addsiblings

//token numbers for the operators, as seen in flex.
typedef int OpKind;

//types of statements
typedef enum {DeclK, StmtK, ExpK} NodeKind;

//Subkinds of declkind
typedef enum {VarK, FuncK, ParamK} DeclKind;

// Subkinds of Statements
typedef enum {NullK, IfK, WhileK, ForK, CompoundK, ReturnK, BreakK, RangeK} StmtKind;

// Subkinds of Expressions
typedef enum {OpK, ConstantK, IdK, AssignK, InitK, CallK} ExpKind;

// ExpType is used for type checking (Void means no type or value, UndefinedType mean undefined)
typedef enum {Void, Integer, Boolean, Char, CharInt, Equal, UndefinedType} ExpType;

// What kind of scoping is used? (decided during typing)
typedef enum {None, Local, Global, Parameter, LocalStatic} VarKind;

typedef enum {LocalR, GlobalR, StaticR, ParamR, NoneR} RefType;

class TreeNode {
private: //could move the bool variables here, to enforce function usage
    bool isArray; // is this an array
    bool isStatic; // is staticly allocated?
    bool isInitialized;//is it initialized
    bool isUsed;//to check for global declarations to see if they are used
    int memLoc;//memory location
    int size;//size in memory
    RefType reference_t;
    int regVal;//used in codegen
    

public:
    TreeNode(DeclKind declK, ExpType eType, LToken *token, TreeNode* c0, TreeNode* c1, TreeNode* c2, int nsize=1, RefType rType = GlobalR);
    TreeNode(StmtKind stmtK, LToken *token, TreeNode* c0, TreeNode* c1, TreeNode* c2, RefType rType = GlobalR);
    TreeNode(ExpKind expK, LToken *token, TreeNode* c0, TreeNode* c1, TreeNode* c2, int nsize=1, RefType rType = GlobalR);
    ~TreeNode();
    
	TreeNode *child[MAXCHILDREN]; // children of the node
    TreeNode *sibling; //forms a linked list of siblings for the node
    TreeNode *parent;//the parent of the node. Try and figure out how to add this to each

    void AddChildren(TreeNode *nchild);

    // what kind of node
    int lineno; // linenum relevant to this node
    NodeKind nkind; // type of this node
    union // subtype of type
    {
        DeclKind decl; // used when DeclK
        StmtKind stmt; // used when StmtK
        ExpKind exp; // used when ExpK
    } subkind;

    int tokkind; // type of token
    std::string text;//the original text of the token

    // extra properties about the node depending on type of the node
    union // relevant data to type -> attr
    {
        OpKind opkind;//used to store the operator type. an int typedef
        int value; // used when an integer constant or boolean
        char cvalue; // used when a character
    } attr;

    ExpType expType; // used when ExpK for type checking
    int paramnum;//for funcK nodes, to see the number of parameters
    //more semantic stuff will go here in later assignments.


    void setInitialize(bool init, int line = -2);
    void setArray(bool array, int line = -2);
    void setStatic(bool stat, int line = -2);
    void setUsage(bool used, int line = -2);
    
    void setMem(int t);
    void setSize(int t);
    void setRType(RefType r);
    bool getInitialize();
    bool getArray();
    bool getStatic();
    bool getUsage();
    int getMem();
    int getSize();
    RefType getRType();
    int getR();//returns register value
    void setR(int t);//sets the register

    //returns the number of parameters. returns -1 if not a function
    int numParams();
};

void freeTree(TreeNode *root);
TreeNode *addSibling(TreeNode *t, TreeNode *s);
void updateChildrensParent(TreeNode *parentNode, int debug);
#endif /*TREECLASS_H*/

/*
tokenclass:
1: num
2: bool
3: id
4: char
5: string
6: operator
*/