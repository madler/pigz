/* 
 * spin4.c
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
 * Declare a static spinlock object, lock it, spin on it, 
 * and then unlock it again.
 */

#include "test.h"
#include <sys/timeb.h>
 
pthread_spinlock_t lock = PTHREAD_SPINLOCK_INITIALIZER;
__PTW32_STRUCT_TIMEB currSysTimeStart;
__PTW32_STRUCT_TIMEB currSysTimeStop;

#define GetDurationMilliSecs(_TStart, _TStop) ((_TStop.time*1000+_TStop.millitm) \
					       - (_TStart.time*1000+_TStart.millitm))

static int washere = 0;

void * func(void * arg)
{
   __PTW32_FTIME(&currSysTimeStart);
  washere = 1;
  assert(pthread_spin_lock(&lock) == 0);
  assert(pthread_spin_unlock(&lock) == 0);
   __PTW32_FTIME(&currSysTimeStop);

  return (void *)(size_t)GetDurationMilliSecs(currSysTimeStart, currSysTimeStop);
}
 
int
main()
{
  void* result = (void*)0;
  pthread_t t;
  int CPUs;
   __PTW32_STRUCT_TIMEB sysTime;

  if ((CPUs = pthread_num_processors_np()) == 1)
    {
      printf("Test not run - it requires multiple CPUs.\n");
	exit(0);
    }

  assert(pthread_spin_lock(&lock) == 0);

  assert(pthread_create(&t, NULL, func, NULL) == 0);

  while (washere == 0)
    {
      sched_yield();
    }

  do
    {
      sched_yield();
       __PTW32_FTIME(&sysTime);
    }
  while (GetDurationMilliSecs(currSysTimeStart, sysTime) <= 1000);

  assert(pthread_spin_unlock(&lock) == 0);

  assert(pthread_join(t, &result) == 0);
  assert((int)(size_t)result > 1000);

  assert(pthread_spin_destroy(&lock) == 0);

  assert(washere == 1);

  return 0;
}
