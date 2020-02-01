/*
 *
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads4w - POSIX Threads for Windows
 *      Copyright 1998 John E. Bossom
 *      Copyright 1999-2018, Pthreads4w contributors
 *
 *      Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *
 *      https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "../config.h"

enum {
  OLD_WIN32CS,
  OLD_WIN32MUTEX
};

extern int old_mutex_use;

struct old_mutex_t_ {
  HANDLE mutex;
  CRITICAL_SECTION cs;
};

typedef struct old_mutex_t_ * old_mutex_t;

struct old_mutexattr_t_ {
  int pshared;
};

typedef struct old_mutexattr_t_ * old_mutexattr_t;

extern BOOL (WINAPI *__ptw32_try_enter_critical_section)(LPCRITICAL_SECTION);
extern HINSTANCE __ptw32_h_kernel32;

#define  __PTW32_OBJECT_AUTO_INIT ((void *) -1)

void dummy_call(int * a);
void interlocked_inc_with_conditionals(int *a);
void interlocked_dec_with_conditionals(int *a);
int old_mutex_init(old_mutex_t *mutex, const old_mutexattr_t *attr);
int old_mutex_lock(old_mutex_t *mutex);
int old_mutex_unlock(old_mutex_t *mutex);
int old_mutex_trylock(old_mutex_t *mutex);
int old_mutex_destroy(old_mutex_t *mutex);
/****************************************************************************************/
