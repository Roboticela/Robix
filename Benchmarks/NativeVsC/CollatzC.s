	.text
	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"CollatzBenchmark.c"
	.def	main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
.seh_proc main
# %bb.0:
	pushq	%rsi
	.seh_pushreg %rsi
	subq	$32, %rsp
	.seh_stackalloc 32
	.seh_endprologue
	movl	$1, %eax
	xorl	%esi, %esi
	jmp	.LBB0_1
	.p2align	4, 0x90
.LBB0_4:                                #   in Loop: Header=BB0_1 Depth=1
	cmpq	%rsi, %rcx
	cmovgq	%rcx, %rsi
	incq	%rax
	cmpq	$2000001, %rax                  # imm = 0x1E8481
	je	.LBB0_5
.LBB0_1:                                # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_3 Depth 2
	xorl	%ecx, %ecx
	cmpq	$2, %rax
	jb	.LBB0_4
# %bb.2:                                #   in Loop: Header=BB0_1 Depth=1
	movq	%rax, %r8
	movq	%rax, %rdx
	.p2align	4, 0x90
.LBB0_3:                                #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leaq	(%r8,%r8,2), %r9
	incq	%r9
	shrq	%rdx
	testb	$1, %r8b
	cmovneq	%r9, %rdx
	incq	%rcx
	movq	%rdx, %r8
	cmpq	$1, %rdx
	ja	.LBB0_3
	jmp	.LBB0_4
.LBB0_5:
	leaq	.Lstr(%rip), %rcx
	callq	puts
	leaq	"??_C@_05IBFPJDFI@?$CFlld?6?$AA@"(%rip), %rcx
	movq	%rsi, %rdx
	callq	printf
	xorl	%eax, %eax
	addq	$32, %rsp
	popq	%rsi
	retq
	.seh_endproc
                                        # -- End function
	.def	printf;
	.scl	2;
	.type	32;
	.endef
	.section	.text,"xr",discard,printf
	.globl	printf                          # -- Begin function printf
	.p2align	4, 0x90
printf:                                 # @printf
.seh_proc printf
# %bb.0:
	pushq	%rsi
	.seh_pushreg %rsi
	pushq	%rdi
	.seh_pushreg %rdi
	pushq	%rbx
	.seh_pushreg %rbx
	subq	$48, %rsp
	.seh_stackalloc 48
	.seh_endprologue
	movq	%rcx, %rsi
	movq	%rdx, 88(%rsp)
	movq	%r8, 96(%rsp)
	movq	%r9, 104(%rsp)
	leaq	88(%rsp), %rbx
	movq	%rbx, 40(%rsp)
	movl	$1, %ecx
	callq	__acrt_iob_func
	movq	%rax, %rdi
	callq	__local_stdio_printf_options
	movq	(%rax), %rcx
	movq	%rbx, 32(%rsp)
	movq	%rdi, %rdx
	movq	%rsi, %r8
	xorl	%r9d, %r9d
	callq	__stdio_common_vfprintf
	nop
	addq	$48, %rsp
	popq	%rbx
	popq	%rdi
	popq	%rsi
	retq
	.seh_endproc
                                        # -- End function
	.def	__local_stdio_printf_options;
	.scl	2;
	.type	32;
	.endef
	.section	.text,"xr",discard,__local_stdio_printf_options
	.globl	__local_stdio_printf_options    # -- Begin function __local_stdio_printf_options
	.p2align	4, 0x90
__local_stdio_printf_options:           # @__local_stdio_printf_options
# %bb.0:
	leaq	__local_stdio_printf_options._OptionsStorage(%rip), %rax
	retq
                                        # -- End function
	.section	.rdata,"dr",discard,"??_C@_05IBFPJDFI@?$CFlld?6?$AA@"
	.globl	"??_C@_05IBFPJDFI@?$CFlld?6?$AA@" # @"??_C@_05IBFPJDFI@?$CFlld?6?$AA@"
"??_C@_05IBFPJDFI@?$CFlld?6?$AA@":
	.asciz	"%lld\n"

	.lcomm	__local_stdio_printf_options._OptionsStorage,8,8 # @__local_stdio_printf_options._OptionsStorage
	.section	.rdata,"dr"
.Lstr:                                  # @str
	.asciz	"C Native Max Collatz Steps (1..2000000):"

	.addrsig
	.addrsig_sym __local_stdio_printf_options._OptionsStorage
