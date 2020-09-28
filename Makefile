PROJ := trdetz
CC := gcc
CCFLAGS := -Wall -ansi -pedantic
LINKER := gcc
LIBS := -lm
LDFLAGS :=
#LDFLAGS := -static
VERSION := 0.3b3
INSTALL_PREFIX := /usr/local

%.o : %.c
	${CC} -DVERSION=\"$(VERSION)\" ${CCFLAGS} -c -o $@ $<

.PHONY : clean

.PHONY: all

all: ${PROJ}

main.o: trd.h defines.h

trd.o: trd.h defines.h

clean:
	rm -f *.o
	rm -f ${PROJ}

install: ${PROJ}
	install ./${PROJ} $(INSTALL_PREFIX)/bin	
	
dist: clean
	rm -rf ./${PROJ}-${VERSION}
	ln -s ./ ./${PROJ}-${VERSION}
	tar --exclude ${PROJ}-${VERSION}/${PROJ}-${VERSION} --exclude ${PROJ}-${VERSION}/${PROJ}-${VERSION}.tar.gz -hcf - ./${PROJ}-${VERSION}/ | gzip -f9 > ${PROJ}-${VERSION}.tar.gz
	rm -rf ./${PROJ}-${VERSION}

${PROJ}: trd.o main.o
	${LINKER} -o $@ $^ ${LDFLAGS} ${LIBS}	
	strip $@


#EOF