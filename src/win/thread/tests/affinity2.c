/*
 * affinity2.c
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
 * Have the process switch CPUs.
 *
 */

#if ! defined(WINCE)

#include "test.h"

int
main()
{
  unsigned int cpu;
  int result;
  cpu_set_t newmask;
  cpu_set_t mask;
  cpu_set_t switchmask;
  cpu_set_t flipmask;

  CPU_ZERO(&mask);
  CPU_ZERO(&switchmask);
  CPU_ZERO(&flipmask);

  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &switchmask);				/* 0b01010101010101010101010101010101 */
    }
  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu++)
    {
	  CPU_SET(cpu, &flipmask);					/* 0b11111111111111111111111111111111 */
    }

  assert(sched_getaffinity(0, sizeof(cpu_set_t), &newmask) == 0);
  assert(!CPU_EQUAL(&newmask, &mask));

  result = sched_setaffinity(0, sizeof(cpu_set_t), &newmask);
  if (result != 0)
	{
	  int err =
#if defined (__PTW32_USES_SEPARATE_CRT)
	  GetLastError();
#else
      errno;
#endif

	  assert(err != ESRCH);
	  assert(err != EFAULT);
	  assert(err != EPERM);
	  assert(err != EINVAL);
	  assert(err != EAGAIN);
	  assert(err == ENOSYS);
	  assert(CPU_COUNT(&mask) == 1);
	}
  else
	{
	  if (CPU_COUNT(&mask) > 1)
		{
		  CPU_AND(&newmask, &mask, &switchmask); /* Remove every other CPU */
		  assert(sched_setaffinity(0, sizeof(cpu_set_t), &newmask) == 0);
		  assert(sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0);
		  CPU_XOR(&newmask, &mask, &flipmask);  /* Switch to all alternative CPUs */
		  assert(sched_setaffinity(0, sizeof(cpu_set_t), &newmask) == 0);
		  assert(sched_getaffinity(0, sizeof(cpu_set_t), &mask) == 0);
		  assert(!CPU_EQUAL(&newmask, &mask));
		}
	}

  return 0;
}

#else

#include <stdio.h>

int
main()
{
  fprintf(stderr, "Test N/A for this target environment.\n");
  return 0;
}

#endif
