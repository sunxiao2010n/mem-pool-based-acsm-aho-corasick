CFLAGS = -pipe -Wpointer-arith -Wunused-variable -O2 -fPIC -fpermissive

all : acsmex

acsmex : acsmex.c
	g++ -c acsmex.c ${CFLAGS} -std=c++0x
	gcc -o libacsmex.so -shared acsmex.o ${CFLAGS}

test : acsmex.c test_acsmex.c
	g++ -o test_acsmex test_acsmex.c acsmex.c ${CFLAGS} -lmm

clean: 
	rm -f acsmex.[oa]
	rm -f *.o
	rm -f core*
	rm -f *.mem *.sem
	rm -f *.so

install:
	cp libacsmex.so /usr/local/lib/libacsmex.so
	ldconfig;
