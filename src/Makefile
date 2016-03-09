all: make4061
	echo 'make4061'

make4061: main.o util.o 
	gcc -o make4061 main.o util.o

main.o: main.c
	gcc -c main.c

util.o: util.c
	gcc -c util.c

cal.o: cal.c parse.c
	gcc -c cal.c parse.c

parse.o: parse.c 
	gcc -o parse parse.c

clean:
	rm -rf main.o cal.o parse.o util.o make4061
