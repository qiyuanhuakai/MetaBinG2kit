.PHONY: all clean

all: runMetaBinG2 MetaBinG2 addref

runMetaBinG2: runMetaBinG2.c io.h pthread.h
	gcc -o runMetaBinG2 runMetaBinG2.c

MetaBinG2: MetaBinG2.cu
	nvcc -o MetaBinG2 MetaBinG2.cu -lcudart -lcublas

addref: addref.cpp
	g++ -std=c++20 -o addref addref.cpp

clean:
	rm -f runMetaBinG2 MetaBinG2 addref