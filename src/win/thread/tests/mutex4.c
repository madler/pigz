/* 
 * mutex4.c
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
 * Thread A locks mutex - thread B tries to unlock.
 *
 * Depends on API functions: 
 *	pthread_mutex_lock()
 *	pthread_mutex_trylock()
 *	pthread_mutex_unlock()
 */

#include "test.h"

static int wasHere = 0;

static pthread_mutex_t mutex1;
 
void * unlocker(void * arg)
{
  int expectedResult = (int)(size_t)arg;

  wasHere++;
  assert(pthread_mutex_unlock(&mutex1) == expectedResult);
  wasHere++;
  return NULL;
}
 
int
main()
{
  pthread_t t;
  pthread_mutexattr_t ma;

  assert(pthread_mutexattr_init(&ma) == 0);

  BEGIN_MUTEX_STALLED_ROBUST(ma)

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_DEFAULT) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *)(size_t)(IS_ROBUST?EPERM:0)) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == 0);
  assert(wasHere == 2);

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_NORMAL) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *)(size_t)(IS_ROBUST?EPERM:0)) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == 0);
  assert(wasHere == 2);

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *)(size_t) EPERM) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == 0);
  assert(wasHere == 2);

  wasHere = 0;
  assert(pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE) == 0);
  assert(pthread_mutex_init(&mutex1, &ma) == 0);
  assert(pthread_mutex_lock(&mutex1) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *)(size_t) EPERM) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_mutex_unlock(&mutex1) == 0);
  assert(wasHere == 2);

  END_MUTEX_STALLED_ROBUST(ma)

  return 0;
}
