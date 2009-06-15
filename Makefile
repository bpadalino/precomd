all:
	gcc -o precomd ./src/precomd.c -lusb -Wall
    
clean:
	rm precomd