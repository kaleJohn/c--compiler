# C- Compiler
A school project for  to create a compiler for the C- language to the [TM virtual machine](http://www2.cs.uidaho.edu/~mdwilder/cs445/refs/tm.c). Some code provided from that class is used, and attributions are retained in the corresponding files.
## Compiling the project
Requirements: A C++ compiler that can use features from at least C++ 11, flex (version 2.6.4), and bison(3.8.2). Older versions of flex and bison should work up to a point, although issues were enountered with bison 2.x.
To compile, enter "make" into your terminal, or alternatively enter the following commands:
bison -d -v --debug parser.y
flex  parser.l
g++ lex.yy.c parser.tab.c parser.tab.h LToken.cpp TreeNode.cpp semantic.cpp symbolTable.cpp yyerror.cpp emitcode.cpp codeGen.cpp   -o parser -Wno-write-strings