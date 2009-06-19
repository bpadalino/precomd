all:
	echo "Please choose osx or linux as your target"
osx:
	gcc -o precomd -I/opt/local/include -L/opt/local/lib ./src/precomd.c -lusb -Wall

linux:
	gcc -o precomd ./src/precomd.c -lusb -Wall    

clean:
	rm precomd
