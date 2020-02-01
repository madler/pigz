/*
 * Test for pthread_timedjoin_np() timing out.
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
 * Depends on API functions: pthread_create().
 */

#include "test.h"

void *
func(void * arg)
{
        Sleep(1200);
        return arg;
}

int
main(int argc, char * argv[])
{
        pthread_t id;
        struct timespec abstime, reltime = { 1, 0 };
        void* result = (void*)-1;

        assert(pthread_create(&id, NULL, func, (void *)(size_t)999) == 0);

        /*
         * Let thread start before we attempt to join it.
         */
        Sleep(100);

        (void) pthread_win32_getabstime_np(&abstime, &reltime);

        /* Test for pthread_timedjoin_np timeout */
        assert(pthread_timedjoin_np(id, &result, &abstime) == ETIMEDOUT);
        assert((int)(size_t)result == -1);

        /* Test for pthread_tryjoin_np behaviour before thread has exited */
        assert(pthread_tryjoin_np(id, &result) == EBUSY);
        assert((int)(size_t)result == -1);

        Sleep(500);

        /* Test for pthread_tryjoin_np behaviour after thread has exited */
        assert(pthread_tryjoin_np(id, &result) == 0);
        assert((int)(size_t)result == 999);

        /* Success. */
        return 0;
}
