CC?=gcc
RM?=rm
CFLAGS+=-O2 -Wall

all: grpar.c
	${CC} ${CFLAGS} grpar.c -o grpar ${LIBS}

clean:
	${RM} -f grpar
