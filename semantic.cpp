/**
 * @file semantic.cpp
 * @author Kaleb Johnson
 * @brief 
 * @version 0.4
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "semantic.h"

void semanticAnalysis(TreeNode *root, SymbolTable *sTable) {

    //sTable->enter("global"); should be unneccesary, due to the constructor
    TreeNode *ioRoot = generateIOTree();

    treeTraverse(ioRoot, sTable, false);

    treeTraverse(root, sTable, true);

    //TODO:fix. throwing false negatives right now
    sTable->applyToAllGlobal(warnIfNotUsed);
    
    TreeNode *mainNode = (TreeNode*) sTable->lookupGlobal((std::string)"main");
    //if the main node is not found, or it isn't a function declaration, throw an error.
    if(!(mainNode !=NULL && mainNode->nkind == DeclK && mainNode->subkind.decl == FuncK && mainNode->child[0]==NULL)) {
        printf("ERROR(LINKER): A function named 'main()' with no parameters must be defined.\n");
        errors++;
    }

    freeTree(ioRoot);
    return;
}

//recursive function that does semantic analysis and updates types of TreeNodes
void treeTraverse(TreeNode *node, SymbolTable *sTable, bool updateErrors) {
    if(node == NULL)//insurance in case I mess up
        return;

    //if a new scope is being entered, add it.
    
    //enter the scope here. Also start traversal of function arguments
    int foffset_store = foffset;
    int ret = enterScope(node, sTable, updateErrors);
    bool newscope = (ret & ( 1 << 0 )) >> 0;//get the values from the first bit
    bool newloop = (ret & ( 1 << 1 )) >> 1;//get the value from the second bit
    //TODO: add another bit for a new function

    addNode(node, sTable);//add the declarations before visiting the children
    
    //store the previous scope start (even if null) to replace the old value when done
    void *prevScopeStart = sTable->getScopeStart();
    if(newscope) {
        sTable->setScopeStart(node);
    }

    for(int i = 0; i < MAXCHILDREN; i++) {//calls analysis on children recursively
        //special case for function arguments
        if(node->nkind == DeclK && node->subkind.decl == FuncK && i == 0) 
            i++;//skip them, they are handled by the compound statement
        if(node->child[i] != NULL) {
            treeTraverse(node->child[i], sTable, updateErrors);
        }
    }

    if(updateErrors) {
        switch(node->nkind) {//update the parent node's types after recursively updating the children. Then check for errors
            case StmtK:
                checkStmtNode(node, sTable, foffset_store);//this just wasn't here for a while? not sure if I was handling it elsewhere
                break;
            case DeclK://currently doing nothing with this
                checkDeclNode(node, sTable);
                break;
            case ExpK:
                checkExpNode(node, sTable);
                break;
            default:
                break;
        }
    } else {
        if(node->nkind == DeclK)
            checkDeclNode(node, sTable);
    }

    if(newscope) {//leave the scope after completing the node
        sTable->leave();
        sTable->setScopeStart(prevScopeStart);
        foffset = foffset_store;
    }
    if(newloop) {//decrement the loop count after leaving it
        sTable->addToLoopNum(-1);
    }
    //after the children are explored, continue with each sibling
    if(!(node->sibling == NULL)) {
        treeTraverse(node->sibling, sTable, updateErrors);
    }
}

//add declarations to the symboltable before descending the rest of the way.
//also update localR type for each node in a function
void addNode(TreeNode *node, SymbolTable *sTable) {
    TreeNode *origNode;
    //updates the variables that are in the function. carve-out clause for constants, which are global, and things that are specified as static/other
    if(sTable->getCurrentFunction()!=NULL && node->getRType()==GlobalR && !(node->nkind==ExpK && node->subkind.exp == ConstantK)) {//u
        origNode = (TreeNode *) sTable->getCurrentFunction();
        node->setRType(LocalR);
        //printf("Current func:%s, line:%d. Node:%s line:%d\n", origNode->text.c_str(), origNode->lineno, node->text.c_str(), node->lineno);
    }
    if(node->nkind == DeclK && node->tokkind != -1/* Check that the node recieved a valid token*/) {
        if(!sTable->insert(node->text, node)) {//add the symbol and node to the table. if there is an error, note it
            errors++;
            origNode = (TreeNode *) sTable->lookup((std::string)node->text);
            printf("ERROR(%d): Symbol '%s' is already declared at line %d.\n", node->lineno, node->text.c_str(), origNode->lineno);
        } else {
            if(node->subkind.decl==FuncK) {//update the current function to be the node
                sTable->setCurrentFunction(node);
                node->setRType(GlobalR);
                int param = node->numParams();
                node->setSize(-param-2);
                foffset -= 2;//the params add their own sizes in
                if(sTable->getDebug()) {
                    printf("STATUS(%d): Node:%s size:%d, Param num:%d\n", node->lineno, node->text.c_str(), node->getSize(), param);
                }
            }

            if(sTable->getScopeStart() == NULL||node->getStatic()) {//don't check if global vars are initialized. static vars start initialized to 0
                if(sTable->getDebug()) {
                    printf("STATUS(%d): Symbol '%s' is being initialized.\n", node->lineno, node->text.c_str());
                }
                node->setInitialize(true);//TODO: make this more accurate (it's not initialized, it's just not being checked.)
            }
        }
    } 
}

void checkStmtNode(TreeNode *node, SymbolTable *sTable, int start_foffset) {
    switch (node->subkind.stmt) {
        case IfK:
            if(node->child[0]->expType != Boolean) {
                printf("ERROR(%d): Expecting Boolean test condition in if statement but got %s.\n", node->lineno, printType(node->child[0]->expType).c_str());
                errors++;
            }
            break;
        case WhileK:
            if(node->child[0]->expType != Boolean) {
                printf("ERROR(%d): Expecting Boolean test condition in while statement but got %s.\n", node->lineno, printType(node->child[0]->expType).c_str());
                errors++;
            }
            break;
        case ForK:
            for(int i = 0; i<3; i++){//check that the type of each child of the range is an int
                TreeNode *rangeN = node->child[1];
                if(rangeN != NULL) {
                    if(rangeN->child[i] != NULL && rangeN->child[i]->expType != Integer) {
                        errors++;
                        printf("ERROR(%d): Expecting type int in position %d in range of for statement but got %s. (name: %s)\n", 
                        node->lineno, i+1, printType(rangeN->child[i]->expType).c_str(), rangeN->child[i]->text.c_str());
                    }
                    if(rangeN->child[i] != NULL && rangeN->child[i]->getArray()) {//check that each is not an array
                        errors++;
                        printf("ERROR(%d): Cannot use array in position %d in range of for statement. (name: %s)\n", 
                        node->lineno, i+1, rangeN->child[i]->text.c_str());
                    }
                }
            }
            node->setMem(0);
            node->setSize(foffset);
            if(sTable->getDebug())
                printf("Status(%d): Updating foffset of for node to be %d\n", node->lineno,  foffset);
            break;    
        case CompoundK:
            node->setRType(NoneR);
            node->setSize(foffset);
            //node->setMem(0);
            break; 
        case ReturnK:
            if(sTable->getCurrentFunction()!=NULL) {//return must be inside a function
                TreeNode *funcNode = (TreeNode *) sTable->getCurrentFunction();
                if(node->child[0]!= NULL) {
                    node->expType = node->child[0]->expType;
                    if(node->child[0]->getArray()) {
                        printf("ERROR(%d): Cannot return an array.\n", node->lineno);
                        errors++;
                    }
                    if(funcNode->expType != node->child[0]->expType){
                        if(funcNode->expType == Void) {
                            printf("ERROR(%d): Function '%s' at line %d is expecting no return value, but return has a value.\n", 
                        node->lineno, funcNode->text.c_str(), funcNode->lineno);
                        } else {
                            printf("ERROR(%d): Function '%s' at line %d is expecting to return %s but returns %s.\n", 
                            node->lineno, funcNode->text.c_str(), funcNode->lineno, printType(funcNode->expType).c_str(), printType(node->expType).c_str());
                        }
                        errors++;
                    }
                } else if(funcNode->expType != Void) {//error, the function expects a return type
                    printf("ERROR(%d): Function '%s' at line %d is expecting to return %s but return has no value.\n", 
                        node->lineno, funcNode->text.c_str(), funcNode->lineno, printType(funcNode->expType).c_str());
                    errors++;
                }
            } else {//error case for when return is not in a function
                //not in current list of error cases. Add it when it is.
                //errors++;
            }
            break; 
        case BreakK:
        {
            TreeNode *scopeStartNode = (TreeNode *) sTable->getScopeStart();
            if(sTable->getLoopNum()<=0) {
                printf("ERROR(%d): Cannot have a break statement outside of loop.\n", node->lineno);
                errors++;
                sTable->addToLoopNum(-sTable->getLoopNum());//reset to 0 if it somehow was below that
            }
            break;
        }
        case RangeK:
            break; 
        default:
            break;
    }
}

//currently only updating the active function for sTable
//added in updating memory position and the offset
void checkDeclNode(TreeNode *node, SymbolTable *sTable) {
    switch (node->subkind.decl) {
        case VarK:
            switch(node->getRType()) {
                case StaticR:
                case GlobalR:
                    if(node->getArray()) {
                        node->setMem(goffset-1);
                    } else {
                        node->setMem(goffset);
                    }
                    goffset-=node->getSize();
                    break;
                case ParamR:
                case LocalR:
                    if(node->getArray()) {
                        node->setMem(foffset-1);
                    } else {
                        node->setMem(foffset);
                    }
                    foffset-=node->getSize();
                    break;
                default:
                    printf("Variable without proper memory type\n");
                    break;
            }
            break;
        case FuncK://leave the current function after finishing with the node.
            //TODO: get the return type and check that it existed (correct return is checked elsewhere, just need to see that it exists)
            sTable->setCurrentFunction(NULL);
            if(sTable->getDebug()) {
                printf("STATUS(%d) Leaving function Node:%s\n", node->lineno, node->text.c_str());
            }
            foffset = 0;
            break;
        case ParamK:
            node->setRType(ParamR);
            node->setSize(1);
            node->setMem(foffset);
            foffset--;
            break;    
        default:
            break;
    }
}

void checkExpNode(TreeNode *node, SymbolTable *sTable) {
    switch (node->subkind.exp) {
        case OpK:
        {
            int flag = opFlag(node->attr.opkind);
            if(flag == 1) {//for boolean equality comparisons with two children
                node->expType = Boolean;
                
                if(node->child[0]->expType != node->child[1]->expType) {
                    errors++;
                    printf("ERROR(%d): '%s' requires operands of the same type but lhs is %s and rhs is %s.\n", node->lineno,
                        node->text.c_str(), printType(node->child[0]->expType).c_str(), printType(node->child[1]->expType).c_str());
                }
            } else if(flag == 2) {//for everything else?
                node->expType = Integer;//default, change with later checks if needed
                switch(node->attr.opkind) {
                    case 13://+
                    case 16://'/'
                    case 17://%
                        if(node->child[0]!= NULL && node->child[0]->expType != Integer) {
                            errors++;
                            printf("ERROR(%d): '%s' requires operands of the type int but lhs is %s.\n", node->lineno,
                            node->text.c_str(), printType(node->child[0]->expType).c_str());
                        }
                        if(node->child[1]!= NULL && node->child[1]->expType != Integer) {
                            errors++;
                            printf("ERROR(%d): '%s' requires operands of the type int but rhs is %s.\n", node->lineno,
                            node->text.c_str(), printType(node->child[1]->expType).c_str());
                        }
                        if(node->child[0]->getArray()||node->child[1]->getArray()) {
                            errors++;
                            printf("ERROR(%d): The operation '%s' does not work with arrays.\n", node->lineno, node->text.c_str());
                        }
                        break;
                    case 14://- TODO:add unary check
                        if(node->child[1] == NULL) {
                            if(node->child[0] != NULL && node->child[0]->expType != Integer) {
                                errors++;
                                printf("ERROR(%d): Unary chsign requires an operand of type int but was given %s.\n", 
                                node->lineno, printType(node->child[0]->expType).c_str());
                            }
                        } else {
                            if(node->child[0]!= NULL && node->child[0]->expType != Integer) {
                                errors++;
                                printf("ERROR(%d): '%s' requires operands of the type int but lhs is %s.\n", node->lineno,
                                node->text.c_str(), printType(node->child[0]->expType).c_str());
                            }
                            if(node->child[1]->expType != Integer) {
                                errors++;
                                printf("ERROR(%d): '%s' requires operands of the type int but rhs is %s.\n", node->lineno,
                                node->text.c_str(), printType(node->child[1]->expType).c_str());
                            }
                        }
                        
                        break;
                    case 15://* or sizeof
                        if(node->child[1] == NULL && !(node->child[0]->getArray())) {
                            errors++;
                            printf("ERROR(%d): The operation sizeof only works with arrays.\n", node->lineno);
                        }
                        if(node->child[0]!= NULL && node->child[1]!= NULL && node->child[0]->expType != Integer) {//TODO:lots of duplicate code. should find a way to compress this
                            errors++;
                            printf("ERROR(%d): '%s' requires operands of the type int but lhs is %s.\n", node->lineno,
                            node->text.c_str(), printType(node->child[0]->expType).c_str());
                        }
                        if(node->child[1]!= NULL && node->child[1]->expType != Integer) {
                            errors++;
                            printf("ERROR(%d): '%s' requires operands of the type int but rhs is %s.\n", node->lineno,
                            node->text.c_str(), printType(node->child[1]->expType).c_str());
                        }
                        break;
                    case 18://? or qmark
                        if(node->child[0] != NULL && node->child[0]->expType != Integer) {
                            errors++;
                            printf("ERROR(%d): Unary '?' requires an operand of type int but was given %s.\n", 
                            node->lineno, printType(node->child[0]->expType).c_str());
                        }
                        if(node->child[0]->getArray()) {
                            errors++;
                            printf("ERROR(%d): The operation ? does not work with arrays.\n", node->lineno);
                        }
                        break;
                    case 19://Lbrack or '['
                        if(node->child[0]!=NULL)//update the type to match the array type
                            node->expType = node->child[0]->expType;
                        if(node->child[1] != NULL) {
                            bool indexed = true;
                            if(node->child[1]->expType != Integer){
                                errors++;
                                indexed = false;
                                printf("ERROR(%d): Array '%s' should be indexed by type int but got %s.\n", node->lineno, node->child[0]->text.c_str(), printType(node->child[1]->expType).c_str());
                            }
                            if(node->child[1]->getArray()) {
                                printf("ERROR(%d): Array index is the unindexed array '%s'.\n", node->lineno, node->child[1]->text.c_str());
                                errors++;
                                indexed = false;
                            }
                            if(indexed)
                                node->setArray(false);
                        }
                        break;
                    default:
                        break;  
                }
            } else if (flag == 3) {//for two comparison booleans that require booleans
                node->expType = Boolean;
                if(node->child[0] != NULL) {//TODO: Stop the crashes
                    if(node->child[0]->expType != Boolean) {
                        errors++;
                        printf("ERROR(%d): '%s' requires operands of type bool but lhs is of type %s.\n", 
                        node->lineno, node->text.c_str(), printType(node->child[0]->expType).c_str());
                    }
                }
                if(node->child[1]!=NULL) {
                    if(node->child[1]->expType != Boolean) {
                        errors++;
                        printf("ERROR(%d): '%s' requires operands of type bool but rhs is of type %s.\n",
                         node->lineno, node->text.c_str(),printType(node->child[0]->expType).c_str());
                    }
                }
            } else if (flag == 4) {//for unary booleans that require booleans
                node->expType = Boolean;
                if(node->child[0] != NULL) {
                    if(node->child[0]->expType != Boolean)
                        printf("ERROR(%d): Unary 'not' requires an operand of type bool but was given type %s.\n", node->lineno, printType(node->child[0]->expType).c_str());
                }
            } else if (flag == -1) {//default case, only show error message if in debug mode
                printf("Error (%d) with operator values, text:%s op:%d\n", node->lineno, node->text.c_str(), node->attr.opkind);
                //something went wrong, once each opk value is chosen this shouldn't happen anymore
            }
            break;
        }
        case ConstantK:
            if(node->getArray()) {
                node->setMem(goffset-1);
                goffset-=node->getSize();
            }
            break;
        case IdK:
            if(node->tokkind != -1) {
                TreeNode *declNode = (TreeNode *) sTable->lookup((std::string)node->text);
                if(declNode == NULL) {
                    errors++;
                    printf("ERROR(%d): Symbol '%s' is not declared.\n", node->lineno, node->text.c_str());
                    //if(sTable->getDebug())
                } else if(declNode->subkind.decl == FuncK) {
                    errors++;
                    printf("ERROR(%d): Cannot use function '%s' as a variable.\n", node->lineno, node->text.c_str());
                } else {
                    declNode->setUsage(true);
                    //every node is returning as initialized rn for some reason
                    //printf("(%d) name: %s, decl:%d, going to check init: %d\n", node->lineno, node->text.c_str(), declNode->lineno, declNode->isInitialized);
                    
                    node->setSize(declNode->getSize());
                    node->setMem(declNode->getMem());
                    node->setRType(declNode->getRType());
                    node->setStatic(declNode->getStatic());

                    TreeNode* par = node->parent;
                    if(!(declNode->getInitialize()) && par != NULL) {
                        if(par->nkind == ExpK && (par->subkind.exp==AssignK && par->child[0] == node)||
                        (par->subkind.exp == OpK && par->attr.opkind == 19 && par->parent!=NULL && par->parent->nkind == ExpK && par->parent->subkind.exp == AssignK && par->parent->child[0] == par)) {//the second case is a pain to write out. there has to be a better way
                        }/* else {//TODO: fix this
                            warnings++;
                            printf("WARNING(%d): Variable '%s' may be uninitialized when used here.\n", node->lineno, node->text.c_str());
                        }*/
                    }
                    if(sTable->getDebug()) {
                        printf("STATUS(%d) Updating Id node %s to have expType %s and array status %d\n", node->lineno, node->text.c_str(), printType(declNode->expType).c_str(), declNode->getArray());
                    }
                    node->expType = declNode->expType;
                    node->setArray(declNode->getArray());
                }
                if(node->parent!=NULL && node->parent->nkind==ExpK && node->parent->subkind.exp==OpK) {//if the idk is a child of an op      
                    if(node->parent->attr.opkind == 19) {//the id is a child of an array, since op is LBRACK, or "["
                        //check that it is the first child of the parent and that the decl is not an array
                        if((declNode == NULL || !declNode->getArray()) && node->parent->child[0] == node) {
                            errors++;
                            printf("ERROR(%d): Cannot index nonarray '%s'.\n", node->lineno, node->text.c_str());
                        }
                    }
                }
            }
            break;    
        case AssignK:
            if(node->child[1] != NULL) {//if this is a binary assignment
                if(node->child[0]->expType != node->child[1]->expType) {
                    errors++;
                    printf("ERROR(%d): '%s' requires operands of the same type but lhs is %s and rhs is %s.\n", node->lineno,
                        node->text.c_str(), printType(node->child[0]->expType).c_str(), printType(node->child[1]->expType).c_str());
                }
                //debug message. currently the array differences are not being flagged bc things are not properly labeled as arrays
                if(sTable->getDebug()) {
                    printf("child 1 array status: %d, child 2 array status: %d\n", node->child[0]->getArray(), node->child[1]->getArray());
                }
                if(node->child[0]->getArray() != node->child[1]->getArray()) {
                    errors++;
                    if(node->child[0]->getArray()) {
                        printf("ERROR(%d): '%s' requires both operands be arrays or not but lhs is an array and rhs is not an array.\n", 
                            node->lineno, node->text.c_str());
                    } else {
                        printf("ERROR(%d): '%s' requires both operands be arrays or not but lhs is not an array and rhs is an array.\n", 
                            node->lineno, node->text.c_str());
                    }
                }

                node->expType = node->child[0]->expType;
                node->setArray(node->child[0]->getArray());
            } else {//if this is an increment or decrement
                if(node->child[0]->expType != Integer) {
                    errors++;
                    printf("ERROR(%d): Unary '%s' requires an operand of int but was given %s.\n", node->lineno, 
                        node->text.c_str(), printType(node->child[0]->expType).c_str());
                } else {
                    node->expType = node->child[0]->expType;
                }
                TreeNode *c1 = node->child[0];
                if(c1->nkind ==ExpK &&c1->subkind.exp==OpK && c1->attr.opkind == 19) {//check if the node is an array
                    if(c1->child[0] != NULL) {
                        TreeNode *declNode = (TreeNode *) sTable->lookup((std::string)c1->child[0]->text);
                        if(declNode != NULL)
                            declNode->setInitialize(true);
                    }
                } else {
                    TreeNode *declNode = (TreeNode *) sTable->lookup((std::string)c1->text);
                    if(declNode != NULL)
                        declNode->setInitialize(true);
                }
            }
            break;
        case InitK:
            break;
        case CallK:
            if(node->tokkind != -1){//kind of meaningless safety check
                TreeNode *funcNode = (TreeNode *) sTable->lookup((std::string)node->text);
                if(funcNode == NULL) {//there is no previous declaration
                    errors++;
                    printf("ERROR(%d): Symbol '%s' is not declared.\n", node->lineno, node->text.c_str());
                } else if(funcNode->subkind.decl != FuncK){//if trying to call something that is not a function
                    errors++;
                    printf("ERROR(%d): '%s' is a simple variable and cannot be called.\n", node->lineno, node->text.c_str());
                } else {
                    funcNode->setUsage(true);
                    int numParam = 0;//define the number of parameters
                    TreeNode *param = funcNode->child[0];
                    while(param != NULL) {//iterate through each parameter
                        numParam++;
                        param = param->sibling;
                    }
                    param = funcNode->child[0];
                    bool isPArray[numParam];
                    ExpType pType[numParam];
                    for(int i = 0; i<numParam; i++) {//iterate through the parameters again, this time defining the array status and ExpType
                        if(param == NULL) {//safety flag
                            printf("ERROR:Incorrect Handling of function parameters\n");
                            break;
                        }
                        isPArray[i] = param->getArray();
                        pType[i] = param->expType;
                        param = param->sibling;
                    }
                    int numArg = 0;//check against the number and exptype of the defining node's arguments.
                    TreeNode *arg = node->child[0];
                    //there could be a crashing error if param is NULL here
                    while(arg!=NULL) {//iterate through each argument
                        if(numArg>=numParam) {//if the number of arguments exceeds the array thresholds, exit before comparing against them
                            printf("ERROR(%d): Too many parameters passed for function '%s' declared on line %d.\n", 
                            node->lineno, node->text.c_str(), funcNode->lineno);
                            errors++;
                            break;
                        }
                        if(arg->expType != pType[numArg]){
                            printf("ERROR(%d): Expecting %s in parameter %i of call to '%s' declared on line %d but got %s.\n", node->lineno, 
                            printType(pType[numArg]).c_str(), numArg+1, node->text.c_str(), funcNode->lineno, printType(arg->expType).c_str());
                            errors++;
                        }
                        if(arg->getArray() != isPArray[numArg]) {
                            if(isPArray[numArg]){
                                printf("ERROR(%d): Expecting array in parameter %i of call to '%s' declared on line %d.\n", 
                                node->lineno, numArg+1, node->text.c_str(), funcNode->lineno);
                                errors++;
                            } else {
                                printf("ERROR(%d): Not expecting array in parameter %i of call to '%s' declared on line %d.\n", 
                                node->lineno, numArg+1, node->text.c_str(), funcNode->lineno);
                                errors++;
                            }
                        }
                        arg = arg->sibling;
                        numArg++;
                    }
                    if(numArg < numParam) {//checking for the correct number of arguments
                        printf("ERROR(%d): Too few parameters passed for function '%s' declared on line %d.\n", 
                        node->lineno, node->text.c_str(), funcNode->lineno);
                        errors++;
                    }
                    node->expType = funcNode->expType;
                }
            }
            break;
        default:
            break;
    }
    return;
}

int opFlag(OpKind o) {
    int flag;
    switch(o) {
        case 7://
        case 8://
        case 9://
        case 10:
        case 11:
        case 12:
            flag = 1;//boolean opkind on same types
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            flag = 2;//integer opkind (and extra stuff)
            break;
        case 29://and
        case 30://or
            flag = 3;//boolean operator on booleans
            break;
        case 31://not
            flag = 4;//unary boolean operator on booleans
            break;
        default:
            flag = -1;//error checking value
            break;
    }
    return flag;
}

//given an expType, return the equivalent string
std::string printType(ExpType type) {
    
    switch(type) {
        case 0:
            return "void";
        case 1:
            return "int";
        case 2:
            return "bool";
        case 3:
            return "char";
        case 4:
            return "charInt";
        case 5:
            return "equal";
        default:
            return "undefinedType";
    }
    
}

TreeNode * generateIOTree() {
    //create each node.
    TreeNode *input = new TreeNode(FuncK, Integer, new LToken(3, -1, "input"), NULL, NULL, NULL);
    TreeNode *inputb = new TreeNode(FuncK, Boolean, new LToken(3, -1, "inputb"), NULL, NULL, NULL);
    TreeNode *inputc = new TreeNode(FuncK, Char, new LToken(3, -1, "inputc"), NULL, NULL, NULL);
    TreeNode *dummy = new TreeNode(ParamK, Integer, new LToken(3, -1, "dummy"), NULL, NULL, NULL);
    TreeNode *output = new TreeNode(FuncK, Void, new LToken(3, -1, "output"), dummy, NULL, NULL);
    TreeNode *dummyb = new TreeNode(ParamK, Boolean, new LToken(3, -1, "dummyb"), NULL, NULL, NULL);
    TreeNode *outputb = new TreeNode(FuncK, Void, new LToken(3, -1, "outputb"), dummyb, NULL, NULL);
    TreeNode *dummyc = new TreeNode(ParamK, Char, new LToken(3, -1, "dummyc"), NULL, NULL, NULL);
    TreeNode *outputc = new TreeNode(FuncK, Void, new LToken(3, -1, "outputc"), dummyc, NULL, NULL);
    TreeNode *outnl = new TreeNode(FuncK, Void, new LToken(3, -1, "outnl"), NULL, NULL, NULL);
    //make the nodes siblings
    addSibling(input, inputb);
    addSibling(inputb, inputc);
    addSibling(inputc, output);
    addSibling(output, outputb);
    addSibling(outputb, outputc);
    addSibling(outputc, outnl);
    //return the root node
    return input;
}

void warnIfNotUsed(std::string name, void * node) {
    TreeNode *tNode = (TreeNode *) node;//don't check main or io functions
    if(name=="main" || name=="input" || name == "inputb" || name == "inputc" || name == "output" || name == "outputb" || name == "outputc" || name == "outnl")
        return;
    if(node == NULL)//safety return. could setup error checking here
        return;
    if(tNode->nkind == DeclK && !tNode->getUsage()) {
        switch (tNode->subkind.decl) {
            case FuncK:
                printf("WARNING(%d): The function '%s' seems not to be used.\n", tNode->lineno, tNode->text.c_str());
                warnings++;
                break;
            case ParamK:
                printf("WARNING(%d): The parameter '%s' seems not to be used.\n", tNode->lineno, tNode->text.c_str());
                warnings++;
                break;
            case VarK:
                //TODO: fix this.
                printf("WARNING(%d): The variable '%s' seems not to be used.\n", tNode->lineno, tNode->text.c_str());
                warnings++;
                break;
            default:
                //could put an error warning here
                break;

        }
    }
}

//enters a scope if neccessary, and returns an int that contains two bools: bit 1 (0 or 1) is whether a newscope was entered or not
//bit 2(0 or 2) tracks whether a loop was entered or not
//bit 3 tracks wheter a function was entered.
//Also start traversal of function arguments
int enterScope(TreeNode *node, SymbolTable *sTable, bool updateErrors) {
    bool newscope = false;
    bool newloop = false;
    bool newfunc = false;
    if(node->nkind == StmtK) {
        if(node->subkind.stmt == CompoundK) {//function arguments need to be local to their function. Currently it works, but is looking back to the first declaration when an overlap exists
            sTable->enter("Compound Line "+std::to_string(node->lineno));
            newscope = true;
            //a special case for function arguments, which are located inside the compound of the function
            if(node->parent != NULL && node->parent->nkind == DeclK && node->parent->subkind.decl == FuncK) {
                treeTraverse(node->parent->child[0], sTable, updateErrors);
            }

        } else if(node->subkind.stmt == ForK) {//case of for, the index variable needs to be inside this scope. Check that.
            sTable->enter("For Line "+std::to_string(node->lineno));
            sTable->addToLoopNum(1);
            newscope = true;
            newloop = true;
        } else if(node->subkind.stmt == WhileK) {
            sTable->enter("While Line "+std::to_string(node->lineno));
            sTable->addToLoopNum(1);
            newscope = true;
            newloop = true;
            
        } else if(node->subkind.stmt == IfK) {
            sTable->enter("If Line "+std::to_string(node->lineno));
            newscope = true;
        }
    }
    if(node->nkind == DeclK && node->subkind.decl == FuncK) {
        newfunc = true;
    }
    int retval = 0;
    if(newscope)
        retval++;
    if(newloop)
        retval+=2;
    if(newfunc)
        retval+=4;
    return retval;
}
/*
working
"ERROR(%d): '%s' is a simple variable and cannot be called.\n"
"ERROR(%d): Symbol '%s' is already declared at line %d.\n"
"ERROR(%d): Symbol '%s' is not declared.\n"
"ERROR(%d): '%s' requires operands of the same type but lhs is %s and rhs is %s.\n"
"ERROR(%d): Cannot use array in position %d in range of for statement.\n"
"ERROR(%d): Array '%s' should be indexed by type int but got %s.\n"
"ERROR(%d): Array index is the unindexed array '%s'.\n"
"ERROR(%d): Cannot index nonarray '%s'.\n"
"ERROR(%d): Expecting %s in position %d in range of for statement but got %s.\n"
"ERROR(%d): Expecting array in parameter %i of call to '%s' declared on line %d.\n"
"ERROR(%d): Initializer for variable '%s' of %s is of %s\n"
"ERROR(%d): Initializer for variable '%s' requires both operands be arrays or not
but variable is%s an array and rhs is%s an array.\n"
"ERROR(%d): The operation '%s' does not work with arrays.\n"
"ERROR(%d): The operation '%s' only works with arrays.\n"
"ERROR(%d): Unary '%s' requires an operand of %s but was given %s.\n"
"ERROR(%d): Too few parameters passed for function '%s' declared on line %d.\n"
"ERROR(%d): Too many parameters passed for function '%s' declared on line %d.\n"
"ERROR(%d): Cannot have a break statement outside of loop.\n"
"ERROR(%d): '%s' requires both operands be arrays or not but lhs is%s an array and
rhs is%s an array.\n"
"ERROR(%d): Not expecting array in parameter %i of call to '%s' declared on line
%d.\n"
"ERROR(%d): Expecting %s in parameter %i of call to '%s' declared on line %d but
got %s.\n"
"ERROR(%d): '%s' requires operands of %s but lhs is of %s.\n"
"ERROR(%d): '%s' requires operands of %s but rhs is of %s.\n"
"ERROR(%d): Cannot use function '%s' as a variable.\n"


not implemented/not working properly
"ERROR(%d): Cannot use array as test condition in %s statement.\n"
"ERROR(%d): Initializer for variable '%s' is not a constant expression.\n"
"WARNING(%d): Expecting to return %s but function '%s' has no return statement.\n"
"WARNING(%d): Variable '%s' may be uninitialized when used here.\n"
"ERROR(%d): Expecting Boolean test condition in %s statement but got %s.\n"

*/
