/* 
 * robust1.c
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
 * --------------------------------------------------------------------------
 *
 * For all robust mutex types.
 * Thread A locks mutex
 * Thread A terminates with no threads waiting on robust mutex
 * Thread B acquires (inherits) mutex and unlocks
 * Main attempts to lock mutex with unrecovered state.
 *
 * Depends on API functions: 
 *      pthread_create()
 *      pthread_join()
 *	pthread_mutex_init()
 *	pthread_mutex_lock()
 *	pthread_mutex_unlock()
 *	pthread_mutex_destroy()
 *	pthread_mutexattr_init()
 *	pthread_mutexattr_setrobust()
 *	pthread_mutexattr_settype()
 *	pthread_mutexattr_destroy()
 */

#include "test.h"

static int lockCount;

static pthread_mutex_t mutex;

void * owner(void * arg)
{
  assert(pthread_mutex_lock(&mutex) == 0);
  lockCount++;

  return 0;
}
 
void * inheritor(void * arg)
{
  assert(pthread_mutex_lock(&mutex) == EOWNERDEAD);
  lockCount++;
  assert(pthread_mutex_unlock(&mutex) == 0);

  return 0;
}
 
int
main()
{
  pthread_t to, ti;
  pthread_mutexattr_t ma;

  assert(pthread_mutexattr_init(&ma) == 0);
  assert(pthread_mutexattr_setrobust(&ma, PTHREAD_MUTEX_ROBUST) == 0);

  /* Default (NORMAL) type */ 
  lockCount = 0;
  assert(pthread_mutex_init(&mutex, &ma) == 0);
  assert(pthread_create(&to, NULL, owner, NULL) == 0);
  assert(pthread_join(to, NULL) == 0);
  assert(pthread_create(&ti, NULL, inheritor, NULL) == 0);
  assert(pthread_join(ti, NULL) == 0);
  assert(lockCount == 2);
  assert(pthread_mutex_lock(&mutex) == ENOTRECOVERABLE);
  assert(pthread_mutex_unlock(&mutex) == EPERM);
  assert(pthread_mutex_destroy(&mutex) == 0);

  /* NORMAL type */ 
  lockCount = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_NORMAL) == 0);
  assert(pthread_mutex_init(&mutex, &ma) == 0);
  assert(pthread_create(&to, NULL, owner, NULL) == 0);
  assert(pthread_join(to, NULL) == 0);
  assert(pthread_create(&ti, NULL, inheritor, NULL) == 0);
  assert(pthread_join(ti, NULL) == 0);
  assert(lockCount == 2);
  assert(pthread_mutex_lock(&mutex) == ENOTRECOVERABLE);
  assert(pthread_mutex_unlock(&mutex) == EPERM);
  assert(pthread_mutex_destroy(&mutex) == 0);

  /* ERRORCHECK type */ 
  lockCount = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK) == 0);
  assert(pthread_mutex_init(&mutex, &ma) == 0);
  assert(pthread_create(&to, NULL, owner, NULL) == 0);
  assert(pthread_join(to, NULL) == 0);
  assert(pthread_create(&ti, NULL, inheritor, NULL) == 0);
  assert(pthread_join(ti, NULL) == 0);
  assert(lockCount == 2);
  assert(pthread_mutex_lock(&mutex) == ENOTRECOVERABLE);
  assert(pthread_mutex_unlock(&mutex) == EPERM);
  assert(pthread_mutex_destroy(&mutex) == 0);

  /* RECURSIVE type */ 
  lockCount = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE) == 0);
  assert(pthread_mutex_init(&mutex, &ma) == 0);
  assert(pthread_create(&to, NULL, owner, NULL) == 0);
  assert(pthread_join(to, NULL) == 0);
  assert(pthread_create(&ti, NULL, inheritor, NULL) == 0);
  assert(pthread_join(ti, NULL) == 0);
  assert(lockCount == 2);
  assert(pthread_mutex_lock(&mutex) == ENOTRECOVERABLE);
  assert(pthread_mutex_unlock(&mutex) == EPERM);
  assert(pthread_mutex_destroy(&mutex) == 0);

  assert(pthread_mutexattr_destroy(&ma) == 0);

  return 0;
}
