/*
 * self1.c
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
 * Test for pthread_self().
 *
 * Depends on API functions:
 *	pthread_self()
 *
 * Implicitly depends on:
 *	pthread_getspecific()
 *	pthread_setspecific()
 */

#include "test.h"

int
main(int argc, char * argv[])
{
	/*
	 * This should always succeed unless the system has no
	 * resources (memory) left.
	 */
	pthread_t self;

#if defined (__PTW32_STATIC_LIB) && !(defined(_MSC_VER) || defined(__MINGW32__))
	pthread_win32_process_attach_np();
#endif

	self = pthread_self();

	assert(self.p != NULL);

#if defined (__PTW32_STATIC_LIB) && !(defined(_MSC_VER) || defined(__MINGW32__))
	pthread_win32_process_detach_np();
#endif
	return 0;
}
