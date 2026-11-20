all: libplenja

libplenja:
	gcc -fPIC fs/libplenja-fs.c -o fs/libplenja-fs.o -c
	gcc -fPIC str/libplenja-str.c -o str/libplenja-str.o -c
	gcc fs/libplenja-fs.o str/libplenja-str.o -shared -o libplenja.so
	echo '' | cat fs/libplenja-fs.h - str/libplenja-str.h > libplenja.h

debug:
	gcc -fPIC fs/libplenja-fs.c -o fs/libplenja-fs.o -c -g
	gcc -fPIC str/libplenja-str.c -o str/libplenja-str.o -c -g
	gcc fs/libplenja-fs.o str/libplenja-str.o -shared -o libplenja.so -g
	echo '' | cat fs/libplenja-fs.h - str/libplenja-str.h > libplenja.h

install:
	./preinstall.sh
	cp libplenja.so /usr/lib/libplenja.so.0.0.0
	ln -s libplenja.so.0.0.0 /usr/lib/libplenja.so.0
	ln -s libplenja.so.0.0.0 /usr/lib/libplenja.so
	cp libplenja.h /usr/include/libplenja.h
