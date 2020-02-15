cd zlib
nmake -f win32\Makefile.msc
cd ..\thread
nmake clean VC-static
cd ..
rm *.obj


cl -c ../pigz.c /utf-8 /Idirent /Ithread /Izlib -DNOZOPFLI
cl -c ../yarn.c /utf-8 /Idirent /Ithread /Izlib
cl -c ../try.c /utf-8 /Idirent /Ithread /Izlib
cl /Fepigz pigz.obj yarn.obj try.obj  /utf-8 /Idirent /Ithread /Izlib  /link  shell32.lib zlib/zlib.lib thread/libpthreadVC3.lib /NODEFAULTLIB:MSVCRT
REM pigz -f -k Описание.pdf 测试.obj fx.pdf
REM  pigz -d -f -k Описание.pdf.gz 测试.obj.gz