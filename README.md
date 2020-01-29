## About

The GZip format is a popular format for file compression. For example, it is widely used in Neuroimaging, for example in the NIfTI format (e.g. [FSL](https://fsl.fmrib.ox.ac.uk/fsl/fslwiki/) and [AFNI](https://afni.nimh.nih.gov)) and AFNI's BRIK format. While GZip is very fast to decompress, it is slow to create compressed files. One solution to this problem is [pigz](https://zlib.net/pigz/) which leverages the fact that modern computers have multiple cores to compress sections of your file in parallel. With pigz, a system with four cores can compress files about twice as quick. A second approach is to use [modern computer instructions](https://github.com/cloudflare/zlib) to accelerate compression. This approach leverages instructions built into modern computers since 2009. 

This project combines these two approaches, building pigz using the Cloudflare accelerated zlib. This repository also includes a [CMake build script](https://github.com/madler/pigz/issues/62) to ease cross platform support (using @ningfei's superbuild scripts).

Be aware that some pigz does not work on some [Linux systems](https://github.com/madler/pigz/issues/68) and will generate an `internal threads error`. In my experience, most Linux distributions work fine.

## Installing

The recommended method is to compile your own copy of pigz. This will ensure that pigz is built using the latest [glibc](https://github.com/madler/pigz/issues/68) version on your system:

```
git clone https://github.com/neurolabusc/pigz.git
cd pigz
mkdir build && cd build
cmake ..
make
```


You can get a precompiled version by going to the  [releases](https://github.com/neurolabusc/pigz/releases) tab. Different Linux versions are provided, e.g. for Ubuntu 16.04, 18.04 or 19.10. You should use the latest version supported by your system, as [glibc](https://github.com/madler/pigz/issues/68) has been improved to fix parallel threading issues.

## Installation and testing

Tools like afni and dcm2niix use whichever version of pigz they find in your system path. You can find the current location of pigz using `which pigz`. I would recommend backing up your prior version of pigz. If you use AFNI, remember to set up you [environment](https://afni.nimh.nih.gov/pub/dist/doc/program_help/README.environment.html) to use pigz.

Here is the performance on a 4-core (8-thread) MacOS laptop. It takes just under 4 seconds to filter this image without compression, just over seven seconds with the default pigz, and just over six with the optimized pigz - the compression time is accelerated 37%. 
```
>time 3dmerge -1blur_fwhm 6.0 -doall -prefix afni.nii rest.nii 
...
real	0m3.859s
>time 3dmerge -1blur_fwhm 6.0 -doall -prefix afni.brik.gz rest.nii 
....
real	0m7.264s
>rm afni.brik.gz+orig.*
>which pigz
/usr/local/bin/pigz
>sudo cp /usr/local/bin/pigz /usr/local/bin/pigz_old
>sudo cp ./pigz /usr/local/bin/pigz
>time 3dmerge -1blur_fwhm 6.0 -doall -prefix afni.brik.gz rest.nii 
....
real	0m6.343s
```


Here is the performance on a 12-core (24-thread) Linux computer. This shows a 40% acceleration.

```
>time 3dmerge -1blur_fwhm 6.0 -doall -prefix afni.nii rest.nii 
...
real	0m1.679s
>time 3dmerge -1blur_fwhm 6.0 -doall -prefix afni.brik.gz rest.nii 
...
real	0m3.094s
> rm afni.brik.gz+orig.*
>which pigz
/usr/local/bin/pigz
>sudo cp /usr/bin/pigz /usr/bin/pigz_old
>sudo cp ./pigz /usr/bin/pigz
>time 3dmerge -1blur_fwhm 6.0 -doall -prefix afni.brik.gz rest.nii 
...
real	0m2.691s
```



## Details

pigz 2.4 (26 Dec 2017) by Mark Adler

pigz, which stands for Parallel Implementation of GZip, is a fully functional replacement for gzip that exploits multiple processors and multiple cores to the hilt when compressing data.

pigz was written by Mark Adler and does not include third-party code. I am making my contributions to and distributions of this project solely in my personal capacity, and am not conveying any rights to any intellectual property of any third parties.

This version of pigz is written to be portable across Unix-style operating systems that provide the zlib and pthread libraries.

Type "make" in this directory to build the "pigz" executable.  You can then install the executable wherever you like in your path (e.g. /usr/local/bin/). Type "pigz" to see the command help and all of the command options.

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
