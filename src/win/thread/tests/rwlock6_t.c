/*
 * rwlock6_t.c
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
 * Check writer and reader locking with reader timeouts
 *
 * Depends on API functions: 
 *      pthread_rwlock_timedrdlock()
 *      pthread_rwlock_wrlock()
 *      pthread_rwlock_unlock()
 */

#include "test.h"
#include <sys/timeb.h>

static pthread_rwlock_t rwlock1 = PTHREAD_RWLOCK_INITIALIZER;

static int bankAccount = 0;

void * wrfunc(void * arg)
{
  assert(pthread_rwlock_wrlock(&rwlock1) == 0);
  Sleep(2000);
  bankAccount += 10;
  assert(pthread_rwlock_unlock(&rwlock1) == 0);

  return ((void *)(size_t)bankAccount);
}

void * rdfunc(void * arg)
{
  int ba = -1;
  struct timespec abstime;

  (void) pthread_win32_getabstime_np(&abstime, NULL);

  if ((int) (size_t)arg == 1)
    {
      abstime.tv_sec += 1;
      assert(pthread_rwlock_timedrdlock(&rwlock1, &abstime) == ETIMEDOUT);
      ba = 0;
    }
  else if ((int) (size_t)arg == 2)
    {
      abstime.tv_sec += 3;
      assert(pthread_rwlock_timedrdlock(&rwlock1, &abstime) == 0);
      ba = bankAccount;
      assert(pthread_rwlock_unlock(&rwlock1) == 0);
    }

  return ((void *)(size_t)ba);
}

int
main()
{
  pthread_t wrt1;
  pthread_t wrt2;
  pthread_t rdt1;
  pthread_t rdt2;
  void* wr1Result = (void*)0;
  void* wr2Result = (void*)0;
  void* rd1Result = (void*)0;
  void* rd2Result = (void*)0;

  bankAccount = 0;

  assert(pthread_create(&wrt1, NULL, wrfunc, NULL) == 0);
  Sleep(500);
  assert(pthread_create(&rdt1, NULL, rdfunc, (void *)(size_t)1) == 0);
  Sleep(500);
  assert(pthread_create(&wrt2, NULL, wrfunc, NULL) == 0);
  Sleep(500);
  assert(pthread_create(&rdt2, NULL, rdfunc, (void *)(size_t)2) == 0);

  assert(pthread_join(wrt1, &wr1Result) == 0);
  assert(pthread_join(rdt1, &rd1Result) == 0);
  assert(pthread_join(wrt2, &wr2Result) == 0);
  assert(pthread_join(rdt2, &rd2Result) == 0);

  assert((int)(size_t)wr1Result == 10);
  assert((int)(size_t)rd1Result == 0);
  assert((int)(size_t)wr2Result == 20);
  assert((int)(size_t)rd2Result == 20);

  return 0;
}
