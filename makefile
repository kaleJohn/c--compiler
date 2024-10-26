cc = g++
ccopts = #-ly
lex = flex
lexopts =
lexgens = lex.yy.c
yacc = bison
prj = parser
yaccopts = -d -v --debug
yaccgens = $(prj).tab.c $(prj).tab.h
cppclass = LToken.cpp TreeNode.cpp semantic.cpp symbolTable.cpp yyerror.cpp emitcode.cpp codeGen.cpp
cppcomp = #LToken.o TreeNode.o

$(prj).exe: $(lexgens) $(yaccgens) $(cppclass)
	$(cc) $(lexgens) $(yaccgens) $(cppclass)  $(ccopts) -o $(prj) -Wno-write-strings 

cleanw:
	rm $(lexgens) $(yaccgens) $(prj).exe $(cppcomp) parser.output

cleanl:
	rm $(lexgens) $(yaccgens) $(prj) parser.output

tarout:
	tar -cvf hw7.tar $(cppclass) LToken.h TreeNode.h semantic.h symbolTable.h yyerror.h emitcode.h codeGen.h makefile $(prj).l $(prj).y

#copyout:
#	$(cppclass) LToken.h TreeNode.h semantic.h symbolTable.h yyerror.h emitcode.h codeGen.h makefile $(prj).l $(prj).y

$(yaccgens): $(prj).y
	$(yacc) $(yaccopts) $(prj).y

$(lexgens): $(prj).l $(yaccgens)
	$(lex) $(lexopts) $(prj).l