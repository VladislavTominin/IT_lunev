CFLAGS= -g -MD
ttt : proc_last.o read_int.o
	gcc -o ttt proc_last.o read_int.o -g -lm -pthread
clean :
	rm -f proc_last.o read_int.o ttt proc_last.d read_int.d
-include  *.d
