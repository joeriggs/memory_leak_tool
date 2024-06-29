	.file	"breakpoint_return.c"
	.text
	.globl	func_retaddr
	.section	.tbss,"awT",@nobits
	.align 8
	.type	func_retaddr, @object
	.size	func_retaddr, 8
func_retaddr:
	.zero	8
	.globl	curr_ctx
	.align 8
	.type	curr_ctx, @object
	.size	curr_ctx, 8
curr_ctx:
	.zero	8
	.text
	.globl	breakpoint_return
	.type	breakpoint_return, @function
breakpoint_return:
.LFB0:
	.cfi_startproc
	pushq	%rax
	endbr64
	movq	%rax, %rdx
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movq	%fs:func_retaddr@tpoff, %rax
	movq	%rax, -8(%rbp)
	movq	%fs:curr_ctx@tpoff, %rax
	testq	%rax, %rax
	je	.L3
	movq	%fs:curr_ctx@tpoff, %rax
	movq	32(%rax), %rax
	testq	%rax, %rax
	je	.L3
	movq	%fs:curr_ctx@tpoff, %rax
	movq	32(%rax), %rax
	movq	%rdx, %rdi
	call	*%rax

	leave
	popq	%rax
	movq	%fs:func_retaddr@tpoff, %rbx
	push	%rbx
	ret

.L3:
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	breakpoint_return, .-breakpoint_return
	.ident	"GCC: (Ubuntu 9.4.0-1ubuntu1~20.04.2) 9.4.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	 1f - 0f
	.long	 4f - 1f
	.long	 5
0:
	.string	 "GNU"
1:
	.align 8
	.long	 0xc0000002
	.long	 3f - 2f
2:
	.long	 0x3
3:
	.align 8
4:
