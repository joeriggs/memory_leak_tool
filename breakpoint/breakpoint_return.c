
/* *****************************************************************************
 * This file is NOT built into the the breakpoint executable.  Refer to the
 * README file for an explanation.
 * ************************************************************************** */

#include "breakpoint.h"
#include "breakpoint_internal.h"

__thread unsigned long func_retaddr;

__thread breakpoint_context *curr_ctx;

/* *****************************************************************************
 * This function intercepts the return from the breakpoint function.  It can do
 * any processing that it wants, and then it returns back to the original
 * caller.
 *
 * It needs to perform something similar to the following:
 *
 * breakpoint_return:
 * .LFB0:
 * 	.cfi_startproc
 * 	pushq	%rax
 * 	endbr64
 * 	movq	%rax, %rdx
 * 	pushq	%rbp
 * 	.cfi_def_cfa_offset 16
 * 	.cfi_offset 6, -16
 * 	movq	%rsp, %rbp
 * 	.cfi_def_cfa_register 6
 * 	subq	$16, %rsp
 * 	movq	%fs:func_retaddr@tpoff, %rax
 * 	movq	%rax, -8(%rbp)
 * 	movq	%fs:curr_ctx@tpoff, %rax
 * 	testq	%rax, %rax
 * 	je	.L3
 * 	movq	%fs:curr_ctx@tpoff, %rax
 * 	movq	32(%rax), %rax
 * 	testq	%rax, %rax
 * 	je	.L3
 * 	movq	%fs:curr_ctx@tpoff, %rax
 * 	movq	32(%rax), %rax
 * 	movq	%rdx, %rdi
 * 	call	*%rax
 * 
 * 	leave
 * 	popq	%rax
 * 	movq	%fs:func_retaddr@tpoff, %rbx
 * 	push	%rbx
 * 	ret
 *
 * ************************************************************************** */

void breakpoint_return(void)
{
	unsigned long m = func_retaddr;

	if (curr_ctx) {
		if (curr_ctx->post_cb) {
			curr_ctx->post_cb(func_retaddr);
		}
	}
}

