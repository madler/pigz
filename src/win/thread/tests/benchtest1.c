/*
 * benchtest1.c
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
 * - Mutex
 *   Single thread iteration over lock/unlock for each mutex type.
 */

#include "test.h"

#ifdef __GNUC__
#include <stdlib.h>
#endif

#include "benchtest.h"

#define  __PTW32_MUTEX_TYPES
#define ITERATIONS      10000000L

pthread_mutex_t mx;
pthread_mutexattr_t ma;
__PTW32_STRUCT_TIMEB currSysTimeStart;
__PTW32_STRUCT_TIMEB currSysTimeStop;
long durationMilliSecs;
long overHeadMilliSecs = 0;
int two = 2;
int one = 1;
int zero = 0;
int iter;

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
runTest (char * testNameString, int mType)
{
#ifdef  __PTW32_MUTEX_TYPES
  assert(pthread_mutexattr_settype(&ma, mType) == 0);
#endif
  assert(pthread_mutex_init(&mx, &ma) == 0);

  TESTSTART
  assert((pthread_mutex_lock(&mx),1) == one);
  assert((pthread_mutex_unlock(&mx),2) == two);
  TESTSTOP

  assert(pthread_mutex_destroy(&mx) == 0);

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    testNameString,
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);
}


int
main (int argc, char *argv[])
{
  int i = 0;
  CRITICAL_SECTION cs;
  old_mutex_t ox;
  pthread_mutexattr_init(&ma);

  printf( "=============================================================================\n");
  printf( "\nLock plus unlock on an unlocked mutex.\n%ld iterations\n\n",
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
  assert(2 == two);
  TESTSTOP

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;
  overHeadMilliSecs = durationMilliSecs;


  TESTSTART
  assert((dummy_call(&i), 1) == one);
  assert((dummy_call(&i), 2) == two);
  TESTSTOP

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    "Dummy call x 2",
          durationMilliSecs,
          (float) (durationMilliSecs * 1E3 / ITERATIONS));


  TESTSTART
  assert((interlocked_inc_with_conditionals(&i), 1) == one);
  assert((interlocked_dec_with_conditionals(&i), 2) == two);
  TESTSTOP

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    "Dummy call -> Interlocked with cond x 2",
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);


  TESTSTART
  assert((InterlockedIncrement((LPLONG)&i), 1) == (LONG)one);
  assert((InterlockedDecrement((LPLONG)&i), 2) == (LONG)two);
  TESTSTOP

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    "InterlockedOp x 2",
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);


  InitializeCriticalSection(&cs);

  TESTSTART
  assert((EnterCriticalSection(&cs), 1) == one);
  assert((LeaveCriticalSection(&cs), 2) == two);
  TESTSTOP

  DeleteCriticalSection(&cs);

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    "Simple Critical Section",
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);


  old_mutex_use = OLD_WIN32CS;
  assert(old_mutex_init(&ox, NULL) == 0);

  TESTSTART
  assert(old_mutex_lock(&ox) == zero);
  assert(old_mutex_unlock(&ox) == zero);
  TESTSTOP

  assert(old_mutex_destroy(&ox) == 0);

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    "Old PT Mutex using a Critical Section (WNT)",
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);


  old_mutex_use = OLD_WIN32MUTEX;
  assert(old_mutex_init(&ox, NULL) == 0);

  TESTSTART
  assert(old_mutex_lock(&ox) == zero);
  assert(old_mutex_unlock(&ox) == zero);
  TESTSTOP

  assert(old_mutex_destroy(&ox) == 0);

  durationMilliSecs = GetDurationMilliSecs(currSysTimeStart, currSysTimeStop) - overHeadMilliSecs;

  printf( "%-45s %15ld %15.3f\n",
	    "Old PT Mutex using a Win32 Mutex (W9x)",
          durationMilliSecs,
          (float) durationMilliSecs * 1E3 / ITERATIONS);

  printf( ".............................................................................\n");

  /*
   * Now we can start the actual tests
   */
#ifdef  __PTW32_MUTEX_TYPES
  runTest("PTHREAD_MUTEX_DEFAULT", PTHREAD_MUTEX_DEFAULT);

  runTest("PTHREAD_MUTEX_NORMAL", PTHREAD_MUTEX_NORMAL);

  runTest("PTHREAD_MUTEX_ERRORCHECK", PTHREAD_MUTEX_ERRORCHECK);

  runTest("PTHREAD_MUTEX_RECURSIVE", PTHREAD_MUTEX_RECURSIVE);
#else
  runTest("Non-blocking lock", 0);
#endif

  printf( ".............................................................................\n");

  pthread_mutexattr_setrobust(&ma, PTHREAD_MUTEX_ROBUST);

#ifdef  __PTW32_MUTEX_TYPES
  runTest("PTHREAD_MUTEX_DEFAULT (Robust)", PTHREAD_MUTEX_DEFAULT);

  runTest("PTHREAD_MUTEX_NORMAL (Robust)", PTHREAD_MUTEX_NORMAL);

  runTest("PTHREAD_MUTEX_ERRORCHECK (Robust)", PTHREAD_MUTEX_ERRORCHECK);

  runTest("PTHREAD_MUTEX_RECURSIVE (Robust)", PTHREAD_MUTEX_RECURSIVE);
#else
  runTest("Non-blocking lock", 0);
#endif

  printf( "=============================================================================\n");

  /*
   * End of tests.
   */

  pthread_mutexattr_destroy(&ma);

  one = i; /* Dummy assignment to avoid 'variable unused' warning */
  return 0;
}
