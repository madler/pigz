/*
 * File: context2.c
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
 * Test Synopsis: Test context switching method.
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
 * - pthread_create
 *   pthread_exit
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */

#define _WIN32_WINNT 0x400

#include "test.h"
/* Cheating here - sneaking a peek at library internals */
#include "../config.h"
#include "../implement.h"
#include "../context.h"

static int washere = 0;
static volatile size_t tree_counter = 0;

#ifdef _MSC_VER
# pragma inline_depth(0)
# pragma optimize("g", off)
#endif

static size_t tree(size_t depth)
{
  if (! depth--)
    return tree_counter++;

  return tree(depth) + tree(depth);
}

static void * func(void * arg)
{
  washere = 1;

  return (void *) tree(64);
}

static void
anotherEnding ()
{
  /*
   * Switched context
   */
  washere++;
  pthread_exit(0);
}

#ifdef _MSC_VER
# pragma inline_depth()
# pragma optimize("", on)
#endif

int
main()
{
  pthread_t t;
  HANDLE hThread;

  assert(pthread_create(&t, NULL, func, NULL) == 0);

  hThread = ((__ptw32_thread_t *)t.p)->threadH;

  Sleep(500);

  SuspendThread(hThread);

  if (WaitForSingleObject(hThread, 0) == WAIT_TIMEOUT)
    {
      /*
       * Ok, thread did not exit before we got to it.
       */
      CONTEXT context;

      context.ContextFlags = CONTEXT_CONTROL;

      GetThreadContext(hThread, &context);
       __PTW32_PROGCTR (context) = (DWORD_PTR) anotherEnding;
      SetThreadContext(hThread, &context);
      ResumeThread(hThread);
    }
  else
    {
      printf("Exited early\n");
      fflush(stdout);
    }

  Sleep(1000);

  assert(washere == 2);

  return 0;
}
