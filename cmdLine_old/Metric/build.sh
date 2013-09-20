#gcc foo.c `pkg-config --cflags --libs glib-2.0` -o foo
gcc -fPIC -O2 `pkg-config --cflags --libs glib-2.0` -c hash_csv.c 
gcc -O2 -shared hash_csv.o -o libcsv.so `pkg-config --cflags --libs glib-2.0`

