declare i64 @Text_Length(...)
declare i64 @Text_Equals(...)
declare i64 @Text_Concat(...)
declare i64 @Text_Substring(...)
declare i64 @Text_Character(...)
declare i64 @Text_Lower(...)
declare i64 @Text_Upper(...)
declare i64 @Text_StartsWith(...)
declare i64 @Text_EndsWith(...)
declare i64 @Text_Contains(...)
declare i64 @Text_IndexOf(...)
declare i64 @Text_LastIndexOf(...)
declare i64 @Text_Replace(...)
declare i64 @Text_ToInt(...)
declare i64 @Text_FromInt(...)
declare i64 @Text_FromBool(...)
declare i64 @Text_FromChar(...)
declare i64 @Text_FromChars(...)
declare i64 @Text_Trim(...)
declare i64 @Text_Split(...)
declare i64 @List_Create(...)
declare i64 @List_Count(...)
declare i64 @List_Get(...)
declare i64 @List_TakeFront(...)
declare i64 @List_TakeBack(...)
declare i64 @List_TextOf(...)
declare i64 @List_Settle(...)
declare i64 @List_Range(...)
declare i64 @Map_Create(...)
declare i64 @Map_Get(...)
declare i64 @Map_SetHas(...)
declare i64 @Object_Create(...)
declare i64 @Object_Get(...)
declare i64 @Files_Read(...)
declare i64 @Files_Exists(...)
declare i64 @Files_Name(...)
declare i64 @Files_Join(...)
declare i64 @Files_DirectoryExists(...)
declare i64 @Files_List(...)
declare i64 @Process_Environment(...)
declare i64 @Process_Run(...)
declare i64 @Math_GreatestCommonDivisor(...)
declare i64 @Math_IntegerSquareRoot(...)
declare i64 @Float64_Parse(...)
declare i64 @Float64_Text(...)
declare i64 @Float64_FromInt(...)
declare i64 @Float64_ToInt(...)
declare i64 @Float64_ToIntMode(...)
declare i64 @Float64_ToIntChecked(...)
declare i64 @Float64_Format(...)
declare i64 @Text_IsInt(...)
declare i64 @Float32_Parse(...)
declare i64 @Float16_Parse(...)
declare i64 @Unsigned64_Parse(...)
declare i64 @Float64_FormatFixed(...)
declare i64 @Any_Text(...)
declare i64 @Any_ListText(...)
declare i64 @Any_MapText(...)
declare i64 @Any_ToInt(...)
declare i64 @Any_ToFloat(...)
declare i64 @Any_Length(...)
declare i64 @Any_Upper(...)
declare i64 @Any_Lower(...)
declare i64 @Process_Input(...)
declare i64 @Ownership_Allocate(...)
declare i64 @Ownership_AllocateAligned(...)
declare i64 @Ownership_Keep(...)
declare i64 @Ownership_MarkDepth(...)
declare void @List_Add(...)
declare void @List_Set(...)
declare void @List_Sort(...)
declare void @List_Reverse(...)
declare void @Map_Put(...)
declare void @Map_AddToSet(...)
declare void @Object_Set(...)
declare void @Process_Print(...)
declare void @Files_WriteText(...)
declare void @Files_WriteBytes(...)
declare void @Files_Delete(...)
declare void @Files_CreateDirectory(...)
declare void @Files_DeleteDirectory(...)
declare void @Ownership_Copy(...)
declare void @Ownership_Free(...)
declare void @Ownership_Retain(...)
declare void @Ownership_Release(...)
declare void @Ownership_PushScope(...)
declare void @Ownership_PopScope(...)
declare void @Ownership_Give(...)
declare void @Ownership_Unwind(...)

@.str.1.1 = private unnamed_addr constant [13 x i8] c"Hello, Robix\00"
define i64 @rxf_Main(i64 %p_args) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_args)
  %slot3 = alloca i64
  store i64 %p_args, ptr %slot3
  call void @Ownership_PushScope()
  %slot4 = alloca i64
  store i64 0, ptr %slot4
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot5 = alloca i64
  store i64 1, ptr %slot5
  br label %for6
for6:
  %v10 = load i64, ptr %slot5
  %cond11 = icmp sle i64 %v10, 5
  br i1 %cond11, label %fbody7, label %fend9
fbody7:
  call void @Ownership_PushScope()
  %v12 = load i64, ptr %slot4
  %v13 = load i64, ptr %slot5
  %v14 = add i64 %v12, %v13
  store i64 %v14, ptr %slot4
  call void @Ownership_PopScope()
  br label %fstep8
fstep8:
  %v15 = load i64, ptr %slot5
  %v16 = add i64 %v15, 1
  store i64 %v16, ptr %slot5
  br label %for6
fend9:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str17 = getelementptr [13 x i8], ptr @.str.1.1, i64 0, i64 0
  %v18 = ptrtoint ptr %str17 to i64
  call void @Process_Print(i64 %v18, i64 1)
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v19 = load i64, ptr %slot4
  call void @Process_Print(i64 %v19, i64 0)
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v20 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v20)
  call void @Ownership_Give(i64 %v20)
  %p21 = inttoptr i64 %v20 to ptr
  %s22 = getelementptr i64, ptr %p21, i64 1
  %v23 = load i64, ptr %s22
  %v24 = icmp sgt i64 %v23, 0
  %v25 = zext i1 %v24 to i64
  %cond26 = icmp ne i64 %v25, 0
  br i1 %cond26, label %then27, label %else28
then27:
  call void @Ownership_PushScope()
  %v30 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v30)
  call void @Ownership_Give(i64 %v30)
  %p31 = inttoptr i64 %v30 to ptr
  %items32 = load ptr, ptr %p31
  %s33 = getelementptr i64, ptr %items32, i64 0
  %v34 = load i64, ptr %s33
  call void @Process_Print(i64 %v34, i64 1)
  call void @Ownership_PopScope()
  br label %merge29
else28:
  br label %merge29
merge29:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v35 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v35)
  %v36 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v36)
  ret i64 0
}

declare i64 @Process_Arguments(i32, ptr)
declare void @Process_Finish(i64)

define i32 @main(i32 %argc, ptr %argv) {
  %packed = call i64 @Process_Arguments(i32 %argc, ptr %argv)
  %result = call i64 @rxf_Main(i64 %packed)
  %code = trunc i64 %result to i32
  ret i32 %code
}
