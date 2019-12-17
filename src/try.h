/* try.h -- try / catch / throw exception handling for C99
  Copyright (C) 2013, 2015 Mark Adler
  Version 1.2  19 January 2015

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */

/*
    Version History
    1.0    7 Jan 2013   - First version
    1.1    2 Nov 2013   - Use variadic macros and functions instead of partial
                          structure assignment, allowing arbitrary arguments
                          to printf()
    1.2   19 Jan 2015   - Obey setjmp() invocation limits from C standard
 */

/* To use, include try.h in all source files that use these operations, and
   compile and link try.c.  If pthread threads are used, then there must be an
   #include <pthread.h> in try.h to make the exception handling thread-safe.
   (Uncomment the include below.)  If threads other than pthread are being
   used, then try.h and try.c must be modified to use that environment's
   thread-local storage for the try_stack_ pointer.  try.h and try.c assume
   that the compiler and library conform to the C99 standard, at least with
   respect to the use of variadic macro and function arguments. */

/*
   try.h provides a try / catch / throw exception handler, which allows
   catching exceptions across any number of levels of function calls.  try
   blocks can be nested as desired, with a throw going to the end of the
   innermost enclosing try, passing the thrown information to the associated
   catch block.  A global try stack is used, to avoid having to pass exception
   handler information through all of the functions down to the invocations of
   throw.  The try stack is thread-unique if requested by uncommenting the
   pthread.h include below.  In addition to the macros try, catch, and throw,
   the macros always, retry, punt, and drop, and the type ball_t are created.
   All other symbols are of the form try_*_ or TRY_*_, where the final
   underscore should avoid conflicts with application symbols. The eight
   exposed names can be changed easily in #defines below.

   A try block encloses code that may throw an exception with the throw()
   macro, either directly in the try block or in any function called directly
   or indirectly from the try block.  throw() must have at least one argument,
   which is an integer.  The try block is followed by a catch block whose code
   will be executed when throw() is called with a non-zero first argument.  If
   the first argument of throw() is zero, then execution continues after the
   catch block.  If the try block completes normally, with no throw() being
   called, then execution continues normally after the catch block.

   There can be only one catch block.  catch has one argument which must be a
   ball_t type variable declared in the current function or block containing
   the try and catch.  That variable is loaded with the information sent by the
   throw() for use in the catch block.

   throw() may optionally include more information that is passed to the catch
   block in the ball_t structure.  throw() can have one or more arguments,
   where the first (possibly only) argument is an integer code.  The second
   argument can be a pointer, which will be replaced by NULL in the ball_t
   structure if not provided.  The implementation of throw() in try.c assumes
   that if the second argument is present and is not NULL, that it is a string.
   If that string has any percent (%) signs in it, then throw() will run that
   string through vsnprintf() with any other arguments provided after the
   string in the throw() invocation, and save the resulting formatted string in
   the ball_t structure.  Information on whether or not the string was
   allocated is also maintained in the ball_t structure.

   throw() in try.c can be modified to not assume that the second argument is a
   string.  For example, an application may want to assume instead that the
   second argument is a pointer to a set of information for use in the catch
   block.

   The catch block may conditionally do a punt(), where the argument of punt()
   is the argument of catch.  This passes the exception on to the next
   enclosing try/catch handler.

   If a catch block does not always end with a punt(), it should contain a
   drop(), where the argument of drop() is the argument of catch.  This frees
   the allocated string made if vsnprintf() was used by throw() to generate the
   string.  If printf() format strings are never used, then drop() is not
   required.

   An always block may be placed between the try and catch block.  The
   statements in that block will be executed regardless of whether or not the
   try block completed normally.  As indicated by the ordering, the always
   block will be executed before the catch block.  This block is not named
   "finally", since it is different from the finally block in other languages
   which is executed after the catch block.

   A naked break or continue in a try or always block will go directly to the
   end of that block.

   A retry from the try block or from any function called from the try block at
   any level of nesting will restart the try block from the beginning.

   try is thread-safe when compiled with pthread.h.  A throw() in a thread can
   only be caught in the same thread.  If a throw() is attempted from a thread
   without an enclosing try in that thread, even if in another thread there is
   a try around the pthread_create() that spawned this thread, then the throw
   will fail on an assert.  Each thread has its own thread-unique try stack,
   which starts off empty.

   If an intermediate function does not have a need for operations in a catch
   block other than punt, and does not need an always block, then that function
   does not need a try block.  "try { block } catch (err) { punt(err); }" is
   the same as just "block".  More precisely, it's equivalent to "do { block }
   while (0);", which replicates the behavior of a naked break or continue in a
   block when it follows try.  throw() can be used from a function that has no
   try.  All that is necessary is that there is a try somewhere up the function
   chain that called the current function in the current thread.

   There must not be a return in any try block, nor a goto in any try block
   that leaves that block.  The always block does not catch a return from the
   try block.  There is no check or protection for an improper use of return or
   goto.  It is up to the user to assure that this doesn't happen. If it does
   happen, then the reference to the current try block is left on the try
   stack, and the next throw which is supposed to go to an enclosing try would
   instead go to this try, possibly after the enclosing function has returned.
   Mayhem will then ensue.  This may be caught by the longjmp() implementation,
   which would report "longjmp botch" and then abort.

   Any automatic storage variables that are modified in the try block and used
   in the catch or always block must be declared volatile.  Otherwise their
   value in the catch or always block is indeterminate.

   Any statements between try and always, between try and catch if there is no
   always, or between always and catch are part of those respective try or
   always blocks.  Use of { } to enclose those blocks is optional, but { }
   should be used anyway for clarity, style, and to inform smart source editors
   that the enclosed code is to be indented.  Enclosing the catch block with {
   } is not optional if there is more than one statement in the block.
   However, even if there is just one statement in the catch block, it should
   be enclosed in { } anyway for style and editing convenience.

   The contents of the ball_t structure after the first element (int code) can
   be customized for the application.  If ball_t is customized, then the code
   in try.c should be updated accordingly.  If there is no memory allocation in
   throw(), then drop() can be eliminated.

   Example usage:

    ball_t err;
    volatile char *temp = NULL;
    try {
        ... do something ...
        if (ret == -1)
            throw(1, "bad thing happened to %s\n", me);
        temp = malloc(sizeof(me) + 1);
        if (temp == NULL)
            throw(2, "out of memory");
        ... do more ...
        if (ret == -1)
            throw(3, "worse thing happened to %s\n", temp);
        ... some more code ...
    }
    always {
        free(temp);
    }
    catch (err) {
        fputs(err.why, stderr);
        drop(err);
        return err.code;
    }
    ... end up here if nothing bad happened ...


   More involved example:

    void check_part(void)
    {
        ball_t err;

        try {
            ...
            if (part == bad1)
                throw(1);
            ...
            if (part == bad2)
                throw(1);
            ...
        }
        catch (err) {
            drop(err);
            throw(3, "part was bad");
        }
    }

    void check_input(void)
    {
        ...
        if (input == wrong)
            throw(4, "input was wrong");
        ...
        if (input == stupid)
            throw(5, "input was stupid");
        ...
        check_part();
        ...
    }

    void *build_something(void)
    {
        ball_t err;
        volatile void *thing;
        try {
            thing = malloc(sizeof(struct thing));
            ... build up thing ...
            check_input();
            ... finish building it ...
        }
        catch (err) {
            free(thing);
            punt(err);
        }
        return thing;
    }

    int grand_central(void)
    {
        ball_t err;
        void *thing;
        try {
            thing = build_something();
        }
        catch (err) {
            fputs(err.why, stderr);
            drop(err);
            return err.code;
        }
        ... use thing ...
        free(thing);
        return 0;
    }

 */

#ifndef _TRY_H
#define _TRY_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

/* If pthreads are used, uncomment this include to make try thread-safe. */
#ifndef NOTHREAD
#  include <pthread.h>
#endif

/* The exposed names can be changed here. */
#define ball_t try_ball_t_
#define try TRY_TRY_
#define always TRY_ALWAYS_
#define catch TRY_CATCH_
#define throw TRY_THROW_
#define retry TRY_RETRY_
#define punt TRY_PUNT_
#define drop TRY_DROP_

/* Package of an integer code and any other data to be thrown and caught. Here,
   why is a string with information to be displayed to indicate why an
   exception was thrown.  free is true if why was allocated and should be freed
   when no longer needed.  This structure can be customized as needed, but it
   must start with an int code.  If it is customized, the try_throw_() function
   in try.c must also be updated accordingly.  As an example, why could be a
   structure with information for use in the catch block. */
typedef struct {
    int code;           /* integer code (required) */
    int free;           /* if true, the message string was allocated */
    char *why;          /* informational string or NULL */
} try_ball_t_;

/* Element in the global try stack (a linked list). */
typedef struct try_s_ try_t_;
struct try_s_ {
    jmp_buf env;        /* state information for longjmp() to jump back */
    try_ball_t_ ball;   /* data passed from the throw() */
    try_t_ *next;       /* link to the next enclosing try_t, or NULL */
};

/* Global try stack.  try.c must be compiled and linked to provide the stack
   pointer.  Use thread-local storage if pthread.h is included before this.
   Note that a throw can only be caught within the same thread.  A new and
   unique try stack is created for each thread, so any attempt to throw across
   threads will fail with an assert, by virtue of reaching the end of the
   stack. */
#ifdef PTHREAD_ONCE_INIT
    extern pthread_key_t try_key_;
    void try_setup_(void);
#   define try_stack_ ((try_t_ *)pthread_getspecific(try_key_))
#   define try_stack_set_(next) \
        do { \
            int try_ret_ = pthread_setspecific(try_key_, next); \
            assert(try_ret_ == 0 && "try: pthread_setspecific() failed"); \
        } while (0)
#else /* !PTHREAD_ONCE_INIT */
    extern try_t_ *try_stack_;
#   define try_setup_()
#   define try_stack_set_(next) try_stack_ = (next)
#endif /* PTHREAD_ONCE_INIT */

/* Try a block.  The block should follow the invocation of try enclosed in { }.
   The block must be immediately followed by an always or a catch.  You must
   not goto or return out of the try block.  A naked break or continue in the
   try block will go to the end of the block. */
#define TRY_TRY_ \
    do { \
        try_t_ try_this_; \
        int try_pushed_ = 1; \
        try_this_.ball.code = 0; \
        try_this_.ball.free = 0; \
        try_this_.ball.why = NULL; \
        try_setup_(); \
        try_this_.next = try_stack_; \
        try_stack_set_(&try_this_); \
        if (setjmp(try_this_.env) < 2) \
            do { \

/* Execute the code between always and catch, whether or not something was
   thrown.  An always block is optional.  If present, the always block must
   follow a try block and be followed by a catch block.  The always block
   should be enclosed in { }.  A naked break or continue in the always block
   will go to the end of the block.  It is permitted to use throw in the always
   block, which will fall up to the next enclosing try.  However this will
   result in a memory leak if the original throw() allocated space for the
   informational string.  So it's best to not throw() in an always block.  Keep
   the always block simple.

   Great care must be taken if the always block uses an automatic storage
   variable local to the enclosing function that can be modified in the try
   block.  Such variables must be declared volatile.  If such a variable is not
   declared volatile, and if the compiler elects to keep that variable in a
   register, then the throw will restore that variable to its state at the
   beginning of the try block, wiping out any change that occurred in the try
   block.  This can cause very confusing bugs until you remember that you
   didn't follow this rule. */
#define TRY_ALWAYS_ \
            } while (0); \
        if (try_pushed_) { \
            try_stack_set_(try_this_.next); \
            try_pushed_ = 0; \
        } \
            do {

/* Catch an error thrown in the preceding try block.  The catch block must
   follow catch and its parameter, and must be enclosed in { }.  The catch must
   immediately follow the try or always block.  It is permitted to use throw()
   in the catch block, which will fall up to the next enclosing try.  However
   the ball_t passed by throw() must be freed using drop() before doing another
   throw, to avoid a potential memory leak. The parameter of catch must be a
   ball_t declared in the function or block containing the catch.  It is set to
   the parameters of the throw() that jumped to the catch.  The catch block is
   not executed if the first parameter of the throw() was zero.

   A catch block should end with either a punt() or a drop().

   Great care must be taken if the catch block uses an automatic storage
   variable local to the enclosing function that can be modified in the try
   block.  Such variables must be declared volatile.  If such a variable is not
   declared volatile, and if the compiler elects to keep that variable in a
   register, then the throw will restore that variable to its state at the
   beginning of the try block, wiping out any change that occurred in the try
   block.  This can cause very confusing bugs until you remember that you
   didn't follow this rule. */
#define TRY_CATCH_(try_ball_) \
            } while (0); \
        if (try_pushed_) { \
            try_stack_set_(try_this_.next); \
            try_pushed_ = 0; \
        } \
        try_ball_ = try_this_.ball; \
    } while (0); \
    if (try_ball_.code)

/* Throw an error.  This can be in the try block or in any function called from
   the try block, at any level of nesting.  This will fall back to the end of
   the first enclosing try block in the same thread, invoking the associated
   catch block with a ball_t set to the arguments of throw().  throw() will
   abort the program with an assert() if there is no nesting try.  Make sure
   that there's a nesting try!

   try may have one or more arguments, where the first argument is an int, the
   optional second argument is a string, and the remaining optional arguments
   are referred to by printf() formatting commands in the string.  If there are
   formatting commands in the string, i.e. any percent (%) signs, then
   vsnprintf() is used to generate the formatted string from the arguments
   before jumping to the enclosing try block.  This allows throw() to use
   information on the stack in the scope of the throw() statement, which will
   be lost after jumping back to the enclosing try block.  That formatted
   string will use allocated memory, which is why it is important to use drop()
   in catch blocks to free that memory, or punt() to pass the string on to
   another catch block.  Eventually some catch block down the chain will have
   to drop() it.

   If a memory allocation fails during the execution of a throw(), then the
   string provided to the catch block is not the formatted string at all, but
   rather the string: "try: out of memory", with the integer code from the
   throw() unchanged.

   If the first argument of throw is zero, then the catch block is not
   executed.  A throw(0) from a function called in the try block is equivalent
   to a break or continue in the try block.  A throw(0) should not have any
   other arguments, to avoid a potential memory leak.  There is no opportunity
   to make use of any arguments after the 0 anyway.

   try.c must be compiled and linked to provide the try_throw_() function. */
void try_throw_(int code, char *fmt, ...);
#define TRY_THROW_(...) try_throw_(__VA_ARGS__, NULL)

/* Retry the try block.  This will start over at the beginning of the try
   block.  This can be used in the try block or in any function called from the
   try block at any level of nesting, just like throw.  retry has no argument.
   If there is a retry in the always or catch block, then it will retry the
   next enclosing try, not the immediately preceding try.

   If you use this, make sure you have carefully thought through how it will
   work.  It can be tricky to correctly rerun a chunk of code that has been
   partially executed.  Especially if there are different degrees of progress
   that could have been made.  Also note that automatic variables changed in
   the try block and not declared volatile will have indeterminate values.

   We use 1 here instead of 0, since some implementations prevent returning a
   zero value from longjmp() to setjmp(). */
#define TRY_RETRY_ \
    do { \
        try_setup_(); \
        assert(try_stack_ != NULL && "try: naked retry"); \
        longjmp(try_stack_->env, 1); \
    } while (0)

/* Punt a caught error on to the next enclosing catcher.  This is normally used
   in a catch block with same argument as the catch. */
#define TRY_PUNT_(try_ball_) \
    do { \
        try_setup_(); \
        assert(try_stack_ != NULL && "try: naked punt"); \
        try_stack_->ball = try_ball_; \
        longjmp(try_stack_->env, 2); \
    } while (0)

/* Clean up at the end of the line in a catch (no more punts). */
#define TRY_DROP_(try_ball_) \
    do { \
        if (try_ball_.free) { \
            free(try_ball_.why); \
            try_ball_.free = 0; \
            try_ball_.why = NULL; \
        } \
    } while (0)

#endif /* _TRY_H */
