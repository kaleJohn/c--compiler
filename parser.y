%code requires {
    /*
    * Parser that will take a stream of tokens from a c- scanner and construct an AST
    * Then apply semantic analysis and update the types of the AST
    * 
    * Kaleb Johnson 
    * 11/05/23
    */
    #include "LToken.h"//Token class
    #include "TreeNode.h"//Tree Node Class
    #include "symbolTable.h"//Symbol Table and Scope Classes
    #include "semantic.h"//semantic analysis
    #include "emitcode.h"
    #include "codeGen.h"
    #include "yyerror.h"//error printing and handling
    #include <stdio.h>//file*, printf
    #include <stdlib.h>//free, malloc
    #include <string.h>//strings
    #include <unistd.h>//getopt
}

%code {

#ifndef YYERROR_VERBOSE
#define YYERROR_VERBOSE
#endif

extern FILE *yyin;
extern std::vector<char*> tokenStore;
int warnings;
int errors;
int foffset;
int goffset;
FILE *code;
extern void yyerror(char*);//might need to make this an extern
int yylex (void);//yylex needs to be defined for my version of bison. this is not used.
extern int yydebug;
TreeNode *syntaxTree;

void freeTree(TreeNode *root);
void printTree(TreeNode *root, FILE* out, int indent, int cnum, bool printTypes, bool printLoc);
void printNode(TreeNode *printed, FILE* out, bool printTypes, bool printLoc);
ExpType genType(int typenum);
//these two could be in the treenode.cpp file
void UpdateSiblingType(TreeNode *sibling, ExpType newtype, int updated = 0);
void updateParenting(TreeNode *root, int debug);
}

%error-verbose

%union {
    TreeNode *node;//nonterminal class
    LToken *token;//terminal class
    int etype;//for passing types
}

%token <token> NUMCONST STRINGCONST ID CHARCONST BOOLCONST LTHAN GTHAN EQ PLUS MINUS STAR DIV MODULO QMARK LBRACK LBRACE
%token <token> IF THEN ELSE WHILE DO FOR TO BY RETURN BREAK OR AND NOT STATIC BOOL CHAR INT ASSIGN PLUSEQ MINUSEQ MULEQ DIVEQ NEQ LEQ GEQ INC DEC

%type <token> unaryop mulop sumop relop assignop varDeclId

%type <node> program declList decl varDecl funDecl scopedVarDecl varDeclList varDeclInit localDeclsExist stmtListExist
%type <node> parms parmList parmTypeList parmIdList parmId stmt matchStmt unmatchStmt selectStmt_M selectStmt_U iterStmt_M iterStmt_U iterRange
%type <node> otherStmt compoundStmt localDecls stmtList expStmt returnStmt breakStmt
%type <node> exp simpleExp andExp unaryRelExp relExp sumExp mulExp unaryExp
%type <node> constant call immutable mutable factor argList args

%type <etype> typeSpec
%%

program  
    : declList { $$ = $1; 
    syntaxTree = $$;}
    ;

declList
    : declList decl { addSibling($1 , $2); $$ = $1;}
    | decl { $$ = $1;}
    ;

decl
    : varDecl { $$ = $1;}
    | funDecl { $$ = $1;}
    | error { $$ = NULL; }
    ;

varDecl
    : typeSpec varDeclList ';' { $$ = $2; UpdateSiblingType($2, genType($1)); yyerrok; }
    | error varDeclList { $$ = NULL; yyerrok; }
    | typeSpec error { $$ = NULL; yyerrok; }
    ;

scopedVarDecl
    : STATIC typeSpec varDeclList ';' { $$ = $3; UpdateSiblingType($3, genType($2), 1); 
    /*$$->isStatic = true; not needed with siblingType update*/ yyerrok; }
    | typeSpec varDeclList ';' { $$ = $2; UpdateSiblingType($2, genType($1)); yyerrok; }
    ;

varDeclList
    : varDeclList ',' varDeclInit { $$ = $1; addSibling($$, $3); yyerrok; }
    | varDeclInit { $$ = $1; yyerrok; }
    | varDeclList ',' 
    | error { $$ = NULL; }
    ;

varDeclInit
    : varDeclId { bool array = $1->isArray;
        int temp = $1->nvalue;
        //printf("in nval: %d, array:%d\n",temp, array);
        $$ = new TreeNode(VarK, genType(-1), $1, NULL, NULL, NULL); 
        if(array) {
            $$->setArray(true);
            $$->setSize(temp+1);
        } else {
            $$->setSize(1);
        }
    }
    | varDeclId ':' simpleExp { bool array = $1->isArray;
        int temp = $1->nvalue;
        //printf("in nval: %d, array:%d\n",temp, array);
        $$ = new TreeNode(VarK, genType(-1), $1, $3, NULL, NULL);
        if(array) {
            $$->setArray(true);
            $$->setSize(temp+1);
        } else {
            $$->setSize(1);
        }
        $$->setInitialize(true);
    } 
    | error ':' simpleExp { $$ = NULL; yyerrok; }
    ;

varDeclId
    : ID { $$ = $1; $$->nvalue = 1;} 
    | ID LBRACK NUMCONST ']' { $$ = $1; 
    //printf("3 nval: %d, ", $3->nvalue);
    delete $2; 
    $$->nvalue = $3->nvalue; 
    $$->isArray = true;
    delete $3; 
    //printf("out nval: %d\n", $$->nvalue);
    /*TODO: extract the array status and number from here.*/} 
    | ID LBRACK error { $$ = NULL; }
    | error ']' {$$ = NULL; yyerrok; }
    ;

typeSpec
    : BOOL {$$ = 2;} 
    | CHAR { $$ = 3;} 
    | INT { $$ = 1;} 
    ;

funDecl
    : typeSpec ID '(' parms ')' compoundStmt { $$ = new TreeNode(FuncK, genType($1), $2, $4, $6, NULL);}
    | ID '(' parms ')' compoundStmt { $$ = new TreeNode(FuncK, Void, $1, $3, $5, NULL);}
    | typeSpec error { $$ = NULL; /*STATUS: this rule is listed as being useless by bison*/}
    | typeSpec ID '(' error { $$ = NULL; }
    | ID '(' error { $$ = NULL; }
    | ID '(' parms ')' error { $$ = NULL; }
    ;

parms
    : parmList { $$ = $1; }
    | { $$ = NULL; }
    ;

parmList
    : parmList ';' parmTypeList { $$ = $1; addSibling($$, $3);}
    | parmTypeList { $$ = $1; }
    | parmList ';' error { $$ = NULL; }
    | error { $$ = NULL; }
    ;

parmTypeList
    : typeSpec parmIdList { $$ = $2; UpdateSiblingType($2, genType($1));}
    | typeSpec error { $$ = NULL; }
    ;

parmIdList
    : parmIdList ',' parmId { $$ = $1; addSibling($$, $3); yyerrok; }
    | parmId { $$ = $1; yyerrok; }
    | parmIdList ',' error { $$ = NULL; }
    | error { $$ = NULL; }
    ;

parmId
    : ID { $$ = new TreeNode(ParamK, genType(-1), $1, NULL, NULL, NULL); } 
    | ID LBRACK ']' {$$ = new TreeNode(ParamK, genType(-1), $1, NULL, NULL, NULL);
        $$->setArray(true);/*TODO:ADD ARRAY OVERLOAD*/} 
    ;

stmt  
    : matchStmt { $$ = $1; /*confirm that everything works. not confident that grammar is correct post-error inclusion*/}
    | unmatchStmt { $$ = $1; }
    ;

matchStmt
    : selectStmt_M { $$ = $1; }
    | iterStmt_M { $$ = $1; }
    | otherStmt { $$ = $1; }
    ;

unmatchStmt
    : selectStmt_U { $$ = $1; }
    | iterStmt_U { $$ = $1; }
    ;

selectStmt_M
    : IF simpleExp THEN matchStmt ELSE matchStmt {$$ = new TreeNode(IfK, $1, $2, $4, $6);}
    | IF error { $$ = NULL; }
    | IF error ELSE matchStmt { $$ = NULL; yyerrok; }
    | IF error THEN matchStmt ELSE matchStmt { $$ = NULL; yyerrok; }
    ;

iterStmt_M
    : WHILE simpleExp DO matchStmt { $$ = new TreeNode(WhileK, $1, $2, $4, NULL);} 
    | FOR ID ASSIGN iterRange DO matchStmt { $$ = new TreeNode(ForK, $1, new TreeNode(VarK, Integer, $2, NULL, NULL, NULL), $4, $6);}
    | WHILE error DO matchStmt { $$ = NULL; yyerrok; }
    | WHILE error { $$ = NULL; }
    | FOR ID ASSIGN error DO matchStmt { $$ = NULL; yyerrok; }
    | FOR error { $$ = NULL; }
    ;

selectStmt_U
    : IF simpleExp THEN stmt { $$ = new TreeNode(IfK, $1, $2, $4, NULL);} 
    | IF simpleExp THEN matchStmt ELSE unmatchStmt { $$ = new TreeNode(IfK, $1, $2, $4, $6);} 
    | IF error THEN stmt { $$ = NULL; yyerrok; }
    | IF error THEN matchStmt ELSE unmatchStmt { $$ = NULL; yyerrok; }
    ;

iterStmt_U
    : WHILE simpleExp DO unmatchStmt {$$ = new TreeNode(WhileK, $1, $2, $4, NULL);} 
    | FOR ID ASSIGN iterRange DO unmatchStmt {$$ = new TreeNode(ForK, $1, new TreeNode(VarK, Integer, $2, NULL, NULL, NULL), $4, $6);}
    ;
    
otherStmt
    : expStmt { $$ = $1; }
    | compoundStmt { $$ = $1; }
    | returnStmt { $$ = $1; }
    | breakStmt { $$ = $1; }
    ;

expStmt  
    : exp ';' { $$ = $1;} 
    | ';' { $$ = NULL; /*$$ = newExpNode(OpK, NULL, NULL, NULL, NULL); Not how it's handled in exemplars*/}
    | error ';' { $$ = NULL; yyerrok; }
    ;
compoundStmt  
    : LBRACE localDeclsExist stmtListExist '}' { $$ = new TreeNode(CompoundK, $1, $2, $3, NULL); yyerrok; } 
    ;

localDeclsExist
    : localDecls {$$ = $1;}
    | {$$ = NULL;}
    ;

localDecls 
    : localDecls scopedVarDecl {$$ = $1; addSibling($1 , $2);}
    | scopedVarDecl {$$ = $1;}
    ;

stmtListExist
    : stmtList {$$ = $1;}
    | {$$ = NULL;}
    ;

stmtList 
    : stmtList stmt {$$ = $1; addSibling($1 , $2);}
    | stmt {$$ = $1;}
    ;

iterRange  
    : simpleExp TO simpleExp { $$ = new TreeNode(RangeK, $2, $1, $3, NULL);} 
    | simpleExp TO simpleExp BY simpleExp{ $$ = new TreeNode(RangeK, $2, $1, $3, $5);}
    | simpleExp TO error { $$ = NULL; }
    | error BY error { $$ = NULL; yyerrok; }
    | simpleExp TO simpleExp BY error { $$ = NULL; }
    ;

returnStmt 
    : RETURN ';' { $$ = new TreeNode(ReturnK, $1, NULL, NULL, NULL); } 
    | RETURN exp ';' { $$ = new TreeNode(ReturnK, $1, $2, NULL, NULL); yyerrok;}
    | RETURN error ';' { $$ = NULL; yyerrok; }
    ;

breakStmt 
    : BREAK ';' { $$ = new TreeNode(BreakK, $1, NULL, NULL, NULL);}
    ;

exp 
    : mutable assignop exp { $$ = new TreeNode(AssignK, $2, $1, $3, NULL);}
    | mutable INC { $$ = new TreeNode(AssignK, $2, $1, NULL, NULL);}
    | mutable DEC { $$ = new TreeNode(AssignK, $2, $1, NULL, NULL);}
    | simpleExp { $$ = $1; }
    | error assignop exp { $$ = NULL; yyerrok; }
    | mutable assignop error { $$ = NULL; }
    | error INC { $$ = NULL; yyerrok; }
    | error DEC { $$ = NULL; yyerrok; }
    ;

assignop 
    : ASSIGN { $$ = $1; }
    | PLUSEQ { $$ = $1; }
    | MINUSEQ { $$ = $1; }
    | MULEQ { $$ = $1; }
    | DIVEQ { $$ = $1; }
    ;

simpleExp 
    : simpleExp OR andExp {$$ = new TreeNode(OpK, $2, $1, $3, NULL);}
    | andExp { $$ = $1; }
    | simpleExp OR error { $$ = NULL;}
    ;

andExp 
    : andExp AND unaryRelExp {$$ = new TreeNode(OpK, $2, $1, $3, NULL);}
    | unaryRelExp { $$ = $1; }
    | andExp AND error { $$ = NULL; }
    ;

unaryRelExp 
    : NOT unaryRelExp { $$ = new TreeNode(OpK, $1, $2, NULL, NULL);}
    | relExp { $$ = $1; }
    | NOT error { $$ = NULL; }
    ;

relExp 
    : sumExp relop sumExp {$$ = new TreeNode(OpK, $2, $1, $3, NULL);}
    | sumExp { $$ = $1; }
    ;

relop 
    : LTHAN { $$ = $1; }
    | LEQ { $$ = $1; }
    | GTHAN { $$ = $1; }
    | GEQ { $$ = $1; }
    | EQ { $$ = $1; }
    | NEQ { $$ = $1; }
    ;

sumExp 
    : sumExp sumop mulExp { $$ = new TreeNode(OpK, $2, $1, $3, NULL);}
    | mulExp { $$ = $1; }
    | sumExp sumop error { $$ = NULL; }
    ;

sumop 
    : PLUS { $$ = $1; }
    | MINUS { $$ = $1; }
    ;

mulExp 
    : mulExp mulop unaryExp {$$ = new TreeNode(OpK, $2, $1, $3, NULL);}
    | unaryExp { $$ = $1; }
    | mulExp mulop error { $$ = NULL; }
    ;

mulop 
    : STAR { $$ = $1; }
    | DIV { $$ = $1; }
    | MODULO { $$ = $1; }
    ;

unaryExp 
    : unaryop unaryExp {$$ = new TreeNode(OpK, $1, $2, NULL, NULL);}
    | factor { $$ = $1; }
    | unaryop error { $$ = NULL; }
    ;

unaryop 
    : MINUS { $$ = $1; }
    | STAR { $$ = $1; }
    | QMARK { $$ = $1; }
    ;

factor 
    : mutable { $$ = $1; }
    | immutable { $$ = $1; }
    ;

mutable 
    : ID { $$ = new TreeNode(IdK, $1, NULL, NULL, NULL);}
    | ID LBRACK exp ']' { $$ = new TreeNode(OpK, $2, new TreeNode(IdK, $1, NULL, NULL, NULL), $3, NULL);
        $$->setArray(true);
        $$->setSize($3->attr.value+1);
        //printf("Line: %d, isArray:%d\n", $$->lineno, $$->getArray());
        }
    ;

immutable 
    : '(' exp ')' { $$ = $2; yyerrok; }
    | call  { $$ = $1; }
    | constant { $$ = $1; }
    | '(' error { $$ = NULL; }
    ;

call  
    : ID '(' args ')' { $$ = new TreeNode(CallK, $1, $3, NULL, NULL);}
    | error '(' { $$ = NULL; yyerrok; }
    ;

args  
    : argList { $$ = $1; }
    | { $$ = NULL; }
    ;

argList  
    : argList ',' exp  {$$ =$1; addSibling($1, $3); yyerrok; }
    | exp { $$ = $1;}
    | argList ',' error { $$ = NULL; }
    ;

constant 
    : NUMCONST {$$ = new TreeNode(ConstantK, $1, NULL, NULL, NULL); $$->expType=Integer;}
    | CHARCONST { $$ = new TreeNode(ConstantK, $1, NULL, NULL, NULL); $$->expType=Char;}
    | STRINGCONST {int temp = $1->getText().length()-1;//-2 for the quotes, +1 for the index storing the length
        $$ = new TreeNode(ConstantK, $1, NULL, NULL, NULL); 
        $$->expType=Char; $$->setArray(true); $$->setSize(temp);
        $$->text = $$->text.substr(1, $$->text.size()-2);}
    | BOOLCONST {$$ = new TreeNode(ConstantK, $1, NULL, NULL, NULL); $$->expType=Boolean;}
    ;


%%
int main(int argc, char *argv[])
{
    int fileInd = 0;
    if(argc > 1) {
        for(int i = 1; i < argc; i++) {
            if(argv[i][0]!= '-') {
                if(yyin = fopen(argv[i], "r")) {
                    //file opened
                    fileInd = i;
                    i = argc;
                } else {
                    fprintf(stderr, "ERROR: Failed to open %s\n", argv[1]);
                    return 0;
                }
            }
        }
    } else {
        yyin = stdin;
    }
    tokenStore.clear();
    initErrorProcessing();
    warnings = 0;
    errors = 0;
    foffset = 0;
    goffset = 0;
    //create the .tm file
    {
        int fileLen = strlen(argv[fileInd])-3;
        char newFileName[fileLen+4];
        char* newEnd = ".tm";
        strncpy(newFileName, argv[fileInd], fileLen);
        strcpy( newFileName + fileLen, newEnd);
        if(code = fopen(newFileName, "w")) {
            //success
        } else {
            printf("Error creating the file %s\n", newFileName);
            return 0;
        }
    }
    
    //get the command line arguments and trigger the appropriate flags
    opterr = 0;
    bool pflag = 0;
    bool dflag = 0;
    bool hflag = 0;
    bool pflag2 = 0;
    bool dflag2 = 0;
    bool mflag = 0;
    int flag;
    while((flag = getopt(argc, argv, "pdhPDM"))!=-1) {
        switch (flag){
            case 'p':
                pflag = true;
                break;
            case 'd':
                dflag = true;
                break;
            case 'h':
                hflag = true;
                break;
            case 'P':
                pflag2 = true;
                break;
            case 'D':
                dflag2 = true;
                break;
            case 'M':
                mflag = true;
            case '?':
                break;
            default:
                break;
        }
    }
    if(hflag) {//if the help flag is enabled, then print out the help command and exit
        printf("usage: -c [options] [sourcefile]\noptions:\n-d \t- turn on parser debugging\n");
        printf("-D \t- turn on symbol table debugging\n-h \t- print this usage message\n");
        printf("-p \t- print the abstract syntax tree\n-P \t- print the abstract syntax tree plus type information\n");
        return 1;
    }
    //if the flag -d is enabled, set yydebug to 1
    #ifdef YYDEBUG//if this isn't here, yydebug isn't defined during compilation before it's checked
    if(!dflag) {
        yydebug = 0;
    } else {
        yydebug = 1;
    }
    #endif
    yyparse();

    //if the flag -p is enabled, print the tree.
    if(pflag) {
        printTree(syntaxTree, stdout, 0, MAXCHILDREN, false, false);
    }

    if(errors == 0) {//only do semantic analysis if there are no errors in the AST construction
        //after the parse tree is constructed, update the parents of each node (doing it in the constructor was tricky)
        updateParenting(syntaxTree, 0/*dflag + dflag2*/);//the second value is the debug value

        SymbolTable *symTable = new SymbolTable();//create the symbol table
        if(dflag2)//check if the -D flag is enabled. if it is, enable debugging for the symbol table
            symTable->debug(true);
        
        
        //do the analysis
        semanticAnalysis(syntaxTree, symTable);
        
        //if the flag -P or -M is enabled, print the tree with types.
        if(mflag && (errors < 1 || dflag2)) {//with memory locations added
            printTree(syntaxTree, stdout, 0, MAXCHILDREN, true, true);
            //print the global offset
            printf("Offset for end of global space: %d\n", goffset);
        } else if(pflag2 && (errors < 1 || dflag2)) {
            printTree(syntaxTree, stdout, 0, MAXCHILDREN, true, false);
        }

        if(errors == 0) {
            //Generate the Code:
            //Do the header here
            emitComment("** C- compiler version 0.1");
            emitComment("By Kaleb Johnson");
            emitComment("File compiled: %s", argv[fileInd]);
            generateCode(syntaxTree, goffset);
        }
        delete symTable;//delete the symTable from memory
    }
    //print the number of errors and warnings
    printf("Number of warnings: %d\n", warnings);
    printf("Number of errors: %d\n", errors);
    //free/delete the allocated things
    for(auto token : tokenStore) {
        free(token);
    }
    freeTree(syntaxTree);//delete the tree nodes from memory.
    return 1;
}

void updateParenting(TreeNode *root, int debug) {
    if(debug > 0)
        printf("Updating %s at line %d children\n", root->text.c_str(), root->lineno);
    updateChildrensParent(root, debug);
    for(int i = 0; i< MAXCHILDREN; i++) {
        if(root->child[i] != NULL) {
            updateParenting(root->child[i], debug);
        }
    }
    if(root->sibling != NULL) {
        updateParenting(root->sibling, debug);
    }
}

void printTree(TreeNode *root, FILE* out, int indent, int cnum, bool printTypes, bool printLoc) {
    //indent the tree to the correct degree
    int i;
    for(i = 0; i< indent; i++) {
        fprintf(out, ".   ");
    }

    //check for child, sibling, or root status, then print the prefix
    if(cnum < 0) {//sibling
        fprintf(out, "Sibling: %d  ", abs(cnum));
    } else if(cnum < MAXCHILDREN) {//child
        fprintf(out, "Child: %d  ", cnum);
    }//root doesn't print any prefix

    printNode(root, out, printTypes, printLoc);

    fprintf(out, " [line: %d]\n",  root->lineno);
    int j;
    for(j = 0; j < MAXCHILDREN; j++) {
        if(root->child[j] != NULL) {
            printTree(root->child[j], out, indent+1, j, printTypes, printLoc);
        }
    }

    if(!(root->sibling == NULL)) {
        TreeNode* temp = root->sibling;
        //root->sibling = NULL;
        int k;
        if(cnum < 0) {
            k = abs(cnum) + 1;
        } else {
            k = 1;
        }
        printTree(temp, out, indent, -k, printTypes, printLoc);
    }
    
    return;
}

void printNode(TreeNode *pnode, FILE* out, bool printTypes, bool printLoc) {
    //useful for converting from the ExpType enum to a string
    const char nTypes[7][15] = {"void", "int", "bool", "char", "charInt", "equal", "UndefinedType"};
    if(pnode->nkind == DeclK) {//print a declaration
        if(pnode->subkind.decl == VarK) {//variable declaration 
            fprintf(out, "Var: %s", pnode->text.c_str());
            if(printTypes) {
                if(pnode->getStatic()) {
                    if(pnode->getArray()) {
                        fprintf(out, " is static array of");
                    } else {
                        fprintf(out, " of static");
                    }
                    fprintf(out, " type %s", nTypes[pnode->expType]);
                    
                } else {
                    if(pnode->getArray())
                        fprintf(out, " is array");
                    fprintf(out, " of type %s", nTypes[pnode->expType]);
                }
            }
                

        } else if(pnode->subkind.decl == FuncK) {//function
            fprintf(out, "Func: %s", pnode->text.c_str());
            if(printTypes)
                fprintf(out, " returns type %s", nTypes[pnode->expType]);

        } else {//parameter declaration

            fprintf(out, "Parm: %s", pnode->text.c_str());
            if(printTypes&&pnode->getArray())
                fprintf(out, " is array");
            if(printTypes)
                fprintf(out, " of type %s", nTypes[pnode->expType]);

        }

    } else if (pnode->nkind == StmtK) {//print a statement
        if(pnode->subkind.stmt == NullK) {//null statement
            fprintf(out, "Null");//have not found an example of this one
        } else if(pnode->subkind.stmt == IfK) {//if statement
            fprintf(out, "If");
        } else if(pnode->subkind.stmt == WhileK) {//while statement
            fprintf(out, "While");
        } else if(pnode->subkind.stmt == ForK) {//for statement
            fprintf(out, "For");
        } else if(pnode->subkind.stmt == CompoundK) {//compound statement
            fprintf(out, "Compound");
        } else if(pnode->subkind.stmt == ReturnK) { //return statement
            fprintf(out, "Return");
        } else if(pnode->subkind.stmt == BreakK) {//break statement
            fprintf(out, "Break");
        } else if(pnode->subkind.stmt == RangeK) {//range statement
            fprintf(out, "Range");
        }

    } else {//print an expression
        if(pnode->subkind.exp == OpK) {//operator
            if(pnode->attr.opkind == 15) {//distinct identifier for the star operator
                if(pnode->child[1] == NULL) {
                    fprintf(out,"Op: sizeof");
                } else { 
                    fprintf(out,"Op: %s", pnode->text.c_str());
                }
            } else if (pnode->attr.opkind == 14) {//'-' operator
                if(pnode->child[1] == NULL) {
                    fprintf(out,"Op: chsign");
                } else { 
                    fprintf(out,"Op: %s", pnode->text.c_str());
                }
            } else {
                fprintf(out,"Op: %s", pnode->text.c_str());//currently just print the text. Maybe switch to an operator dictionary (like nTypes)
            }
        } else if(pnode->subkind.exp == ConstantK) {//constant
            fprintf(out, "Const ");
            if(pnode->getArray())
                fprintf(out, "is array ");
            if(pnode->tokkind == 1) {//numeric
                fprintf(out, "%d", pnode->attr.value);
            } else if (pnode->tokkind == 5){//string
                fprintf(out, "%s", pnode->text.c_str());//currently just print the text
            } else if (pnode->tokkind == 4){//char
                fprintf(out, "'%c'", pnode->text.c_str()[1]);//TODO:fix this. c_value isn't printing
            } else if (pnode->tokkind == 2){//boolean
                if(pnode->attr.value) {
                    fprintf(out, "true" );
                } else {//display the int as a bool
                    fprintf(out, "false" );
                }
            }

        } else if(pnode->subkind.exp == IdK) {//identifier
            fprintf(out,"Id: %s", pnode->text.c_str());//currently just print the text
        } else if(pnode->subkind.exp == AssignK) {//assign
            fprintf(out,"Assign: %s", pnode->text.c_str());//currently just print the text
        } else if(pnode->subkind.exp == InitK) {//initialize
            fprintf(out,"Init: %s", pnode->text.c_str());//currently just print the text
        } else if(pnode->subkind.exp == CallK) {//call
            fprintf(out,"Call: %s", pnode->text.c_str());//currently just print the text
        }
        if(printTypes) {
            if(pnode->getStatic()) {
                if(pnode->getArray()) {
                    fprintf(out, " is static array of");
                } else {
                    fprintf(out, " of static");
                }
                fprintf(out, " type %s", nTypes[pnode->expType]);
            } else {
                if(pnode->getArray())
                    fprintf(out, " is array");
                fprintf(out, " of type %s", nTypes[pnode->expType]);
            }
        }
    }
    //print memory type, size, location
    /*exclude list:
    opk
    assignk
    constantk except for arrays
    callK
    returnk
    range
    */
    if(printLoc && !(pnode->nkind == ExpK && pnode->subkind.exp == OpK) && !(pnode->nkind == ExpK && pnode->subkind.exp == AssignK) &&
        !(pnode->nkind == ExpK && pnode->subkind.exp == CallK) &&
        !(pnode->nkind == StmtK && pnode->subkind.stmt == ReturnK) &&
        !(pnode->nkind == StmtK && pnode->subkind.stmt == RangeK) &&
        ( /*only array constant(strings)*/!(pnode->nkind == ExpK && pnode->subkind.exp == ConstantK) || pnode->getArray())
      ) {
        char *memtype;
        switch(pnode->getRType()){
            case GlobalR:
                memtype = "Global";
                break;
            case LocalR:
                memtype = "Local";
                break;
            case ParamR:
                memtype = "Parameter";
                break;
            case StaticR:
                memtype = "LocalStatic";
                break;
            default:
                memtype = "None";
                break;
        }
        printf(" [mem: %s loc: %d size: %d]", memtype, pnode->getMem(), pnode->getSize());
    }

    return;
}

ExpType genType(int typenum) {
    
    switch(typenum) {
        case 0:
            return Void;
        case 1:
            return Integer;
        case 2:
            return Boolean;
        case 3:
            return Char;
        case 4:
            return CharInt;
        case 5:
            return Equal;
        default:
            return UndefinedType;
    }
    
}

void UpdateSiblingType(TreeNode *root, ExpType newtype, int updated) {
    if(root!= NULL) {
        switch(updated){
            case 1://static
                root->setStatic(true);
                root->expType = newtype;
                root->setRType(StaticR);
                if(root->sibling != NULL) 
                    UpdateSiblingType(root->sibling, newtype, 1);
                break;
            default:
                root->expType = newtype;
                if(root->sibling != NULL) 
                    UpdateSiblingType(root->sibling, newtype);
                break;
        }
    } 
}