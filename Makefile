CC = gcc
CFLAGS = -O -Wall -std=c99
EXEC = ftpclient

ALL: build 

build: clean $(EXEC).c
	$(CC) $(CFLAGS) -o $(EXEC) $(EXEC).c

test:
	./$(EXEC) ftp://ftp.fit.vutbr.cz
	./$(EXEC) ftp://anonymous:secret@ftp.fit.vutbr.cz:21/pub/systems/centos
	./$(EXEC) ftp.linux.cz/pub/local/

clean:
	rm -f $(EXEC)

