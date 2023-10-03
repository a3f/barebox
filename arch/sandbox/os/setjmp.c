// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * sigaltstack coroutine initialization code
 *
 * Copyright (C) 2006  Anthony Liguori <anthony@codemonkey.ws>
 * Copyright (C) 2011  Kevin Wolf <kwolf@redhat.com>
 * Copyright (C) 2012  Alex Barcelo <abarcelo@ac.upc.edu>
 * Copyright (C) 2021  Ahmad Fatoum, Pengutronix
 * This file is partly based on pth_mctx.c, from the GNU Portable Threads
 *  Copyright (c) 1999-2006 Ralf S. Engelschall <rse@engelschall.com>
 */

/* XXX Is there a nicer way to disable glibc's stack check for longjmp? */
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#include <ucontext.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>

typedef sigjmp_buf _jmp_buf __attribute__((aligned((16))));
_Static_assert(sizeof(_jmp_buf) <= 512, "sigjmp_buf size exceeds expectation");

static _jmp_buf setup;

/*
 * Information for the trampoline
 */
struct tr_state {
	_jmp_buf *reenter;
	void (*entry)(void);
};

/*
 * va_args to makecontext() must be type 'int', so passing
 * the pointer we need may require several int args. This
 * union is a quick hack to let us do that
 */
union cc_arg {
    struct tr_state tr_state;
    int i[4];
};

/*
 * This is used as the signal handler. This is called with the brand new stack
 * (thanks to sigaltstack). We have to return, given that this is a signal
 * handler and the sigmask and some other things are changed.
 */
static void coroutine_trampoline(int i0, int i1, int i2, int i3)
{
	union cc_arg arg = { .i = { i0, i1, i2, i3 } };

	printf("%s:%d prep setjmp2\n", __func__, __LINE__);

	if (!sigsetjmp(*arg.tr_state.reenter, 0)) {
		printf("%s:%d co longjmp: %p\n", __func__, __LINE__,
		       setup);
		siglongjmp(setup, 1);
	}

	printf("%s:%d entry %p\n", __func__, __LINE__, arg.tr_state.entry);

	for (;;)
		arg.tr_state.entry();
}

int initjmp(_jmp_buf jmp, void (*func)(void), void *stack_top)
{

	ucontext_t old_uc, uc;
	union cc_arg arg = {};

	/* The ucontext functions preserve signal masks which incurs a
	 * system call overhead.  sigsetjmp(buf, 0)/siglongjmp() does not
	 * preserve signal masks but only works on the current stack.
	 * Since we need a way to create and switch to a new stack, use
	 * the ucontext functions for that but sigsetjmp()/siglongjmp() for
	 * everything else.
	 */

	if (getcontext(&uc) == -1)
		return -errno;

	uc.uc_link = &old_uc;

	/*
	 * Set the new stack.
	 */
	uc.uc_stack.ss_sp = stack_top - CONFIG_STACK_SIZE;
	uc.uc_stack.ss_size = CONFIG_STACK_SIZE;
	uc.uc_stack.ss_flags = 0;

	arg.tr_state.reenter = (void *)jmp;
	arg.tr_state.entry = func;

	makecontext(&uc, (void (*)(void))coroutine_trampoline,
		    4, arg.i[0], arg.i[1], arg.i[2], arg.i[3]);


	/* swapcontext() in, siglongjmp() back out */
	if (sigsetjmp(setup, 0) == 0)
		swapcontext(&old_uc, &uc);

	/*
	 * jmp can now be used to enter the trampoline again, but without
	 * having to involve swapcontext.
	 */

	return 0;
}

int  __attribute__((returns_twice)) barebox_setjmp(_jmp_buf jmp)
{
	return sigsetjmp(jmp, 0);
}

void __attribute((noreturn)) barebox_longjmp(_jmp_buf jmp, int ret)
{
	siglongjmp(jmp, ret);
}
