/*
 * File: reuse1.c
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
 * - Confirm that thread reuse works for joined threads.
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
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */

#include "test.h"

enum {
	NUMTHREADS = 100
};

static int washere = 0;

void * func(void * arg)
{
  washere = 1;
  return arg; 
}
 
int
main()
{
  pthread_t t,
            last_t;
  pthread_attr_t attr;
  void * result = NULL;
  int i;

  assert(pthread_attr_init(&attr) == 0);;
  assert(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) == 0);

  washere = 0;
  assert(pthread_create(&t, &attr, func, NULL) == 0);
  assert(pthread_join(t, &result) == 0);;
  assert((int)(size_t)result == 0);
  assert(washere == 1);
  last_t = t;

  for (i = 1; i < NUMTHREADS; i++)
    {
      washere = 0;
      assert(pthread_create(&t, &attr, func, (void *)(size_t)i) == 0);
      pthread_join(t, &result);
      assert((int)(size_t) result == i);
      assert(washere == 1);
      /* thread IDs should be unique */
      assert(!pthread_equal(t, last_t));
      /* thread struct pointers should be the same */
      assert(t.p == last_t.p);
      /* thread handle reuse counter should be different by one */
      assert(t.x == last_t.x+1);
      last_t = t;
    }

  return 0;
}
