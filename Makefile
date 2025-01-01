.PHONY: default clean format test examples-and-tests compile_commands.json

MAIN = main

SRCS = $(shell find ./ -name "*.c")
HDRS = $(shell find ./ -name "*.h")
OBJS = $(SRCS:.c=.o)

CC       := gcc
CFLAGS   := -std=gnu23 -pedantic -g -Wall -Wextra
LFLAGS   := -lm $$(pkg-config --libs glfw3 opengl glu glew)
INCLUDES := -I.
LIBS     :=

default: $(MAIN)

$(MAIN): $(OBJS)
	@mkdir -p $$(dirname $(MAIN))
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

tests/all-tests.o: tests/all-tests.c $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -c tests/all-tests.c  -o tests/all-tests.o

format: $(SRCS) $(INCLS)
	clang-format -style=file -i main.c

clean:
	rm -rf $(MAIN)
	rm -rf $(OBJS)
	rm -rf **/*.o
	rm -rf bin

test: default
	./$(MAIN)

compile_commands.json:
	make --always-make --dry-run | grep -wE 'gcc|g\+\+|c\+\+' | grep -w '\-c' | sed 's|cd.*.\&\&||g' | jq -nR '[inputs|{directory:"'`pwd`'", command:., file: (match(" [^ ]+$$").string[1:-1] + "c")}]' > compile_commands.json

