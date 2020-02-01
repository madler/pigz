/*
 * Test for pthread_detach().
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
 * Depends on API functions: pthread_create(), pthread_detach(), pthread_exit().
 */

#include "test.h"


enum {
  NUMTHREADS = 100
};

void *
func(void * arg)
{
    int i = (int)(size_t)arg;

    Sleep(i * 10);

    pthread_exit(arg);

    /* Never reached. */
    exit(1);
}

int
main(int argc, char * argv[])
{
	pthread_t id[NUMTHREADS];
	int i;

	/* Create a few threads and then exit. */
	for (i = 0; i < NUMTHREADS; i++)
	  {
	    assert(pthread_create(&id[i], NULL, func, (void *)(size_t)i) == 0);
	  }

	/* Some threads will finish before they are detached, some after. */
	Sleep(NUMTHREADS/2 * 10 + 50);

	for (i = 0; i < NUMTHREADS; i++)
	  {
	    assert(pthread_detach(id[i]) == 0);
	  }

	Sleep(NUMTHREADS * 10 + 100);

	/*
	 * Check that all threads are now invalid.
	 * This relies on unique thread IDs - e.g. works with
	 * pthreads-w32 or Solaris, but may not work for Linux, BSD etc.
	 */
	for (i = 0; i < NUMTHREADS; i++)
	  {
	    assert(pthread_kill(id[i], 0) == ESRCH);
	  }

	/* Success. */
	return 0;
}
