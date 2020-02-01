/*
 * barrier5.c
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
 * Set up a series of barriers at different heights and test various numbers
 * of threads accessing, especially cases where there are more threads than the
 * barrier height (count), i.e. test contention when the barrier is released.
 */

#include "test.h"

enum {
  NUMTHREADS = 15,
  HEIGHT = 10,
  BARRIERMULTIPLE = 1000
};
 
pthread_barrier_t barrier = NULL;
pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
LONG totalThreadCrossings;

void *
func(void * crossings)
{
  int result;
  int serialThreads = 0;

  while ((LONG)(size_t)crossings >= (LONG)InterlockedIncrement((LPLONG)&totalThreadCrossings))
    {
      result = pthread_barrier_wait(&barrier);

      if (result == PTHREAD_BARRIER_SERIAL_THREAD)
        {
          serialThreads++;
        }
      else if (result != 0)
        {
          printf("Barrier failed: result = %s\n", error_string[result]);
          fflush(stdout);
          return NULL;
        }
    }

  return (void*)(size_t)serialThreads;
}

int
main()
{
  int i, j;
  void* result;
  int serialThreadsTotal;
  LONG Crossings;
  pthread_t t[NUMTHREADS + 1];

  for (j = 1; j <= NUMTHREADS; j++)
    {
      int height = j<HEIGHT?j:HEIGHT;

      totalThreadCrossings = 0;
      Crossings = height * BARRIERMULTIPLE;

      printf("Threads=%d, Barrier height=%d\n", j, height);

      assert(pthread_barrier_init(&barrier, NULL, height) == 0);

      for (i = 1; i <= j; i++)
        {
          assert(pthread_create(&t[i], NULL, func, (void *)(size_t)Crossings) == 0);
        }

      serialThreadsTotal = 0;
      for (i = 1; i <= j; i++)
        {
          assert(pthread_join(t[i], &result) == 0);
          serialThreadsTotal += (int)(size_t)result;
        }

      assert(serialThreadsTotal == BARRIERMULTIPLE);

      assert(pthread_barrier_destroy(&barrier) == 0);
    }

  assert(pthread_mutex_destroy(&mx) == 0);

  return 0;
}
