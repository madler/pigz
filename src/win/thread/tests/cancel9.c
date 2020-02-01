/*
 * File: cancel9.c
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
 * Test Synopsis: Test true asynchronous cancellation with Alert driver.
 *
 * Test Method (Validation or Falsification):
 * -
 *
 * Requirements Tested:
 * - Cancel threads, including those blocked on system recources
 *   such as network I/O.
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
 *   pthread_testcancel, pthread_cancel, pthread_join
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */

#include "test.h"
#include <windows.h>


void *
test_udp (void *arg)
{
  struct sockaddr_in serverAddress;
  struct sockaddr_in clientAddress;
  SOCKET UDPSocket;
  int addr_len;
  int nbyte;
  char buffer[4096];
  WORD wsaVersion = MAKEWORD (2, 2);
  WSADATA wsaData;

  pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  if (WSAStartup (wsaVersion, &wsaData) != 0)
    {
      return NULL;
    }

  UDPSocket = socket (AF_INET, SOCK_DGRAM, 0);
  if ((int)UDPSocket == -1)
    {
      printf ("Server: socket ERROR \n");
      exit (-1);
    }

  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = INADDR_ANY;
  serverAddress.sin_port = htons (9003);

  if (bind
      (UDPSocket, (struct sockaddr *) &serverAddress,
       sizeof (struct sockaddr_in)))
    {
      printf ("Server: ERROR can't bind UDPSocket");
      exit (-1);
    }

  addr_len = sizeof (struct sockaddr);

  nbyte = 512;

  (void) recvfrom (UDPSocket, (char *) buffer, nbyte, 0,
	           (struct sockaddr *) &clientAddress, &addr_len);

  closesocket (UDPSocket);
  WSACleanup ();

  return NULL;
}


void *
test_sleep (void *arg)
{
  pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  Sleep (1000);
  return NULL;

}

void *
test_wait (void *arg)
{
  HANDLE hEvent;

  pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

  (void) WaitForSingleObject (hEvent, 1000);	/* WAIT_IO_COMPLETION */

  return NULL;
}


int
main ()
{
  pthread_t t;
  void *result;

  if (pthread_win32_test_features_np  (__PTW32_ALERTABLE_ASYNC_CANCEL))
    {
      printf ("Cancel sleeping thread.\n");
      assert (pthread_create (&t, NULL, test_sleep, NULL) == 0);
      /* Sleep for a while; then cancel */
      Sleep (100);
      assert (pthread_cancel (t) == 0);
      assert (pthread_join (t, &result) == 0);
      assert (result == PTHREAD_CANCELED && "test_sleep");

      printf ("Cancel waiting thread.\n");
      assert (pthread_create (&t, NULL, test_wait, NULL) == 0);
      /* Sleep for a while; then cancel. */
      Sleep (100);
      assert (pthread_cancel (t) == 0);
      assert (pthread_join (t, &result) == 0);
      assert (result == PTHREAD_CANCELED && "test_wait");

      printf ("Cancel blocked thread (blocked on network I/O).\n");
      assert (pthread_create (&t, NULL, test_udp, NULL) == 0);
      /* Sleep for a while; then cancel. */
      Sleep (100);
      assert (pthread_cancel (t) == 0);
      assert (pthread_join (t, &result) == 0);
      assert (result == PTHREAD_CANCELED && "test_udp");
    }
  else
    {
      printf ("Alertable async cancel not available.\n");
    }

  /*
   * Success.
   */
  return 0;
}
