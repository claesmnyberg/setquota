CC     = gcc
CFLAGS = -Wall
OBJS   = setquota.o
PROG   = setquota

all: ${OBJS}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS}

clean:
	rm -f ${OBJS} ${PROG}

new: clean all
