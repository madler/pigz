/*
 * File: condvar1_1.c
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
 * - Test CV linked list management.
 *
 * Test Method (Validation or Falsification):
 * - Validation:
 *   Initiate and destroy several CVs in random order.
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
 * - Creates and then imediately destroys a CV. Does not
 *   test the CV.
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
 * - All initialised CVs destroyed without segfault.
 * - Successfully broadcasts all remaining CVs after
 *   each CV is removed.
 *
 * Fail Criteria:
 */

#include <stdlib.h>
#include "test.h"

enum {
  NUM_CV = 100
};

static pthread_cond_t cv[NUM_CV];

int
main()
{
  int i, j;

  for (i = 0; i < NUM_CV; i++)
    {
      /* Traverse the list before every init of a CV. */
      assert(pthread_timechange_handler_np(NULL) == (void *) 0);
      assert(pthread_cond_init(&cv[i], NULL) == 0);
    }

  j = NUM_CV;
  (void) srand((unsigned)time(NULL));

  do
    {
      i = (NUM_CV - 1) * rand() / RAND_MAX;
      if (cv[i] != NULL)
        {
          j--;
          assert(pthread_cond_destroy(&cv[i]) == 0);
          /* Traverse the list every time we remove a CV. */
          assert(pthread_timechange_handler_np(NULL) == (void *) 0);
        }
    }
  while (j > 0);

  return 0;
}
