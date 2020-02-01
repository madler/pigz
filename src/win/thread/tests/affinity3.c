/*
 * affinity3.c
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
 * Have the thread switch CPUs.
 *
 */

#if ! defined(WINCE)

#include "test.h"

int
main()
{
  int result;
  unsigned int cpu;
  cpu_set_t newmask;
  cpu_set_t processCpus;
  cpu_set_t mask;
  cpu_set_t switchmask;
  cpu_set_t flipmask;
  pthread_t self = pthread_self();

  CPU_ZERO(&mask);
  CPU_ZERO(&switchmask);
  CPU_ZERO(&flipmask);

  if (pthread_getaffinity_np(self, sizeof(cpu_set_t), &processCpus) == ENOSYS)
    {
      printf("pthread_get/set_affinity_np API not supported for this platform: skipping test.");
      return 0;
    }
  assert(pthread_getaffinity_np(self, sizeof(cpu_set_t), &processCpus) == 0);
  printf("This thread has a starting affinity with %d CPUs\n", CPU_COUNT(&processCpus));
  assert(!CPU_EQUAL(&mask, &processCpus));

  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu += 2)
    {
	  CPU_SET(cpu, &switchmask);				/* 0b01010101010101010101010101010101 */
    }
  for (cpu = 0; cpu < sizeof(cpu_set_t)*8; cpu++)
    {
	  CPU_SET(cpu, &flipmask);					/* 0b11111111111111111111111111111111 */
    }

  result = pthread_setaffinity_np(self, sizeof(cpu_set_t), &processCpus);
  if (result != 0)
	{
	  assert(result != ESRCH);
	  assert(result != EFAULT);
	  assert(result != EPERM);
	  assert(result != EINVAL);
	  assert(result != EAGAIN);
	  assert(result == ENOSYS);
	  assert(CPU_COUNT(&mask) == 1);
	}
  else
	{
	  if (CPU_COUNT(&mask) > 1)
		{
		  CPU_AND(&newmask, &processCpus, &switchmask); /* Remove every other CPU */
		  assert(pthread_setaffinity_np(self, sizeof(cpu_set_t), &newmask) == 0);
		  assert(pthread_getaffinity_np(self, sizeof(cpu_set_t), &mask) == 0);
		  assert(CPU_EQUAL(&mask, &newmask));
		  CPU_XOR(&newmask, &mask, &flipmask);  /* Switch to all alternative CPUs */
		  assert(!CPU_EQUAL(&mask, &newmask));
		  assert(pthread_setaffinity_np(self, sizeof(cpu_set_t), &newmask) == 0);
		  assert(pthread_getaffinity_np(self, sizeof(cpu_set_t), &mask) == 0);
		  assert(CPU_EQUAL(&mask, &newmask));
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
