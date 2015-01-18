/* yarn.c -- generic thread operations implemented using pthread functions
 * Copyright (C) 2008, 2011, 2012, 2015 Mark Adler
 * Version 1.4  19 Jan 2015  Mark Adler
 * For conditions of distribution and use, see copyright notice in yarn.h
 */

/* Basic thread operations implemented using the POSIX pthread library.  All
   pthread references are isolated within this module to allow alternate
   implementations with other thread libraries.  See yarn.h for the description
   of these operations. */

/* Version history:
   1.0    19 Oct 2008  First version
   1.1    26 Oct 2008  No need to set the stack size -- remove
                       Add yarn_abort() function for clean-up on error exit
   1.2    19 Dec 2011  (changes reversed in 1.3)
   1.3    13 Jan 2012  Add large file #define for consistency with pigz.c
                       Update thread portability #defines per IEEE 1003.1-2008
                       Fix documentation in yarn.h for yarn_prefix
   1.4    19 Jan 2015  Allow yarn_abort() to avoid error message to stderr
                       Accept and do nothing for NULL argument to free_lock()
 */

/* for thread portability */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _THREAD_SAFE

/* use large file functions if available */
#define _FILE_OFFSET_BITS 64

/* external libraries and entities referenced */
#include <stdio.h>      /* fprintf(), stderr */
#include <stdlib.h>     /* exit(), malloc(), free(), NULL */
#include <pthread.h>    /* pthread_t, pthread_create(), pthread_join(), */
    /* pthread_attr_t, pthread_attr_init(), pthread_attr_destroy(),
       PTHREAD_CREATE_JOINABLE, pthread_attr_setdetachstate(),
       pthread_self(), pthread_equal(),
       pthread_mutex_t, PTHREAD_MUTEX_INITIALIZER, pthread_mutex_init(),
       pthread_mutex_lock(), pthread_mutex_unlock(), pthread_mutex_destroy(),
       pthread_cond_t, PTHREAD_COND_INITIALIZER, pthread_cond_init(),
       pthread_cond_broadcast(), pthread_cond_wait(), pthread_cond_destroy() */
#include <errno.h>      /* ENOMEM, EAGAIN, EINVAL */

/* interface definition */
#include "yarn.h"

/* constants */
#define local static            /* for non-exported functions and globals */

/* error handling external globals, resettable by application */
char *yarn_prefix = "yarn";
void (*yarn_abort)(int) = NULL;


/* immediately exit -- use for errors that shouldn't ever happen */
local void fail(int err)
{
    if (yarn_abort != NULL)
        yarn_abort(err);
    fprintf(stderr, "%s: %s (%d) -- aborting\n", yarn_prefix,
            err == ENOMEM ? "out of memory" : "internal pthread error", err);
    exit(err == ENOMEM || err == EAGAIN ? err : EINVAL);
}

/* memory handling routines provided by user -- if none are provided, malloc()
   and free() are used, which are therefore assumed to be thread-safe */
typedef void *(*malloc_t)(size_t);
typedef void (*free_t)(void *);
local malloc_t my_malloc_f = malloc;
local free_t my_free = free;

/* use user-supplied allocation routines instead of malloc() and free() */
void yarn_mem(malloc_t lease, free_t vacate)
{
    my_malloc_f = lease;
    my_free = vacate;
}

/* memory allocation that cannot fail (from the point of view of the caller) */
local void *my_malloc(size_t size)
{
    void *block;

    if ((block = my_malloc_f(size)) == NULL)
        fail(ENOMEM);
    return block;
}

/* -- lock functions -- */

struct lock_s {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    long value;
};

lock *new_lock(long initial)
{
    int ret;
    lock *bolt;

    bolt = my_malloc(sizeof(struct lock_s));
    if ((ret = pthread_mutex_init(&(bolt->mutex), NULL)) ||
        (ret = pthread_cond_init(&(bolt->cond), NULL)))
        fail(ret);
    bolt->value = initial;
    return bolt;
}

void possess(lock *bolt)
{
    int ret;

    if ((ret = pthread_mutex_lock(&(bolt->mutex))) != 0)
        fail(ret);
}

void release(lock *bolt)
{
    int ret;

    if ((ret = pthread_mutex_unlock(&(bolt->mutex))) != 0)
        fail(ret);
}

void twist(lock *bolt, enum twist_op op, long val)
{
    int ret;

    if (op == TO)
        bolt->value = val;
    else if (op == BY)
        bolt->value += val;
    if ((ret = pthread_cond_broadcast(&(bolt->cond))) ||
        (ret = pthread_mutex_unlock(&(bolt->mutex))))
        fail(ret);
}

#define until(a) while(!(a))

void wait_for(lock *bolt, enum wait_op op, long val)
{
    int ret;

    switch (op) {
    case TO_BE:
        until (bolt->value == val)
            if ((ret = pthread_cond_wait(&(bolt->cond), &(bolt->mutex))) != 0)
                fail(ret);
        break;
    case NOT_TO_BE:
        until (bolt->value != val)
            if ((ret = pthread_cond_wait(&(bolt->cond), &(bolt->mutex))) != 0)
                fail(ret);
        break;
    case TO_BE_MORE_THAN:
        until (bolt->value > val)
            if ((ret = pthread_cond_wait(&(bolt->cond), &(bolt->mutex))) != 0)
                fail(ret);
        break;
    case TO_BE_LESS_THAN:
        until (bolt->value < val)
            if ((ret = pthread_cond_wait(&(bolt->cond), &(bolt->mutex))) != 0)
                fail(ret);
    }
}

long peek_lock(lock *bolt)
{
    return bolt->value;
}

void free_lock(lock *bolt)
{
    int ret;

    if (bolt == NULL)
        return;
    if ((ret = pthread_cond_destroy(&(bolt->cond))) ||
        (ret = pthread_mutex_destroy(&(bolt->mutex))))
        fail(ret);
    my_free(bolt);
}

/* -- thread functions (uses lock functions above) -- */

struct thread_s {
    pthread_t id;
    int done;                   /* true if this thread has exited */
    thread *next;               /* for list of all launched threads */
};

/* list of threads launched but not joined, count of threads exited but not
   joined (incremented by ignition() just before exiting) */
local lock threads_lock = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_COND_INITIALIZER,
    0                           /* number of threads exited but not joined */
};
local thread *threads = NULL;       /* list of extant threads */

/* structure in which to pass the probe and its payload to ignition() */
struct capsule {
    void (*probe)(void *);
    void *payload;
};

/* mark the calling thread as done and alert join_all() */
local void reenter(void *dummy)
{
    thread *match, **prior;
    pthread_t me;

    (void)dummy;

    /* find this thread in the threads list by matching the thread id */
    me = pthread_self();
    possess(&(threads_lock));
    prior = &(threads);
    while ((match = *prior) != NULL) {
        if (pthread_equal(match->id, me))
            break;
        prior = &(match->next);
    }
    if (match == NULL)
        fail(EINVAL);

    /* mark this thread as done and move it to the head of the list */
    match->done = 1;
    if (threads != match) {
        *prior = match->next;
        match->next = threads;
        threads = match;
    }

    /* update the count of threads to be joined and alert join_all() */
    twist(&(threads_lock), BY, +1);
}

/* all threads go through this routine so that just before the thread exits,
   it marks itself as done in the threads list and alerts join_all() so that
   the thread resources can be released -- use cleanup stack so that the
   marking occurs even if the thread is cancelled */
local void *ignition(void *arg)
{
    struct capsule *capsule = arg;

    /* run reenter() before leaving */
    pthread_cleanup_push(reenter, NULL);

    /* execute the requested function with argument */
    capsule->probe(capsule->payload);
    my_free(capsule);

    /* mark this thread as done and let join_all() know */
    pthread_cleanup_pop(1);

    /* exit thread */
    return NULL;
}

/* not all POSIX implementations create threads as joinable by default, so that
   is made explicit here */
thread *launch(void (*probe)(void *), void *payload)
{
    int ret;
    thread *th;
    struct capsule *capsule;
    pthread_attr_t attr;

    /* construct the requested call and argument for the ignition() routine
       (allocated instead of automatic so that we're sure this will still be
       there when ignition() actually starts up -- ignition() will free this
       allocation) */
    capsule = my_malloc(sizeof(struct capsule));
    capsule->probe = probe;
    capsule->payload = payload;

    /* assure this thread is in the list before join_all() or ignition() looks
       for it */
    possess(&(threads_lock));

    /* create the thread and call ignition() from that thread */
    th = my_malloc(sizeof(struct thread_s));
    if ((ret = pthread_attr_init(&attr)) ||
        (ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) ||
        (ret = pthread_create(&(th->id), &attr, ignition, capsule)) ||
        (ret = pthread_attr_destroy(&attr)))
        fail(ret);

    /* put the thread in the threads list for join_all() */
    th->done = 0;
    th->next = threads;
    threads = th;
    release(&(threads_lock));
    return th;
}

void join(thread *ally)
{
    int ret;
    thread *match, **prior;

    /* wait for thread to exit and return its resources */
    if ((ret = pthread_join(ally->id, NULL)) != 0)
        fail(ret);

    /* find the thread in the threads list */
    possess(&(threads_lock));
    prior = &(threads);
    while ((match = *prior) != NULL) {
        if (match == ally)
            break;
        prior = &(match->next);
    }
    if (match == NULL)
        fail(EINVAL);

    /* remove thread from list and update exited count, free thread */
    if (match->done)
        threads_lock.value--;
    *prior = match->next;
    release(&(threads_lock));
    my_free(ally);
}

/* This implementation of join_all() only attempts to join threads that have
   announced that they have exited (see ignition()).  When there are many
   threads, this is faster than waiting for some random thread to exit while a
   bunch of other threads have already exited. */
int join_all(void)
{
    int ret, count;
    thread *match, **prior;

    /* grab the threads list and initialize the joined count */
    count = 0;
    possess(&(threads_lock));

    /* do until threads list is empty */
    while (threads != NULL) {
        /* wait until at least one thread has reentered */
        wait_for(&(threads_lock), NOT_TO_BE, 0);

        /* find the first thread marked done (should be at or near the top) */
        prior = &(threads);
        while ((match = *prior) != NULL) {
            if (match->done)
                break;
            prior = &(match->next);
        }
        if (match == NULL)
            fail(EINVAL);

        /* join the thread (will be almost immediate), remove from the threads
           list, update the reenter count, and free the thread */
        if ((ret = pthread_join(match->id, NULL)) != 0)
            fail(ret);
        threads_lock.value--;
        *prior = match->next;
        my_free(match);
        count++;
    }

    /* let go of the threads list and return the number of threads joined */
    release(&(threads_lock));
    return count;
}

/* cancel and join the thread -- the thread will cancel when it gets to a file
   operation, a sleep or pause, or a condition wait */
void destruct(thread *off_course)
{
    int ret;

    if ((ret = pthread_cancel(off_course->id)) != 0)
        fail(ret);
    join(off_course);
}
