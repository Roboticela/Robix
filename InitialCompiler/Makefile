# First compile of Robix.
# The C compiler in this directory is the backup. It translates the .rx
# compiler into C, then that binary compiles the same sources to machine code.

CC = clang
CFLAGS = -O2 -std=c11 -Wall -Wno-unused-function
ROOT = ..
SOURCES = \
	$(ROOT)/AstNode.rx $(ROOT)/Diagnostic.rx $(ROOT)/Emitter.rx $(ROOT)/Interpreter.rx \
	$(ROOT)/Lexer.rx $(ROOT)/NameChecker.rx $(ROOT)/NativeEmit.rx $(ROOT)/ParserExpressions.rx \
	$(ROOT)/Parser.rx $(ROOT)/ParserStatements.rx $(ROOT)/ProjectSystem.rx $(ROOT)/Robixc.rx \
	$(ROOT)/Robix.rx $(ROOT)/SyntaxCompiler.rx $(ROOT)/SyntaxCoverage.rx $(ROOT)/TextConvert.rx \
	$(ROOT)/TokenKind.rx $(ROOT)/Token.rx $(ROOT)/TypeChecker.rx

.PHONY: all bootstrap

all: rxc

rxc: rxc.c
	$(CC) $(CFLAGS) -o rxc rxc.c

# Recovery only. The compiler you run is Build/Compiler/Robix.app,
# which Robix builds from the .rx sources. These binaries stay here.
bootstrap: rxc
	mkdir -p bootstrap
	./rxc -o bootstrap/robix.app -e $(ROOT)/Robix.rx $(SOURCES)
	./rxc -o bootstrap/robixc.app -e $(ROOT)/Robixc.rx $(SOURCES)
