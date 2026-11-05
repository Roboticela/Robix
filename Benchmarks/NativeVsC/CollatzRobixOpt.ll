; ModuleID = 'Benchmarks\NativeVsC\CollatzRobix.ll'
source_filename = "Benchmarks\\NativeVsC\\CollatzRobix.ll"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-windows-msvc19.43.34809"

@.str.0 = private unnamed_addr constant [45 x i8] c"Robix Native Max Collatz Steps (1..2000000):\00"

declare void @rbx_print_i64(i64) local_unnamed_addr

declare void @rbx_print_str(ptr) local_unnamed_addr

; Function Attrs: nofree norecurse nosync nounwind memory(none)
define i64 @_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is(i64 %a0) local_unnamed_addr #0 {
entry:
  %v56 = icmp sgt i64 %a0, 1
  br i1 %v56, label %while_body_2, label %while_exit_3

while_body_2:                                     ; preds = %entry, %while_body_2
  %slot.steps.08 = phi i64 [ %v21, %while_body_2 ], [ 0, %entry ]
  %slot.n.07 = phi i64 [ %slot.n.1, %while_body_2 ], [ %a0, %entry ]
  %v8 = and i64 %slot.n.07, 1
  %v10 = icmp eq i64 %v8, 0
  %v135 = lshr exact i64 %slot.n.07, 1
  %v16 = mul i64 %slot.n.07, 3
  %v18 = add i64 %v16, 1
  %slot.n.1 = select i1 %v10, i64 %v135, i64 %v18
  %v21 = add i64 %slot.steps.08, 1
  %v5 = icmp sgt i64 %slot.n.1, 1
  br i1 %v5, label %while_body_2, label %while_exit_3

while_exit_3:                                     ; preds = %while_body_2, %entry
  %slot.steps.0.lcssa = phi i64 [ 0, %entry ], [ %v21, %while_body_2 ]
  ret i64 %slot.steps.0.lcssa
}

; Function Attrs: nounwind
define void @_RBX1_10Benchmarks7Collatz_0_4Main_v__v() local_unnamed_addr #1 {
entry:
  br label %while_body_2

while_body_2:                                     ; preds = %entry, %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit
  %slot.maxSteps.06 = phi i64 [ 0, %entry ], [ %spec.select, %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit ]
  %slot.i.05 = phi i64 [ 1, %entry ], [ %v15, %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit ]
  %v56.i = icmp ugt i64 %slot.i.05, 1
  br i1 %v56.i, label %while_body_2.i, label %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit

while_body_2.i:                                   ; preds = %while_body_2, %while_body_2.i
  %slot.steps.08.i = phi i64 [ %v21.i, %while_body_2.i ], [ 0, %while_body_2 ]
  %slot.n.07.i = phi i64 [ %slot.n.1.i, %while_body_2.i ], [ %slot.i.05, %while_body_2 ]
  %v8.i = and i64 %slot.n.07.i, 1
  %v10.i = icmp eq i64 %v8.i, 0
  %v135.i = lshr exact i64 %slot.n.07.i, 1
  %v16.i = mul i64 %slot.n.07.i, 3
  %v18.i = add i64 %v16.i, 1
  %slot.n.1.i = select i1 %v10.i, i64 %v135.i, i64 %v18.i
  %v21.i = add i64 %slot.steps.08.i, 1
  %v5.i = icmp sgt i64 %slot.n.1.i, 1
  br i1 %v5.i, label %while_body_2.i, label %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit

_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit: ; preds = %while_body_2.i, %while_body_2
  %slot.steps.0.lcssa.i = phi i64 [ 0, %while_body_2 ], [ %v21.i, %while_body_2.i ]
  %spec.select = tail call i64 @llvm.smax.i64(i64 %slot.steps.0.lcssa.i, i64 %slot.maxSteps.06)
  %v15 = add nuw nsw i64 %slot.i.05, 1
  %exitcond.not = icmp eq i64 %v15, 2000001
  br i1 %exitcond.not, label %while_exit_3, label %while_body_2

while_exit_3:                                     ; preds = %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit
  tail call void @rbx_print_str(ptr nonnull @.str.0) #1
  tail call void @rbx_print_i64(i64 %spec.select) #1
  ret void
}

; Function Attrs: nounwind
define noundef i32 @main() local_unnamed_addr #1 {
entry:
  br label %while_body_2.i

while_body_2.i:                                   ; preds = %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i, %entry
  %slot.maxSteps.06.i = phi i64 [ 0, %entry ], [ %spec.select.i, %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i ]
  %slot.i.05.i = phi i64 [ 1, %entry ], [ %v15.i, %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i ]
  %v56.i.i = icmp ugt i64 %slot.i.05.i, 1
  br i1 %v56.i.i, label %while_body_2.i.i, label %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i

while_body_2.i.i:                                 ; preds = %while_body_2.i, %while_body_2.i.i
  %slot.steps.08.i.i = phi i64 [ %v21.i.i, %while_body_2.i.i ], [ 0, %while_body_2.i ]
  %slot.n.07.i.i = phi i64 [ %slot.n.1.i.i, %while_body_2.i.i ], [ %slot.i.05.i, %while_body_2.i ]
  %v8.i.i = and i64 %slot.n.07.i.i, 1
  %v10.i.i = icmp eq i64 %v8.i.i, 0
  %v135.i.i = lshr exact i64 %slot.n.07.i.i, 1
  %v16.i.i = mul i64 %slot.n.07.i.i, 3
  %v18.i.i = add i64 %v16.i.i, 1
  %slot.n.1.i.i = select i1 %v10.i.i, i64 %v135.i.i, i64 %v18.i.i
  %v21.i.i = add i64 %slot.steps.08.i.i, 1
  %v5.i.i = icmp sgt i64 %slot.n.1.i.i, 1
  br i1 %v5.i.i, label %while_body_2.i.i, label %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i

_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i: ; preds = %while_body_2.i.i, %while_body_2.i
  %slot.steps.0.lcssa.i.i = phi i64 [ 0, %while_body_2.i ], [ %v21.i.i, %while_body_2.i.i ]
  %spec.select.i = tail call i64 @llvm.smax.i64(i64 %slot.steps.0.lcssa.i.i, i64 %slot.maxSteps.06.i)
  %v15.i = add nuw nsw i64 %slot.i.05.i, 1
  %exitcond.not.i = icmp eq i64 %v15.i, 2000001
  br i1 %exitcond.not.i, label %_RBX1_10Benchmarks7Collatz_0_4Main_v__v.exit, label %while_body_2.i

_RBX1_10Benchmarks7Collatz_0_4Main_v__v.exit:     ; preds = %_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is.exit.i
  tail call void @rbx_print_str(ptr nonnull @.str.0) #1
  tail call void @rbx_print_i64(i64 %spec.select.i) #1
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i64 @llvm.smax.i64(i64, i64) #2

attributes #0 = { nofree norecurse nosync nounwind memory(none) }
attributes #1 = { nounwind }
attributes #2 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
