cl -c ../pigz.c /Idirent /Ithread /Izlib -DNOZOPFLI
cl -c ../yarn.c /Idirent /Ithread /Izlib
cl -c ../try.c /Idirent /Ithread /Izlib
cl -o pigz yarn.obj try.obj pigz.obj /Idirent /Ithread /Izlib  /link lib/zlib.lib lib/libpthreadVC3.lib /NODEFAULTLIB:MSVCRT

