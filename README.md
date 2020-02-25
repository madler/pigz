## About


[pigz](https://zlib.net/pigz/), which stands for parallel implementation of gzip, is a fully functional replacement for gzip that exploits multiple processors and multiple cores to the hilt when compressing data. pigz was written by [Mark Adler](https://en.wikipedia.org/wiki/Mark_Adler), and uses the [zlib](https://zlib.net) and [pthread](https://en.wikipedia.org/wiki/POSIX_Threads) libraries. To compile and use pigz, please read the README file in the source code distribution. You can read the pigz manual page [here](https://zlib.net/pigz/pigz.pdf).

Most users can install pigz with a simple command (Debian: `sudo yum install pigz`, RedHat:  `apt-get install pigz`, MacOS: `brew install pigz`). 

This repository provides the source code, allowing users to build their own executable. One reason to build your own copy is to improve performance (see CMake notes below). An additional reason is to ensure pigz is build with a current version of glibc. Specifically, pigz can generate an `internal threads error` on [Linux servers with old versions of glibc](https://github.com/madler/pigz/issues/68). If you see these errors you should upgrade your glibc library and recompile pigz.

## Compiling with Make

This is the simplest way to compile pigz. Type "make" in the source directory ("pigz/src") to build the "pigz" executable.  You can then install the executable wherever you like in your path (e.g. /usr/local/bin/). Type "pigz" to see the command help and all of the command options.

## Compiling with CMake

Compiling with CMake is more complicated than using Make. However, it does allow you to use different variants of the zlib compression library that can [improve](https://github.com/neurolabusc/pigz-bench) performance. Compiling with CMake requires the computer to have CMake and git installed. By default, CMake will use the [CloudFlare zlib](https://github.com/cloudflare/zlib):

```
git clone https://github.com/neurolabusc/pigz.git
cd pigz
mkdir build && cd build
cmake ..
make
```

Alternatively, you can build for [zlib-ng](https://github.com/zlib-ng/zlib-ng):

```
git clone https://github.com/neurolabusc/pigz.git
cd pigz
mkdir build && cd build
cmake -DZLIB_IMPLEMENTATION=ng ..
make
```

Finally, you can build for your system zlib, which will likely provide the poorest performance (but is the most popular so least likely to have any issues):

```
git clone https://github.com/neurolabusc/pigz.git
cd pigz
mkdir build && cd build
cmake -DZLIB_IMPLEMENTATION=System ..
make
```

Note that the process is a little different if you are using the Windows operating system. Windows users can compile using the [Microsoft C Compiler](https://visualstudio.microsoft.com/downloads/) or [MinGW](  http://mingw-w64.org/doku.php). Be aware there are several variations of the MinGW compiler, and the CMake script expects a version that supports the [-municode linker flag]( https://sourceforge.net/p/mingw-w64/wiki2/Unicode%20apps/). This flag is required to handle non-Latin letters in filenames. Here is an example of compiling on Windows targeting the Cloudflare zlib:

```
git clone https://github.com/neurolabusc/pigz.git
cd pigz
mkdir build
cd build
cmake  -DZLIB_IMPLEMENTATION=Cloudflare ..
cmake --build . --config Release
```


## Details

pigz 2.4 (26 Dec 2017) by Mark Adler

pigz, which stands for Parallel Implementation of GZip, is a fully functional replacement for gzip that exploits multiple processors and multiple cores to the hilt when compressing data.

pigz was written by Mark Adler and does not include third-party code. I am making my contributions to and distributions of this project solely in my personal capacity, and am not conveying any rights to any intellectual property of any third parties.

This version of pigz is written to be portable across Unix-style operating systems that provide the zlib and pthread libraries.

Type "make" in the source directory ("pigz/src") to build the "pigz" executable.  You can then install the executable wherever you like in your path (e.g. /usr/local/bin/). Type "pigz" to see the command help and all of the command options.

The latest version of pigz can be found at http://zlib.net/pigz/ .  You need zlib version 1.2.3 or later to compile pigz.  zlib version 1.2.6 or later is recommended, which reduces the overhead between blocks.  You can find the latest version of zlib at http://zlib.net/.  You can look in pigz.c for the change history.

Questions, comments, bug reports, fixes, etc. can be emailed to Mark at his address in the license below.

The license from pigz.c is copied here:

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
