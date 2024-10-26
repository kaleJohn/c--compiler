/**
 * @file TreeNode.cpp
 * @author Kaleb Johnson
 * @brief 
 * @version 0.3
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "TreeNode.h"

TreeNode::TreeNode(DeclKind kind, ExpType eType, LToken *token, TreeNode* c0, TreeNode* c1, TreeNode* c2, int nsize, RefType rType) {
    sibling = NULL;
    parent = NULL;
    child[0] = c0;
    child[1] = c1;
    child[2] = c2;

    subkind.decl = kind;
    nkind = DeclK;
    expType = eType;
    size = nsize;
    reference_t = rType;
    regVal = -1;
    memLoc = 0;

    isArray = false;//TODO: add overloaded constructors to change these
    isStatic = false;
    isUsed = false;

    if(token != NULL) {
        //TODO: change string so it's actually copying the value, worried about losing after deleting it.
        text = token->getText();
        
        int tempRef = token->getTClass();
        tokkind = tempRef;
        lineno = token->getLine();

        //update attr union
        if(tempRef == 4) {//character type token
            attr.cvalue = token->getChar();
        } else if(tempRef == 1 || tempRef == 2) {//number or boolean
            attr.value = token->getNVal();
        } else if(tempRef == 5) {//string
            //currently just use the text string
        } else if(tempRef == 3) {//ident
            //currently just use the text string
            if(kind == VarK) {
                if(token->getNVal() == -1)
                    isArray = true;
            }
        } else if(tempRef == 6) {//operator
            attr.opkind = token->getNVal();
            //currently just use text string?
        }

        delete token;
    } else {//create an impossible value to check against
        tokkind = -1;
    }
    isInitialized = false;
}

TreeNode::TreeNode(ExpKind kind, LToken *token, TreeNode* c0, TreeNode* c1, TreeNode* c2, int nsize, RefType rType) {
    sibling = NULL;
    child[0] = c0;
    child[1] = c1;
    child[2] = c2;

    subkind.exp = kind;
    nkind = ExpK;
    size = nsize;
    reference_t = rType;
    regVal = -1;
    memLoc = 0;

    if(token != NULL) {
        text = token->getText();

        int tempRef = token->getTClass();
        tokkind = tempRef;
        lineno = token->getLine();

        //update attr union
        if(tempRef == 4) {//character type token
            attr.cvalue = token->getChar();
        } else if(tempRef == 1 || tempRef == 2) {//number or boolean
            attr.value = token->getNVal();
        } else if(tempRef == 5) {//string
            //currently just use the text string
        } else if(tempRef == 3) {//ident
            //currently just use the text string
        } else if(tempRef == 6) {//operator
            attr.opkind = token->getNVal();
        }

        delete token;
    } else {//create an impossible value to check against
        tokkind = -1;
    }
    isArray = false;//TODO: add overloaded constructors to change these
    isStatic = false;
    isUsed = false;
    isInitialized = false;
    expType = UndefinedType;
}

TreeNode::TreeNode(StmtKind kind, LToken *token, TreeNode* c0, TreeNode* c1, TreeNode* c2, RefType rType) {
    sibling = NULL;
    child[0] = c0;
    child[1] = c1;
    child[2] = c2;

    subkind.stmt = kind;
    nkind = StmtK;
    reference_t = rType;
    regVal = -1;
    memLoc = 0;

    if(token != NULL) {
        text = token->getText();

        int tempRef = token->getTClass();
        tokkind = tempRef;
        lineno = token->getLine();

        //update attr union
        if(tempRef == 4) {//character type token
            attr.cvalue = token->getChar();
        } else if(tempRef == 1 || tempRef == 2) {//number or boolean
            attr.value = token->getNVal();
        } else if(tempRef == 5) {//string
            //currently just use the text string
        } else if(tempRef == 3) {//ident
            //currently just use the text string
        } else if(tempRef == 6) {//operator
            //currently just use text string
        }

        delete token;
    } else {//create an impossible value to check against
        tokkind = -1;
    }
    isArray = false;//TODO: add overloaded constructors to change these
    isStatic = false;
    isUsed = false;
    isInitialized = false;
    expType = UndefinedType;

}

void TreeNode::AddChildren(TreeNode *nchild) {
    for (int i = 0; i<MAXCHILDREN; i++) {
        if(child[i] == NULL) {//might want to create a return value to let the user know if the node is full.
            child[i] = nchild;
        }
    }
    return;
}

TreeNode::~TreeNode() {
    //not sure what to do here.
}

//setter and getter functions. can add prints to see where they are being called
//optional line input to get the line the function is being called from
void TreeNode::setInitialize(bool init, int line) {
    //printf("%s initialization is set to %d. (orig line %d)\n", text.c_str(), init, lineno);
    isInitialized = init;
}
void TreeNode::setArray(bool array, int line) {
    isArray = array;
}
void TreeNode::setStatic(bool stat, int line) {
    isStatic = stat;
}
void TreeNode::setUsage(bool used, int line) {
    isUsed = used;
}
bool TreeNode::getInitialize() {
    return isInitialized;
}
bool TreeNode::getArray() {
    return isArray;
}
bool TreeNode::getStatic() {
    return isStatic;
}
bool TreeNode::getUsage(){
    return isUsed;
}
void TreeNode::setMem(int t) {
    memLoc = t;
}
void TreeNode::setSize(int t){
    size = t;
}
void TreeNode::setRType(RefType r){
    reference_t = r;
}
int TreeNode::getMem() {
    return memLoc;
}
int TreeNode::getSize() {
    return size;
}
RefType TreeNode::getRType() {
    return reference_t;
}
int TreeNode::getR() {//returns register value
    return regVal;
}
void TreeNode::setR(int t) {//sets the register
    regVal = t;
}

int TreeNode::numParams() {
    int params = 0;
    if(child[0]!= NULL && child[0]->nkind == DeclK && child[0]->subkind.decl == ParamK){
        TreeNode *param = child[0];
        while(param != NULL) {
            params++;
            param = param->sibling;
        }
    }
    return params;
}

void updateChildrensParent(TreeNode *parentNode, int debug){
    for(int i = 0; i < MAXCHILDREN; i++) {
        if(parentNode->child[i] != NULL) {
            if(debug > 0)
                printf("Updating child %s at line %d to have parent%s a line %d\n", parentNode->child[i]->text.c_str(), parentNode->child[i]->lineno, 
                parentNode->text.c_str(), parentNode->lineno);
            parentNode->child[i]->parent = parentNode;
        }
    }
}

void freeTree(TreeNode *root) {
    int i;
    for(i = 0; i < MAXCHILDREN; i++) {
        if(root->child[i] != NULL) {
            freeTree(root->child[i]);
        }
    }

    if(root->sibling!=NULL) {
        freeTree(root->sibling);
    }

    delete root;
    return;
}

TreeNode *addSibling(TreeNode *t, TreeNode *s) {
    if (s==NULL && errors == 0) {
        printf("ERROR(SYSTEM): never add a NULL to a sibling list.\n");
        exit(1);
    }
    if (t!=NULL) {
        TreeNode *tmp;
        tmp = t;
        while (tmp->sibling!=NULL) {
            tmp = tmp->sibling;
        }
        tmp->sibling = s;
        return t;
    }
    return s;
}