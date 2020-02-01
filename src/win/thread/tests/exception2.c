/*
 * File: exception2.c
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
 * Test Synopsis: Test passing of exceptions out of thread scope.
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
 * - have working pthread_create, pthread_self, pthread_mutex_lock/unlock
 *   pthread_testcancel, pthread_cancel
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */


#if defined(_MSC_VER) || defined(__cplusplus)

#if defined(_MSC_VER) && defined(__cplusplus)
#include <eh.h>
#elif defined(__cplusplus)
#include <exception>
#endif

#ifdef __GNUC__
#include <stdlib.h>
#endif

#include "test.h"

/*
 * Create NUMTHREADS threads in addition to the Main thread.
 */
enum {
  NUMTHREADS = 1
};


void *
exceptionedThread(void * arg)
{
  int dummy = 0x1;

#if defined(_MSC_VER) && !defined(__cplusplus)

  RaiseException(dummy, 0, 0, NULL);

#elif defined(__cplusplus)

  throw dummy;

#endif

  return (void *) 100;
}

int
main(int argc, char* argv[])
{
  int i;
  pthread_t mt;
  pthread_t et[NUMTHREADS];

  DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
  SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);

  if (argc <= 1)
    {
      int result;

      printf("You should see an \"abnormal termination\" message\n");
      fflush(stdout);

      result = system("exception2.exe die");

      printf("\"exception2.exe die\" returned status %d\n", result);

      /*
       * result should be 0, 1 or 3 depending on build settings
       */
      exit((result == 0 || result == 1 || result == 3) ? 0 : 1);
    }

#if defined(NO_ERROR_DIALOGS)
  SetErrorMode(SEM_NOGPFAULTERRORBOX);
#endif

  assert((mt = pthread_self()).p != NULL);

  for (i = 0; i < NUMTHREADS; i++)
    {
      assert(pthread_create(&et[i], NULL, exceptionedThread, NULL) == 0);
    }

  Sleep(100);

  /*
   * Success.
   */
  return 0;
}

#else /* defined(_MSC_VER) || defined(__cplusplus) */

#include <stdio.h>

int
main()
{
  fprintf(stderr, "Test N/A for this compiler environment.\n");
  return 0;
}

#endif /* defined(_MSC_VER) || defined(__cplusplus) */
