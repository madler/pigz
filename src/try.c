/* try.c -- try / catch / throw exception handling for C99
 * Copyright (C) 2013, 2015 Mark Adler
 * Version 1.2  19 January 2015
 * For conditions of distribution and use, see copyright notice in try.h
 */

/* See try.h for documentation.  This source file provides the global pointer
   and the functions needed by throw().  The pointer is thread-unique if
   pthread.h is included in try.h. */

#include "try.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* Set up the try stack with a global pointer to the next try block.  The
   global is thread-unique if pthread.h is included in try.h. */
#ifdef PTHREAD_ONCE_INIT
    pthread_key_t try_key_;
    static pthread_once_t try_once_ = PTHREAD_ONCE_INIT;
    static void try_create_(void)
    {
        int ret = pthread_key_create(&try_key_, NULL);
        assert(ret == 0 && "try: pthread_key_create() failed");
    }
    void try_setup_(void)
    {
        int ret = pthread_once(&try_once_, try_create_);
        assert(ret == 0 && "try: pthread_once() failed");
    }
#else /* !PTHREAD_ONCE_INIT */
    try_t_ *try_stack_ = NULL;
#endif /* PTHREAD_ONCE_INIT */

/* Throw an exception.  This must always have at least two arguments, where the
   second argument can be a NULL.  The throw() macro is permitted to have one
   argument, since it appends a NULL argument in the call to this function. */
void try_throw_(int code, char *fmt, ...)
{
    /* save the thrown information in the try stack before jumping */
    try_setup_();
    assert(try_stack_ != NULL && "try: naked throw");
    try_stack_->ball.code = code;
    try_stack_->ball.free = 0;
    try_stack_->ball.why = fmt;

    /* consider the second argument to be a string, and if it has formatting
       commands, process them with the subsequent arguments of throw, saving
       the result in allocated memory -- this if statement and clause must be
       updated for a different interpretation of the throw() arguments and
       different contents of the ball_t structure */
    if (fmt != NULL && strchr(fmt, '%') != NULL) {
        char *why, nul[1];
        size_t len;
        va_list ap1, ap2;

        va_start(ap1, fmt);
        va_copy(ap2, ap1);
        len = vsnprintf(nul, 1, fmt, ap1);
        va_end(ap1);
        why = malloc(len + 1);
        if (why == NULL)
            try_stack_->ball.why = "try: out of memory";
        else {
            vsnprintf(why, len + 1, fmt, ap2);
            va_end(ap2);
            try_stack_->ball.free = 1;
            try_stack_->ball.why = why;
        }
    }

    /* jump to the end of the nearest enclosing try block */
    longjmp(try_stack_->env, 2);
}
