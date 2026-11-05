; Robix native LLVM IR - module 'Benchmarks\NativeVsC\CollatzBenchmark.rbx' (profile: release)
target triple = "x86_64-pc-windows-msvc"

declare void @rbx_print_i64(i64)
declare void @rbx_print_f64(double)
declare void @rbx_print_bool(i32)
declare void @rbx_print_str(ptr)

@.str.0 = private unnamed_addr constant [45 x i8] c"Robix Native Max Collatz Steps (1..2000000):\00"

define i64 @"_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is"(i64 %a0) nounwind {
entry:
  %"slot.startVal" = alloca i64
  %"slot.n" = alloca i64
  %"slot.steps" = alloca i64
  store i64 %a0, ptr %"slot.startVal"
  %v1 = load i64, ptr %"slot.startVal"
  store i64 %v1, ptr %"slot.n"
  store i64 0, ptr %"slot.steps"
  br label %while_cond_1
while_cond_1:
  %v3 = load i64, ptr %"slot.n"
  %v5 = icmp sgt i64 %v3, 1
  br i1 %v5, label %while_body_2, label %while_exit_3
while_body_2:
  %v6 = load i64, ptr %"slot.n"
  %v8 = srem i64 %v6, 2
  %v10 = icmp eq i64 %v8, 0
  br i1 %v10, label %if_then_4, label %if_else_5
while_exit_3:
  %v22 = load i64, ptr %"slot.steps"
  ret i64 %v22
if_then_4:
  %v11 = load i64, ptr %"slot.n"
  %v13 = sdiv i64 %v11, 2
  store i64 %v13, ptr %"slot.n"
  br label %if_merge_6
if_else_5:
  %v15 = load i64, ptr %"slot.n"
  %v16 = mul i64 3, %v15
  %v18 = add i64 %v16, 1
  store i64 %v18, ptr %"slot.n"
  br label %if_merge_6
if_merge_6:
  %v19 = load i64, ptr %"slot.steps"
  %v21 = add i64 %v19, 1
  store i64 %v21, ptr %"slot.steps"
  br label %while_cond_1
}

define void @"_RBX1_10Benchmarks7Collatz_0_4Main_v__v"() nounwind {
entry:
  %"slot.limit" = alloca i64
  %"slot.maxSteps" = alloca i64
  %"slot.i" = alloca i64
  %"slot.s" = alloca i64
  store i64 2000000, ptr %"slot.limit"
  store i64 0, ptr %"slot.maxSteps"
  store i64 1, ptr %"slot.i"
  br label %while_cond_1
while_cond_1:
  %v4 = load i64, ptr %"slot.i"
  %v5 = load i64, ptr %"slot.limit"
  %v6 = icmp sle i64 %v4, %v5
  br i1 %v6, label %while_body_2, label %while_exit_3
while_body_2:
  %v7 = load i64, ptr %"slot.i"
  %v8 = call i64 @"_RBX1_10Benchmarks7Collatz_0_12ComputeSteps_is__is"(i64 %v7)
  store i64 %v8, ptr %"slot.s"
  %v9 = load i64, ptr %"slot.s"
  %v10 = load i64, ptr %"slot.maxSteps"
  %v11 = icmp sgt i64 %v9, %v10
  br i1 %v11, label %if_then_4, label %if_merge_5
while_exit_3:
  call void @rbx_print_str(ptr @.str.0)
  %v17 = load i64, ptr %"slot.maxSteps"
  call void @rbx_print_i64(i64 %v17)
  ret void
if_then_4:
  %v12 = load i64, ptr %"slot.s"
  store i64 %v12, ptr %"slot.maxSteps"
  br label %if_merge_5
if_merge_5:
  %v13 = load i64, ptr %"slot.i"
  %v15 = add i64 %v13, 1
  store i64 %v15, ptr %"slot.i"
  br label %while_cond_1
}

define i32 @main() {
entry:
  call void @"_RBX1_10Benchmarks7Collatz_0_4Main_v__v"()
  ret i32 0
}
