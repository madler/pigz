/*
 * name_np2.c
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
 * Description:
 * Create a thread and give it a name.
 *
 * The MSVC version should display the thread name in the MSVS debugger.
 * Confirmed for MSVS10 Express:
 *
 *      VCExpress name_np1.exe /debugexe
 *
 * did indeed display the thread name in the trace output.
 *
 * Depends on API functions:
 *      pthread_create
 *      pthread_join
 *      pthread_self
 *      pthread_attr_init
 *      pthread_getname_np
 *      pthread_attr_setname_np
 *      pthread_barrier_init
 *      pthread_barrier_wait
 */

#include "test.h"

static int washere = 0;
static pthread_attr_t attr;
static pthread_barrier_t sync;
#if defined (__PTW32_COMPATIBILITY_BSD)
static int seqno = 0;
#endif

void * func(void * arg)
{
  char buf[32];
  pthread_t self = pthread_self();

  washere = 1;
  pthread_barrier_wait(&sync);
  assert(pthread_getname_np(self, buf, 32) == 0);
  printf("Thread name: %s\n", buf);
  pthread_barrier_wait(&sync);

  return 0;
}

int
main()
{
  pthread_t t;

  assert(pthread_attr_init(&attr) == 0);
#if defined (__PTW32_COMPATIBILITY_BSD)
  seqno++;
  assert(pthread_attr_setname_np(&attr, "MyThread%d", (void *)&seqno) == 0);
#elif defined (__PTW32_COMPATIBILITY_TRU64)
  assert(pthread_attr_setname_np(&attr, "MyThread1", NULL) == 0);
#else
  assert(pthread_attr_setname_np(&attr, "MyThread1") == 0);
#endif

  assert(pthread_barrier_init(&sync, NULL, 2) == 0);

  assert(pthread_create(&t, &attr, func, NULL) == 0);
  pthread_barrier_wait(&sync);
  pthread_barrier_wait(&sync);

  assert(pthread_join(t, NULL) == 0);

  assert(pthread_barrier_destroy(&sync) == 0);
  assert(pthread_attr_destroy(&attr) == 0);

  assert(washere == 1);

  return 0;
}
