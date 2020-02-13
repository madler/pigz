cd zlib
nmake -f win32\Makefile.msc
cd ..\thread
nmake clean VC-static
cd ..

cl -c ../pigz.c /Idirent /Ithread /Izlib -DNOZOPFLI
cl -c ../yarn.c /Idirent /Ithread /Izlib
cl -c ../try.c /Idirent /Ithread /Izlib
cl -o pigz yarn.obj try.obj pigz.obj /Idirent /Ithread /Izlib  /link  shell32.lib zlib/zlib.lib thread/libpthreadVC3.lib /NODEFAULTLIB:MSVCRT

