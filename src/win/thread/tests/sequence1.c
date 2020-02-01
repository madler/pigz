/*
 * File: sequence1.c
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
 * Test Synopsis:
 * - that unique thread sequence numbers are generated.
 *
 * Test Method (Validation or Falsification):
 * -
 *
 * Requirements Tested:
 * -
 *
 * Features Tested:
 * -
 *
 * Cases Tested:
 * -
 *
 * Description:
 * -
 *
 * Environment:
 * - This test is implementation specific
 * because it uses knowledge of internals that should be
 * opaque to an application.
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - analysis output on success.
 *
 * Assumptions:
 * -
 *
 * Pass Criteria:
 * - unique sequence numbers are generated for every new thread.
 *
 * Fail Criteria:
 * - 
 */

#include "test.h"

/*
 */

enum {
	NUMTHREADS = PTHREAD_THREADS_MAX
};


static long done = 0;
/*
 * seqmap should have 1 in every element except [0]
 * Thread sequence numbers start at 1 and we will also
 * include this main thread so we need NUMTHREADS+2
 * elements. 
 */
static UINT64 seqmap[NUMTHREADS+2];

void * func(void * arg)
{
  sched_yield();
  seqmap[(int)pthread_getunique_np(pthread_self())] = 1;
  InterlockedIncrement(&done);

  return (void *) 0; 
}
 
int
main()
{
  pthread_t t[NUMTHREADS];
  pthread_attr_t attr;
  int i;

  assert(pthread_attr_init(&attr) == 0);
  assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);

  for (i = 0; i < NUMTHREADS+2; i++)
    {
      seqmap[i] = 0;
    }

  for (i = 0; i < NUMTHREADS; i++)
    {
      if (NUMTHREADS/2 == i)
        {
          /* Include this main thread, which will be an implicit pthread_t */
          seqmap[(int)pthread_getunique_np(pthread_self())] = 1;
        }
      assert(pthread_create(&t[i], &attr, func, NULL) == 0);
    }

  while (NUMTHREADS > InterlockedExchangeAdd((LPLONG)&done, 0L))
    Sleep(100);

  Sleep(100);

  assert(seqmap[0] == 0);
  for (i = 1; i < NUMTHREADS+2; i++)
    {
      assert(seqmap[i] == 1);
    }

  return 0;
}
