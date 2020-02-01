/*
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
 */

#include "../config.h"

#include "pthread.h"
#include "sched.h"
#include "semaphore.h"
#include <windows.h>
#include <stdio.h>

#ifdef __GNUC__
#include <stdlib.h>
#endif

#include "benchtest.h"

int old_mutex_use = OLD_WIN32CS;

BOOL (WINAPI *__ptw32_try_enter_critical_section)(LPCRITICAL_SECTION) = NULL;
HINSTANCE __ptw32_h_kernel32;

void
dummy_call(int * a)
{
}

void
interlocked_inc_with_conditionals(int * a)
{
  if (a != NULL)
    if (InterlockedIncrement((long *) a) == -1)
      {
        *a = 0;
      }
}

void
interlocked_dec_with_conditionals(int * a)
{
  if (a != NULL)
    if (InterlockedDecrement((long *) a) == -1)
      {
        *a = 0;
      }
}

int
old_mutex_init(old_mutex_t *mutex, const old_mutexattr_t *attr)
{
  int result = 0;
  old_mutex_t mx;

  if (mutex == NULL)
    {
      return EINVAL;
    }

  mx = (old_mutex_t) calloc(1, sizeof(*mx));

  if (mx == NULL)
    {
      result = ENOMEM;
      goto FAIL0;
    }

  mx->mutex = 0;

  if (attr != NULL
      && *attr != NULL
      && (*attr)->pshared == PTHREAD_PROCESS_SHARED
      )
    {
      result = ENOSYS;
    }
  else
    {
        CRITICAL_SECTION cs;

        /*
         * Load KERNEL32 and try to get address of TryEnterCriticalSection
         */
        __ptw32_h_kernel32 = LoadLibrary(TEXT("KERNEL32.DLL"));
        __ptw32_try_enter_critical_section = (BOOL (WINAPI *)(LPCRITICAL_SECTION))

#if defined(NEED_UNICODE_CONSTS)
        GetProcAddress(__ptw32_h_kernel32,
                       (const TCHAR *)TEXT("TryEnterCriticalSection"));
#else
        GetProcAddress(__ptw32_h_kernel32,
                       (LPCSTR) "TryEnterCriticalSection");
#endif

        if (__ptw32_try_enter_critical_section != NULL)
          {
            InitializeCriticalSection(&cs);
            if ((*__ptw32_try_enter_critical_section)(&cs))
              {
                LeaveCriticalSection(&cs);
              }
            else
              {
                /*
                 * Not really supported (Win98?).
                 */
                __ptw32_try_enter_critical_section = NULL;
              }
            DeleteCriticalSection(&cs);
          }

        if (__ptw32_try_enter_critical_section == NULL)
          {
            (void) FreeLibrary(__ptw32_h_kernel32);
            __ptw32_h_kernel32 = 0;
          }

      if (old_mutex_use == OLD_WIN32CS)
	{
	  InitializeCriticalSection(&mx->cs);
	}
      else if (old_mutex_use == OLD_WIN32MUTEX)
      {
	  mx->mutex = CreateMutex (NULL,
				   FALSE,
				   NULL);

	  if (mx->mutex == 0)
	    {
	      result = EAGAIN;
	    }
	}
      else
	{
        result = EINVAL;
      }
    }

  if (result != 0 && mx != NULL)
    {
      free(mx);
      mx = NULL;
    }

FAIL0:
  *mutex = mx;

  return(result);
}


int
old_mutex_lock(old_mutex_t *mutex)
{
  int result = 0;
  old_mutex_t mx;

  if (mutex == NULL || *mutex == NULL)
    {
      return EINVAL;
    }

  if (*mutex == (old_mutex_t)  __PTW32_OBJECT_AUTO_INIT)
    {
      /*
       * Don't use initialisers when benchtesting.
       */
      result = EINVAL;
    }

  mx = *mutex;

  if (result == 0)
    {
      if (mx->mutex == 0)
	{
	  EnterCriticalSection(&mx->cs);
	}
      else
	{
	  result = (WaitForSingleObject(mx->mutex, INFINITE) 
		    == WAIT_OBJECT_0)
	    ? 0
	    : EINVAL;
	}
    }

  return(result);
}

int
old_mutex_unlock(old_mutex_t *mutex)
{
  int result = 0;
  old_mutex_t mx;

  if (mutex == NULL || *mutex == NULL)
    {
      return EINVAL;
    }

  mx = *mutex;

  if (mx != (old_mutex_t)  __PTW32_OBJECT_AUTO_INIT)
    {
      if (mx->mutex == 0)
	{
	  LeaveCriticalSection(&mx->cs);
	}
      else
	{
	  result = (ReleaseMutex (mx->mutex) ? 0 : EINVAL);
	}
    }
  else
    {
      result = EINVAL;
    }

  return(result);
}


int
old_mutex_trylock(old_mutex_t *mutex)
{
  int result = 0;
  old_mutex_t mx;

  if (mutex == NULL || *mutex == NULL)
    {
      return EINVAL;
    }

  if (*mutex == (old_mutex_t)  __PTW32_OBJECT_AUTO_INIT)
    {
      /*
       * Don't use initialisers when benchtesting.
       */
      result = EINVAL;
    }

  mx = *mutex;

  if (result == 0)
    {
      if (mx->mutex == 0)
	{
	  if (__ptw32_try_enter_critical_section == NULL)
          {
            result = 0;
          }
        else if ((*__ptw32_try_enter_critical_section)(&mx->cs) != TRUE)
	    {
	      result = EBUSY;
	    }
	}
      else
	{
	  DWORD status;

	  status = WaitForSingleObject (mx->mutex, 0);

	  if (status != WAIT_OBJECT_0)
	    {
	      result = ((status == WAIT_TIMEOUT)
			? EBUSY
			: EINVAL);
	    }
	}
    }

  return(result);
}


int
old_mutex_destroy(old_mutex_t *mutex)
{
  int result = 0;
  old_mutex_t mx;

  if (mutex == NULL
      || *mutex == NULL)
    {
      return EINVAL;
    }

  if (*mutex != (old_mutex_t)  __PTW32_OBJECT_AUTO_INIT)
    {
      mx = *mutex;

      if ((result = old_mutex_trylock(&mx)) == 0)
        {
          *mutex = NULL;

          (void) old_mutex_unlock(&mx);

          if (mx->mutex == 0)
            {
              DeleteCriticalSection(&mx->cs);
            }
          else
            {
              result = (CloseHandle (mx->mutex) ? 0 : EINVAL);
            }

          if (result == 0)
            {
              mx->mutex = 0;
              free(mx);
            }
          else
            {
              *mutex = mx;
            }
        }
    }
  else
    {
      result = EINVAL;
    }

  if (__ptw32_try_enter_critical_section != NULL)
    {
      (void) FreeLibrary(__ptw32_h_kernel32);
      __ptw32_h_kernel32 = 0;
    }

  return(result);
}

/****************************************************************************************/
