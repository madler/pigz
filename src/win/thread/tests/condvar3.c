/*
 * File: condvar3.c
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
 * - Test basic function of a CV
 *
 * Test Method (Validation or Falsification):
 * - Validation
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
 * - The primary thread takes the lock before creating any threads.
 *   The secondary thread blocks on the lock allowing the primary
 *   thread to enter the cv wait state which releases the lock.
 *   The secondary thread then takes the lock and signals the waiting
 *   primary thread.
 *
 * Environment:
 * - 
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - No output on success.
 *
 * Assumptions:
 * - 
 *
 * Pass Criteria:
 * - pthread_cond_timedwait returns 0.
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - pthread_cond_timedwait returns ETIMEDOUT.
 * - Process returns non-zero exit status.
 */

#include "test.h"
#include <sys/timeb.h>

static pthread_cond_t cv;
static pthread_mutex_t mutex;
static int shared = 0;

enum {
  NUMTHREADS = 2         /* Including the primary thread. */
};

void *
mythread(void * arg)
{
  int result = 0;

  assert(pthread_mutex_lock(&mutex) == 0);
  shared++;
  assert(pthread_mutex_unlock(&mutex) == 0);

  if ((result = pthread_cond_signal(&cv)) != 0)
    {
      printf("Error = %s\n", error_string[result]);
    }
  assert(result == 0);


  return (void *) 0;
}

int
main()
{
  pthread_t t[NUMTHREADS];
  struct timespec abstime, reltime = { 5, 0 };

  assert((t[0] = pthread_self()).p != NULL);

  assert(pthread_cond_init(&cv, NULL) == 0);

  assert(pthread_mutex_init(&mutex, NULL) == 0);

  assert(pthread_mutex_lock(&mutex) == 0);

  assert(pthread_create(&t[1], NULL, mythread, (void *) 1) == 0);

  (void) pthread_win32_getabstime_np(&abstime, &reltime);

  while (! (shared > 0))
    assert(pthread_cond_timedwait(&cv, &mutex, &abstime) == 0);

  assert(shared > 0);

  assert(pthread_mutex_unlock(&mutex) == 0);

  assert(pthread_join(t[1], NULL) == 0);

  assert(pthread_cond_destroy(&cv) == 0);

  return 0;
}
