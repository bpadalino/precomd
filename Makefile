all:
	gcc -o novacom -I/opt/local/include -L/opt/local/lib ./src/novacom.c -lusb -Wall
	
clean:
	rm novacom
