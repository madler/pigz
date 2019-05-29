/* tailor.h -- target dependent definitions
 * Copyright (C) 2007-2017 Mark Adler
 * Version 2.4.1x  xx Dec 2017  Mark Adler
 */

 /*
  This software is provided 'as-is', without any express or implied
  warranty. In no event will the author be held liable for any damages
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

 */

#ifndef S_IFLNK
#define S_IFLNK 0
#endif

#ifdef __MINGW32__
#define chown(p,o,g) 0
#define utimes(p,t)  0
#define lstat(p,s)   stat(p,s)
#define _exit(s)     exit(s)
#endif