CFLAGS= -g -MD -pthread
all : manager worker
manager : manager.o read_int.o
	gcc -o manager manager.o read_int.o -g -lm
worker : worker.o worker_proc.o
	gcc -o worker worker.o worker_proc.o -g -lm -pthread
clean :
	rm -f read_int.o read_int.d manager.o manager.d worker.o  worker.d worker_proc.o worker_proc.d manager  worker
-include  *.d
