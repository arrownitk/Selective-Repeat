
build:
	gcc  lib.o crc.c  send.c -o send
	gcc    crc.c lib.o recv.c -o recv


clean:
	-rm send.o recv.o send recv 
