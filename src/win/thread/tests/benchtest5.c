/*
 * benchtest5.c
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
 * Measure time taken to complete an elementary operation.
 *
 * - Semaphore
 *   Single thread iteration over post/wait for a semaphore.
 */

#include "test.h"

#ifdef __GNUC__
#include <stdlib.h>
#endif

#include "benchtest.h"

#define ITERATIONS      1000000L

sem_t sema;
HANDLE w32sema;

__PTW32_STRUCT_TIMEB currSysTimeStart;
__PTW32_STRUCT_TIMEB currSysTimeStop;
long durationMilliSecs;
long overHeadMilliSecs = 0;
int one = 1;
int zero = 0;

#define GetDurationMilliSecs(_TStart, _TStop) ((long)((_TStop.time*1000+_TStop.millitm) \
                                               - (_TStart.time*1000+_TStart.millitm)))

/*
 * Dummy use of j, otherwise the loop may be removed by the optimiser
 * when doing the overhead timing with an empty loop.
 */
#define TESTSTART \
  { int i, j = 0, k = 0;  __PTW32_FTIME(&currSysTimeStart); for (i = 0; i < ITERATIONS; i++) { j++;

#define TESTSTOP \
  };  __PTW32_FTIME(&currSysTimeStop); if (j + k == i) j++; }


void
reportTest (char * testNameString)
{
  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    testNameString,
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);
}


int
main (int argc, char *argv[])
{
  printf( "=============================================================================\n");
  printf( "\nOperations on a semaphore.\n%ld iterations\n\n",
          ITERATIONS);
  printf( "%-45s %15s %15s\n",
	    "Test",
	    "Total(msec)",
	    "average(usec)");
  printf( "-----------------------------------------------------------------------------\n");

  /*
   * Time the loop overhead so we can subtract it from the actual test times.
   */

  TESTSTART
  assert(1 == one);
  TESTSTOP

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;
  overHeadMilliSecs = durationMilliSecs;


  /*
   * Now we can start the actual tests
   */
  assert((w32sema = CreateSemaphore(NULL, (long) 0, (long) ITERATIONS, NULL)) != 0);
  TESTSTART
  assert((ReleaseSemaphore(w32sema, 1, NULL),1) == one);
  TESTSTOP
  assert(CloseHandle(w32sema) != 0);

  reportTest("W32 Post with no waiters");


  assert((w32sema = CreateSemaphore(NULL, (long) ITERATIONS, (long) ITERATIONS, NULL)) != 0);
  TESTSTART
  assert((WaitForSingleObject(w32sema, INFINITE),1) == one);
  TESTSTOP
  assert(CloseHandle(w32sema) != 0);

  reportTest("W32 Wait without blocking");


  assert(sem_init(&sema, 0, 0) == 0);
  TESTSTART
  assert((sem_post(&sema),1) == one);
  TESTSTOP
  assert(sem_destroy(&sema) == 0);

  reportTest("POSIX Post with no waiters");


  assert(sem_init(&sema, 0, ITERATIONS) == 0);
  TESTSTART
  assert((sem_wait(&sema),1) == one);
  TESTSTOP
  assert(sem_destroy(&sema) == 0);

  reportTest("POSIX Wait without blocking");


  printf( "=============================================================================\n");

  /*
   * End of tests.
   */

  return 0;
}
