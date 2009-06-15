all:
    gcc -o precomd precomd.c -lusb -Wall
    
clean:
    rm precomd