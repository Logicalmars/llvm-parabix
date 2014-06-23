LLFILE = test

test: main.c $(LLFILE).ll
	gcc -c main.c -o main.o
	llc-svn -filetype=obj $(LLFILE).ll -o test.o
	gcc main.o test.o -o $(LLFILE).out

clean:
	rm -f *.o *.s *.out