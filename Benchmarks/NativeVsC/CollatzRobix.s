	.text
	.def	@feat.00;
	.scl	3;
	.type	0;
	.endef
	.globl	@feat.00
.set @feat.00, 0
	.file	"CollatzRobix.ll"
	.def	_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is;
	.scl	2;
	.type	32;
	.endef
	.globl	_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is # -- Begin function _RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is
	.p2align	4, 0x90
_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is: # @_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is
# %bb.0:                                # %entry
	xorl	%eax, %eax
	cmpq	$2, %rcx
	jl	.LBB0_3
# %bb.1:                                # %while_body_2.preheader
	movq	%rcx, %rdx
	.p2align	4, 0x90
.LBB0_2:                                # %while_body_2
                                        # =>This Inner Loop Header: Depth=1
	leaq	(%rcx,%rcx,2), %r8
	incq	%r8
	shrq	%rdx
	testb	$1, %cl
	cmovneq	%r8, %rdx
	incq	%rax
	movq	%rdx, %rcx
	cmpq	$1, %rdx
	jg	.LBB0_2
.LBB0_3:                                # %while_exit_3
	retq
                                        # -- End function
	.def	_RBX1_10Benchmarks7Collatz_0_4Main_v__v;
	.scl	2;
	.type	32;
	.endef
	.globl	_RBX1_10Benchmarks7Collatz_0_4Main_v__v # -- Begin function _RBX1_10Benchmarks7Collatz_0_4Main_v__v
	.p2align	4, 0x90
_RBX1_10Benchmarks7Collatz_0_4Main_v__v: # @_RBX1_10Benchmarks7Collatz_0_4Main_v__v
# %bb.0:                                # %entry
	pushq	%rsi
	subq	$32, %rsp
	movl	$1, %eax
	xorl	%esi, %esi
	jmp	.LBB1_1
	.p2align	4, 0x90
.LBB1_4:                                # %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit
                                        #   in Loop: Header=BB1_1 Depth=1
	cmpq	%rsi, %rcx
	cmovgq	%rcx, %rsi
	incq	%rax
	cmpq	$2000001, %rax                  # imm = 0x1E8481
	je	.LBB1_5
.LBB1_1:                                # %while_body_2
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB1_3 Depth 2
	xorl	%ecx, %ecx
	cmpq	$2, %rax
	jb	.LBB1_4
# %bb.2:                                # %while_body_2.i.preheader
                                        #   in Loop: Header=BB1_1 Depth=1
	movq	%rax, %r8
	movq	%rax, %rdx
	.p2align	4, 0x90
.LBB1_3:                                # %while_body_2.i
                                        #   Parent Loop BB1_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leaq	(%r8,%r8,2), %r9
	incq	%r9
	shrq	%rdx
	testb	$1, %r8b
	cmovneq	%r9, %rdx
	incq	%rcx
	movq	%rdx, %r8
	cmpq	$1, %rdx
	jg	.LBB1_3
	jmp	.LBB1_4
.LBB1_5:                                # %while_exit_3
	leaq	.L.str.0(%rip), %rcx
	callq	rbx_print_str
	movq	%rsi, %rcx
	addq	$32, %rsp
	popq	%rsi
	jmp	rbx_print_i64                   # TAILCALL
                                        # -- End function
	.def	main;
	.scl	2;
	.type	32;
	.endef
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
main:                                   # @main
# %bb.0:                                # %entry
	pushq	%rsi
	subq	$32, %rsp
	movl	$1, %eax
	xorl	%esi, %esi
	jmp	.LBB2_1
	.p2align	4, 0x90
.LBB2_4:                                # %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i
                                        #   in Loop: Header=BB2_1 Depth=1
	cmpq	%rsi, %rcx
	cmovgq	%rcx, %rsi
	incq	%rax
	cmpq	$2000001, %rax                  # imm = 0x1E8481
	je	.LBB2_5
.LBB2_1:                                # %while_body_2.i
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB2_3 Depth 2
	xorl	%ecx, %ecx
	cmpq	$2, %rax
	jb	.LBB2_4
# %bb.2:                                # %while_body_2.i.i.preheader
                                        #   in Loop: Header=BB2_1 Depth=1
	movq	%rax, %r8
	movq	%rax, %rdx
	.p2align	4, 0x90
.LBB2_3:                                # %while_body_2.i.i
                                        #   Parent Loop BB2_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	leaq	(%r8,%r8,2), %r9
	incq	%r9
	shrq	%rdx
	testb	$1, %r8b
	cmovneq	%r9, %rdx
	incq	%rcx
	movq	%rdx, %r8
	cmpq	$1, %rdx
	jg	.LBB2_3
	jmp	.LBB2_4
.LBB2_5:                                # %_RBX1_10Benchmarks7Collatz_0_4Main_v__v.exit
	leaq	.L.str.0(%rip), %rcx
	callq	rbx_print_str
	movq	%rsi, %rcx
	callq	rbx_print_i64
	xorl	%eax, %eax
	addq	$32, %rsp
	popq	%rsi
	retq
                                        # -- End function
	.section	.rdata,"dr"
	.p2align	4, 0x0                          # @.str.0
.L.str.0:
	.asciz	"Robix Native Max Collatz Steps (1..2000000):"

	.addrsig
