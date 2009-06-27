all:
	echo "Please choose osx or linux as your target"
osx:
	gcc -o novacom -I/opt/local/include -L/opt/local/lib ./src/novacom.c -lusb -Wall

linux:
	gcc -o novacom ./src/novacom.c -lusb -Wall    

clean:
	rm novacom

