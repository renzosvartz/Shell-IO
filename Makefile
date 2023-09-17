#

CC = gcc
CFLAGS = -ansi -Wall -g -O0 -Wwrite-strings -Wshadow \
         -pedantic-errors -fstack-protector-all -Wextra

PROGS: d8sh 

#	public00 public01 public02 public03 public04 public05 \
#	public06 public07 public08 public09 public10 public11 \
#	public12 public13

.PHONY: all clean

all: $(PROGS)

clean:
	rm -f *.o $(PROGS)

#Executables
d8sh: d8sh.o executor.o lexer.o parser.tab.o
	$(CC) -lreadline -o d8sh d8sh.o executor.o lexer.o parser.tab.o

#public1%: public1%.o
#	$(CC) -o public1% public1%.o

#public0%: public0%.o
#	$(CC) -o public0% public0%.o


#Object Files
d8sh.o: d8sh.c executor.h lexer.h
	$(CC) $(CFLAGS) -c d8sh.c

executor.o: executor.c executor.h command.h
	$(CC) $(CFLAGS) -c executor.c

lexer.o: lexer.c lexer.h parser.tab.h
	$(CC) $(CFLAGS) -c lexer.c

parser.tab.o: parser.tab.c parser.tab.h executor.h
	$(CC) $(CFLAGS) -c parser.tab.c

#public1%.o: public1%.c
#	$(CC) $(CFLAGS) -c public1%.c

#public0%.o: public0%.c
#	$(CC) $(CFLAGS) -c public0%.c
