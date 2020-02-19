REM zlib
git clone https://github.com/rordenlab/zlib.git
cd zlib
nmake -f win32\Makefile.msc
cd ..
REM pthreads
git clone https://github.com/indigo-astronomy/pthreads4w
cd pthreads4w
nmake clean VC-static
cd ..
rm *.obj


cl -c ../pigz.c /utf-8 /Idirent /Ipthreads4w /Izlib -DNOZOPFLI
cl -c ../yarn.c /utf-8 /Idirent /Ipthreads4w /Izlib
cl -c ../try.c /utf-8 /Idirent /Ipthreads4w /Izlib
cl /Fepigz pigz.obj yarn.obj try.obj  /utf-8 /Idirent /Ipthreads4w /Izlib  /link  shell32.lib zlib/zlib.lib pthreads4w/libpthreadVC3.lib /NODEFAULTLIB:MSVCRT
rm *.obj
REM pigz -f -k Описание.pdf 测试.obj fx.pdf
