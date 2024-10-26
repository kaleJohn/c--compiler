/**
 * @file codeGen.cpp
 * @author Kaleb Johnson
 * @brief 
 * @version 0.1
 * @date 2023-05-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "codeGen.h"
struct rStore r[rNum];
int tOffset;
bool dataReg[drNum];
std::vector<int> breakpoints;
std::vector<func> funcs;
std::vector<TreeNode*> globals;

void generateCode(TreeNode *root, int globalEnd) {
    tOffset = 0;
    for(int i=0; i<rNum; i++){
        r[i].used = false;
    }
    breakpoints.push_back(-1);
    emitSkip(1);
    genIO();//generate the built-in io functions
    generateTreeCode(root);//generate the code from the AST
    backPatchAJumpToHere(0, "Jump to init [backpatch]");
    emitComment("INIT");
    emitRM("LDA", 1, globalEnd, 0, "set the first frame at the end of globals");
    emitRM("ST", 1, 0, 1, "stores old fp (points to self?)");
    emitComment("INIT ALL GLOBALS AND STATICS");
    loadGlobals();
    //TODO: init
    emitComment("END INIT GLOBALS AND STATICS");
    //head to main
    int mainAddr = 1;
    for(auto fun : funcs) {//this should always produce a function, or the semantic analysis would have thrown an error
        if(fun.name == "main")
            mainAddr = fun.line;
    }
    emitRM("LDA", 3, 1, 7, "return address in register");
    emitGotoAbs(mainAddr, "jump to main");
    //end the program
    emitRO("HALT", 0, 0, 0, "DONE");
    emitComment("END PROGRAM");
}

void generateTreeCode(TreeNode *root) {
    if(root == NULL) 
        return;

    switch(root->nkind){
        case DeclK:
            generateDecl(root);
            break;
        case StmtK:
            generateStmt(root);
            break;
        case ExpK:
            generateExp(root);
            break;
    }

    // Process the sibling nodes
    if(root->sibling != NULL)
        generateTreeCode(root->sibling);
    
}

/*const char* symbolGen (TreeNode *symbol) {

}*/

int regAlloc(const char* name){
    static int lastReg;
    for(int i = 3; i<rNum; i++){
        if(r[i].used == false){
            r[i].used = true;
            strcpy(r[i].name, name);
            lastReg = i;
            return i;
        }
    }
    emitComment("Error: No registers to allocate");
    if(lastReg < 6){//a fail case that is less likely to blow stuff up
        lastReg++;
    } else {
        lastReg = 3;
    }
    return lastReg;
}
int dataRegAlloc() {
    for(int i = 0; i<drNum; i++) {
        if(dataReg[i] == false) {
            dataReg[i] == true;
            return i;
        }
    }
    emitComment("Error: maximum \'data registers\' used.");
    return drNum-1;
}
void freeReg(int index) {
    if(index > -1  && index < rNum)
        r[index].used = false;
}
const char* regName(int index) {
    return r[index].name;
}
void clearReg(){
    for(int i = 3; i<rNum; i++){
        r[i].used = false;
    }
}
void clearDReg() {
    for(int i = 0; i<drNum; i++){
        dataReg[i] = false;
    }
}
void freeDReg(int index) {
    if(index > -1  && index < drNum)
        dataReg[index] == false;
}
int storeDReg(int reg, int index) {
    if(index > -1) {
        emitRM("ST", reg, tOffset-index, 1, "store from the register to a stack \'register\'");
        return index;
    } else {
        int dIndex = dataRegAlloc();
        emitRM("ST", reg, tOffset-dIndex, 1, "store from the register to a stack \'register\'");
        return dIndex;
    }
}
//pops the data from the stack into a register
void loadDReg(int reg, int dIndex) {
    emitRM("LD", reg, tOffset-dIndex, 1, "load from a stack \'register\' to a register");
    freeDReg(dIndex);
}

void generateDecl(TreeNode *node) {
    if(node == NULL)
        return;
    switch(node->subkind.decl) {
        case FuncK:
        {
            emitComment((char *) "FUNCTION ", (char *) node->text.c_str());
            tOffset = node->getSize();
            emitComment((char *) "TOFF set ", node->getSize());
            func genFunc = {node->text, emitWhereAmI()};
            funcs.push_back(genFunc);
            emitRM("ST", 3, -1, 1, "Store return address");
            //continue to the compound statement
            generateTreeCode(node->child[1]);//over traverse children, bc I don't think that traversing params is needed?
            emitComment((char *) "Add standard closing in case there is no return statement");
            emitRM("LDC", 2, 0, 6, "Set return value to 0");
            emitRM("LD", 3, -1, 1, "Load return address");
            emitRM("LD", 1, 0, 1, "Adjust fp");
            emitRM("JMP", 7, 0, 3, "Return");
            emitComment((char *) "END FUNCTION ", (char *) node->text.c_str());
            clearDReg();
            emitComment("TOFF set", 0);
            tOffset = 0;
            break;
        }
        case VarK:
            emitComment((char *) "VARIABLE DEC", (char *) node->text.c_str());
            if(node->getRType() == GlobalR || node->getRType() == StaticR) {
                globals.push_back(node);
            } else {
                if(node->getArray()) {
                    node->setR(regAlloc("Array start"));
                    //minus 1 because it includes the size counter
                    emitRM("LDC", node->getR(), node->getSize()-1, 6, "load size of array ", (char *) node->text.c_str());
                    emitRM("ST", node->getR(), node->getMem()+1, 1, "save size of array ", (char *) node->text.c_str());
                    freeReg(node->getR());
                }
                if(node->child[0] != NULL) {//assign case
                    generateTreeCode(node->child[0]);
                    int childReg = node->child[0]->getR();
                    if(node->getArray()){
                        //get the shortest length
                        int rhsLenReg = regAlloc("RHS len");
                        int lhsLenReg = regAlloc("LHS len");
                        int lhsLoc = regAlloc("LHS addr");
                        emitRM("LDA", lhsLoc, node->getMem(), 1, "get address of lhs");
                        emitRM("LD", rhsLenReg, 1, childReg, "get length of rhs");
                        emitRM("LD", lhsLenReg, 1, lhsLoc, "get length of lhs");
                        emitRO("SWP", rhsLenReg, lhsLenReg, 6, "get the minimum of the two lengths");
                        emitRO("MOV", lhsLoc, childReg, rhsLenReg, "assign the array to ", (char *) node->text.c_str());
                        freeReg(lhsLenReg);
                        freeReg(rhsLenReg);
                        freeReg(lhsLoc);
                    } else {
                        if(node->getRType() == GlobalR || node->getRType() == StaticR) {
                            emitRM("ST", childReg, node->getMem(), 0, "store variable ", (char *) node->text.c_str());
                        } else {
                            emitRM("ST", childReg, node->getMem(), 1, "store variable ", (char *) node->text.c_str());
                        }
                        
                    }
                    freeReg(childReg);
                }
            }
            //emitRM((char *)"LDC", 3, 22, 6, (char *)"load size of array z");
            break;
        case ParamK:
            emitComment((char *) "PARAMETER ", (char *) node->text.c_str());
            traverseChildren(node);//this is also I think unused?
            break;
    }
}

void generateStmt(TreeNode *node) {
    if(node == NULL)
        return;
    switch(node->subkind.stmt) {
        case NullK://I believe entirely unused
            traverseChildren(node);
            break;
        case IfK:
            {
                tOffset = node->getSize();
                emitComment((char *) "IF ", (char *) node->text.c_str());
                emitComment("TOFF set", node->getSize());
                generateTreeCode(node->child[0]);
                emitRM("JNZ", node->child[0]->getR(), 1, 7, "skip the jump to else");
                freeReg(node->child[0]->getR());
                int ifAddr = emitWhereAmI();
                //breakpoints.push_back(ifAddr);
                emitSkip(1);
                emitComment("THEN");
                generateTreeCode(node->child[1]);
                if(node->child[2]!=NULL) {
                    int elseAddr = emitWhereAmI();
                    emitSkip(1);
                    emitComment("ELSE");
                    backPatchAJumpToHere(ifAddr, "jump to the else-part[backpatch]");
                    generateTreeCode(node->child[2]);
                    backPatchAJumpToHere(elseAddr, "jump to the end of the if statement[backpatch]");
                }
                emitComment((char *) "END IF ", (char *) node->text.c_str());
            }
            break;
        case WhileK:
            {//allows me to declare ints
                tOffset = node->getSize();
                emitComment((char *) "WHILE ");
                emitComment("TOFF set", node->getSize());
                int whileStart = emitWhereAmI();
                generateTreeCode(node->child[0]);
                emitRM("JNZ", node->child[0]->getR(), 1, 7, "jump to while part");
                int loopAddr = emitWhereAmI();
                breakpoints.push_back(loopAddr);
                emitSkip(1);
                freeReg(node->child[0]->getR());
                emitComment((char *) "DO");
                generateTreeCode(node->child[1]);
                emitGotoAbs(whileStart, "repeat the while loop");
                backPatchAJumpToHere(loopAddr, "skip the while loop [backpatch]");
                emitComment((char *) "END WHILE ");
            }
            break;
        case ForK:
        {
            tOffset = node->getSize();
            emitComment((char *) "FOR ", (char *) node->text.c_str());
            emitComment("TOFF set", node->getSize());
            generateTreeCode(node->child[0]);//loop variable
            generateTreeCode(node->child[1]);//range
            int varLoc = node->child[0]->getMem();
            if(node->child[0]->nkind == ExpK && node->child[0]->subkind.exp == OpK && node->child[0]->attr.opkind == 19)
                varLoc = getArrayAddr(node->child[0]);
            int endVReg = dataRegAlloc();
            TreeNode* range = node->child[1];
            int startreg = range->child[0]->getR();
            int endreg = range->child[1]->getR();
            emitRM("ST", endreg, endVReg, 1, "Store the exit condition");
            int forStart = emitWhereAmI();
            emitRM("ST", startreg, varLoc, 1, "Store the for variable");//copies startReg over the for variable
            emitRO("TGE", endreg, startreg, endreg, "store whether the index is greater than or equal to the exit condition");//overwrites endreg
            //backpatch position
            emitRM("JZR", endreg, 1, 7, "Skip the backpatched jump unless the index meets the exit condition");
            int patchPos = emitWhereAmI();
            emitSkip(1);
            breakpoints.push_back(patchPos);
            generateTreeCode(node->child[2]);
            freeReg(node->child[2]->getR());
            if(range->child[2] == NULL) {
                emitRM("LDA", startreg, 1, startreg, "Increment the for variable by 1");
            } else {
                generateTreeCode(node->child[2]);
                emitRO("ADD", startreg, node->child[2]->getR(), startreg, "Add the increment to the storage variables");
                freeReg(node->child[2]->getR());
            }
            emitRM("LD", endreg, endVReg, 1, "load the exit value back into the register");
            emitGotoAbs(forStart, "Loop back to the start");
            backPatchAJumpToHere("JMP", endreg, patchPos, "exit the for loop [backpatch]");
            //free up the registers (and stack) from internal tracking.
            freeReg(startreg);
            freeReg(endreg);
            freeDReg(endVReg);
            emitComment((char *) "END FOR ", (char *) node->text.c_str());
        }
            break;
        case CompoundK:
            emitComment((char *) "COMPOUND ", (char *) node->text.c_str());
            emitComment((char *) "TOFF set ", node->getSize());
            tOffset = node->getSize();
            traverseChildren(node);
            /*visit the statemtents (not the decls)//just skip the decls?
            generateTreeCode(node->child[1]);*/
            for(int i = 0; i<MAXCHILDREN; i++) {
                if(node->child[i]!=NULL) {
                    freeReg(node->child[i]->getR());
                }
            }
            emitComment((char *) "END COMPOUND ", (char *) node->text.c_str());
            break;
        case ReturnK:
            emitComment((char *) "RETURN ", (char *) node->text.c_str());
            if(node->child[0]!= NULL) {
                generateTreeCode(node->child[0]);
                if(node->child[0]->getR()>-1)
                    emitRM("LDA", 2, 0, node->child[0]->getR(), "Put the return value in the return register");
            }
            emitRM("LD", 3, -1, 1, "Load return address");
            emitRM("LD", 1, 0, 1, "Adjust fp");
            emitRM("JMP", 7, 0, 3, "Return");
            //if(node->child[0] != NULL)
            //the return register is 2
            break;
        case BreakK:
            emitComment((char *) "BREAK ", (char *) node->text.c_str());
            if(breakpoints.back()>-1) {
                emitGotoAbs(breakpoints.back(), "break jump to a backpatch exit");
                breakpoints.pop_back();
            } else {
                emitComment("Error when calculating breakpoint");
            }
            break;
        case RangeK:
            emitComment((char *) "RANGE ", (char *) node->text.c_str());
            traverseChildren(node);
            //handle it all in the for loop
            break;
    }
}

void generateExp(TreeNode *node) {
    if(node == NULL)
        return;
    switch(node->subkind.exp) {
        case OpK:
            generateOp(node);
            break;
        case ConstantK:
            node->setR(regAlloc("Constant"));
            if(node->getArray()) {//string case
                //globals.push_back(node); //not needed. the storage is handled here
                emitStrLit(node->getMem(), (char *) node->text.c_str());//might need mem to be negative.
                emitRM("LDA", node->getR(), node->getMem(), 0, "load the address of char array");//0 since a global
            } else {
                node->setSize(0);
                switch(node->expType) {
                    case Integer:
                        emitRM("LDC", node->getR(), node->attr.value, 6, "load integer constant");
                        break;
                    case Boolean:
                        emitRM("LDC", node->getR(), node->attr.value, 6, "load boolean constant (just treated as an int)");
                        break;
                    case Char:
                        emitRM("LDC", node->getR(), node->attr.cvalue, 6, "load char constant");
                        break;
                    default:
                        emitComment("Error in constant type checking");
                        freeReg(node->getR());
                }                
            }  
            break;
        case IdK://Done
            node->setR(regAlloc("Id"));
            if(node->getRType() == GlobalR || node->getRType() == StaticR) {
                emitRM("LD", node->getR(), node->getMem(), 0, "Loading variable into register");
            } else {
                /*alt node->text.c_str()*/
                emitRM("LD", node->getR(), node->getMem(), 1, "Loading variable into register");
            }
            
            break;
        case AssignK:
        {
            generateTreeCode(node->child[1]);
            bool isLbrack = (node->child[0]->nkind == ExpK && node->child[0]->subkind.exp == OpK && node->child[0]->attr.opkind == 19);
            if(isLbrack) {//this is a horrific way to manage arrays, should probably make a function for this
                int c0Addr = getArrayAddr(node->child[0]);
                if(node->child[1] != NULL) {
                    if(node->text == ":=") {//basic assign
                        //TODO: update the memory so that the [ parent of an array contains the memory location of the array + the index
                        emitRM("ST", node->child[1]->getR(), 0, c0Addr, "Assigning to variable");
                        if(node->parent != NULL) {//if there is something that might want this data
                            node->setR(node->child[1]->getR());
                        } else {
                            freeReg(node->child[1]->getR());
                        }
                    } else {//need a case for +=, -=, /=, etc.
                        //if(node->child[0]->getInitialize()) {//the initialization check doesn't work, so I can't do this w/ out a ton of unintended effects
                        int varStore = regAlloc(node->child[0]->text.c_str());
                        emitRM("LD", varStore, 0, c0Addr, "Load variable for arithmatic assignment");
                        if(node->text == "+=") {
                            emitRO("ADD", varStore, varStore, node->child[1]->getR(), "Assigning addition");
                        } else if(node->text == "-=") {
                            emitRO("SUB", varStore, varStore, node->child[1]->getR(), "Assigning subtraction");
                        } else if(node->text == "*=") {
                            emitRO("MUL", varStore, varStore, node->child[1]->getR(), "Assigning multiplication");
                        } else if(node->text == "/=") {
                            emitRO("DIV", varStore, varStore, node->child[1]->getR(), "Assigning division (truncating)");
                        } else {
                            emitComment("Error with the assign classification");
                            freeReg(varStore);
                            freeReg(node->child[1]->getR());
                            break;
                        }
                        emitRM("ST", varStore, 0, c0Addr, "Assigning to variable");
                        if(node->parent != NULL) {//if there is something that might want this data
                            node->setR(varStore);
                        } else {
                            freeReg(varStore);
                        }
                        freeReg(node->child[1]->getR());       
                    }
                } else {//inc/dec case
                    emitComment("INC/DEC case of assignk");
                    int reg = regAlloc("indexed array");
                    emitRM("LD", reg, 0, c0Addr, "Fetch the indexed value");
                    if(node->text == "++") {
                        emitRM("LDA", reg, 1, reg, "++ (increment assign)");
                    } else if(node->text == "--") {
                        emitRM("LDA", reg, -1, reg, "-- (decrement assign)");
                    }
                    emitRM("ST", reg, 0, c0Addr, "Assigning to variable");
                    if(node->parent != NULL) {//if there is something that might want this data
                        node->setR(reg);
                    } else {
                        freeReg(reg);
                    }
                }
            } else {
                int c0Mem = node->child[0]->getMem();
                if(node->child[1] != NULL) {
                    if(node->text == ":=") {//basic assign
                        //TODO: update the memory so that the [ parent of an array contains the memory location of the array + the index
                        if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR){
                            emitRM("ST", node->child[1]->getR(), c0Mem, 0, "Assigning to variable");
                        } else {
                            emitRM("ST", node->child[1]->getR(), c0Mem, 1, "Assigning to variable");
                        }
                        if(node->parent != NULL) {//if there is something that might want this data
                            node->setR(node->child[1]->getR());
                        } else {
                            freeReg(node->child[1]->getR());
                        }
                    } else {//need a case for +=, -=, /=, etc.
                        //if(node->child[0]->getInitialize()) {//the initialization check doesn't work, so I can't do this w/ out a ton of unintended effects
                        int varStore = regAlloc(node->child[0]->text.c_str());
                        if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR) {
                            emitRM("LD", varStore, c0Mem, 0, "Load variable for arithmatic assignment");
                        } else {
                            emitRM("LD", varStore, c0Mem, 1, "Load variable for arithmatic assignment");
                        }
                        if(node->text == "+=") {
                            emitRO("ADD", varStore, varStore, node->child[1]->getR(), "Assigning addition");
                        } else if(node->text == "-=") {
                            emitRO("SUB", varStore, varStore, node->child[1]->getR(), "Assigning subtraction");
                        } else if(node->text == "*=") {
                            emitRO("MUL", varStore, varStore, node->child[1]->getR(), "Assigning multiplication");
                        } else if(node->text == "/=") {
                            emitRO("DIV", varStore, varStore, node->child[1]->getR(), "Assigning division (truncating)");
                        } else {
                            emitComment("Error with the assign classification");
                            freeReg(varStore);
                            freeReg(node->child[1]->getR());
                            break;
                        }
                        if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR) {
                            emitRM("ST", varStore, c0Mem, 0, "Assigning to variable");
                        } else {
                            emitRM("ST", varStore, c0Mem, 1, "Assigning to variable");
                        }
                        if(node->parent != NULL) {//if there is something that might want this data
                            node->setR(varStore);
                        } else {
                            freeReg(varStore);
                        }
                        freeReg(node->child[1]->getR());       
                    }
                } else {//inc/dec case
                    emitComment("INC/DEC case of assignk");
                    generateTreeCode(node->child[0]);
                    int reg = node->child[0]->getR();
                    if(node->text == "++") {
                        emitRM("LDA", reg, 1, reg, "++ (increment assign)");
                    
                    } else if(node->text == "--") {
                        emitRM("LDA", reg, -1, reg, "-- (decrement assign)");
                    }
                    if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR) {
                        emitRM("ST", reg, c0Mem, 0, "Assigning to variable");
                    } else {
                        emitRM("ST", reg, c0Mem, 1, "Assigning to variable");
                    }
                    if(node->parent != NULL) {//if there is something that might want this data
                        node->setR(reg);
                    } else {
                        freeReg(reg);
                    }
                }
            }
        }
            break;
        case InitK://Done?
            emitComment("INIT");
            traverseChildren(node);
            break;
        case CallK:
        {
            int tempToff = tOffset;
            emitComment((char *) "CALL ", (char *) node->text.c_str());
            int frameSize = node->getSize();
            int funcLoc = -1;
            for(auto fun : funcs) {
                if(fun.name == node->text)
                    funcLoc = fun.line;
            }
            if(funcLoc > -1) {
                /*int replacementR = -1;
                if(r[3].used == true) {//register 3 is in use
                    replacementR = regAlloc("Call Replacement");
                    emitRM("LDA", replacementR, 0, 3, "swap the value into a new register.");
                }*/
                int frameLoc = tOffset;
                emitRM("ST", 1, frameLoc, 1, "store the fp in a ghost frame");
                tOffset -= 2;
                emitComment("TOFF dec (ghost frame) ", tOffset);
                //TODO:store each param in the ghost frame
                callParam(node->child[0], 0);
                emitRM("LDA", 1, frameLoc, 1, "ghost frame becomes the active frame");
                emitRM("LDA", 3, 1, 7, "return address in register");
                emitGotoAbs(funcLoc, "CALL ", (char *) node->text.c_str());
                emitRM("LDA", 3, 0, 2, "save the result in the register");
                node->setR(3);
                emitComment("Set TOFF ", tempToff);
                tOffset = tempToff;
            } else {
                emitComment("Error in finding the called function.");
            }
            emitComment((char *) "END CALL", (char *) node->text.c_str());

            break;
        }
    }
}


//only pass Opk nodes with attr.opkinds to this function
void generateOp(TreeNode *node) {
    if(node==NULL)
        return;
    switch(node->attr.opkind) {
        case 7://><
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("TNE", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "r3 = r3 != r4");
            freeReg(node->child[1]->getR());
            break;
        case 8://>=
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("TGE", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "r3 = r3 >= r4");
            freeReg(node->child[1]->getR());
            break;
        case 9://<=
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("TLE", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "r3 = r3 <= r4");
            freeReg(node->child[1]->getR());
            break;
        case 10://<
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("TLT", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "r3 = r3 < r4");
            freeReg(node->child[1]->getR());
            break;
        case 11://>
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("TGT", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "r3 = r3 > r4");
            freeReg(node->child[1]->getR());
            break;
        case 12://= equality
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("TEQ", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "r3 = r3 == r4");
            freeReg(node->child[1]->getR());
            break;
        case 13://+
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("ADD", node->child[0]->getR(), node->child[1]->getR(), node->child[0]->getR(), "Add two numbers");
            freeReg(node->child[1]->getR());
            break;
        case 14://-
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            if(node->child[1] == NULL) {//unary change sign case
                emitRO("NEG", node->child[0]->getR(), node->child[0]->getR(), 6, "change the sign (negative)");
            } else {
                emitRO("SUB", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "Subtract two numbers");
                freeReg(node->child[1]->getR());
            }
            break;
        case 15://*
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            if(node->child[1] == NULL) {//unary sizeof case
                if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR ) {
                    emitRM("LD", node->getR(), node->child[0]->getMem()+1, 0, "Get size of the array (* op)");
                } else {
                    emitRM("LD", node->getR(), node->child[0]->getMem()+1, 1, "Get size of the array (* op)");
                }
            } else {      
                emitRO("MUL", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "Multiply two numbers");
                freeReg(node->child[1]->getR());
            }
            
            break;
        case 16:// /
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("DIV", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "Divide two numbers (truncating)");
            freeReg(node->child[1]->getR());
            break;
        case 17://%
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("MOD", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "a % b");
            freeReg(node->child[1]->getR());
            break;
        case 18://?
            generateTreeCode(node->child[0]);
            node->setR(node->child[0]->getR());
            emitRO("RND", node->child[0]->getR(), node->child[0]->getR(), 6, "?a");
            break;
        case 19://[ indexing an array
            emitComment("TODO: indexing an array");
            if(node->child[1] != NULL) {
                if(node->child[1]->getR()<0)//since this will be called multiple times, ensure that genTReeCode only runs once
                    generateTreeCode(node->child[1]);
                //load the address of the base of the array
                if(node->child[0]!=NULL) {
                    node->setRType(node->child[0]->getRType());
                    int arrayReg = regAlloc("array");
                    if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR) {
                        emitRM("LDA", arrayReg, node->child[0]->getMem(), 0, "load the address of the base of array ", (char *) node->child[0]->text.c_str());
                    } else {
                        emitRM("LDA", arrayReg, node->child[0]->getMem(), 1, "load the address of the base of array ", (char *) node->child[0]->text.c_str());
                    }
                    emitRO("SUB", arrayReg, arrayReg, node->child[1]->getR(), "compute offset of value");
                    //load the value into it instead of the address, for consistency with other node return values.
                    emitRM("LD", arrayReg, 0, arrayReg, "Load the value of the array at that index");
                    node->setR(arrayReg);
                } else {
                    emitComment("Error when finding array");
                }
                freeReg(node->child[1]->getR());
            } else {
                emitComment("Error when indexing array");
            }
            
            break;
        case 29://and
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("AND", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "a AND b (bitwise)");
            freeReg(node->child[1]->getR());
            break;
        case 30://or
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            emitRO("OR", node->child[0]->getR(), node->child[0]->getR(), node->child[1]->getR(), "a OR b (bitwise)");
            freeReg(node->child[1]->getR());
            break;
        case 31://not
        {
            traverseChildren(node);
            node->setR(node->child[0]->getR());
            //printf("child[0] value, ");
            int oneReg = regAlloc("one");
            emitRM("LDC", oneReg, 1, 6, "create a 1 to XOR against");
            emitRO("XOR", node->child[0]->getR(), node->child[0]->getR(), oneReg, "XOR x, 1 (used as logical not)");
            freeReg(oneReg);
        }
            break;
        default:
            emitComment("Opk Error. Now traversing children: ", (char*) node->text.c_str());
            traverseChildren(node);
            for(int i = 0; i < MAXCHILDREN; i++) {
                if(node->child[i]!= NULL)
                    freeReg(node->child[i]->getR());
            }
            break;
    }
}

void genIO() {
    //input
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION input");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRO("IN", 2, 2, 2, "Grab int input");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION input");

    //output
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION output");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRM("LD", 3, -2, 1, "Load parameter");
    emitRO("OUT", 3, 3, 3, "Output integer");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION output");

    //inputb
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION inputb");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRO("INB", 2, 2, 2, "Grab bool input");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION inputb");

    //outputb
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION outputb");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRM("LD", 3, -2, 1, "Load parameter");
    emitRO("OUTB", 3, 3, 3, "Output bool");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION outputb");

    //inputc
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION inputc");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRO("INC", 2, 2, 2, "Grab char input");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION inputc");

    //outputc
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION outputc");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRM("LD", 3, -2, 1, "Load parameter");
    emitRO("OUTC", 3, 3, 3, "Output char");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION outputc");

    //outnl
    emitComment("** ** ** ** ** ** ** ** ** ** ** ** **");
    emitComment("FUNCTION outnl");
    emitRM("ST", 3, -1, 1, "Store return address");
    emitRO("OUTNL", 3, 3, 3, "Output a newline");
    emitRM("LD", 3, -1, 1, "Load return address");
    emitRM("LD", 1, 0, 1, "Adjust fp");
    emitGoto(0, 3, "Return");
    emitComment("END FUNCTION outnl");

    //update the funcs vector
    func inp = {"input", 1};
    funcs.push_back(inp);
    func out = {"output", 6};
    funcs.push_back(out);
    func inpb = {"inputb", 12};
    funcs.push_back(inpb);
    func outb = {"outputb", 17};
    funcs.push_back(outb);
    func inpc = {"inputc", 23};
    funcs.push_back(inpc);
    func outc = {"outputc", 28};
    funcs.push_back(outc);
    func outnl = {"outnl", 34};
    funcs.push_back(outnl);
}

//handle children
void traverseChildren(TreeNode* node) {
    for (int i = 0; i < MAXCHILDREN; ++i) {
        if(node->child[i] != NULL)
            generateTreeCode(node->child[i]);
    }
}

void callParam(TreeNode* param, int index) {
    if(param!= NULL) {
        emitComment("Param ", index+1);
        generateTreeCode(param);
        int pReg = param->getR();
        //("param: %s has register: %d", param->text.c_str(), pReg);
        if(pReg > -1) {
            emitRM("ST", pReg, tOffset, 1, "push parameter");
        } else {
            emitComment("Error: Parameter could not be pushed");
        }
        tOffset--;
        emitComment("Decrement TOFF ", tOffset);
        freeReg(pReg);
        callParam(param->sibling, index+1);
    }
}

void clearBreaks() {
    while(breakpoints.back() != -1) {
        breakpoints.pop_back();
    }
}

void loadGlobals() {
    for(auto glVar : globals) {
        if(glVar->getArray()) {
            emitRM("LDC", 3, glVar->getSize()-1, 6, "load size of array ", (char *) glVar->text.c_str());
            emitRM("ST", 3, glVar->getMem()+1, 0, "save size of array ", (char *) glVar->text.c_str());
        }
    }
}

//only use this on '[' opk nodes (aka indexed arrays)
//creates a register containing the array index and returns the corresponding int
int getArrayAddr(TreeNode* node){
    if(node->child[1] != NULL) {
        if(node->child[1]->getR()<0)//to prevent multiple generate-codes being run
            generateTreeCode(node->child[1]);
        //load the address of the base of the array
        if(node->child[0]!=NULL) {
            node->setRType(node->child[0]->getRType());
            int arrayReg = regAlloc("array");
            if(node->child[0]->getRType() == GlobalR || node->child[0]->getRType() == StaticR) {
                emitRM("LDA", arrayReg, node->child[0]->getMem(), 0, "load the address of the base of array ", (char *) node->child[0]->text.c_str());
            } else {
                emitRM("LDA", arrayReg, node->child[0]->getMem(), 1, "load the address of the base of array ", (char *) node->child[0]->text.c_str());
            }
            emitRO("SUB", arrayReg, arrayReg, node->child[1]->getR(), "compute offset of value");
            freeReg(node->child[1]->getR());
            return arrayReg;
        } else {
            emitComment("Error when finding array");
        }
        freeReg(node->child[1]->getR());
        return -1;
    } else {
        emitComment("Error when indexing array");
        return -1;
    }
}