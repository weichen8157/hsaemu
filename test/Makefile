CC=gcc 
CFLAGS=-g -c -Wall -O2 -fPIC

all:
	$(CC) $(CFLAGS) -I. thread_pool.c
	ar -r libThread-pool.a thread_pool.o
	$(CC) $(CFLAGS) -I. comm.c
	$(CC) $(CFLAGS) -I. hsa.c
	$(CC) $(CFLAGS) -I. compute_unit.c
	$(CC) $(CFLAGS) -I. processing_element.c
	ar -r libHSA.a comm.o hsa.o compute_unit.o processing_element.o
	$(CC) $(CFLAGS) -I. test.c
	$(CC) test.o libHSA.a libThread-pool.a -lpthread
clean:
	rm *.o *.a a.out

