/*
 * delay1.c
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
 * Depends on API functions:
 *    pthread_delay_np
 */

#include "test.h"

pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;

void *
func(void * arg)
{
  struct timespec interval = {5, 500000000L};

  assert(pthread_mutex_lock(&mx) == 0);

#ifdef _MSC_VER
#pragma inline_depth(0)
#endif
  pthread_cleanup_push(pthread_mutex_unlock, &mx);
  assert(pthread_delay_np(&interval) == 0);
  pthread_cleanup_pop(1);
#ifdef _MSC_VER
#pragma inline_depth()
#endif

  return (void *)(size_t)1;
}

int
main(int argc, char * argv[])
{
  pthread_t t;
  void* result = (void*)0;

  assert(pthread_mutex_lock(&mx) == 0);

  assert(pthread_create(&t, NULL, func, NULL) == 0);
  assert(pthread_cancel(t) == 0);

  assert(pthread_mutex_unlock(&mx) == 0);

  assert(pthread_join(t, &result) == 0);
  assert(result == (void*)PTHREAD_CANCELED);

  return 0;
}

