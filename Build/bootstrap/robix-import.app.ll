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

@.str.1.1 = private unnamed_addr constant [1 x i8] c"\00"
define void @rxm_ImportHost_new(i64 %p_This) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_PushScope()
  %str4 = getelementptr [1 x i8], ptr @.str.1.1, i64 0, i64 0
  %v5 = ptrtoint ptr %str4 to i64
  %v6 = load i64, ptr %slot3
  %p9 = inttoptr i64 %v6 to ptr
  %s10 = getelementptr i64, ptr %p9, i64 0
  %v11 = load i64, ptr %s10
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Release(i64 %v11)
  %p7 = inttoptr i64 %v6 to ptr
  %s8 = getelementptr i64, ptr %p7, i64 0
  store i64 %v5, ptr %s8
  call void @Ownership_PopScope()
  %v12 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v12)
  %v13 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v13)
  ret void
}

@.str.2.1 = private unnamed_addr constant [4 x i8] c".rx\00"
@.str.2.2 = private unnamed_addr constant [1 x i8] c"\00"
@.str.2.3 = private unnamed_addr constant [11 x i8] c"Build/temp\00"
@.str.2.4 = private unnamed_addr constant [25 x i8] c"Build/temp/robix-unit.rx\00"
@.str.2.5 = private unnamed_addr constant [35 x i8] c"Build/bootstrap/robix-run-core.app\00"
@.str.2.6 = private unnamed_addr constant [30 x i8] c"Build/bootstrap/robix-run.app\00"
@.str.2.7 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.2.8 = private unnamed_addr constant [4 x i8] c"\22 \22\00"
@.str.2.9 = private unnamed_addr constant [29 x i8] c"\22 \22Build/temp/robix-unit.rx\22\00"
@.str.2.10 = private unnamed_addr constant [3 x i8] c" \22\00"
@.str.2.11 = private unnamed_addr constant [2 x i8] c"\22\00"
define i64 @rxm_ImportHost_Run(i64 %p_This, i64 %p_args) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_args)
  %slot4 = alloca i64
  store i64 %p_args, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %p6 = inttoptr i64 %v5 to ptr
  %s7 = getelementptr i64, ptr %p6, i64 1
  %v8 = load i64, ptr %s7
  %v9 = icmp slt i64 %v8, 2
  %v10 = zext i1 %v9 to i64
  %logic13 = alloca i64
  %cond14 = icmp ne i64 %v10, 0
  store i64 1, ptr %logic13
  br i1 %cond14, label %logic12, label %rhs11
rhs11:
  %str15 = getelementptr [4 x i8], ptr @.str.2.1, i64 0, i64 0
  %v16 = ptrtoint ptr %str15 to i64
  %v17 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v17)
  call void @Ownership_Give(i64 %v17)
  %p18 = inttoptr i64 %v17 to ptr
  %items19 = load ptr, ptr %p18
  %s20 = getelementptr i64, ptr %items19, i64 1
  %v21 = load i64, ptr %s20
  %v22 = call i64 @Text_EndsWith(i64 %v21, i64 %v16)
  %v23 = icmp eq i64 %v22, 0
  %v24 = zext i1 %v23 to i64
  %cond25 = icmp ne i64 %v24, 0
  %v26 = zext i1 %cond25 to i64
  store i64 %v26, ptr %logic13
  br label %logic12
logic12:
  %v27 = load i64, ptr %logic13
  %cond28 = icmp ne i64 %v27, 0
  br i1 %cond28, label %then29, label %else30
then29:
  call void @Ownership_PushScope()
  %v32 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v32)
  call void @Ownership_Give(i64 %v32)
  %v33 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v33)
  call void @Ownership_Give(i64 %v33)
  %v34 = call i64 @rxm_ImportHost_Forward(i64 %v33, i64 %v32)
  %v35 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v35)
  %v36 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v36)
  %v37 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v37)
  ret i64 %v34
else30:
  br label %merge31
merge31:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v38 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v38)
  call void @Ownership_Give(i64 %v38)
  %p39 = inttoptr i64 %v38 to ptr
  %items40 = load ptr, ptr %p39
  %s41 = getelementptr i64, ptr %items40, i64 1
  %v42 = load i64, ptr %s41
  %v43 = call i64 @Ownership_Keep(i64 %v42)
  %cond44 = icmp eq i64 %v43, 0
  br i1 %cond44, label %own45, label %owned46
own45:
  call void @Ownership_Retain(i64 %v42)
  br label %owned46
owned46:
  %slot47 = alloca i64
  store i64 %v42, ptr %slot47
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v48 = load i64, ptr %slot47
  call void @Ownership_Retain(i64 %v48)
  call void @Ownership_Give(i64 %v48)
  %v49 = call i64 @Files_Exists(i64 %v48)
  %v50 = icmp eq i64 %v49, 0
  %v51 = zext i1 %v50 to i64
  %cond52 = icmp ne i64 %v51, 0
  br i1 %cond52, label %then53, label %else54
then53:
  call void @Ownership_PushScope()
  %v56 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v56)
  call void @Ownership_Give(i64 %v56)
  %v57 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v57)
  call void @Ownership_Give(i64 %v57)
  %v58 = call i64 @rxm_ImportHost_Forward(i64 %v57, i64 %v56)
  %v59 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v59)
  %v60 = load i64, ptr %slot47
  call void @Ownership_Release(i64 %v60)
  ret i64 %v58
else54:
  br label %merge55
merge55:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v61 = load i64, ptr %slot47
  call void @Ownership_Retain(i64 %v61)
  call void @Ownership_Give(i64 %v61)
  %v62 = call i64 @Files_Read(i64 %v61)
  %v63 = call i64 @Ownership_Keep(i64 %v62)
  %cond64 = icmp eq i64 %v63, 0
  br i1 %cond64, label %own65, label %owned66
own65:
  call void @Ownership_Retain(i64 %v62)
  br label %owned66
owned66:
  %slot67 = alloca i64
  store i64 %v62, ptr %slot67
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v68 = load i64, ptr %slot67
  call void @Ownership_Retain(i64 %v68)
  call void @Ownership_Give(i64 %v68)
  %v69 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v69)
  call void @Ownership_Give(i64 %v69)
  call void @rxm_ImportHost_RequireUses(i64 %v69, i64 %v68)
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v70 = load i64, ptr %slot3
  %p71 = inttoptr i64 %v70 to ptr
  %s72 = getelementptr i64, ptr %p71, i64 0
  %v73 = load i64, ptr %s72
  %str74 = getelementptr [1 x i8], ptr @.str.2.2, i64 0, i64 0
  %v75 = ptrtoint ptr %str74 to i64
  %v76 = call i64 @Text_Equals(i64 %v73, i64 %v75)
  %v77 = xor i64 %v76, 1
  %cond78 = icmp ne i64 %v77, 0
  br i1 %cond78, label %then79, label %else80
then79:
  call void @Ownership_PushScope()
  %v82 = load i64, ptr %slot3
  %p83 = inttoptr i64 %v82 to ptr
  %s84 = getelementptr i64, ptr %p83, i64 0
  %v85 = load i64, ptr %s84
  call void @Process_Print(i64 %v85, i64 1)
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v86 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v86)
  %v87 = load i64, ptr %slot67
  call void @Ownership_Release(i64 %v87)
  ret i64 1
else80:
  br label %merge81
merge81:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v88 = load i64, ptr %slot67
  call void @Ownership_Retain(i64 %v88)
  call void @Ownership_Give(i64 %v88)
  %v89 = load i64, ptr %slot47
  call void @Ownership_Retain(i64 %v89)
  call void @Ownership_Give(i64 %v89)
  %v90 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v90)
  call void @Ownership_Give(i64 %v90)
  %v91 = call i64 @rxm_ImportHost_Expand(i64 %v90, i64 %v88, i64 %v89)
  %v92 = call i64 @Ownership_Keep(i64 %v91)
  %cond93 = icmp eq i64 %v92, 0
  br i1 %cond93, label %own94, label %owned95
own94:
  call void @Ownership_Retain(i64 %v91)
  br label %owned95
owned95:
  %slot96 = alloca i64
  store i64 %v91, ptr %slot96
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str97 = getelementptr [11 x i8], ptr @.str.2.3, i64 0, i64 0
  %v98 = ptrtoint ptr %str97 to i64
  call void @Files_CreateDirectory(i64 %v98)
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str99 = getelementptr [25 x i8], ptr @.str.2.4, i64 0, i64 0
  %v100 = ptrtoint ptr %str99 to i64
  %v101 = load i64, ptr %slot96
  call void @Ownership_Retain(i64 %v101)
  call void @Ownership_Give(i64 %v101)
  call void @Files_WriteText(i64 %v100, i64 %v101)
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str102 = getelementptr [35 x i8], ptr @.str.2.5, i64 0, i64 0
  %v103 = ptrtoint ptr %str102 to i64
  %v104 = call i64 @Ownership_Keep(i64 %v103)
  %cond105 = icmp eq i64 %v104, 0
  br i1 %cond105, label %own106, label %owned107
own106:
  call void @Ownership_Retain(i64 %v103)
  br label %owned107
owned107:
  %slot108 = alloca i64
  store i64 %v103, ptr %slot108
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v109 = load i64, ptr %slot108
  call void @Ownership_Retain(i64 %v109)
  call void @Ownership_Give(i64 %v109)
  %v110 = call i64 @Files_Exists(i64 %v109)
  %v111 = icmp eq i64 %v110, 0
  %v112 = zext i1 %v111 to i64
  %cond113 = icmp ne i64 %v112, 0
  br i1 %cond113, label %then114, label %else115
then114:
  call void @Ownership_PushScope()
  %str117 = getelementptr [30 x i8], ptr @.str.2.6, i64 0, i64 0
  %v118 = ptrtoint ptr %str117 to i64
  %v119 = call i64 @Ownership_Keep(i64 %v118)
  %cond120 = icmp eq i64 %v119, 0
  br i1 %cond120, label %own121, label %owned122
own121:
  call void @Ownership_Retain(i64 %v118)
  br label %owned122
owned122:
  %v123 = load i64, ptr %slot108
  call void @Ownership_Release(i64 %v123)
  store i64 %v118, ptr %slot108
  call void @Ownership_PopScope()
  br label %merge116
else115:
  br label %merge116
merge116:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str124 = getelementptr [2 x i8], ptr @.str.2.7, i64 0, i64 0
  %v125 = ptrtoint ptr %str124 to i64
  %v126 = load i64, ptr %slot108
  call void @Ownership_Retain(i64 %v126)
  call void @Ownership_Give(i64 %v126)
  %v127 = call i64 @Text_Concat(i64 %v125, i64 %v126)
  %str128 = getelementptr [4 x i8], ptr @.str.2.8, i64 0, i64 0
  %v129 = ptrtoint ptr %str128 to i64
  %v130 = call i64 @Text_Concat(i64 %v127, i64 %v129)
  %v131 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v131)
  call void @Ownership_Give(i64 %v131)
  %p132 = inttoptr i64 %v131 to ptr
  %items133 = load ptr, ptr %p132
  %s134 = getelementptr i64, ptr %items133, i64 0
  %v135 = load i64, ptr %s134
  %v136 = call i64 @Text_Concat(i64 %v130, i64 %v135)
  %str137 = getelementptr [29 x i8], ptr @.str.2.9, i64 0, i64 0
  %v138 = ptrtoint ptr %str137 to i64
  %v139 = call i64 @Text_Concat(i64 %v136, i64 %v138)
  %v140 = call i64 @Ownership_Keep(i64 %v139)
  %cond141 = icmp eq i64 %v140, 0
  br i1 %cond141, label %own142, label %owned143
own142:
  call void @Ownership_Retain(i64 %v139)
  br label %owned143
owned143:
  %slot144 = alloca i64
  store i64 %v139, ptr %slot144
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot145 = alloca i64
  store i64 2, ptr %slot145
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while146
while146:
  %v149 = load i64, ptr %slot145
  %v150 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v150)
  call void @Ownership_Give(i64 %v150)
  %p151 = inttoptr i64 %v150 to ptr
  %s152 = getelementptr i64, ptr %p151, i64 1
  %v153 = load i64, ptr %s152
  %v154 = icmp slt i64 %v149, %v153
  %v155 = zext i1 %v154 to i64
  %cond156 = icmp ne i64 %v155, 0
  br i1 %cond156, label %wbody147, label %wend148
wbody147:
  call void @Ownership_PushScope()
  %v157 = load i64, ptr %slot144
  call void @Ownership_Retain(i64 %v157)
  call void @Ownership_Give(i64 %v157)
  %str158 = getelementptr [3 x i8], ptr @.str.2.10, i64 0, i64 0
  %v159 = ptrtoint ptr %str158 to i64
  %v160 = call i64 @Text_Concat(i64 %v157, i64 %v159)
  %v161 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v161)
  call void @Ownership_Give(i64 %v161)
  %v162 = load i64, ptr %slot145
  %p163 = inttoptr i64 %v161 to ptr
  %items164 = load ptr, ptr %p163
  %s165 = getelementptr i64, ptr %items164, i64 %v162
  %v166 = load i64, ptr %s165
  %v167 = call i64 @Text_Concat(i64 %v160, i64 %v166)
  %str168 = getelementptr [2 x i8], ptr @.str.2.11, i64 0, i64 0
  %v169 = ptrtoint ptr %str168 to i64
  %v170 = call i64 @Text_Concat(i64 %v167, i64 %v169)
  %v171 = call i64 @Ownership_Keep(i64 %v170)
  %cond172 = icmp eq i64 %v171, 0
  br i1 %cond172, label %own173, label %owned174
own173:
  call void @Ownership_Retain(i64 %v170)
  br label %owned174
owned174:
  %v175 = load i64, ptr %slot144
  call void @Ownership_Release(i64 %v175)
  store i64 %v170, ptr %slot144
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v176 = load i64, ptr %slot145
  %v177 = add i64 %v176, 1
  store i64 %v177, ptr %slot145
  call void @Ownership_PopScope()
  br label %while146
wend148:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v178 = load i64, ptr %slot144
  call void @Ownership_Retain(i64 %v178)
  call void @Ownership_Give(i64 %v178)
  %v179 = call i64 @Process_Run(i64 %v178)
  %v180 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v180)
  %v181 = load i64, ptr %slot144
  call void @Ownership_Release(i64 %v181)
  %v182 = load i64, ptr %slot108
  call void @Ownership_Release(i64 %v182)
  %v183 = load i64, ptr %slot96
  call void @Ownership_Release(i64 %v183)
  ret i64 %v179
}

@.str.3.1 = private unnamed_addr constant [35 x i8] c"Build/bootstrap/robix-run-core.app\00"
@.str.3.2 = private unnamed_addr constant [30 x i8] c"Build/bootstrap/robix-run.app\00"
@.str.3.3 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.3.4 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.3.5 = private unnamed_addr constant [3 x i8] c" \22\00"
@.str.3.6 = private unnamed_addr constant [2 x i8] c"\22\00"
define i64 @rxm_ImportHost_Forward(i64 %p_This, i64 %p_args) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_args)
  %slot4 = alloca i64
  store i64 %p_args, ptr %slot4
  call void @Ownership_PushScope()
  %str5 = getelementptr [35 x i8], ptr @.str.3.1, i64 0, i64 0
  %v6 = ptrtoint ptr %str5 to i64
  %v7 = call i64 @Ownership_Keep(i64 %v6)
  %cond8 = icmp eq i64 %v7, 0
  br i1 %cond8, label %own9, label %owned10
own9:
  call void @Ownership_Retain(i64 %v6)
  br label %owned10
owned10:
  %slot11 = alloca i64
  store i64 %v6, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v12 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v12)
  call void @Ownership_Give(i64 %v12)
  %v13 = call i64 @Files_Exists(i64 %v12)
  %v14 = icmp eq i64 %v13, 0
  %v15 = zext i1 %v14 to i64
  %cond16 = icmp ne i64 %v15, 0
  br i1 %cond16, label %then17, label %else18
then17:
  call void @Ownership_PushScope()
  %str20 = getelementptr [30 x i8], ptr @.str.3.2, i64 0, i64 0
  %v21 = ptrtoint ptr %str20 to i64
  %v22 = call i64 @Ownership_Keep(i64 %v21)
  %cond23 = icmp eq i64 %v22, 0
  br i1 %cond23, label %own24, label %owned25
own24:
  call void @Ownership_Retain(i64 %v21)
  br label %owned25
owned25:
  %v26 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v26)
  store i64 %v21, ptr %slot11
  call void @Ownership_PopScope()
  br label %merge19
else18:
  br label %merge19
merge19:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str27 = getelementptr [2 x i8], ptr @.str.3.3, i64 0, i64 0
  %v28 = ptrtoint ptr %str27 to i64
  %v29 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v29)
  call void @Ownership_Give(i64 %v29)
  %v30 = call i64 @Text_Concat(i64 %v28, i64 %v29)
  %str31 = getelementptr [2 x i8], ptr @.str.3.4, i64 0, i64 0
  %v32 = ptrtoint ptr %str31 to i64
  %v33 = call i64 @Text_Concat(i64 %v30, i64 %v32)
  %v34 = call i64 @Ownership_Keep(i64 %v33)
  %cond35 = icmp eq i64 %v34, 0
  br i1 %cond35, label %own36, label %owned37
own36:
  call void @Ownership_Retain(i64 %v33)
  br label %owned37
owned37:
  %slot38 = alloca i64
  store i64 %v33, ptr %slot38
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot39 = alloca i64
  store i64 0, ptr %slot39
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while40
while40:
  %v43 = load i64, ptr %slot39
  %v44 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v44)
  call void @Ownership_Give(i64 %v44)
  %p45 = inttoptr i64 %v44 to ptr
  %s46 = getelementptr i64, ptr %p45, i64 1
  %v47 = load i64, ptr %s46
  %v48 = icmp slt i64 %v43, %v47
  %v49 = zext i1 %v48 to i64
  %cond50 = icmp ne i64 %v49, 0
  br i1 %cond50, label %wbody41, label %wend42
wbody41:
  call void @Ownership_PushScope()
  %v51 = load i64, ptr %slot38
  call void @Ownership_Retain(i64 %v51)
  call void @Ownership_Give(i64 %v51)
  %str52 = getelementptr [3 x i8], ptr @.str.3.5, i64 0, i64 0
  %v53 = ptrtoint ptr %str52 to i64
  %v54 = call i64 @Text_Concat(i64 %v51, i64 %v53)
  %v55 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v55)
  call void @Ownership_Give(i64 %v55)
  %v56 = load i64, ptr %slot39
  %p57 = inttoptr i64 %v55 to ptr
  %items58 = load ptr, ptr %p57
  %s59 = getelementptr i64, ptr %items58, i64 %v56
  %v60 = load i64, ptr %s59
  %v61 = call i64 @Text_Concat(i64 %v54, i64 %v60)
  %str62 = getelementptr [2 x i8], ptr @.str.3.6, i64 0, i64 0
  %v63 = ptrtoint ptr %str62 to i64
  %v64 = call i64 @Text_Concat(i64 %v61, i64 %v63)
  %v65 = call i64 @Ownership_Keep(i64 %v64)
  %cond66 = icmp eq i64 %v65, 0
  br i1 %cond66, label %own67, label %owned68
own67:
  call void @Ownership_Retain(i64 %v64)
  br label %owned68
owned68:
  %v69 = load i64, ptr %slot38
  call void @Ownership_Release(i64 %v69)
  store i64 %v64, ptr %slot38
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v70 = load i64, ptr %slot39
  %v71 = add i64 %v70, 1
  store i64 %v71, ptr %slot39
  call void @Ownership_PopScope()
  br label %while40
wend42:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v72 = load i64, ptr %slot38
  call void @Ownership_Retain(i64 %v72)
  call void @Ownership_Give(i64 %v72)
  %v73 = call i64 @Process_Run(i64 %v72)
  %v74 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v74)
  %v75 = load i64, ptr %slot38
  call void @Ownership_Release(i64 %v75)
  %v76 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v76)
  %v77 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v77)
  %v78 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v78)
  ret i64 %v73
}

@.str.4.1 = private unnamed_addr constant [2 x i8] c";\00"
@.str.4.2 = private unnamed_addr constant [2 x i8] c";\00"
@.str.4.3 = private unnamed_addr constant [1 x i8] c"\00"
@.str.4.4 = private unnamed_addr constant [1 x i8] c"\00"
@.str.4.5 = private unnamed_addr constant [2 x i8] c";\00"
@.str.4.6 = private unnamed_addr constant [2 x i8] c";\00"
@.str.4.7 = private unnamed_addr constant [2 x i8] c";\00"
@.str.4.8 = private unnamed_addr constant [1 x i8] c"\00"
@.str.4.9 = private unnamed_addr constant [1 x i8] c"\00"
@.str.4.10 = private unnamed_addr constant [2 x i8] c"\0A\00"
define i64 @rxm_ImportHost_Expand(i64 %p_This, i64 %p_source, i64 %p_sourcePath) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_source)
  %slot4 = alloca i64
  store i64 %p_source, ptr %slot4
  call void @Ownership_Retain(i64 %p_sourcePath)
  %slot5 = alloca i64
  store i64 %p_sourcePath, ptr %slot5
  call void @Ownership_PushScope()
  %v6 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v6)
  call void @Ownership_Give(i64 %v6)
  %v7 = call i64 @Ownership_Keep(i64 %v6)
  %cond8 = icmp eq i64 %v7, 0
  br i1 %cond8, label %own9, label %owned10
own9:
  call void @Ownership_Retain(i64 %v6)
  br label %owned10
owned10:
  %slot11 = alloca i64
  store i64 %v6, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v12 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v12)
  call void @Ownership_Give(i64 %v12)
  %v13 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v13)
  call void @Ownership_Give(i64 %v13)
  %v14 = call i64 @rxm_ImportHost_DirectImports(i64 %v13, i64 %v12)
  %v15 = call i64 @Ownership_Keep(i64 %v14)
  %cond16 = icmp eq i64 %v15, 0
  br i1 %cond16, label %own17, label %owned18
own17:
  call void @Ownership_Retain(i64 %v14)
  br label %owned18
owned18:
  %slot19 = alloca i64
  store i64 %v14, ptr %slot19
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str20 = getelementptr [2 x i8], ptr @.str.4.1, i64 0, i64 0
  %v21 = ptrtoint ptr %str20 to i64
  %v22 = call i64 @Ownership_Keep(i64 %v21)
  %cond23 = icmp eq i64 %v22, 0
  br i1 %cond23, label %own24, label %owned25
own24:
  call void @Ownership_Retain(i64 %v21)
  br label %owned25
owned25:
  %slot26 = alloca i64
  store i64 %v21, ptr %slot26
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot27 = alloca i64
  store i64 0, ptr %slot27
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while28
while28:
  %v31 = load i64, ptr %slot27
  %v32 = load i64, ptr %slot19
  call void @Ownership_Retain(i64 %v32)
  call void @Ownership_Give(i64 %v32)
  %v33 = call i64 @Text_Length(i64 %v32)
  %v34 = icmp slt i64 %v31, %v33
  %v35 = zext i1 %v34 to i64
  %cond36 = icmp ne i64 %v35, 0
  br i1 %cond36, label %wbody29, label %wend30
wbody29:
  call void @Ownership_PushScope()
  %str37 = getelementptr [2 x i8], ptr @.str.4.2, i64 0, i64 0
  %v38 = ptrtoint ptr %str37 to i64
  %v39 = load i64, ptr %slot27
  %v40 = load i64, ptr %slot19
  call void @Ownership_Retain(i64 %v40)
  call void @Ownership_Give(i64 %v40)
  %v41 = call i64 @Text_IndexOf(i64 %v40, i64 %v38, i64 %v39)
  %slot42 = alloca i64
  store i64 %v41, ptr %slot42
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v43 = load i64, ptr %slot42
  %v44 = icmp slt i64 %v43, 0
  %v45 = zext i1 %v44 to i64
  %cond46 = icmp ne i64 %v45, 0
  br i1 %cond46, label %then47, label %else48
then47:
  call void @Ownership_PushScope()
  %v50 = load i64, ptr %slot19
  call void @Ownership_Retain(i64 %v50)
  call void @Ownership_Give(i64 %v50)
  %v51 = call i64 @Text_Length(i64 %v50)
  store i64 %v51, ptr %slot27
  call void @Ownership_PopScope()
  br label %merge49
else48:
  call void @Ownership_PushScope()
  %str52 = getelementptr [1 x i8], ptr @.str.4.3, i64 0, i64 0
  %v53 = ptrtoint ptr %str52 to i64
  %v54 = call i64 @Ownership_Keep(i64 %v53)
  %cond55 = icmp eq i64 %v54, 0
  br i1 %cond55, label %own56, label %owned57
own56:
  call void @Ownership_Retain(i64 %v53)
  br label %owned57
owned57:
  %slot58 = alloca i64
  store i64 %v53, ptr %slot58
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v59 = load i64, ptr %slot42
  %v60 = load i64, ptr %slot27
  %v61 = icmp sgt i64 %v59, %v60
  %v62 = zext i1 %v61 to i64
  %cond63 = icmp ne i64 %v62, 0
  br i1 %cond63, label %then64, label %else65
then64:
  call void @Ownership_PushScope()
  %v67 = load i64, ptr %slot27
  %v68 = load i64, ptr %slot42
  %v69 = load i64, ptr %slot27
  %v70 = sub i64 %v68, %v69
  %v71 = load i64, ptr %slot19
  call void @Ownership_Retain(i64 %v71)
  call void @Ownership_Give(i64 %v71)
  %v72 = call i64 @Text_Substring(i64 %v71, i64 %v67, i64 %v70)
  %v73 = call i64 @Ownership_Keep(i64 %v72)
  %cond74 = icmp eq i64 %v73, 0
  br i1 %cond74, label %own75, label %owned76
own75:
  call void @Ownership_Retain(i64 %v72)
  br label %owned76
owned76:
  %v77 = load i64, ptr %slot58
  call void @Ownership_Release(i64 %v77)
  store i64 %v72, ptr %slot58
  call void @Ownership_PopScope()
  br label %merge66
else65:
  br label %merge66
merge66:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v78 = load i64, ptr %slot42
  %v79 = add i64 %v78, 1
  store i64 %v79, ptr %slot27
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v80 = load i64, ptr %slot58
  call void @Ownership_Retain(i64 %v80)
  call void @Ownership_Give(i64 %v80)
  %str81 = getelementptr [1 x i8], ptr @.str.4.4, i64 0, i64 0
  %v82 = ptrtoint ptr %str81 to i64
  %v83 = call i64 @Text_Equals(i64 %v80, i64 %v82)
  %v84 = xor i64 %v83, 1
  %logic87 = alloca i64
  %cond88 = icmp ne i64 %v84, 0
  store i64 0, ptr %logic87
  br i1 %cond88, label %rhs85, label %logic86
rhs85:
  %str89 = getelementptr [2 x i8], ptr @.str.4.5, i64 0, i64 0
  %v90 = ptrtoint ptr %str89 to i64
  %v91 = load i64, ptr %slot58
  call void @Ownership_Retain(i64 %v91)
  call void @Ownership_Give(i64 %v91)
  %v92 = call i64 @Text_Concat(i64 %v90, i64 %v91)
  %str93 = getelementptr [2 x i8], ptr @.str.4.6, i64 0, i64 0
  %v94 = ptrtoint ptr %str93 to i64
  %v95 = call i64 @Text_Concat(i64 %v92, i64 %v94)
  %v96 = load i64, ptr %slot26
  call void @Ownership_Retain(i64 %v96)
  call void @Ownership_Give(i64 %v96)
  %v97 = call i64 @Text_Contains(i64 %v96, i64 %v95)
  %v98 = icmp eq i64 %v97, 0
  %v99 = zext i1 %v98 to i64
  %cond100 = icmp ne i64 %v99, 0
  %v101 = zext i1 %cond100 to i64
  store i64 %v101, ptr %logic87
  br label %logic86
logic86:
  %v102 = load i64, ptr %logic87
  %cond103 = icmp ne i64 %v102, 0
  br i1 %cond103, label %then104, label %else105
then104:
  call void @Ownership_PushScope()
  %v107 = load i64, ptr %slot26
  call void @Ownership_Retain(i64 %v107)
  call void @Ownership_Give(i64 %v107)
  %v108 = load i64, ptr %slot58
  call void @Ownership_Retain(i64 %v108)
  call void @Ownership_Give(i64 %v108)
  %v109 = call i64 @Text_Concat(i64 %v107, i64 %v108)
  %str110 = getelementptr [2 x i8], ptr @.str.4.7, i64 0, i64 0
  %v111 = ptrtoint ptr %str110 to i64
  %v112 = call i64 @Text_Concat(i64 %v109, i64 %v111)
  %v113 = call i64 @Ownership_Keep(i64 %v112)
  %cond114 = icmp eq i64 %v113, 0
  br i1 %cond114, label %own115, label %owned116
own115:
  call void @Ownership_Retain(i64 %v112)
  br label %owned116
owned116:
  %v117 = load i64, ptr %slot26
  call void @Ownership_Release(i64 %v117)
  store i64 %v112, ptr %slot26
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v118 = load i64, ptr %slot58
  call void @Ownership_Retain(i64 %v118)
  call void @Ownership_Give(i64 %v118)
  %v119 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v119)
  call void @Ownership_Give(i64 %v119)
  %v120 = call i64 @rxm_ImportHost_PackagePath(i64 %v119, i64 %v118)
  %v121 = call i64 @Ownership_Keep(i64 %v120)
  %cond122 = icmp eq i64 %v121, 0
  br i1 %cond122, label %own123, label %owned124
own123:
  call void @Ownership_Retain(i64 %v120)
  br label %owned124
owned124:
  %slot125 = alloca i64
  store i64 %v120, ptr %slot125
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot126 = alloca i64
  store i64 0, ptr %slot126
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v127 = load i64, ptr %slot125
  call void @Ownership_Retain(i64 %v127)
  call void @Ownership_Give(i64 %v127)
  %str128 = getelementptr [1 x i8], ptr @.str.4.8, i64 0, i64 0
  %v129 = ptrtoint ptr %str128 to i64
  %v130 = call i64 @Text_Equals(i64 %v127, i64 %v129)
  %v131 = xor i64 %v130, 1
  %logic134 = alloca i64
  %cond135 = icmp ne i64 %v131, 0
  store i64 0, ptr %logic134
  br i1 %cond135, label %rhs132, label %logic133
rhs132:
  %v136 = load i64, ptr %slot125
  call void @Ownership_Retain(i64 %v136)
  call void @Ownership_Give(i64 %v136)
  %v137 = load i64, ptr %slot5
  call void @Ownership_Retain(i64 %v137)
  call void @Ownership_Give(i64 %v137)
  %v138 = call i64 @Text_EndsWith(i64 %v137, i64 %v136)
  %cond139 = icmp ne i64 %v138, 0
  %v140 = zext i1 %cond139 to i64
  store i64 %v140, ptr %logic134
  br label %logic133
logic133:
  %v141 = load i64, ptr %logic134
  %cond142 = icmp ne i64 %v141, 0
  br i1 %cond142, label %then143, label %else144
then143:
  call void @Ownership_PushScope()
  store i64 1, ptr %slot126
  call void @Ownership_PopScope()
  br label %merge145
else144:
  br label %merge145
merge145:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v146 = load i64, ptr %slot125
  call void @Ownership_Retain(i64 %v146)
  call void @Ownership_Give(i64 %v146)
  %str147 = getelementptr [1 x i8], ptr @.str.4.9, i64 0, i64 0
  %v148 = ptrtoint ptr %str147 to i64
  %v149 = call i64 @Text_Equals(i64 %v146, i64 %v148)
  %v150 = xor i64 %v149, 1
  %logic153 = alloca i64
  %cond154 = icmp ne i64 %v150, 0
  store i64 0, ptr %logic153
  br i1 %cond154, label %rhs151, label %logic152
rhs151:
  %v155 = load i64, ptr %slot126
  %v156 = icmp eq i64 %v155, 0
  %v157 = zext i1 %v156 to i64
  %cond158 = icmp ne i64 %v157, 0
  %v159 = zext i1 %cond158 to i64
  store i64 %v159, ptr %logic153
  br label %logic152
logic152:
  %v160 = load i64, ptr %logic153
  %cond161 = icmp ne i64 %v160, 0
  br i1 %cond161, label %then162, label %else163
then162:
  call void @Ownership_PushScope()
  %v165 = load i64, ptr %slot125
  call void @Ownership_Retain(i64 %v165)
  call void @Ownership_Give(i64 %v165)
  %v166 = call i64 @Files_Read(i64 %v165)
  %v167 = call i64 @Ownership_Keep(i64 %v166)
  %cond168 = icmp eq i64 %v167, 0
  br i1 %cond168, label %own169, label %owned170
own169:
  call void @Ownership_Retain(i64 %v166)
  br label %owned170
owned170:
  %slot171 = alloca i64
  store i64 %v166, ptr %slot171
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v172 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v172)
  call void @Ownership_Give(i64 %v172)
  %str173 = getelementptr [2 x i8], ptr @.str.4.10, i64 0, i64 0
  %v174 = ptrtoint ptr %str173 to i64
  %v175 = call i64 @Text_Concat(i64 %v172, i64 %v174)
  %v176 = load i64, ptr %slot171
  call void @Ownership_Retain(i64 %v176)
  call void @Ownership_Give(i64 %v176)
  %v177 = call i64 @Text_Concat(i64 %v175, i64 %v176)
  %v178 = call i64 @Ownership_Keep(i64 %v177)
  %cond179 = icmp eq i64 %v178, 0
  br i1 %cond179, label %own180, label %owned181
own180:
  call void @Ownership_Retain(i64 %v177)
  br label %owned181
owned181:
  %v182 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v182)
  store i64 %v177, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v183 = load i64, ptr %slot19
  call void @Ownership_Retain(i64 %v183)
  call void @Ownership_Give(i64 %v183)
  %v184 = load i64, ptr %slot171
  call void @Ownership_Retain(i64 %v184)
  call void @Ownership_Give(i64 %v184)
  %v185 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v185)
  call void @Ownership_Give(i64 %v185)
  %v186 = call i64 @rxm_ImportHost_DirectImports(i64 %v185, i64 %v184)
  %v187 = call i64 @Text_Concat(i64 %v183, i64 %v186)
  %v188 = call i64 @Ownership_Keep(i64 %v187)
  %cond189 = icmp eq i64 %v188, 0
  br i1 %cond189, label %own190, label %owned191
own190:
  call void @Ownership_Retain(i64 %v187)
  br label %owned191
owned191:
  %v192 = load i64, ptr %slot19
  call void @Ownership_Release(i64 %v192)
  store i64 %v187, ptr %slot19
  call void @Ownership_PopScope()
  %v193 = load i64, ptr %slot171
  call void @Ownership_Release(i64 %v193)
  br label %merge164
else163:
  br label %merge164
merge164:
  call void @Ownership_PopScope()
  %v194 = load i64, ptr %slot125
  call void @Ownership_Release(i64 %v194)
  br label %merge106
else105:
  br label %merge106
merge106:
  call void @Ownership_PopScope()
  %v195 = load i64, ptr %slot58
  call void @Ownership_Release(i64 %v195)
  br label %merge49
merge49:
  call void @Ownership_PopScope()
  br label %while28
wend30:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v196 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v196)
  call void @Ownership_Give(i64 %v196)
  %v197 = call i64 @Ownership_Keep(i64 %v196)
  %cond198 = icmp eq i64 %v197, 0
  br i1 %cond198, label %own199, label %owned200
own199:
  call void @Ownership_Retain(i64 %v196)
  br label %owned200
owned200:
  %v201 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v201)
  %v202 = load i64, ptr %slot26
  call void @Ownership_Release(i64 %v202)
  %v203 = load i64, ptr %slot19
  call void @Ownership_Release(i64 %v203)
  %v204 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v204)
  %v205 = load i64, ptr %slot5
  call void @Ownership_Release(i64 %v205)
  %v206 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v206)
  %v207 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v207)
  call void @Ownership_Give(i64 %v196)
  ret i64 %v196
}

@.str.5.1 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.2 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.3 = private unnamed_addr constant [1 x i8] c"\00"
@.str.5.4 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.5.5 = private unnamed_addr constant [2 x i8] c"/\00"
@.str.5.6 = private unnamed_addr constant [2 x i8] c"/\00"
@.str.5.7 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.5.8 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.5.9 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.5.10 = private unnamed_addr constant [2 x i8] c"\\\00"
@.str.5.11 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.5.12 = private unnamed_addr constant [1 x i8] c"\00"
@.str.5.13 = private unnamed_addr constant [1 x i8] c"\00"
@.str.5.14 = private unnamed_addr constant [2 x i8] c".\00"
@.str.5.15 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.16 = private unnamed_addr constant [2 x i8] c"|\00"
@.str.5.17 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.18 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.19 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.20 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.21 = private unnamed_addr constant [1 x i8] c"\00"
@.str.5.22 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.23 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.24 = private unnamed_addr constant [13 x i8] c"error RX171 \00"
@.str.5.25 = private unnamed_addr constant [2 x i8] c":\00"
@.str.5.26 = private unnamed_addr constant [3 x i8] c" '\00"
@.str.5.27 = private unnamed_addr constant [14 x i8] c"' belongs to \00"
@.str.5.28 = private unnamed_addr constant [19 x i8] c". expected Import \00"
@.str.5.29 = private unnamed_addr constant [39 x i8] c", actual no import correction: Import \00"
@.str.5.30 = private unnamed_addr constant [19 x i8] c" before using it.\0A\00"
@.str.5.31 = private unnamed_addr constant [2 x i8] c";\00"
@.str.5.32 = private unnamed_addr constant [2 x i8] c" \00"
@.str.5.33 = private unnamed_addr constant [2 x i8] c"\09\00"
@.str.5.34 = private unnamed_addr constant [2 x i8] c"\0D\00"
define void @rxm_ImportHost_RequireUses(i64 %p_This, i64 %p_source) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_source)
  %slot4 = alloca i64
  store i64 %p_source, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %v6 = call i64 @rxm_ImportHost_TypeCatalog(i64 %v5)
  %v7 = call i64 @Ownership_Keep(i64 %v6)
  %cond8 = icmp eq i64 %v7, 0
  br i1 %cond8, label %own9, label %owned10
own9:
  call void @Ownership_Retain(i64 %v6)
  br label %owned10
owned10:
  %slot11 = alloca i64
  store i64 %v6, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str12 = getelementptr [2 x i8], ptr @.str.5.1, i64 0, i64 0
  %v13 = ptrtoint ptr %str12 to i64
  %v14 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v14)
  call void @Ownership_Give(i64 %v14)
  %v15 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v15)
  call void @Ownership_Give(i64 %v15)
  %v16 = call i64 @rxm_ImportHost_DirectImports(i64 %v15, i64 %v14)
  %v17 = call i64 @Text_Concat(i64 %v13, i64 %v16)
  %v18 = call i64 @Ownership_Keep(i64 %v17)
  %cond19 = icmp eq i64 %v18, 0
  br i1 %cond19, label %own20, label %owned21
own20:
  call void @Ownership_Retain(i64 %v17)
  br label %owned21
owned21:
  %slot22 = alloca i64
  store i64 %v17, ptr %slot22
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v23 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v23)
  call void @Ownership_Give(i64 %v23)
  %v24 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v24)
  call void @Ownership_Give(i64 %v24)
  %v25 = call i64 @rxm_ImportHost_DeclaredNames(i64 %v24, i64 %v23)
  %v26 = call i64 @Ownership_Keep(i64 %v25)
  %cond27 = icmp eq i64 %v26, 0
  br i1 %cond27, label %own28, label %owned29
own28:
  call void @Ownership_Retain(i64 %v25)
  br label %owned29
owned29:
  %slot30 = alloca i64
  store i64 %v25, ptr %slot30
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str31 = getelementptr [2 x i8], ptr @.str.5.2, i64 0, i64 0
  %v32 = ptrtoint ptr %str31 to i64
  %v33 = call i64 @Ownership_Keep(i64 %v32)
  %cond34 = icmp eq i64 %v33, 0
  br i1 %cond34, label %own35, label %owned36
own35:
  call void @Ownership_Retain(i64 %v32)
  br label %owned36
owned36:
  %slot37 = alloca i64
  store i64 %v32, ptr %slot37
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str38 = getelementptr [1 x i8], ptr @.str.5.3, i64 0, i64 0
  %v39 = ptrtoint ptr %str38 to i64
  %v40 = call i64 @Ownership_Keep(i64 %v39)
  %cond41 = icmp eq i64 %v40, 0
  br i1 %cond41, label %own42, label %owned43
own42:
  call void @Ownership_Retain(i64 %v39)
  br label %owned43
owned43:
  %slot44 = alloca i64
  store i64 %v39, ptr %slot44
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot45 = alloca i64
  store i64 0, ptr %slot45
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot46 = alloca i64
  store i64 1, ptr %slot46
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot47 = alloca i64
  store i64 1, ptr %slot47
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while48
while48:
  %v51 = load i64, ptr %slot45
  %v52 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v52)
  call void @Ownership_Give(i64 %v52)
  %v53 = call i64 @Text_Length(i64 %v52)
  %v54 = icmp slt i64 %v51, %v53
  %v55 = zext i1 %v54 to i64
  %cond56 = icmp ne i64 %v55, 0
  br i1 %cond56, label %wbody49, label %wend50
wbody49:
  call void @Ownership_PushScope()
  %v57 = load i64, ptr %slot45
  %v58 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v58)
  call void @Ownership_Give(i64 %v58)
  %v59 = call i64 @Text_Substring(i64 %v58, i64 %v57, i64 1)
  %v60 = call i64 @Ownership_Keep(i64 %v59)
  %cond61 = icmp eq i64 %v60, 0
  br i1 %cond61, label %own62, label %owned63
own62:
  call void @Ownership_Retain(i64 %v59)
  br label %owned63
owned63:
  %slot64 = alloca i64
  store i64 %v59, ptr %slot64
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v65 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v65)
  call void @Ownership_Give(i64 %v65)
  %str66 = getelementptr [2 x i8], ptr @.str.5.4, i64 0, i64 0
  %v67 = ptrtoint ptr %str66 to i64
  %v68 = call i64 @Text_Equals(i64 %v65, i64 %v67)
  %cond69 = icmp ne i64 %v68, 0
  br i1 %cond69, label %then70, label %else71
then70:
  call void @Ownership_PushScope()
  %v73 = load i64, ptr %slot46
  %v74 = add i64 %v73, 1
  store i64 %v74, ptr %slot46
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  store i64 1, ptr %slot47
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v75 = load i64, ptr %slot45
  %v76 = add i64 %v75, 1
  store i64 %v76, ptr %slot45
  call void @Ownership_PopScope()
  br label %merge72
else71:
  call void @Ownership_PushScope()
  %v77 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v77)
  call void @Ownership_Give(i64 %v77)
  %str78 = getelementptr [2 x i8], ptr @.str.5.5, i64 0, i64 0
  %v79 = ptrtoint ptr %str78 to i64
  %v80 = call i64 @Text_Equals(i64 %v77, i64 %v79)
  %logic83 = alloca i64
  %cond84 = icmp ne i64 %v80, 0
  store i64 0, ptr %logic83
  br i1 %cond84, label %rhs81, label %logic82
rhs81:
  %v85 = load i64, ptr %slot45
  %v86 = add i64 %v85, 1
  %v87 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v87)
  call void @Ownership_Give(i64 %v87)
  %v88 = call i64 @Text_Length(i64 %v87)
  %v89 = icmp slt i64 %v86, %v88
  %v90 = zext i1 %v89 to i64
  %cond91 = icmp ne i64 %v90, 0
  %v92 = zext i1 %cond91 to i64
  store i64 %v92, ptr %logic83
  br label %logic82
logic82:
  %v93 = load i64, ptr %logic83
  %logic96 = alloca i64
  %cond97 = icmp ne i64 %v93, 0
  store i64 0, ptr %logic96
  br i1 %cond97, label %rhs94, label %logic95
rhs94:
  %v98 = load i64, ptr %slot45
  %v99 = add i64 %v98, 1
  %v100 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v100)
  call void @Ownership_Give(i64 %v100)
  %v101 = call i64 @Text_Substring(i64 %v100, i64 %v99, i64 1)
  %str102 = getelementptr [2 x i8], ptr @.str.5.6, i64 0, i64 0
  %v103 = ptrtoint ptr %str102 to i64
  %v104 = call i64 @Text_Equals(i64 %v101, i64 %v103)
  %cond105 = icmp ne i64 %v104, 0
  %v106 = zext i1 %cond105 to i64
  store i64 %v106, ptr %logic96
  br label %logic95
logic95:
  %v107 = load i64, ptr %logic96
  %cond108 = icmp ne i64 %v107, 0
  br i1 %cond108, label %then109, label %else110
then109:
  call void @Ownership_PushScope()
  br label %while112
while112:
  %v115 = load i64, ptr %slot45
  %v116 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v116)
  call void @Ownership_Give(i64 %v116)
  %v117 = call i64 @Text_Length(i64 %v116)
  %v118 = icmp slt i64 %v115, %v117
  %v119 = zext i1 %v118 to i64
  %logic122 = alloca i64
  %cond123 = icmp ne i64 %v119, 0
  store i64 0, ptr %logic122
  br i1 %cond123, label %rhs120, label %logic121
rhs120:
  %v124 = load i64, ptr %slot45
  %v125 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v125)
  call void @Ownership_Give(i64 %v125)
  %v126 = call i64 @Text_Substring(i64 %v125, i64 %v124, i64 1)
  %str127 = getelementptr [2 x i8], ptr @.str.5.7, i64 0, i64 0
  %v128 = ptrtoint ptr %str127 to i64
  %v129 = call i64 @Text_Equals(i64 %v126, i64 %v128)
  %v130 = xor i64 %v129, 1
  %cond131 = icmp ne i64 %v130, 0
  %v132 = zext i1 %cond131 to i64
  store i64 %v132, ptr %logic122
  br label %logic121
logic121:
  %v133 = load i64, ptr %logic122
  %cond134 = icmp ne i64 %v133, 0
  br i1 %cond134, label %wbody113, label %wend114
wbody113:
  call void @Ownership_PushScope()
  %v135 = load i64, ptr %slot45
  %v136 = add i64 %v135, 1
  store i64 %v136, ptr %slot45
  call void @Ownership_PopScope()
  br label %while112
wend114:
  call void @Ownership_PopScope()
  br label %merge111
else110:
  call void @Ownership_PushScope()
  %v137 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v137)
  call void @Ownership_Give(i64 %v137)
  %str138 = getelementptr [2 x i8], ptr @.str.5.8, i64 0, i64 0
  %v139 = ptrtoint ptr %str138 to i64
  %v140 = call i64 @Text_Equals(i64 %v137, i64 %v139)
  %cond141 = icmp ne i64 %v140, 0
  br i1 %cond141, label %then142, label %else143
then142:
  call void @Ownership_PushScope()
  %v145 = load i64, ptr %slot45
  %v146 = add i64 %v145, 1
  store i64 %v146, ptr %slot45
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v147 = load i64, ptr %slot47
  %v148 = add i64 %v147, 1
  store i64 %v148, ptr %slot47
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while149
while149:
  %v152 = load i64, ptr %slot45
  %v153 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v153)
  call void @Ownership_Give(i64 %v153)
  %v154 = call i64 @Text_Length(i64 %v153)
  %v155 = icmp slt i64 %v152, %v154
  %v156 = zext i1 %v155 to i64
  %logic159 = alloca i64
  %cond160 = icmp ne i64 %v156, 0
  store i64 0, ptr %logic159
  br i1 %cond160, label %rhs157, label %logic158
rhs157:
  %v161 = load i64, ptr %slot45
  %v162 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v162)
  call void @Ownership_Give(i64 %v162)
  %v163 = call i64 @Text_Substring(i64 %v162, i64 %v161, i64 1)
  %str164 = getelementptr [2 x i8], ptr @.str.5.9, i64 0, i64 0
  %v165 = ptrtoint ptr %str164 to i64
  %v166 = call i64 @Text_Equals(i64 %v163, i64 %v165)
  %v167 = xor i64 %v166, 1
  %cond168 = icmp ne i64 %v167, 0
  %v169 = zext i1 %cond168 to i64
  store i64 %v169, ptr %logic159
  br label %logic158
logic158:
  %v170 = load i64, ptr %logic159
  %cond171 = icmp ne i64 %v170, 0
  br i1 %cond171, label %wbody150, label %wend151
wbody150:
  call void @Ownership_PushScope()
  %v172 = load i64, ptr %slot45
  %v173 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v173)
  call void @Ownership_Give(i64 %v173)
  %v174 = call i64 @Text_Substring(i64 %v173, i64 %v172, i64 1)
  %str175 = getelementptr [2 x i8], ptr @.str.5.10, i64 0, i64 0
  %v176 = ptrtoint ptr %str175 to i64
  %v177 = call i64 @Text_Equals(i64 %v174, i64 %v176)
  %cond178 = icmp ne i64 %v177, 0
  br i1 %cond178, label %then179, label %else180
then179:
  call void @Ownership_PushScope()
  %v182 = load i64, ptr %slot45
  %v183 = add i64 %v182, 2
  store i64 %v183, ptr %slot45
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v184 = load i64, ptr %slot47
  %v185 = add i64 %v184, 2
  store i64 %v185, ptr %slot47
  call void @Ownership_PopScope()
  br label %merge181
else180:
  call void @Ownership_PushScope()
  %v186 = load i64, ptr %slot45
  %v187 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v187)
  call void @Ownership_Give(i64 %v187)
  %v188 = call i64 @Text_Substring(i64 %v187, i64 %v186, i64 1)
  %str189 = getelementptr [2 x i8], ptr @.str.5.11, i64 0, i64 0
  %v190 = ptrtoint ptr %str189 to i64
  %v191 = call i64 @Text_Equals(i64 %v188, i64 %v190)
  %cond192 = icmp ne i64 %v191, 0
  br i1 %cond192, label %then193, label %else194
then193:
  call void @Ownership_PushScope()
  %v196 = load i64, ptr %slot46
  %v197 = add i64 %v196, 1
  store i64 %v197, ptr %slot46
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  store i64 1, ptr %slot47
  call void @Ownership_PopScope()
  br label %merge195
else194:
  call void @Ownership_PushScope()
  %v198 = load i64, ptr %slot47
  %v199 = add i64 %v198, 1
  store i64 %v199, ptr %slot47
  call void @Ownership_PopScope()
  br label %merge195
merge195:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v200 = load i64, ptr %slot45
  %v201 = add i64 %v200, 1
  store i64 %v201, ptr %slot45
  call void @Ownership_PopScope()
  br label %merge181
merge181:
  call void @Ownership_PopScope()
  br label %while149
wend151:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v202 = load i64, ptr %slot45
  %v203 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v203)
  call void @Ownership_Give(i64 %v203)
  %v204 = call i64 @Text_Length(i64 %v203)
  %v205 = icmp slt i64 %v202, %v204
  %v206 = zext i1 %v205 to i64
  %cond207 = icmp ne i64 %v206, 0
  br i1 %cond207, label %then208, label %else209
then208:
  call void @Ownership_PushScope()
  %v211 = load i64, ptr %slot45
  %v212 = add i64 %v211, 1
  store i64 %v212, ptr %slot45
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v213 = load i64, ptr %slot47
  %v214 = add i64 %v213, 1
  store i64 %v214, ptr %slot47
  call void @Ownership_PopScope()
  br label %merge210
else209:
  br label %merge210
merge210:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str215 = getelementptr [1 x i8], ptr @.str.5.12, i64 0, i64 0
  %v216 = ptrtoint ptr %str215 to i64
  %v217 = call i64 @Ownership_Keep(i64 %v216)
  %cond218 = icmp eq i64 %v217, 0
  br i1 %cond218, label %own219, label %owned220
own219:
  call void @Ownership_Retain(i64 %v216)
  br label %owned220
owned220:
  %v221 = load i64, ptr %slot44
  call void @Ownership_Release(i64 %v221)
  store i64 %v216, ptr %slot44
  call void @Ownership_PopScope()
  br label %merge144
else143:
  call void @Ownership_PushScope()
  %v222 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v222)
  call void @Ownership_Give(i64 %v222)
  %v223 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v223)
  call void @Ownership_Give(i64 %v223)
  %v224 = call i64 @rxm_ImportHost_IsNameStart(i64 %v223, i64 %v222)
  %cond225 = icmp ne i64 %v224, 0
  br i1 %cond225, label %then226, label %else227
then226:
  call void @Ownership_PushScope()
  %v229 = load i64, ptr %slot47
  %slot230 = alloca i64
  store i64 %v229, ptr %slot230
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str231 = getelementptr [1 x i8], ptr @.str.5.13, i64 0, i64 0
  %v232 = ptrtoint ptr %str231 to i64
  %v233 = call i64 @Ownership_Keep(i64 %v232)
  %cond234 = icmp eq i64 %v233, 0
  br i1 %cond234, label %own235, label %owned236
own235:
  call void @Ownership_Retain(i64 %v232)
  br label %owned236
owned236:
  %slot237 = alloca i64
  store i64 %v232, ptr %slot237
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while238
while238:
  %v241 = load i64, ptr %slot45
  %v242 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v242)
  call void @Ownership_Give(i64 %v242)
  %v243 = call i64 @Text_Length(i64 %v242)
  %v244 = icmp slt i64 %v241, %v243
  %v245 = zext i1 %v244 to i64
  %logic248 = alloca i64
  %cond249 = icmp ne i64 %v245, 0
  store i64 0, ptr %logic248
  br i1 %cond249, label %rhs246, label %logic247
rhs246:
  %v250 = load i64, ptr %slot45
  %v251 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v251)
  call void @Ownership_Give(i64 %v251)
  %v252 = call i64 @Text_Substring(i64 %v251, i64 %v250, i64 1)
  %v253 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v253)
  call void @Ownership_Give(i64 %v253)
  %v254 = call i64 @rxm_ImportHost_IsNamePart(i64 %v253, i64 %v252)
  %cond255 = icmp ne i64 %v254, 0
  %v256 = zext i1 %cond255 to i64
  store i64 %v256, ptr %logic248
  br label %logic247
logic247:
  %v257 = load i64, ptr %logic248
  %cond258 = icmp ne i64 %v257, 0
  br i1 %cond258, label %wbody239, label %wend240
wbody239:
  call void @Ownership_PushScope()
  %v259 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v259)
  call void @Ownership_Give(i64 %v259)
  %v260 = load i64, ptr %slot45
  %v261 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v261)
  call void @Ownership_Give(i64 %v261)
  %v262 = call i64 @Text_Substring(i64 %v261, i64 %v260, i64 1)
  %v263 = call i64 @Text_Concat(i64 %v259, i64 %v262)
  %v264 = call i64 @Ownership_Keep(i64 %v263)
  %cond265 = icmp eq i64 %v264, 0
  br i1 %cond265, label %own266, label %owned267
own266:
  call void @Ownership_Retain(i64 %v263)
  br label %owned267
owned267:
  %v268 = load i64, ptr %slot237
  call void @Ownership_Release(i64 %v268)
  store i64 %v263, ptr %slot237
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v269 = load i64, ptr %slot45
  %v270 = add i64 %v269, 1
  store i64 %v270, ptr %slot45
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v271 = load i64, ptr %slot47
  %v272 = add i64 %v271, 1
  store i64 %v272, ptr %slot47
  call void @Ownership_PopScope()
  br label %while238
wend240:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v273 = load i64, ptr %slot44
  call void @Ownership_Retain(i64 %v273)
  call void @Ownership_Give(i64 %v273)
  %str274 = getelementptr [2 x i8], ptr @.str.5.14, i64 0, i64 0
  %v275 = ptrtoint ptr %str274 to i64
  %v276 = call i64 @Text_Equals(i64 %v273, i64 %v275)
  %v277 = xor i64 %v276, 1
  %logic280 = alloca i64
  %cond281 = icmp ne i64 %v277, 0
  store i64 0, ptr %logic280
  br i1 %cond281, label %rhs278, label %logic279
rhs278:
  %v282 = load i64, ptr %slot44
  call void @Ownership_Retain(i64 %v282)
  call void @Ownership_Give(i64 %v282)
  %v283 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v283)
  call void @Ownership_Give(i64 %v283)
  %v284 = call i64 @rxm_ImportHost_IsBinder(i64 %v283, i64 %v282)
  %v285 = icmp eq i64 %v284, 0
  %v286 = zext i1 %v285 to i64
  %cond287 = icmp ne i64 %v286, 0
  %v288 = zext i1 %cond287 to i64
  store i64 %v288, ptr %logic280
  br label %logic279
logic279:
  %v289 = load i64, ptr %logic280
  %logic292 = alloca i64
  %cond293 = icmp ne i64 %v289, 0
  store i64 0, ptr %logic292
  br i1 %cond293, label %rhs290, label %logic291
rhs290:
  %str294 = getelementptr [2 x i8], ptr @.str.5.15, i64 0, i64 0
  %v295 = ptrtoint ptr %str294 to i64
  %v296 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v296)
  call void @Ownership_Give(i64 %v296)
  %v297 = call i64 @Text_Concat(i64 %v295, i64 %v296)
  %str298 = getelementptr [2 x i8], ptr @.str.5.16, i64 0, i64 0
  %v299 = ptrtoint ptr %str298 to i64
  %v300 = call i64 @Text_Concat(i64 %v297, i64 %v299)
  %v301 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v301)
  call void @Ownership_Give(i64 %v301)
  %v302 = call i64 @Text_Contains(i64 %v301, i64 %v300)
  %cond303 = icmp ne i64 %v302, 0
  %v304 = zext i1 %cond303 to i64
  store i64 %v304, ptr %logic292
  br label %logic291
logic291:
  %v305 = load i64, ptr %logic292
  %logic308 = alloca i64
  %cond309 = icmp ne i64 %v305, 0
  store i64 0, ptr %logic308
  br i1 %cond309, label %rhs306, label %logic307
rhs306:
  %str310 = getelementptr [2 x i8], ptr @.str.5.17, i64 0, i64 0
  %v311 = ptrtoint ptr %str310 to i64
  %v312 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v312)
  call void @Ownership_Give(i64 %v312)
  %v313 = call i64 @Text_Concat(i64 %v311, i64 %v312)
  %str314 = getelementptr [2 x i8], ptr @.str.5.18, i64 0, i64 0
  %v315 = ptrtoint ptr %str314 to i64
  %v316 = call i64 @Text_Concat(i64 %v313, i64 %v315)
  %v317 = load i64, ptr %slot30
  call void @Ownership_Retain(i64 %v317)
  call void @Ownership_Give(i64 %v317)
  %v318 = call i64 @Text_Contains(i64 %v317, i64 %v316)
  %v319 = icmp eq i64 %v318, 0
  %v320 = zext i1 %v319 to i64
  %cond321 = icmp ne i64 %v320, 0
  %v322 = zext i1 %cond321 to i64
  store i64 %v322, ptr %logic308
  br label %logic307
logic307:
  %v323 = load i64, ptr %logic308
  %logic326 = alloca i64
  %cond327 = icmp ne i64 %v323, 0
  store i64 0, ptr %logic326
  br i1 %cond327, label %rhs324, label %logic325
rhs324:
  %str328 = getelementptr [2 x i8], ptr @.str.5.19, i64 0, i64 0
  %v329 = ptrtoint ptr %str328 to i64
  %v330 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v330)
  call void @Ownership_Give(i64 %v330)
  %v331 = call i64 @Text_Concat(i64 %v329, i64 %v330)
  %str332 = getelementptr [2 x i8], ptr @.str.5.20, i64 0, i64 0
  %v333 = ptrtoint ptr %str332 to i64
  %v334 = call i64 @Text_Concat(i64 %v331, i64 %v333)
  %v335 = load i64, ptr %slot37
  call void @Ownership_Retain(i64 %v335)
  call void @Ownership_Give(i64 %v335)
  %v336 = call i64 @Text_Contains(i64 %v335, i64 %v334)
  %v337 = icmp eq i64 %v336, 0
  %v338 = zext i1 %v337 to i64
  %cond339 = icmp ne i64 %v338, 0
  %v340 = zext i1 %cond339 to i64
  store i64 %v340, ptr %logic326
  br label %logic325
logic325:
  %v341 = load i64, ptr %logic326
  %cond342 = icmp ne i64 %v341, 0
  br i1 %cond342, label %then343, label %else344
then343:
  call void @Ownership_PushScope()
  %v346 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v346)
  call void @Ownership_Give(i64 %v346)
  %v347 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v347)
  call void @Ownership_Give(i64 %v347)
  %v348 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v348)
  call void @Ownership_Give(i64 %v348)
  %v349 = call i64 @rxm_ImportHost_CatalogPackage(i64 %v348, i64 %v346, i64 %v347)
  %v350 = call i64 @Ownership_Keep(i64 %v349)
  %cond351 = icmp eq i64 %v350, 0
  br i1 %cond351, label %own352, label %owned353
own352:
  call void @Ownership_Retain(i64 %v349)
  br label %owned353
owned353:
  %slot354 = alloca i64
  store i64 %v349, ptr %slot354
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v355 = load i64, ptr %slot354
  call void @Ownership_Retain(i64 %v355)
  call void @Ownership_Give(i64 %v355)
  %str356 = getelementptr [1 x i8], ptr @.str.5.21, i64 0, i64 0
  %v357 = ptrtoint ptr %str356 to i64
  %v358 = call i64 @Text_Equals(i64 %v355, i64 %v357)
  %v359 = xor i64 %v358, 1
  %logic362 = alloca i64
  %cond363 = icmp ne i64 %v359, 0
  store i64 0, ptr %logic362
  br i1 %cond363, label %rhs360, label %logic361
rhs360:
  %str364 = getelementptr [2 x i8], ptr @.str.5.22, i64 0, i64 0
  %v365 = ptrtoint ptr %str364 to i64
  %v366 = load i64, ptr %slot354
  call void @Ownership_Retain(i64 %v366)
  call void @Ownership_Give(i64 %v366)
  %v367 = call i64 @Text_Concat(i64 %v365, i64 %v366)
  %str368 = getelementptr [2 x i8], ptr @.str.5.23, i64 0, i64 0
  %v369 = ptrtoint ptr %str368 to i64
  %v370 = call i64 @Text_Concat(i64 %v367, i64 %v369)
  %v371 = load i64, ptr %slot22
  call void @Ownership_Retain(i64 %v371)
  call void @Ownership_Give(i64 %v371)
  %v372 = call i64 @Text_Contains(i64 %v371, i64 %v370)
  %v373 = icmp eq i64 %v372, 0
  %v374 = zext i1 %v373 to i64
  %cond375 = icmp ne i64 %v374, 0
  %v376 = zext i1 %cond375 to i64
  store i64 %v376, ptr %logic362
  br label %logic361
logic361:
  %v377 = load i64, ptr %logic362
  %cond378 = icmp ne i64 %v377, 0
  br i1 %cond378, label %then379, label %else380
then379:
  call void @Ownership_PushScope()
  %v382 = load i64, ptr %slot3
  %p383 = inttoptr i64 %v382 to ptr
  %s384 = getelementptr i64, ptr %p383, i64 0
  %v385 = load i64, ptr %s384
  %str386 = getelementptr [13 x i8], ptr @.str.5.24, i64 0, i64 0
  %v387 = ptrtoint ptr %str386 to i64
  %v388 = call i64 @Text_Concat(i64 %v385, i64 %v387)
  %v389 = load i64, ptr %slot46
  %v390 = call i64 @Text_FromInt(i64 %v389)
  %v391 = call i64 @Text_Concat(i64 %v388, i64 %v390)
  %str392 = getelementptr [2 x i8], ptr @.str.5.25, i64 0, i64 0
  %v393 = ptrtoint ptr %str392 to i64
  %v394 = call i64 @Text_Concat(i64 %v391, i64 %v393)
  %v395 = load i64, ptr %slot230
  %v396 = call i64 @Text_FromInt(i64 %v395)
  %v397 = call i64 @Text_Concat(i64 %v394, i64 %v396)
  %str398 = getelementptr [3 x i8], ptr @.str.5.26, i64 0, i64 0
  %v399 = ptrtoint ptr %str398 to i64
  %v400 = call i64 @Text_Concat(i64 %v397, i64 %v399)
  %v401 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v401)
  call void @Ownership_Give(i64 %v401)
  %v402 = call i64 @Text_Concat(i64 %v400, i64 %v401)
  %str403 = getelementptr [14 x i8], ptr @.str.5.27, i64 0, i64 0
  %v404 = ptrtoint ptr %str403 to i64
  %v405 = call i64 @Text_Concat(i64 %v402, i64 %v404)
  %v406 = load i64, ptr %slot354
  call void @Ownership_Retain(i64 %v406)
  call void @Ownership_Give(i64 %v406)
  %v407 = call i64 @Text_Concat(i64 %v405, i64 %v406)
  %str408 = getelementptr [19 x i8], ptr @.str.5.28, i64 0, i64 0
  %v409 = ptrtoint ptr %str408 to i64
  %v410 = call i64 @Text_Concat(i64 %v407, i64 %v409)
  %v411 = load i64, ptr %slot354
  call void @Ownership_Retain(i64 %v411)
  call void @Ownership_Give(i64 %v411)
  %v412 = call i64 @Text_Concat(i64 %v410, i64 %v411)
  %str413 = getelementptr [39 x i8], ptr @.str.5.29, i64 0, i64 0
  %v414 = ptrtoint ptr %str413 to i64
  %v415 = call i64 @Text_Concat(i64 %v412, i64 %v414)
  %v416 = load i64, ptr %slot354
  call void @Ownership_Retain(i64 %v416)
  call void @Ownership_Give(i64 %v416)
  %v417 = call i64 @Text_Concat(i64 %v415, i64 %v416)
  %str418 = getelementptr [19 x i8], ptr @.str.5.30, i64 0, i64 0
  %v419 = ptrtoint ptr %str418 to i64
  %v420 = call i64 @Text_Concat(i64 %v417, i64 %v419)
  %v421 = load i64, ptr %slot3
  %p424 = inttoptr i64 %v421 to ptr
  %s425 = getelementptr i64, ptr %p424, i64 0
  %v426 = load i64, ptr %s425
  call void @Ownership_Retain(i64 %v420)
  call void @Ownership_Release(i64 %v426)
  %p422 = inttoptr i64 %v421 to ptr
  %s423 = getelementptr i64, ptr %p422, i64 0
  store i64 %v420, ptr %s423
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v427 = load i64, ptr %slot37
  call void @Ownership_Retain(i64 %v427)
  call void @Ownership_Give(i64 %v427)
  %v428 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v428)
  call void @Ownership_Give(i64 %v428)
  %v429 = call i64 @Text_Concat(i64 %v427, i64 %v428)
  %str430 = getelementptr [2 x i8], ptr @.str.5.31, i64 0, i64 0
  %v431 = ptrtoint ptr %str430 to i64
  %v432 = call i64 @Text_Concat(i64 %v429, i64 %v431)
  %v433 = call i64 @Ownership_Keep(i64 %v432)
  %cond434 = icmp eq i64 %v433, 0
  br i1 %cond434, label %own435, label %owned436
own435:
  call void @Ownership_Retain(i64 %v432)
  br label %owned436
owned436:
  %v437 = load i64, ptr %slot37
  call void @Ownership_Release(i64 %v437)
  store i64 %v432, ptr %slot37
  call void @Ownership_PopScope()
  br label %merge381
else380:
  br label %merge381
merge381:
  call void @Ownership_PopScope()
  %v438 = load i64, ptr %slot354
  call void @Ownership_Release(i64 %v438)
  br label %merge345
else344:
  br label %merge345
merge345:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v439 = load i64, ptr %slot237
  call void @Ownership_Retain(i64 %v439)
  call void @Ownership_Give(i64 %v439)
  %v440 = call i64 @Ownership_Keep(i64 %v439)
  %cond441 = icmp eq i64 %v440, 0
  br i1 %cond441, label %own442, label %owned443
own442:
  call void @Ownership_Retain(i64 %v439)
  br label %owned443
owned443:
  %v444 = load i64, ptr %slot44
  call void @Ownership_Release(i64 %v444)
  store i64 %v439, ptr %slot44
  call void @Ownership_PopScope()
  %v445 = load i64, ptr %slot237
  call void @Ownership_Release(i64 %v445)
  br label %merge228
else227:
  call void @Ownership_PushScope()
  %v446 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v446)
  call void @Ownership_Give(i64 %v446)
  %str447 = getelementptr [2 x i8], ptr @.str.5.32, i64 0, i64 0
  %v448 = ptrtoint ptr %str447 to i64
  %v449 = call i64 @Text_Equals(i64 %v446, i64 %v448)
  %v450 = xor i64 %v449, 1
  %logic453 = alloca i64
  %cond454 = icmp ne i64 %v450, 0
  store i64 0, ptr %logic453
  br i1 %cond454, label %rhs451, label %logic452
rhs451:
  %v455 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v455)
  call void @Ownership_Give(i64 %v455)
  %str456 = getelementptr [2 x i8], ptr @.str.5.33, i64 0, i64 0
  %v457 = ptrtoint ptr %str456 to i64
  %v458 = call i64 @Text_Equals(i64 %v455, i64 %v457)
  %v459 = xor i64 %v458, 1
  %cond460 = icmp ne i64 %v459, 0
  %v461 = zext i1 %cond460 to i64
  store i64 %v461, ptr %logic453
  br label %logic452
logic452:
  %v462 = load i64, ptr %logic453
  %logic465 = alloca i64
  %cond466 = icmp ne i64 %v462, 0
  store i64 0, ptr %logic465
  br i1 %cond466, label %rhs463, label %logic464
rhs463:
  %v467 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v467)
  call void @Ownership_Give(i64 %v467)
  %str468 = getelementptr [2 x i8], ptr @.str.5.34, i64 0, i64 0
  %v469 = ptrtoint ptr %str468 to i64
  %v470 = call i64 @Text_Equals(i64 %v467, i64 %v469)
  %v471 = xor i64 %v470, 1
  %cond472 = icmp ne i64 %v471, 0
  %v473 = zext i1 %cond472 to i64
  store i64 %v473, ptr %logic465
  br label %logic464
logic464:
  %v474 = load i64, ptr %logic465
  %cond475 = icmp ne i64 %v474, 0
  br i1 %cond475, label %then476, label %else477
then476:
  call void @Ownership_PushScope()
  %v479 = load i64, ptr %slot64
  call void @Ownership_Retain(i64 %v479)
  call void @Ownership_Give(i64 %v479)
  %v480 = call i64 @Ownership_Keep(i64 %v479)
  %cond481 = icmp eq i64 %v480, 0
  br i1 %cond481, label %own482, label %owned483
own482:
  call void @Ownership_Retain(i64 %v479)
  br label %owned483
owned483:
  %v484 = load i64, ptr %slot44
  call void @Ownership_Release(i64 %v484)
  store i64 %v479, ptr %slot44
  call void @Ownership_PopScope()
  br label %merge478
else477:
  br label %merge478
merge478:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v485 = load i64, ptr %slot45
  %v486 = add i64 %v485, 1
  store i64 %v486, ptr %slot45
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v487 = load i64, ptr %slot47
  %v488 = add i64 %v487, 1
  store i64 %v488, ptr %slot47
  call void @Ownership_PopScope()
  br label %merge228
merge228:
  call void @Ownership_PopScope()
  br label %merge144
merge144:
  call void @Ownership_PopScope()
  br label %merge111
merge111:
  call void @Ownership_PopScope()
  br label %merge72
merge72:
  call void @Ownership_PopScope()
  %v489 = load i64, ptr %slot64
  call void @Ownership_Release(i64 %v489)
  br label %while48
wend50:
  call void @Ownership_PopScope()
  %v490 = load i64, ptr %slot44
  call void @Ownership_Release(i64 %v490)
  %v491 = load i64, ptr %slot37
  call void @Ownership_Release(i64 %v491)
  %v492 = load i64, ptr %slot30
  call void @Ownership_Release(i64 %v492)
  %v493 = load i64, ptr %slot22
  call void @Ownership_Release(i64 %v493)
  %v494 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v494)
  %v495 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v495)
  %v496 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v496)
  %v497 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v497)
  ret void
}

@.str.6.1 = private unnamed_addr constant [6 x i8] c"Class\00"
@.str.6.2 = private unnamed_addr constant [7 x i8] c"Struct\00"
@.str.6.3 = private unnamed_addr constant [5 x i8] c"Enum\00"
@.str.6.4 = private unnamed_addr constant [10 x i8] c"Interface\00"
@.str.6.5 = private unnamed_addr constant [9 x i8] c"Function\00"
@.str.6.6 = private unnamed_addr constant [5 x i8] c"Test\00"
@.str.6.7 = private unnamed_addr constant [8 x i8] c"Package\00"
@.str.6.8 = private unnamed_addr constant [7 x i8] c"Import\00"
define i64 @rxm_ImportHost_IsBinder(i64 %p_This, i64 %p_token) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_token)
  %slot4 = alloca i64
  store i64 %p_token, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %str6 = getelementptr [6 x i8], ptr @.str.6.1, i64 0, i64 0
  %v7 = ptrtoint ptr %str6 to i64
  %v8 = call i64 @Text_Equals(i64 %v5, i64 %v7)
  %logic11 = alloca i64
  %cond12 = icmp ne i64 %v8, 0
  store i64 1, ptr %logic11
  br i1 %cond12, label %logic10, label %rhs9
rhs9:
  %v13 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v13)
  call void @Ownership_Give(i64 %v13)
  %str14 = getelementptr [7 x i8], ptr @.str.6.2, i64 0, i64 0
  %v15 = ptrtoint ptr %str14 to i64
  %v16 = call i64 @Text_Equals(i64 %v13, i64 %v15)
  %cond17 = icmp ne i64 %v16, 0
  %v18 = zext i1 %cond17 to i64
  store i64 %v18, ptr %logic11
  br label %logic10
logic10:
  %v19 = load i64, ptr %logic11
  %logic22 = alloca i64
  %cond23 = icmp ne i64 %v19, 0
  store i64 1, ptr %logic22
  br i1 %cond23, label %logic21, label %rhs20
rhs20:
  %v24 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v24)
  call void @Ownership_Give(i64 %v24)
  %str25 = getelementptr [5 x i8], ptr @.str.6.3, i64 0, i64 0
  %v26 = ptrtoint ptr %str25 to i64
  %v27 = call i64 @Text_Equals(i64 %v24, i64 %v26)
  %cond28 = icmp ne i64 %v27, 0
  %v29 = zext i1 %cond28 to i64
  store i64 %v29, ptr %logic22
  br label %logic21
logic21:
  %v30 = load i64, ptr %logic22
  %logic33 = alloca i64
  %cond34 = icmp ne i64 %v30, 0
  store i64 1, ptr %logic33
  br i1 %cond34, label %logic32, label %rhs31
rhs31:
  %v35 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v35)
  call void @Ownership_Give(i64 %v35)
  %str36 = getelementptr [10 x i8], ptr @.str.6.4, i64 0, i64 0
  %v37 = ptrtoint ptr %str36 to i64
  %v38 = call i64 @Text_Equals(i64 %v35, i64 %v37)
  %cond39 = icmp ne i64 %v38, 0
  %v40 = zext i1 %cond39 to i64
  store i64 %v40, ptr %logic33
  br label %logic32
logic32:
  %v41 = load i64, ptr %logic33
  %logic44 = alloca i64
  %cond45 = icmp ne i64 %v41, 0
  store i64 1, ptr %logic44
  br i1 %cond45, label %logic43, label %rhs42
rhs42:
  %v46 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v46)
  call void @Ownership_Give(i64 %v46)
  %str47 = getelementptr [9 x i8], ptr @.str.6.5, i64 0, i64 0
  %v48 = ptrtoint ptr %str47 to i64
  %v49 = call i64 @Text_Equals(i64 %v46, i64 %v48)
  %cond50 = icmp ne i64 %v49, 0
  %v51 = zext i1 %cond50 to i64
  store i64 %v51, ptr %logic44
  br label %logic43
logic43:
  %v52 = load i64, ptr %logic44
  %logic55 = alloca i64
  %cond56 = icmp ne i64 %v52, 0
  store i64 1, ptr %logic55
  br i1 %cond56, label %logic54, label %rhs53
rhs53:
  %v57 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v57)
  call void @Ownership_Give(i64 %v57)
  %str58 = getelementptr [5 x i8], ptr @.str.6.6, i64 0, i64 0
  %v59 = ptrtoint ptr %str58 to i64
  %v60 = call i64 @Text_Equals(i64 %v57, i64 %v59)
  %cond61 = icmp ne i64 %v60, 0
  %v62 = zext i1 %cond61 to i64
  store i64 %v62, ptr %logic55
  br label %logic54
logic54:
  %v63 = load i64, ptr %logic55
  %logic66 = alloca i64
  %cond67 = icmp ne i64 %v63, 0
  store i64 1, ptr %logic66
  br i1 %cond67, label %logic65, label %rhs64
rhs64:
  %v68 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v68)
  call void @Ownership_Give(i64 %v68)
  %str69 = getelementptr [8 x i8], ptr @.str.6.7, i64 0, i64 0
  %v70 = ptrtoint ptr %str69 to i64
  %v71 = call i64 @Text_Equals(i64 %v68, i64 %v70)
  %cond72 = icmp ne i64 %v71, 0
  %v73 = zext i1 %cond72 to i64
  store i64 %v73, ptr %logic66
  br label %logic65
logic65:
  %v74 = load i64, ptr %logic66
  %logic77 = alloca i64
  %cond78 = icmp ne i64 %v74, 0
  store i64 1, ptr %logic77
  br i1 %cond78, label %logic76, label %rhs75
rhs75:
  %v79 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v79)
  call void @Ownership_Give(i64 %v79)
  %str80 = getelementptr [7 x i8], ptr @.str.6.8, i64 0, i64 0
  %v81 = ptrtoint ptr %str80 to i64
  %v82 = call i64 @Text_Equals(i64 %v79, i64 %v81)
  %cond83 = icmp ne i64 %v82, 0
  %v84 = zext i1 %cond83 to i64
  store i64 %v84, ptr %logic77
  br label %logic76
logic76:
  %v85 = load i64, ptr %logic77
  %cond86 = icmp ne i64 %v85, 0
  br i1 %cond86, label %then87, label %else88
then87:
  call void @Ownership_PushScope()
  %v90 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v90)
  %v91 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v91)
  %v92 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v92)
  ret i64 1
else88:
  br label %merge89
merge89:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v93 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v93)
  ret i64 0
}

@.str.7.1 = private unnamed_addr constant [54 x i8] c"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_\00"
define i64 @rxm_ImportHost_IsNameStart(i64 %p_This, i64 %p_piece) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_piece)
  %slot4 = alloca i64
  store i64 %p_piece, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %str6 = getelementptr [54 x i8], ptr @.str.7.1, i64 0, i64 0
  %v7 = ptrtoint ptr %str6 to i64
  %v8 = call i64 @Text_Contains(i64 %v7, i64 %v5)
  %v9 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v9)
  %v10 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v10)
  %v11 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v11)
  ret i64 %v8
}

@.str.8.1 = private unnamed_addr constant [11 x i8] c"0123456789\00"
define i64 @rxm_ImportHost_IsNamePart(i64 %p_This, i64 %p_piece) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_piece)
  %slot4 = alloca i64
  store i64 %p_piece, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %v6 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v6)
  call void @Ownership_Give(i64 %v6)
  %v7 = call i64 @rxm_ImportHost_IsNameStart(i64 %v6, i64 %v5)
  %cond8 = icmp ne i64 %v7, 0
  br i1 %cond8, label %then9, label %else10
then9:
  call void @Ownership_PushScope()
  %v12 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v12)
  %v13 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v13)
  %v14 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v14)
  ret i64 1
else10:
  br label %merge11
merge11:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v15 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v15)
  call void @Ownership_Give(i64 %v15)
  %str16 = getelementptr [11 x i8], ptr @.str.8.1, i64 0, i64 0
  %v17 = ptrtoint ptr %str16 to i64
  %v18 = call i64 @Text_Contains(i64 %v17, i64 %v15)
  %v19 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v19)
  ret i64 %v18
}

@.str.9.1 = private unnamed_addr constant [2 x i8] c";\00"
@.str.9.2 = private unnamed_addr constant [9 x i8] c"Packages\00"
@.str.9.3 = private unnamed_addr constant [4 x i8] c".rx\00"
@.str.9.4 = private unnamed_addr constant [9 x i8] c".test.rx\00"
@.str.9.5 = private unnamed_addr constant [10 x i8] c"Packages/\00"
define i64 @rxm_ImportHost_TypeCatalog(i64 %p_This) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_PushScope()
  %str4 = getelementptr [2 x i8], ptr @.str.9.1, i64 0, i64 0
  %v5 = ptrtoint ptr %str4 to i64
  %v6 = call i64 @Ownership_Keep(i64 %v5)
  %cond7 = icmp eq i64 %v6, 0
  br i1 %cond7, label %own8, label %owned9
own8:
  call void @Ownership_Retain(i64 %v5)
  br label %owned9
owned9:
  %slot10 = alloca i64
  store i64 %v5, ptr %slot10
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str11 = getelementptr [9 x i8], ptr @.str.9.2, i64 0, i64 0
  %v12 = ptrtoint ptr %str11 to i64
  %v13 = call i64 @Files_List(i64 %v12)
  %v14 = call i64 @Ownership_Keep(i64 %v13)
  %cond15 = icmp eq i64 %v14, 0
  br i1 %cond15, label %own16, label %owned17
own16:
  call void @Ownership_Retain(i64 %v13)
  br label %owned17
owned17:
  %slot18 = alloca i64
  store i64 %v13, ptr %slot18
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot19 = alloca i64
  store i64 0, ptr %slot19
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while20
while20:
  %v23 = load i64, ptr %slot19
  %v24 = load i64, ptr %slot18
  call void @Ownership_Retain(i64 %v24)
  call void @Ownership_Give(i64 %v24)
  %p25 = inttoptr i64 %v24 to ptr
  %s26 = getelementptr i64, ptr %p25, i64 1
  %v27 = load i64, ptr %s26
  %v28 = icmp slt i64 %v23, %v27
  %v29 = zext i1 %v28 to i64
  %cond30 = icmp ne i64 %v29, 0
  br i1 %cond30, label %wbody21, label %wend22
wbody21:
  call void @Ownership_PushScope()
  %v31 = load i64, ptr %slot18
  call void @Ownership_Retain(i64 %v31)
  call void @Ownership_Give(i64 %v31)
  %v32 = load i64, ptr %slot19
  %p33 = inttoptr i64 %v31 to ptr
  %items34 = load ptr, ptr %p33
  %s35 = getelementptr i64, ptr %items34, i64 %v32
  %v36 = load i64, ptr %s35
  %v37 = call i64 @Ownership_Keep(i64 %v36)
  %cond38 = icmp eq i64 %v37, 0
  br i1 %cond38, label %own39, label %owned40
own39:
  call void @Ownership_Retain(i64 %v36)
  br label %owned40
owned40:
  %slot41 = alloca i64
  store i64 %v36, ptr %slot41
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v42 = load i64, ptr %slot19
  %v43 = add i64 %v42, 1
  store i64 %v43, ptr %slot19
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str44 = getelementptr [4 x i8], ptr @.str.9.3, i64 0, i64 0
  %v45 = ptrtoint ptr %str44 to i64
  %v46 = load i64, ptr %slot41
  call void @Ownership_Retain(i64 %v46)
  call void @Ownership_Give(i64 %v46)
  %v47 = call i64 @Text_EndsWith(i64 %v46, i64 %v45)
  %logic50 = alloca i64
  %cond51 = icmp ne i64 %v47, 0
  store i64 0, ptr %logic50
  br i1 %cond51, label %rhs48, label %logic49
rhs48:
  %str52 = getelementptr [9 x i8], ptr @.str.9.4, i64 0, i64 0
  %v53 = ptrtoint ptr %str52 to i64
  %v54 = load i64, ptr %slot41
  call void @Ownership_Retain(i64 %v54)
  call void @Ownership_Give(i64 %v54)
  %v55 = call i64 @Text_EndsWith(i64 %v54, i64 %v53)
  %v56 = icmp eq i64 %v55, 0
  %v57 = zext i1 %v56 to i64
  %cond58 = icmp ne i64 %v57, 0
  %v59 = zext i1 %cond58 to i64
  store i64 %v59, ptr %logic50
  br label %logic49
logic49:
  %v60 = load i64, ptr %logic50
  %cond61 = icmp ne i64 %v60, 0
  br i1 %cond61, label %then62, label %else63
then62:
  call void @Ownership_PushScope()
  %str65 = getelementptr [10 x i8], ptr @.str.9.5, i64 0, i64 0
  %v66 = ptrtoint ptr %str65 to i64
  %v67 = load i64, ptr %slot41
  call void @Ownership_Retain(i64 %v67)
  call void @Ownership_Give(i64 %v67)
  %v68 = call i64 @Text_Concat(i64 %v66, i64 %v67)
  %v69 = call i64 @Files_Read(i64 %v68)
  %v70 = call i64 @Ownership_Keep(i64 %v69)
  %cond71 = icmp eq i64 %v70, 0
  br i1 %cond71, label %own72, label %owned73
own72:
  call void @Ownership_Retain(i64 %v69)
  br label %owned73
owned73:
  %slot74 = alloca i64
  store i64 %v69, ptr %slot74
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v75 = load i64, ptr %slot74
  call void @Ownership_Retain(i64 %v75)
  call void @Ownership_Give(i64 %v75)
  %v76 = load i64, ptr %slot74
  call void @Ownership_Retain(i64 %v76)
  call void @Ownership_Give(i64 %v76)
  %v77 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v77)
  call void @Ownership_Give(i64 %v77)
  %v78 = call i64 @rxm_ImportHost_PackageName(i64 %v77, i64 %v76)
  %v79 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v79)
  call void @Ownership_Give(i64 %v79)
  %v80 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v80)
  call void @Ownership_Give(i64 %v80)
  %v81 = call i64 @rxm_ImportHost_CollectTypes(i64 %v80, i64 %v75, i64 %v78, i64 %v79)
  %v82 = call i64 @Ownership_Keep(i64 %v81)
  %cond83 = icmp eq i64 %v82, 0
  br i1 %cond83, label %own84, label %owned85
own84:
  call void @Ownership_Retain(i64 %v81)
  br label %owned85
owned85:
  %v86 = load i64, ptr %slot10
  call void @Ownership_Release(i64 %v86)
  store i64 %v81, ptr %slot10
  call void @Ownership_PopScope()
  %v87 = load i64, ptr %slot74
  call void @Ownership_Release(i64 %v87)
  br label %merge64
else63:
  br label %merge64
merge64:
  call void @Ownership_PopScope()
  %v88 = load i64, ptr %slot41
  call void @Ownership_Release(i64 %v88)
  br label %while20
wend22:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v89 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v89)
  call void @Ownership_Give(i64 %v89)
  %v90 = call i64 @Ownership_Keep(i64 %v89)
  %cond91 = icmp eq i64 %v90, 0
  br i1 %cond91, label %own92, label %owned93
own92:
  call void @Ownership_Retain(i64 %v89)
  br label %owned93
owned93:
  %v94 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v94)
  %v95 = load i64, ptr %slot18
  call void @Ownership_Release(i64 %v95)
  %v96 = load i64, ptr %slot10
  call void @Ownership_Release(i64 %v96)
  %v97 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v97)
  call void @Ownership_Give(i64 %v89)
  ret i64 %v89
}

@.str.10.1 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.10.2 = private unnamed_addr constant [9 x i8] c"Package \00"
@.str.10.3 = private unnamed_addr constant [1 x i8] c"\00"
define i64 @rxm_ImportHost_PackageName(i64 %p_This, i64 %p_text) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_PushScope()
  %slot5 = alloca i64
  store i64 0, ptr %slot5
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot6 = alloca i64
  store i64 0, ptr %slot6
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while7
while7:
  %v10 = load i64, ptr %slot5
  %v11 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v11)
  call void @Ownership_Give(i64 %v11)
  %v12 = call i64 @Text_Length(i64 %v11)
  %v13 = icmp sle i64 %v10, %v12
  %v14 = zext i1 %v13 to i64
  %cond15 = icmp ne i64 %v14, 0
  br i1 %cond15, label %wbody8, label %wend9
wbody8:
  call void @Ownership_PushScope()
  %slot16 = alloca i64
  store i64 0, ptr %slot16
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v17 = load i64, ptr %slot5
  %v18 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v18)
  call void @Ownership_Give(i64 %v18)
  %v19 = call i64 @Text_Length(i64 %v18)
  %v20 = icmp eq i64 %v17, %v19
  %v21 = zext i1 %v20 to i64
  %cond22 = icmp ne i64 %v21, 0
  br i1 %cond22, label %then23, label %else24
then23:
  call void @Ownership_PushScope()
  store i64 1, ptr %slot16
  call void @Ownership_PopScope()
  br label %merge25
else24:
  br label %merge25
merge25:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v26 = load i64, ptr %slot16
  %v27 = icmp eq i64 %v26, 1
  %v28 = zext i1 %v27 to i64
  %logic31 = alloca i64
  %cond32 = icmp ne i64 %v28, 0
  store i64 1, ptr %logic31
  br i1 %cond32, label %logic30, label %rhs29
rhs29:
  %v33 = load i64, ptr %slot5
  %v34 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v34)
  call void @Ownership_Give(i64 %v34)
  %v35 = call i64 @Text_Substring(i64 %v34, i64 %v33, i64 1)
  %str36 = getelementptr [2 x i8], ptr @.str.10.1, i64 0, i64 0
  %v37 = ptrtoint ptr %str36 to i64
  %v38 = call i64 @Text_Equals(i64 %v35, i64 %v37)
  %cond39 = icmp ne i64 %v38, 0
  %v40 = zext i1 %cond39 to i64
  store i64 %v40, ptr %logic31
  br label %logic30
logic30:
  %v41 = load i64, ptr %logic31
  %cond42 = icmp ne i64 %v41, 0
  br i1 %cond42, label %then43, label %else44
then43:
  call void @Ownership_PushScope()
  %v46 = load i64, ptr %slot6
  %v47 = load i64, ptr %slot5
  %v48 = load i64, ptr %slot6
  %v49 = sub i64 %v47, %v48
  %v50 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v50)
  call void @Ownership_Give(i64 %v50)
  %v51 = call i64 @Text_Substring(i64 %v50, i64 %v46, i64 %v49)
  %v52 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v52)
  call void @Ownership_Give(i64 %v52)
  %v53 = call i64 @rxm_ImportHost_TrimLeft(i64 %v52, i64 %v51)
  %v54 = call i64 @Ownership_Keep(i64 %v53)
  %cond55 = icmp eq i64 %v54, 0
  br i1 %cond55, label %own56, label %owned57
own56:
  call void @Ownership_Retain(i64 %v53)
  br label %owned57
owned57:
  %slot58 = alloca i64
  store i64 %v53, ptr %slot58
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str59 = getelementptr [9 x i8], ptr @.str.10.2, i64 0, i64 0
  %v60 = ptrtoint ptr %str59 to i64
  %v61 = load i64, ptr %slot58
  call void @Ownership_Retain(i64 %v61)
  call void @Ownership_Give(i64 %v61)
  %v62 = call i64 @Text_StartsWith(i64 %v61, i64 %v60)
  %cond63 = icmp ne i64 %v62, 0
  br i1 %cond63, label %then64, label %else65
then64:
  call void @Ownership_PushScope()
  %v67 = load i64, ptr %slot58
  call void @Ownership_Retain(i64 %v67)
  call void @Ownership_Give(i64 %v67)
  %v68 = call i64 @Text_Length(i64 %v67)
  %v69 = sub i64 %v68, 8
  %v70 = call i64 @Text_Substring(i64 %v67, i64 8, i64 %v69)
  %v71 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v71)
  call void @Ownership_Give(i64 %v71)
  %v72 = call i64 @rxm_ImportHost_ImportName(i64 %v71, i64 %v70)
  %v73 = call i64 @Ownership_Keep(i64 %v72)
  %cond74 = icmp eq i64 %v73, 0
  br i1 %cond74, label %own75, label %owned76
own75:
  call void @Ownership_Retain(i64 %v72)
  br label %owned76
owned76:
  %v77 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v77)
  %v78 = load i64, ptr %slot58
  call void @Ownership_Release(i64 %v78)
  %v79 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v79)
  %v80 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v80)
  call void @Ownership_Give(i64 %v72)
  ret i64 %v72
else65:
  br label %merge66
merge66:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v81 = load i64, ptr %slot5
  %v82 = add i64 %v81, 1
  store i64 %v82, ptr %slot6
  call void @Ownership_PopScope()
  br label %merge45
else44:
  br label %merge45
merge45:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v83 = load i64, ptr %slot5
  %v84 = add i64 %v83, 1
  store i64 %v84, ptr %slot5
  call void @Ownership_PopScope()
  br label %while7
wend9:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str85 = getelementptr [1 x i8], ptr @.str.10.3, i64 0, i64 0
  %v86 = ptrtoint ptr %str85 to i64
  %v87 = call i64 @Ownership_Keep(i64 %v86)
  %cond88 = icmp eq i64 %v87, 0
  br i1 %cond88, label %own89, label %owned90
own89:
  call void @Ownership_Retain(i64 %v86)
  br label %owned90
owned90:
  %v91 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v91)
  call void @Ownership_Give(i64 %v86)
  ret i64 %v86
}

@.str.11.1 = private unnamed_addr constant [1 x i8] c"\00"
@.str.11.2 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.11.3 = private unnamed_addr constant [1 x i8] c"\00"
@.str.11.4 = private unnamed_addr constant [2 x i8] c";\00"
@.str.11.5 = private unnamed_addr constant [2 x i8] c"|\00"
@.str.11.6 = private unnamed_addr constant [2 x i8] c"|\00"
@.str.11.7 = private unnamed_addr constant [2 x i8] c";\00"
define i64 @rxm_ImportHost_CollectTypes(i64 %p_This, i64 %p_text, i64 %p_packageName, i64 %p_catalog) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_Retain(i64 %p_packageName)
  %slot5 = alloca i64
  store i64 %p_packageName, ptr %slot5
  call void @Ownership_Retain(i64 %p_catalog)
  %slot6 = alloca i64
  store i64 %p_catalog, ptr %slot6
  call void @Ownership_PushScope()
  %v7 = load i64, ptr %slot6
  call void @Ownership_Retain(i64 %v7)
  call void @Ownership_Give(i64 %v7)
  %v8 = call i64 @Ownership_Keep(i64 %v7)
  %cond9 = icmp eq i64 %v8, 0
  br i1 %cond9, label %own10, label %owned11
own10:
  call void @Ownership_Retain(i64 %v7)
  br label %owned11
owned11:
  %slot12 = alloca i64
  store i64 %v7, ptr %slot12
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v13 = load i64, ptr %slot5
  call void @Ownership_Retain(i64 %v13)
  call void @Ownership_Give(i64 %v13)
  %str14 = getelementptr [1 x i8], ptr @.str.11.1, i64 0, i64 0
  %v15 = ptrtoint ptr %str14 to i64
  %v16 = call i64 @Text_Equals(i64 %v13, i64 %v15)
  %cond17 = icmp ne i64 %v16, 0
  br i1 %cond17, label %then18, label %else19
then18:
  call void @Ownership_PushScope()
  %v21 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v21)
  call void @Ownership_Give(i64 %v21)
  %v22 = call i64 @Ownership_Keep(i64 %v21)
  %cond23 = icmp eq i64 %v22, 0
  br i1 %cond23, label %own24, label %owned25
own24:
  call void @Ownership_Retain(i64 %v21)
  br label %owned25
owned25:
  %v26 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v26)
  %v27 = load i64, ptr %slot12
  call void @Ownership_Release(i64 %v27)
  %v28 = load i64, ptr %slot6
  call void @Ownership_Release(i64 %v28)
  %v29 = load i64, ptr %slot5
  call void @Ownership_Release(i64 %v29)
  %v30 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v30)
  %v31 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v31)
  call void @Ownership_Give(i64 %v21)
  ret i64 %v21
else19:
  br label %merge20
merge20:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot32 = alloca i64
  store i64 0, ptr %slot32
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot33 = alloca i64
  store i64 0, ptr %slot33
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while34
while34:
  %v37 = load i64, ptr %slot32
  %v38 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v38)
  call void @Ownership_Give(i64 %v38)
  %v39 = call i64 @Text_Length(i64 %v38)
  %v40 = icmp sle i64 %v37, %v39
  %v41 = zext i1 %v40 to i64
  %cond42 = icmp ne i64 %v41, 0
  br i1 %cond42, label %wbody35, label %wend36
wbody35:
  call void @Ownership_PushScope()
  %slot43 = alloca i64
  store i64 0, ptr %slot43
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v44 = load i64, ptr %slot32
  %v45 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v45)
  call void @Ownership_Give(i64 %v45)
  %v46 = call i64 @Text_Length(i64 %v45)
  %v47 = icmp eq i64 %v44, %v46
  %v48 = zext i1 %v47 to i64
  %cond49 = icmp ne i64 %v48, 0
  br i1 %cond49, label %then50, label %else51
then50:
  call void @Ownership_PushScope()
  store i64 1, ptr %slot43
  call void @Ownership_PopScope()
  br label %merge52
else51:
  br label %merge52
merge52:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v53 = load i64, ptr %slot43
  %v54 = icmp eq i64 %v53, 1
  %v55 = zext i1 %v54 to i64
  %logic58 = alloca i64
  %cond59 = icmp ne i64 %v55, 0
  store i64 1, ptr %logic58
  br i1 %cond59, label %logic57, label %rhs56
rhs56:
  %v60 = load i64, ptr %slot32
  %v61 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v61)
  call void @Ownership_Give(i64 %v61)
  %v62 = call i64 @Text_Substring(i64 %v61, i64 %v60, i64 1)
  %str63 = getelementptr [2 x i8], ptr @.str.11.2, i64 0, i64 0
  %v64 = ptrtoint ptr %str63 to i64
  %v65 = call i64 @Text_Equals(i64 %v62, i64 %v64)
  %cond66 = icmp ne i64 %v65, 0
  %v67 = zext i1 %cond66 to i64
  store i64 %v67, ptr %logic58
  br label %logic57
logic57:
  %v68 = load i64, ptr %logic58
  %cond69 = icmp ne i64 %v68, 0
  br i1 %cond69, label %then70, label %else71
then70:
  call void @Ownership_PushScope()
  %v73 = load i64, ptr %slot33
  %v74 = load i64, ptr %slot32
  %v75 = load i64, ptr %slot33
  %v76 = sub i64 %v74, %v75
  %v77 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v77)
  call void @Ownership_Give(i64 %v77)
  %v78 = call i64 @Text_Substring(i64 %v77, i64 %v73, i64 %v76)
  %v79 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v79)
  call void @Ownership_Give(i64 %v79)
  %v80 = call i64 @rxm_ImportHost_TrimLeft(i64 %v79, i64 %v78)
  %v81 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v81)
  call void @Ownership_Give(i64 %v81)
  %v82 = call i64 @rxm_ImportHost_TypeNameOn(i64 %v81, i64 %v80)
  %v83 = call i64 @Ownership_Keep(i64 %v82)
  %cond84 = icmp eq i64 %v83, 0
  br i1 %cond84, label %own85, label %owned86
own85:
  call void @Ownership_Retain(i64 %v82)
  br label %owned86
owned86:
  %slot87 = alloca i64
  store i64 %v82, ptr %slot87
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v88 = load i64, ptr %slot87
  call void @Ownership_Retain(i64 %v88)
  call void @Ownership_Give(i64 %v88)
  %str89 = getelementptr [1 x i8], ptr @.str.11.3, i64 0, i64 0
  %v90 = ptrtoint ptr %str89 to i64
  %v91 = call i64 @Text_Equals(i64 %v88, i64 %v90)
  %v92 = xor i64 %v91, 1
  %logic95 = alloca i64
  %cond96 = icmp ne i64 %v92, 0
  store i64 0, ptr %logic95
  br i1 %cond96, label %rhs93, label %logic94
rhs93:
  %str97 = getelementptr [2 x i8], ptr @.str.11.4, i64 0, i64 0
  %v98 = ptrtoint ptr %str97 to i64
  %v99 = load i64, ptr %slot87
  call void @Ownership_Retain(i64 %v99)
  call void @Ownership_Give(i64 %v99)
  %v100 = call i64 @Text_Concat(i64 %v98, i64 %v99)
  %str101 = getelementptr [2 x i8], ptr @.str.11.5, i64 0, i64 0
  %v102 = ptrtoint ptr %str101 to i64
  %v103 = call i64 @Text_Concat(i64 %v100, i64 %v102)
  %v104 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v104)
  call void @Ownership_Give(i64 %v104)
  %v105 = call i64 @Text_Contains(i64 %v104, i64 %v103)
  %v106 = icmp eq i64 %v105, 0
  %v107 = zext i1 %v106 to i64
  %cond108 = icmp ne i64 %v107, 0
  %v109 = zext i1 %cond108 to i64
  store i64 %v109, ptr %logic95
  br label %logic94
logic94:
  %v110 = load i64, ptr %logic95
  %cond111 = icmp ne i64 %v110, 0
  br i1 %cond111, label %then112, label %else113
then112:
  call void @Ownership_PushScope()
  %v115 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v115)
  call void @Ownership_Give(i64 %v115)
  %v116 = load i64, ptr %slot87
  call void @Ownership_Retain(i64 %v116)
  call void @Ownership_Give(i64 %v116)
  %v117 = call i64 @Text_Concat(i64 %v115, i64 %v116)
  %str118 = getelementptr [2 x i8], ptr @.str.11.6, i64 0, i64 0
  %v119 = ptrtoint ptr %str118 to i64
  %v120 = call i64 @Text_Concat(i64 %v117, i64 %v119)
  %v121 = load i64, ptr %slot5
  call void @Ownership_Retain(i64 %v121)
  call void @Ownership_Give(i64 %v121)
  %v122 = call i64 @Text_Concat(i64 %v120, i64 %v121)
  %str123 = getelementptr [2 x i8], ptr @.str.11.7, i64 0, i64 0
  %v124 = ptrtoint ptr %str123 to i64
  %v125 = call i64 @Text_Concat(i64 %v122, i64 %v124)
  %v126 = call i64 @Ownership_Keep(i64 %v125)
  %cond127 = icmp eq i64 %v126, 0
  br i1 %cond127, label %own128, label %owned129
own128:
  call void @Ownership_Retain(i64 %v125)
  br label %owned129
owned129:
  store i64 %v125, ptr %slot12
  call void @Ownership_PopScope()
  br label %merge114
else113:
  br label %merge114
merge114:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v130 = load i64, ptr %slot32
  %v131 = add i64 %v130, 1
  store i64 %v131, ptr %slot33
  call void @Ownership_PopScope()
  %v132 = load i64, ptr %slot87
  call void @Ownership_Release(i64 %v132)
  br label %merge72
else71:
  br label %merge72
merge72:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v133 = load i64, ptr %slot32
  %v134 = add i64 %v133, 1
  store i64 %v134, ptr %slot32
  call void @Ownership_PopScope()
  br label %while34
wend36:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v135 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v135)
  call void @Ownership_Give(i64 %v135)
  %v136 = call i64 @Ownership_Keep(i64 %v135)
  %cond137 = icmp eq i64 %v136, 0
  br i1 %cond137, label %own138, label %owned139
own138:
  call void @Ownership_Retain(i64 %v135)
  br label %owned139
owned139:
  %v140 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v140)
  call void @Ownership_Give(i64 %v135)
  ret i64 %v135
}

@.str.12.1 = private unnamed_addr constant [8 x i8] c"Public \00"
@.str.12.2 = private unnamed_addr constant [1 x i8] c"\00"
@.str.12.3 = private unnamed_addr constant [7 x i8] c"Class \00"
@.str.12.4 = private unnamed_addr constant [7 x i8] c"Class \00"
@.str.12.5 = private unnamed_addr constant [8 x i8] c"Struct \00"
@.str.12.6 = private unnamed_addr constant [8 x i8] c"Struct \00"
@.str.12.7 = private unnamed_addr constant [6 x i8] c"Enum \00"
@.str.12.8 = private unnamed_addr constant [6 x i8] c"Enum \00"
@.str.12.9 = private unnamed_addr constant [11 x i8] c"Interface \00"
@.str.12.10 = private unnamed_addr constant [11 x i8] c"Interface \00"
@.str.12.11 = private unnamed_addr constant [1 x i8] c"\00"
@.str.12.12 = private unnamed_addr constant [1 x i8] c"\00"
@.str.12.13 = private unnamed_addr constant [2 x i8] c" \00"
@.str.12.14 = private unnamed_addr constant [2 x i8] c"{\00"
@.str.12.15 = private unnamed_addr constant [2 x i8] c":\00"
@.str.12.16 = private unnamed_addr constant [2 x i8] c"<\00"
define i64 @rxm_ImportHost_TypeNameOn(i64 %p_This, i64 %p_trimmed) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_trimmed)
  %slot4 = alloca i64
  store i64 %p_trimmed, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %v6 = call i64 @Ownership_Keep(i64 %v5)
  %cond7 = icmp eq i64 %v6, 0
  br i1 %cond7, label %own8, label %owned9
own8:
  call void @Ownership_Retain(i64 %v5)
  br label %owned9
owned9:
  %slot10 = alloca i64
  store i64 %v5, ptr %slot10
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str11 = getelementptr [8 x i8], ptr @.str.12.1, i64 0, i64 0
  %v12 = ptrtoint ptr %str11 to i64
  %v13 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v13)
  call void @Ownership_Give(i64 %v13)
  %v14 = call i64 @Text_StartsWith(i64 %v13, i64 %v12)
  %cond15 = icmp ne i64 %v14, 0
  br i1 %cond15, label %then16, label %else17
then16:
  call void @Ownership_PushScope()
  %v19 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v19)
  call void @Ownership_Give(i64 %v19)
  %v20 = call i64 @Text_Length(i64 %v19)
  %v21 = sub i64 %v20, 7
  %v22 = call i64 @Text_Substring(i64 %v19, i64 7, i64 %v21)
  %v23 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v23)
  call void @Ownership_Give(i64 %v23)
  %v24 = call i64 @rxm_ImportHost_TrimLeft(i64 %v23, i64 %v22)
  %v25 = call i64 @Ownership_Keep(i64 %v24)
  %cond26 = icmp eq i64 %v25, 0
  br i1 %cond26, label %own27, label %owned28
own27:
  call void @Ownership_Retain(i64 %v24)
  br label %owned28
owned28:
  %v29 = load i64, ptr %slot10
  call void @Ownership_Release(i64 %v29)
  store i64 %v24, ptr %slot10
  call void @Ownership_PopScope()
  br label %merge18
else17:
  br label %merge18
merge18:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str30 = getelementptr [1 x i8], ptr @.str.12.2, i64 0, i64 0
  %v31 = ptrtoint ptr %str30 to i64
  %v32 = call i64 @Ownership_Keep(i64 %v31)
  %cond33 = icmp eq i64 %v32, 0
  br i1 %cond33, label %own34, label %owned35
own34:
  call void @Ownership_Retain(i64 %v31)
  br label %owned35
owned35:
  %slot36 = alloca i64
  store i64 %v31, ptr %slot36
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str37 = getelementptr [7 x i8], ptr @.str.12.3, i64 0, i64 0
  %v38 = ptrtoint ptr %str37 to i64
  %v39 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v39)
  call void @Ownership_Give(i64 %v39)
  %v40 = call i64 @Text_StartsWith(i64 %v39, i64 %v38)
  %cond41 = icmp ne i64 %v40, 0
  br i1 %cond41, label %then42, label %else43
then42:
  call void @Ownership_PushScope()
  %str45 = getelementptr [7 x i8], ptr @.str.12.4, i64 0, i64 0
  %v46 = ptrtoint ptr %str45 to i64
  %v47 = call i64 @Ownership_Keep(i64 %v46)
  %cond48 = icmp eq i64 %v47, 0
  br i1 %cond48, label %own49, label %owned50
own49:
  call void @Ownership_Retain(i64 %v46)
  br label %owned50
owned50:
  %v51 = load i64, ptr %slot36
  call void @Ownership_Release(i64 %v51)
  store i64 %v46, ptr %slot36
  call void @Ownership_PopScope()
  br label %merge44
else43:
  br label %merge44
merge44:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str52 = getelementptr [8 x i8], ptr @.str.12.5, i64 0, i64 0
  %v53 = ptrtoint ptr %str52 to i64
  %v54 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v54)
  call void @Ownership_Give(i64 %v54)
  %v55 = call i64 @Text_StartsWith(i64 %v54, i64 %v53)
  %cond56 = icmp ne i64 %v55, 0
  br i1 %cond56, label %then57, label %else58
then57:
  call void @Ownership_PushScope()
  %str60 = getelementptr [8 x i8], ptr @.str.12.6, i64 0, i64 0
  %v61 = ptrtoint ptr %str60 to i64
  %v62 = call i64 @Ownership_Keep(i64 %v61)
  %cond63 = icmp eq i64 %v62, 0
  br i1 %cond63, label %own64, label %owned65
own64:
  call void @Ownership_Retain(i64 %v61)
  br label %owned65
owned65:
  %v66 = load i64, ptr %slot36
  call void @Ownership_Release(i64 %v66)
  store i64 %v61, ptr %slot36
  call void @Ownership_PopScope()
  br label %merge59
else58:
  br label %merge59
merge59:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str67 = getelementptr [6 x i8], ptr @.str.12.7, i64 0, i64 0
  %v68 = ptrtoint ptr %str67 to i64
  %v69 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v69)
  call void @Ownership_Give(i64 %v69)
  %v70 = call i64 @Text_StartsWith(i64 %v69, i64 %v68)
  %cond71 = icmp ne i64 %v70, 0
  br i1 %cond71, label %then72, label %else73
then72:
  call void @Ownership_PushScope()
  %str75 = getelementptr [6 x i8], ptr @.str.12.8, i64 0, i64 0
  %v76 = ptrtoint ptr %str75 to i64
  %v77 = call i64 @Ownership_Keep(i64 %v76)
  %cond78 = icmp eq i64 %v77, 0
  br i1 %cond78, label %own79, label %owned80
own79:
  call void @Ownership_Retain(i64 %v76)
  br label %owned80
owned80:
  %v81 = load i64, ptr %slot36
  call void @Ownership_Release(i64 %v81)
  store i64 %v76, ptr %slot36
  call void @Ownership_PopScope()
  br label %merge74
else73:
  br label %merge74
merge74:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str82 = getelementptr [11 x i8], ptr @.str.12.9, i64 0, i64 0
  %v83 = ptrtoint ptr %str82 to i64
  %v84 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v84)
  call void @Ownership_Give(i64 %v84)
  %v85 = call i64 @Text_StartsWith(i64 %v84, i64 %v83)
  %cond86 = icmp ne i64 %v85, 0
  br i1 %cond86, label %then87, label %else88
then87:
  call void @Ownership_PushScope()
  %str90 = getelementptr [11 x i8], ptr @.str.12.10, i64 0, i64 0
  %v91 = ptrtoint ptr %str90 to i64
  %v92 = call i64 @Ownership_Keep(i64 %v91)
  %cond93 = icmp eq i64 %v92, 0
  br i1 %cond93, label %own94, label %owned95
own94:
  call void @Ownership_Retain(i64 %v91)
  br label %owned95
owned95:
  %v96 = load i64, ptr %slot36
  call void @Ownership_Release(i64 %v96)
  store i64 %v91, ptr %slot36
  call void @Ownership_PopScope()
  br label %merge89
else88:
  br label %merge89
merge89:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v97 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v97)
  call void @Ownership_Give(i64 %v97)
  %str98 = getelementptr [1 x i8], ptr @.str.12.11, i64 0, i64 0
  %v99 = ptrtoint ptr %str98 to i64
  %v100 = call i64 @Text_Equals(i64 %v97, i64 %v99)
  %cond101 = icmp ne i64 %v100, 0
  br i1 %cond101, label %then102, label %else103
then102:
  call void @Ownership_PushScope()
  %str105 = getelementptr [1 x i8], ptr @.str.12.12, i64 0, i64 0
  %v106 = ptrtoint ptr %str105 to i64
  %v107 = call i64 @Ownership_Keep(i64 %v106)
  %cond108 = icmp eq i64 %v107, 0
  br i1 %cond108, label %own109, label %owned110
own109:
  call void @Ownership_Retain(i64 %v106)
  br label %owned110
owned110:
  %v111 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v111)
  %v112 = load i64, ptr %slot36
  call void @Ownership_Release(i64 %v112)
  %v113 = load i64, ptr %slot10
  call void @Ownership_Release(i64 %v113)
  %v114 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v114)
  %v115 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v115)
  call void @Ownership_Give(i64 %v106)
  ret i64 %v106
else103:
  br label %merge104
merge104:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v116 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v116)
  call void @Ownership_Give(i64 %v116)
  %v117 = call i64 @Text_Length(i64 %v116)
  %v118 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v118)
  call void @Ownership_Give(i64 %v118)
  %v119 = call i64 @Text_Length(i64 %v118)
  %v120 = sub i64 %v119, %v117
  %v121 = call i64 @Text_Substring(i64 %v118, i64 %v117, i64 %v120)
  %str122 = getelementptr [2 x i8], ptr @.str.12.13, i64 0, i64 0
  %v123 = ptrtoint ptr %str122 to i64
  %v124 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v124)
  call void @Ownership_Give(i64 %v124)
  %v125 = call i64 @rxm_ImportHost_BeforeMark(i64 %v124, i64 %v121, i64 %v123)
  %v126 = call i64 @Ownership_Keep(i64 %v125)
  %cond127 = icmp eq i64 %v126, 0
  br i1 %cond127, label %own128, label %owned129
own128:
  call void @Ownership_Retain(i64 %v125)
  br label %owned129
owned129:
  %slot130 = alloca i64
  store i64 %v125, ptr %slot130
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v131 = load i64, ptr %slot130
  call void @Ownership_Retain(i64 %v131)
  call void @Ownership_Give(i64 %v131)
  %str132 = getelementptr [2 x i8], ptr @.str.12.14, i64 0, i64 0
  %v133 = ptrtoint ptr %str132 to i64
  %v134 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v134)
  call void @Ownership_Give(i64 %v134)
  %v135 = call i64 @rxm_ImportHost_BeforeMark(i64 %v134, i64 %v131, i64 %v133)
  %v136 = call i64 @Ownership_Keep(i64 %v135)
  %cond137 = icmp eq i64 %v136, 0
  br i1 %cond137, label %own138, label %owned139
own138:
  call void @Ownership_Retain(i64 %v135)
  br label %owned139
owned139:
  %v140 = load i64, ptr %slot130
  call void @Ownership_Release(i64 %v140)
  store i64 %v135, ptr %slot130
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v141 = load i64, ptr %slot130
  call void @Ownership_Retain(i64 %v141)
  call void @Ownership_Give(i64 %v141)
  %str142 = getelementptr [2 x i8], ptr @.str.12.15, i64 0, i64 0
  %v143 = ptrtoint ptr %str142 to i64
  %v144 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v144)
  call void @Ownership_Give(i64 %v144)
  %v145 = call i64 @rxm_ImportHost_BeforeMark(i64 %v144, i64 %v141, i64 %v143)
  %v146 = call i64 @Ownership_Keep(i64 %v145)
  %cond147 = icmp eq i64 %v146, 0
  br i1 %cond147, label %own148, label %owned149
own148:
  call void @Ownership_Retain(i64 %v145)
  br label %owned149
owned149:
  %v150 = load i64, ptr %slot130
  call void @Ownership_Release(i64 %v150)
  store i64 %v145, ptr %slot130
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v151 = load i64, ptr %slot130
  call void @Ownership_Retain(i64 %v151)
  call void @Ownership_Give(i64 %v151)
  %str152 = getelementptr [2 x i8], ptr @.str.12.16, i64 0, i64 0
  %v153 = ptrtoint ptr %str152 to i64
  %v154 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v154)
  call void @Ownership_Give(i64 %v154)
  %v155 = call i64 @rxm_ImportHost_BeforeMark(i64 %v154, i64 %v151, i64 %v153)
  %v156 = call i64 @Ownership_Keep(i64 %v155)
  %cond157 = icmp eq i64 %v156, 0
  br i1 %cond157, label %own158, label %owned159
own158:
  call void @Ownership_Retain(i64 %v155)
  br label %owned159
owned159:
  %v160 = load i64, ptr %slot130
  call void @Ownership_Release(i64 %v160)
  store i64 %v155, ptr %slot130
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v161 = load i64, ptr %slot130
  call void @Ownership_Retain(i64 %v161)
  call void @Ownership_Give(i64 %v161)
  %v162 = call i64 @Ownership_Keep(i64 %v161)
  %cond163 = icmp eq i64 %v162, 0
  br i1 %cond163, label %own164, label %owned165
own164:
  call void @Ownership_Retain(i64 %v161)
  br label %owned165
owned165:
  %v166 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v166)
  %v167 = load i64, ptr %slot130
  call void @Ownership_Release(i64 %v167)
  call void @Ownership_Give(i64 %v161)
  ret i64 %v161
}

@.str.13.1 = private unnamed_addr constant [1 x i8] c"\00"
@.str.13.2 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.13.3 = private unnamed_addr constant [8 x i8] c"Import \00"
@.str.13.4 = private unnamed_addr constant [1 x i8] c"\00"
@.str.13.5 = private unnamed_addr constant [2 x i8] c";\00"
@.str.13.6 = private unnamed_addr constant [2 x i8] c";\00"
define i64 @rxm_ImportHost_DirectImports(i64 %p_This, i64 %p_text) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_PushScope()
  %str5 = getelementptr [1 x i8], ptr @.str.13.1, i64 0, i64 0
  %v6 = ptrtoint ptr %str5 to i64
  %v7 = call i64 @Ownership_Keep(i64 %v6)
  %cond8 = icmp eq i64 %v7, 0
  br i1 %cond8, label %own9, label %owned10
own9:
  call void @Ownership_Retain(i64 %v6)
  br label %owned10
owned10:
  %slot11 = alloca i64
  store i64 %v6, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot12 = alloca i64
  store i64 0, ptr %slot12
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot13 = alloca i64
  store i64 0, ptr %slot13
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while14
while14:
  %v17 = load i64, ptr %slot12
  %v18 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v18)
  call void @Ownership_Give(i64 %v18)
  %v19 = call i64 @Text_Length(i64 %v18)
  %v20 = icmp sle i64 %v17, %v19
  %v21 = zext i1 %v20 to i64
  %cond22 = icmp ne i64 %v21, 0
  br i1 %cond22, label %wbody15, label %wend16
wbody15:
  call void @Ownership_PushScope()
  %slot23 = alloca i64
  store i64 0, ptr %slot23
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v24 = load i64, ptr %slot12
  %v25 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v25)
  call void @Ownership_Give(i64 %v25)
  %v26 = call i64 @Text_Length(i64 %v25)
  %v27 = icmp eq i64 %v24, %v26
  %v28 = zext i1 %v27 to i64
  %cond29 = icmp ne i64 %v28, 0
  br i1 %cond29, label %then30, label %else31
then30:
  call void @Ownership_PushScope()
  store i64 1, ptr %slot23
  call void @Ownership_PopScope()
  br label %merge32
else31:
  br label %merge32
merge32:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v33 = load i64, ptr %slot23
  %v34 = icmp eq i64 %v33, 1
  %v35 = zext i1 %v34 to i64
  %logic38 = alloca i64
  %cond39 = icmp ne i64 %v35, 0
  store i64 1, ptr %logic38
  br i1 %cond39, label %logic37, label %rhs36
rhs36:
  %v40 = load i64, ptr %slot12
  %v41 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v41)
  call void @Ownership_Give(i64 %v41)
  %v42 = call i64 @Text_Substring(i64 %v41, i64 %v40, i64 1)
  %str43 = getelementptr [2 x i8], ptr @.str.13.2, i64 0, i64 0
  %v44 = ptrtoint ptr %str43 to i64
  %v45 = call i64 @Text_Equals(i64 %v42, i64 %v44)
  %cond46 = icmp ne i64 %v45, 0
  %v47 = zext i1 %cond46 to i64
  store i64 %v47, ptr %logic38
  br label %logic37
logic37:
  %v48 = load i64, ptr %logic38
  %cond49 = icmp ne i64 %v48, 0
  br i1 %cond49, label %then50, label %else51
then50:
  call void @Ownership_PushScope()
  %v53 = load i64, ptr %slot13
  %v54 = load i64, ptr %slot12
  %v55 = load i64, ptr %slot13
  %v56 = sub i64 %v54, %v55
  %v57 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v57)
  call void @Ownership_Give(i64 %v57)
  %v58 = call i64 @Text_Substring(i64 %v57, i64 %v53, i64 %v56)
  %v59 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v59)
  call void @Ownership_Give(i64 %v59)
  %v60 = call i64 @rxm_ImportHost_TrimLeft(i64 %v59, i64 %v58)
  %v61 = call i64 @Ownership_Keep(i64 %v60)
  %cond62 = icmp eq i64 %v61, 0
  br i1 %cond62, label %own63, label %owned64
own63:
  call void @Ownership_Retain(i64 %v60)
  br label %owned64
owned64:
  %slot65 = alloca i64
  store i64 %v60, ptr %slot65
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str66 = getelementptr [8 x i8], ptr @.str.13.3, i64 0, i64 0
  %v67 = ptrtoint ptr %str66 to i64
  %v68 = load i64, ptr %slot65
  call void @Ownership_Retain(i64 %v68)
  call void @Ownership_Give(i64 %v68)
  %v69 = call i64 @Text_StartsWith(i64 %v68, i64 %v67)
  %cond70 = icmp ne i64 %v69, 0
  br i1 %cond70, label %then71, label %else72
then71:
  call void @Ownership_PushScope()
  %v74 = load i64, ptr %slot65
  call void @Ownership_Retain(i64 %v74)
  call void @Ownership_Give(i64 %v74)
  %v75 = call i64 @Text_Length(i64 %v74)
  %v76 = sub i64 %v75, 7
  %v77 = call i64 @Text_Substring(i64 %v74, i64 7, i64 %v76)
  %v78 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v78)
  call void @Ownership_Give(i64 %v78)
  %v79 = call i64 @rxm_ImportHost_ImportName(i64 %v78, i64 %v77)
  %v80 = call i64 @Ownership_Keep(i64 %v79)
  %cond81 = icmp eq i64 %v80, 0
  br i1 %cond81, label %own82, label %owned83
own82:
  call void @Ownership_Retain(i64 %v79)
  br label %owned83
owned83:
  %slot84 = alloca i64
  store i64 %v79, ptr %slot84
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v85 = load i64, ptr %slot84
  call void @Ownership_Retain(i64 %v85)
  call void @Ownership_Give(i64 %v85)
  %str86 = getelementptr [1 x i8], ptr @.str.13.4, i64 0, i64 0
  %v87 = ptrtoint ptr %str86 to i64
  %v88 = call i64 @Text_Equals(i64 %v85, i64 %v87)
  %v89 = xor i64 %v88, 1
  %logic92 = alloca i64
  %cond93 = icmp ne i64 %v89, 0
  store i64 0, ptr %logic92
  br i1 %cond93, label %rhs90, label %logic91
rhs90:
  %v94 = load i64, ptr %slot84
  call void @Ownership_Retain(i64 %v94)
  call void @Ownership_Give(i64 %v94)
  %str95 = getelementptr [2 x i8], ptr @.str.13.5, i64 0, i64 0
  %v96 = ptrtoint ptr %str95 to i64
  %v97 = call i64 @Text_Concat(i64 %v94, i64 %v96)
  %v98 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v98)
  call void @Ownership_Give(i64 %v98)
  %v99 = call i64 @Text_Contains(i64 %v98, i64 %v97)
  %v100 = icmp eq i64 %v99, 0
  %v101 = zext i1 %v100 to i64
  %cond102 = icmp ne i64 %v101, 0
  %v103 = zext i1 %cond102 to i64
  store i64 %v103, ptr %logic92
  br label %logic91
logic91:
  %v104 = load i64, ptr %logic92
  %cond105 = icmp ne i64 %v104, 0
  br i1 %cond105, label %then106, label %else107
then106:
  call void @Ownership_PushScope()
  %v109 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v109)
  call void @Ownership_Give(i64 %v109)
  %v110 = load i64, ptr %slot84
  call void @Ownership_Retain(i64 %v110)
  call void @Ownership_Give(i64 %v110)
  %v111 = call i64 @Text_Concat(i64 %v109, i64 %v110)
  %str112 = getelementptr [2 x i8], ptr @.str.13.6, i64 0, i64 0
  %v113 = ptrtoint ptr %str112 to i64
  %v114 = call i64 @Text_Concat(i64 %v111, i64 %v113)
  %v115 = call i64 @Ownership_Keep(i64 %v114)
  %cond116 = icmp eq i64 %v115, 0
  br i1 %cond116, label %own117, label %owned118
own117:
  call void @Ownership_Retain(i64 %v114)
  br label %owned118
owned118:
  %v119 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v119)
  store i64 %v114, ptr %slot11
  call void @Ownership_PopScope()
  br label %merge108
else107:
  br label %merge108
merge108:
  call void @Ownership_PopScope()
  %v120 = load i64, ptr %slot84
  call void @Ownership_Release(i64 %v120)
  br label %merge73
else72:
  br label %merge73
merge73:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v121 = load i64, ptr %slot12
  %v122 = add i64 %v121, 1
  store i64 %v122, ptr %slot13
  call void @Ownership_PopScope()
  %v123 = load i64, ptr %slot65
  call void @Ownership_Release(i64 %v123)
  br label %merge52
else51:
  br label %merge52
merge52:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v124 = load i64, ptr %slot12
  %v125 = add i64 %v124, 1
  store i64 %v125, ptr %slot12
  call void @Ownership_PopScope()
  br label %while14
wend16:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v126 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v126)
  call void @Ownership_Give(i64 %v126)
  %v127 = call i64 @Ownership_Keep(i64 %v126)
  %cond128 = icmp eq i64 %v127, 0
  br i1 %cond128, label %own129, label %owned130
own129:
  call void @Ownership_Retain(i64 %v126)
  br label %owned130
owned130:
  %v131 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v131)
  %v132 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v132)
  %v133 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v133)
  %v134 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v134)
  call void @Ownership_Give(i64 %v126)
  ret i64 %v126
}

@.str.14.1 = private unnamed_addr constant [2 x i8] c";\00"
@.str.14.2 = private unnamed_addr constant [1 x i8] c"\00"
@.str.14.3 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.14.4 = private unnamed_addr constant [2 x i8] c"\22\00"
@.str.14.5 = private unnamed_addr constant [2 x i8] c"\\\00"
@.str.14.6 = private unnamed_addr constant [1 x i8] c"\00"
@.str.14.7 = private unnamed_addr constant [2 x i8] c"/\00"
@.str.14.8 = private unnamed_addr constant [2 x i8] c"/\00"
@.str.14.9 = private unnamed_addr constant [2 x i8] c"\0A\00"
@.str.14.10 = private unnamed_addr constant [1 x i8] c"\00"
@.str.14.11 = private unnamed_addr constant [2 x i8] c";\00"
@.str.14.12 = private unnamed_addr constant [2 x i8] c";\00"
@.str.14.13 = private unnamed_addr constant [2 x i8] c";\00"
@.str.14.14 = private unnamed_addr constant [2 x i8] c" \00"
@.str.14.15 = private unnamed_addr constant [2 x i8] c"\09\00"
@.str.14.16 = private unnamed_addr constant [2 x i8] c"\0D\00"
@.str.14.17 = private unnamed_addr constant [2 x i8] c"\0A\00"
define i64 @rxm_ImportHost_DeclaredNames(i64 %p_This, i64 %p_text) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_PushScope()
  %str5 = getelementptr [2 x i8], ptr @.str.14.1, i64 0, i64 0
  %v6 = ptrtoint ptr %str5 to i64
  %v7 = call i64 @Ownership_Keep(i64 %v6)
  %cond8 = icmp eq i64 %v7, 0
  br i1 %cond8, label %own9, label %owned10
own9:
  call void @Ownership_Retain(i64 %v6)
  br label %owned10
owned10:
  %slot11 = alloca i64
  store i64 %v6, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str12 = getelementptr [1 x i8], ptr @.str.14.2, i64 0, i64 0
  %v13 = ptrtoint ptr %str12 to i64
  %v14 = call i64 @Ownership_Keep(i64 %v13)
  %cond15 = icmp eq i64 %v14, 0
  br i1 %cond15, label %own16, label %owned17
own16:
  call void @Ownership_Retain(i64 %v13)
  br label %owned17
owned17:
  %slot18 = alloca i64
  store i64 %v13, ptr %slot18
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot19 = alloca i64
  store i64 0, ptr %slot19
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while20
while20:
  %v23 = load i64, ptr %slot19
  %v24 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v24)
  call void @Ownership_Give(i64 %v24)
  %v25 = call i64 @Text_Length(i64 %v24)
  %v26 = icmp slt i64 %v23, %v25
  %v27 = zext i1 %v26 to i64
  %cond28 = icmp ne i64 %v27, 0
  br i1 %cond28, label %wbody21, label %wend22
wbody21:
  call void @Ownership_PushScope()
  %v29 = load i64, ptr %slot19
  %v30 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v30)
  call void @Ownership_Give(i64 %v30)
  %v31 = call i64 @Text_Substring(i64 %v30, i64 %v29, i64 1)
  %v32 = call i64 @Ownership_Keep(i64 %v31)
  %cond33 = icmp eq i64 %v32, 0
  br i1 %cond33, label %own34, label %owned35
own34:
  call void @Ownership_Retain(i64 %v31)
  br label %owned35
owned35:
  %slot36 = alloca i64
  store i64 %v31, ptr %slot36
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v37 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v37)
  call void @Ownership_Give(i64 %v37)
  %str38 = getelementptr [2 x i8], ptr @.str.14.3, i64 0, i64 0
  %v39 = ptrtoint ptr %str38 to i64
  %v40 = call i64 @Text_Equals(i64 %v37, i64 %v39)
  %cond41 = icmp ne i64 %v40, 0
  br i1 %cond41, label %then42, label %else43
then42:
  call void @Ownership_PushScope()
  %v45 = load i64, ptr %slot19
  %v46 = add i64 %v45, 1
  store i64 %v46, ptr %slot19
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while47
while47:
  %v50 = load i64, ptr %slot19
  %v51 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v51)
  call void @Ownership_Give(i64 %v51)
  %v52 = call i64 @Text_Length(i64 %v51)
  %v53 = icmp slt i64 %v50, %v52
  %v54 = zext i1 %v53 to i64
  %logic57 = alloca i64
  %cond58 = icmp ne i64 %v54, 0
  store i64 0, ptr %logic57
  br i1 %cond58, label %rhs55, label %logic56
rhs55:
  %v59 = load i64, ptr %slot19
  %v60 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v60)
  call void @Ownership_Give(i64 %v60)
  %v61 = call i64 @Text_Substring(i64 %v60, i64 %v59, i64 1)
  %str62 = getelementptr [2 x i8], ptr @.str.14.4, i64 0, i64 0
  %v63 = ptrtoint ptr %str62 to i64
  %v64 = call i64 @Text_Equals(i64 %v61, i64 %v63)
  %v65 = xor i64 %v64, 1
  %cond66 = icmp ne i64 %v65, 0
  %v67 = zext i1 %cond66 to i64
  store i64 %v67, ptr %logic57
  br label %logic56
logic56:
  %v68 = load i64, ptr %logic57
  %cond69 = icmp ne i64 %v68, 0
  br i1 %cond69, label %wbody48, label %wend49
wbody48:
  call void @Ownership_PushScope()
  %v70 = load i64, ptr %slot19
  %v71 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v71)
  call void @Ownership_Give(i64 %v71)
  %v72 = call i64 @Text_Substring(i64 %v71, i64 %v70, i64 1)
  %str73 = getelementptr [2 x i8], ptr @.str.14.5, i64 0, i64 0
  %v74 = ptrtoint ptr %str73 to i64
  %v75 = call i64 @Text_Equals(i64 %v72, i64 %v74)
  %cond76 = icmp ne i64 %v75, 0
  br i1 %cond76, label %then77, label %else78
then77:
  call void @Ownership_PushScope()
  %v80 = load i64, ptr %slot19
  %v81 = add i64 %v80, 2
  store i64 %v81, ptr %slot19
  call void @Ownership_PopScope()
  br label %merge79
else78:
  call void @Ownership_PushScope()
  %v82 = load i64, ptr %slot19
  %v83 = add i64 %v82, 1
  store i64 %v83, ptr %slot19
  call void @Ownership_PopScope()
  br label %merge79
merge79:
  call void @Ownership_PopScope()
  br label %while47
wend49:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v84 = load i64, ptr %slot19
  %v85 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v85)
  call void @Ownership_Give(i64 %v85)
  %v86 = call i64 @Text_Length(i64 %v85)
  %v87 = icmp slt i64 %v84, %v86
  %v88 = zext i1 %v87 to i64
  %cond89 = icmp ne i64 %v88, 0
  br i1 %cond89, label %then90, label %else91
then90:
  call void @Ownership_PushScope()
  %v93 = load i64, ptr %slot19
  %v94 = add i64 %v93, 1
  store i64 %v94, ptr %slot19
  call void @Ownership_PopScope()
  br label %merge92
else91:
  br label %merge92
merge92:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str95 = getelementptr [1 x i8], ptr @.str.14.6, i64 0, i64 0
  %v96 = ptrtoint ptr %str95 to i64
  %v97 = call i64 @Ownership_Keep(i64 %v96)
  %cond98 = icmp eq i64 %v97, 0
  br i1 %cond98, label %own99, label %owned100
own99:
  call void @Ownership_Retain(i64 %v96)
  br label %owned100
owned100:
  %v101 = load i64, ptr %slot18
  call void @Ownership_Release(i64 %v101)
  store i64 %v96, ptr %slot18
  call void @Ownership_PopScope()
  br label %merge44
else43:
  call void @Ownership_PushScope()
  %v102 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v102)
  call void @Ownership_Give(i64 %v102)
  %str103 = getelementptr [2 x i8], ptr @.str.14.7, i64 0, i64 0
  %v104 = ptrtoint ptr %str103 to i64
  %v105 = call i64 @Text_Equals(i64 %v102, i64 %v104)
  %logic108 = alloca i64
  %cond109 = icmp ne i64 %v105, 0
  store i64 0, ptr %logic108
  br i1 %cond109, label %rhs106, label %logic107
rhs106:
  %v110 = load i64, ptr %slot19
  %v111 = add i64 %v110, 1
  %v112 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v112)
  call void @Ownership_Give(i64 %v112)
  %v113 = call i64 @Text_Length(i64 %v112)
  %v114 = icmp slt i64 %v111, %v113
  %v115 = zext i1 %v114 to i64
  %cond116 = icmp ne i64 %v115, 0
  %v117 = zext i1 %cond116 to i64
  store i64 %v117, ptr %logic108
  br label %logic107
logic107:
  %v118 = load i64, ptr %logic108
  %logic121 = alloca i64
  %cond122 = icmp ne i64 %v118, 0
  store i64 0, ptr %logic121
  br i1 %cond122, label %rhs119, label %logic120
rhs119:
  %v123 = load i64, ptr %slot19
  %v124 = add i64 %v123, 1
  %v125 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v125)
  call void @Ownership_Give(i64 %v125)
  %v126 = call i64 @Text_Substring(i64 %v125, i64 %v124, i64 1)
  %str127 = getelementptr [2 x i8], ptr @.str.14.8, i64 0, i64 0
  %v128 = ptrtoint ptr %str127 to i64
  %v129 = call i64 @Text_Equals(i64 %v126, i64 %v128)
  %cond130 = icmp ne i64 %v129, 0
  %v131 = zext i1 %cond130 to i64
  store i64 %v131, ptr %logic121
  br label %logic120
logic120:
  %v132 = load i64, ptr %logic121
  %cond133 = icmp ne i64 %v132, 0
  br i1 %cond133, label %then134, label %else135
then134:
  call void @Ownership_PushScope()
  br label %while137
while137:
  %v140 = load i64, ptr %slot19
  %v141 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v141)
  call void @Ownership_Give(i64 %v141)
  %v142 = call i64 @Text_Length(i64 %v141)
  %v143 = icmp slt i64 %v140, %v142
  %v144 = zext i1 %v143 to i64
  %logic147 = alloca i64
  %cond148 = icmp ne i64 %v144, 0
  store i64 0, ptr %logic147
  br i1 %cond148, label %rhs145, label %logic146
rhs145:
  %v149 = load i64, ptr %slot19
  %v150 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v150)
  call void @Ownership_Give(i64 %v150)
  %v151 = call i64 @Text_Substring(i64 %v150, i64 %v149, i64 1)
  %str152 = getelementptr [2 x i8], ptr @.str.14.9, i64 0, i64 0
  %v153 = ptrtoint ptr %str152 to i64
  %v154 = call i64 @Text_Equals(i64 %v151, i64 %v153)
  %v155 = xor i64 %v154, 1
  %cond156 = icmp ne i64 %v155, 0
  %v157 = zext i1 %cond156 to i64
  store i64 %v157, ptr %logic147
  br label %logic146
logic146:
  %v158 = load i64, ptr %logic147
  %cond159 = icmp ne i64 %v158, 0
  br i1 %cond159, label %wbody138, label %wend139
wbody138:
  call void @Ownership_PushScope()
  %v160 = load i64, ptr %slot19
  %v161 = add i64 %v160, 1
  store i64 %v161, ptr %slot19
  call void @Ownership_PopScope()
  br label %while137
wend139:
  call void @Ownership_PopScope()
  br label %merge136
else135:
  call void @Ownership_PushScope()
  %v162 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v162)
  call void @Ownership_Give(i64 %v162)
  %v163 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v163)
  call void @Ownership_Give(i64 %v163)
  %v164 = call i64 @rxm_ImportHost_IsNameStart(i64 %v163, i64 %v162)
  %cond165 = icmp ne i64 %v164, 0
  br i1 %cond165, label %then166, label %else167
then166:
  call void @Ownership_PushScope()
  %str169 = getelementptr [1 x i8], ptr @.str.14.10, i64 0, i64 0
  %v170 = ptrtoint ptr %str169 to i64
  %v171 = call i64 @Ownership_Keep(i64 %v170)
  %cond172 = icmp eq i64 %v171, 0
  br i1 %cond172, label %own173, label %owned174
own173:
  call void @Ownership_Retain(i64 %v170)
  br label %owned174
owned174:
  %slot175 = alloca i64
  store i64 %v170, ptr %slot175
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while176
while176:
  %v179 = load i64, ptr %slot19
  %v180 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v180)
  call void @Ownership_Give(i64 %v180)
  %v181 = call i64 @Text_Length(i64 %v180)
  %v182 = icmp slt i64 %v179, %v181
  %v183 = zext i1 %v182 to i64
  %logic186 = alloca i64
  %cond187 = icmp ne i64 %v183, 0
  store i64 0, ptr %logic186
  br i1 %cond187, label %rhs184, label %logic185
rhs184:
  %v188 = load i64, ptr %slot19
  %v189 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v189)
  call void @Ownership_Give(i64 %v189)
  %v190 = call i64 @Text_Substring(i64 %v189, i64 %v188, i64 1)
  %v191 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v191)
  call void @Ownership_Give(i64 %v191)
  %v192 = call i64 @rxm_ImportHost_IsNamePart(i64 %v191, i64 %v190)
  %cond193 = icmp ne i64 %v192, 0
  %v194 = zext i1 %cond193 to i64
  store i64 %v194, ptr %logic186
  br label %logic185
logic185:
  %v195 = load i64, ptr %logic186
  %cond196 = icmp ne i64 %v195, 0
  br i1 %cond196, label %wbody177, label %wend178
wbody177:
  call void @Ownership_PushScope()
  %v197 = load i64, ptr %slot175
  call void @Ownership_Retain(i64 %v197)
  call void @Ownership_Give(i64 %v197)
  %v198 = load i64, ptr %slot19
  %v199 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v199)
  call void @Ownership_Give(i64 %v199)
  %v200 = call i64 @Text_Substring(i64 %v199, i64 %v198, i64 1)
  %v201 = call i64 @Text_Concat(i64 %v197, i64 %v200)
  %v202 = call i64 @Ownership_Keep(i64 %v201)
  %cond203 = icmp eq i64 %v202, 0
  br i1 %cond203, label %own204, label %owned205
own204:
  call void @Ownership_Retain(i64 %v201)
  br label %owned205
owned205:
  %v206 = load i64, ptr %slot175
  call void @Ownership_Release(i64 %v206)
  store i64 %v201, ptr %slot175
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v207 = load i64, ptr %slot19
  %v208 = add i64 %v207, 1
  store i64 %v208, ptr %slot19
  call void @Ownership_PopScope()
  br label %while176
wend178:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v209 = load i64, ptr %slot18
  call void @Ownership_Retain(i64 %v209)
  call void @Ownership_Give(i64 %v209)
  %v210 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v210)
  call void @Ownership_Give(i64 %v210)
  %v211 = call i64 @rxm_ImportHost_IsBinder(i64 %v210, i64 %v209)
  %logic214 = alloca i64
  %cond215 = icmp ne i64 %v211, 0
  store i64 0, ptr %logic214
  br i1 %cond215, label %rhs212, label %logic213
rhs212:
  %str216 = getelementptr [2 x i8], ptr @.str.14.11, i64 0, i64 0
  %v217 = ptrtoint ptr %str216 to i64
  %v218 = load i64, ptr %slot175
  call void @Ownership_Retain(i64 %v218)
  call void @Ownership_Give(i64 %v218)
  %v219 = call i64 @Text_Concat(i64 %v217, i64 %v218)
  %str220 = getelementptr [2 x i8], ptr @.str.14.12, i64 0, i64 0
  %v221 = ptrtoint ptr %str220 to i64
  %v222 = call i64 @Text_Concat(i64 %v219, i64 %v221)
  %v223 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v223)
  call void @Ownership_Give(i64 %v223)
  %v224 = call i64 @Text_Contains(i64 %v223, i64 %v222)
  %v225 = icmp eq i64 %v224, 0
  %v226 = zext i1 %v225 to i64
  %cond227 = icmp ne i64 %v226, 0
  %v228 = zext i1 %cond227 to i64
  store i64 %v228, ptr %logic214
  br label %logic213
logic213:
  %v229 = load i64, ptr %logic214
  %cond230 = icmp ne i64 %v229, 0
  br i1 %cond230, label %then231, label %else232
then231:
  call void @Ownership_PushScope()
  %v234 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v234)
  call void @Ownership_Give(i64 %v234)
  %v235 = load i64, ptr %slot175
  call void @Ownership_Retain(i64 %v235)
  call void @Ownership_Give(i64 %v235)
  %v236 = call i64 @Text_Concat(i64 %v234, i64 %v235)
  %str237 = getelementptr [2 x i8], ptr @.str.14.13, i64 0, i64 0
  %v238 = ptrtoint ptr %str237 to i64
  %v239 = call i64 @Text_Concat(i64 %v236, i64 %v238)
  %v240 = call i64 @Ownership_Keep(i64 %v239)
  %cond241 = icmp eq i64 %v240, 0
  br i1 %cond241, label %own242, label %owned243
own242:
  call void @Ownership_Retain(i64 %v239)
  br label %owned243
owned243:
  %v244 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v244)
  store i64 %v239, ptr %slot11
  call void @Ownership_PopScope()
  br label %merge233
else232:
  br label %merge233
merge233:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v245 = load i64, ptr %slot175
  call void @Ownership_Retain(i64 %v245)
  call void @Ownership_Give(i64 %v245)
  %v246 = call i64 @Ownership_Keep(i64 %v245)
  %cond247 = icmp eq i64 %v246, 0
  br i1 %cond247, label %own248, label %owned249
own248:
  call void @Ownership_Retain(i64 %v245)
  br label %owned249
owned249:
  %v250 = load i64, ptr %slot18
  call void @Ownership_Release(i64 %v250)
  store i64 %v245, ptr %slot18
  call void @Ownership_PopScope()
  %v251 = load i64, ptr %slot175
  call void @Ownership_Release(i64 %v251)
  br label %merge168
else167:
  call void @Ownership_PushScope()
  %v252 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v252)
  call void @Ownership_Give(i64 %v252)
  %str253 = getelementptr [2 x i8], ptr @.str.14.14, i64 0, i64 0
  %v254 = ptrtoint ptr %str253 to i64
  %v255 = call i64 @Text_Equals(i64 %v252, i64 %v254)
  %v256 = xor i64 %v255, 1
  %logic259 = alloca i64
  %cond260 = icmp ne i64 %v256, 0
  store i64 0, ptr %logic259
  br i1 %cond260, label %rhs257, label %logic258
rhs257:
  %v261 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v261)
  call void @Ownership_Give(i64 %v261)
  %str262 = getelementptr [2 x i8], ptr @.str.14.15, i64 0, i64 0
  %v263 = ptrtoint ptr %str262 to i64
  %v264 = call i64 @Text_Equals(i64 %v261, i64 %v263)
  %v265 = xor i64 %v264, 1
  %cond266 = icmp ne i64 %v265, 0
  %v267 = zext i1 %cond266 to i64
  store i64 %v267, ptr %logic259
  br label %logic258
logic258:
  %v268 = load i64, ptr %logic259
  %logic271 = alloca i64
  %cond272 = icmp ne i64 %v268, 0
  store i64 0, ptr %logic271
  br i1 %cond272, label %rhs269, label %logic270
rhs269:
  %v273 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v273)
  call void @Ownership_Give(i64 %v273)
  %str274 = getelementptr [2 x i8], ptr @.str.14.16, i64 0, i64 0
  %v275 = ptrtoint ptr %str274 to i64
  %v276 = call i64 @Text_Equals(i64 %v273, i64 %v275)
  %v277 = xor i64 %v276, 1
  %cond278 = icmp ne i64 %v277, 0
  %v279 = zext i1 %cond278 to i64
  store i64 %v279, ptr %logic271
  br label %logic270
logic270:
  %v280 = load i64, ptr %logic271
  %logic283 = alloca i64
  %cond284 = icmp ne i64 %v280, 0
  store i64 0, ptr %logic283
  br i1 %cond284, label %rhs281, label %logic282
rhs281:
  %v285 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v285)
  call void @Ownership_Give(i64 %v285)
  %str286 = getelementptr [2 x i8], ptr @.str.14.17, i64 0, i64 0
  %v287 = ptrtoint ptr %str286 to i64
  %v288 = call i64 @Text_Equals(i64 %v285, i64 %v287)
  %v289 = xor i64 %v288, 1
  %cond290 = icmp ne i64 %v289, 0
  %v291 = zext i1 %cond290 to i64
  store i64 %v291, ptr %logic283
  br label %logic282
logic282:
  %v292 = load i64, ptr %logic283
  %cond293 = icmp ne i64 %v292, 0
  br i1 %cond293, label %then294, label %else295
then294:
  call void @Ownership_PushScope()
  %v297 = load i64, ptr %slot36
  call void @Ownership_Retain(i64 %v297)
  call void @Ownership_Give(i64 %v297)
  %v298 = call i64 @Ownership_Keep(i64 %v297)
  %cond299 = icmp eq i64 %v298, 0
  br i1 %cond299, label %own300, label %owned301
own300:
  call void @Ownership_Retain(i64 %v297)
  br label %owned301
owned301:
  %v302 = load i64, ptr %slot18
  call void @Ownership_Release(i64 %v302)
  store i64 %v297, ptr %slot18
  call void @Ownership_PopScope()
  br label %merge296
else295:
  br label %merge296
merge296:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v303 = load i64, ptr %slot19
  %v304 = add i64 %v303, 1
  store i64 %v304, ptr %slot19
  call void @Ownership_PopScope()
  br label %merge168
merge168:
  call void @Ownership_PopScope()
  br label %merge136
merge136:
  call void @Ownership_PopScope()
  br label %merge44
merge44:
  call void @Ownership_PopScope()
  %v305 = load i64, ptr %slot36
  call void @Ownership_Release(i64 %v305)
  br label %while20
wend22:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v306 = load i64, ptr %slot11
  call void @Ownership_Retain(i64 %v306)
  call void @Ownership_Give(i64 %v306)
  %v307 = call i64 @Ownership_Keep(i64 %v306)
  %cond308 = icmp eq i64 %v307, 0
  br i1 %cond308, label %own309, label %owned310
own309:
  call void @Ownership_Retain(i64 %v306)
  br label %owned310
owned310:
  %v311 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v311)
  %v312 = load i64, ptr %slot18
  call void @Ownership_Release(i64 %v312)
  %v313 = load i64, ptr %slot11
  call void @Ownership_Release(i64 %v313)
  %v314 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v314)
  %v315 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v315)
  call void @Ownership_Give(i64 %v306)
  ret i64 %v306
}

@.str.15.1 = private unnamed_addr constant [2 x i8] c";\00"
@.str.15.2 = private unnamed_addr constant [2 x i8] c"|\00"
@.str.15.3 = private unnamed_addr constant [1 x i8] c"\00"
@.str.15.4 = private unnamed_addr constant [2 x i8] c";\00"
define i64 @rxm_ImportHost_CatalogPackage(i64 %p_This, i64 %p_catalog, i64 %p_name) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_catalog)
  %slot4 = alloca i64
  store i64 %p_catalog, ptr %slot4
  call void @Ownership_Retain(i64 %p_name)
  %slot5 = alloca i64
  store i64 %p_name, ptr %slot5
  call void @Ownership_PushScope()
  %str6 = getelementptr [2 x i8], ptr @.str.15.1, i64 0, i64 0
  %v7 = ptrtoint ptr %str6 to i64
  %v8 = load i64, ptr %slot5
  call void @Ownership_Retain(i64 %v8)
  call void @Ownership_Give(i64 %v8)
  %v9 = call i64 @Text_Concat(i64 %v7, i64 %v8)
  %str10 = getelementptr [2 x i8], ptr @.str.15.2, i64 0, i64 0
  %v11 = ptrtoint ptr %str10 to i64
  %v12 = call i64 @Text_Concat(i64 %v9, i64 %v11)
  %v13 = call i64 @Ownership_Keep(i64 %v12)
  %cond14 = icmp eq i64 %v13, 0
  br i1 %cond14, label %own15, label %owned16
own15:
  call void @Ownership_Retain(i64 %v12)
  br label %owned16
owned16:
  %slot17 = alloca i64
  store i64 %v12, ptr %slot17
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v18 = load i64, ptr %slot17
  call void @Ownership_Retain(i64 %v18)
  call void @Ownership_Give(i64 %v18)
  %v19 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v19)
  call void @Ownership_Give(i64 %v19)
  %v20 = call i64 @Text_IndexOf(i64 %v19, i64 %v18, i64 0)
  %slot21 = alloca i64
  store i64 %v20, ptr %slot21
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v22 = load i64, ptr %slot21
  %v23 = icmp slt i64 %v22, 0
  %v24 = zext i1 %v23 to i64
  %cond25 = icmp ne i64 %v24, 0
  br i1 %cond25, label %then26, label %else27
then26:
  call void @Ownership_PushScope()
  %str29 = getelementptr [1 x i8], ptr @.str.15.3, i64 0, i64 0
  %v30 = ptrtoint ptr %str29 to i64
  %v31 = call i64 @Ownership_Keep(i64 %v30)
  %cond32 = icmp eq i64 %v31, 0
  br i1 %cond32, label %own33, label %owned34
own33:
  call void @Ownership_Retain(i64 %v30)
  br label %owned34
owned34:
  %v35 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v35)
  %v36 = load i64, ptr %slot17
  call void @Ownership_Release(i64 %v36)
  %v37 = load i64, ptr %slot5
  call void @Ownership_Release(i64 %v37)
  %v38 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v38)
  %v39 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v39)
  call void @Ownership_Give(i64 %v30)
  ret i64 %v30
else27:
  br label %merge28
merge28:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v40 = load i64, ptr %slot21
  %v41 = load i64, ptr %slot17
  call void @Ownership_Retain(i64 %v41)
  call void @Ownership_Give(i64 %v41)
  %v42 = call i64 @Text_Length(i64 %v41)
  %v43 = add i64 %v40, %v42
  %v44 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v44)
  call void @Ownership_Give(i64 %v44)
  %v45 = call i64 @Text_Length(i64 %v44)
  %v46 = sub i64 %v45, %v43
  %v47 = call i64 @Text_Substring(i64 %v44, i64 %v43, i64 %v46)
  %str48 = getelementptr [2 x i8], ptr @.str.15.4, i64 0, i64 0
  %v49 = ptrtoint ptr %str48 to i64
  %v50 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v50)
  call void @Ownership_Give(i64 %v50)
  %v51 = call i64 @rxm_ImportHost_BeforeMark(i64 %v50, i64 %v47, i64 %v49)
  %v52 = call i64 @Ownership_Keep(i64 %v51)
  %cond53 = icmp eq i64 %v52, 0
  br i1 %cond53, label %own54, label %owned55
own54:
  call void @Ownership_Retain(i64 %v51)
  br label %owned55
owned55:
  %v56 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v56)
  call void @Ownership_Give(i64 %v51)
  ret i64 %v51
}

@.str.16.1 = private unnamed_addr constant [2 x i8] c";\00"
@.str.16.2 = private unnamed_addr constant [2 x i8] c" \00"
@.str.16.3 = private unnamed_addr constant [3 x i8] c".*\00"
define i64 @rxm_ImportHost_ImportName(i64 %p_This, i64 %p_text) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %str6 = getelementptr [2 x i8], ptr @.str.16.1, i64 0, i64 0
  %v7 = ptrtoint ptr %str6 to i64
  %v8 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v8)
  call void @Ownership_Give(i64 %v8)
  %v9 = call i64 @rxm_ImportHost_BeforeMark(i64 %v8, i64 %v5, i64 %v7)
  %v10 = call i64 @Ownership_Keep(i64 %v9)
  %cond11 = icmp eq i64 %v10, 0
  br i1 %cond11, label %own12, label %owned13
own12:
  call void @Ownership_Retain(i64 %v9)
  br label %owned13
owned13:
  %slot14 = alloca i64
  store i64 %v9, ptr %slot14
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v15 = load i64, ptr %slot14
  call void @Ownership_Retain(i64 %v15)
  call void @Ownership_Give(i64 %v15)
  %str16 = getelementptr [2 x i8], ptr @.str.16.2, i64 0, i64 0
  %v17 = ptrtoint ptr %str16 to i64
  %v18 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v18)
  call void @Ownership_Give(i64 %v18)
  %v19 = call i64 @rxm_ImportHost_BeforeMark(i64 %v18, i64 %v15, i64 %v17)
  %v20 = call i64 @Ownership_Keep(i64 %v19)
  %cond21 = icmp eq i64 %v20, 0
  br i1 %cond21, label %own22, label %owned23
own22:
  call void @Ownership_Retain(i64 %v19)
  br label %owned23
owned23:
  %v24 = load i64, ptr %slot14
  call void @Ownership_Release(i64 %v24)
  store i64 %v19, ptr %slot14
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v25 = load i64, ptr %slot14
  call void @Ownership_Retain(i64 %v25)
  call void @Ownership_Give(i64 %v25)
  %str26 = getelementptr [3 x i8], ptr @.str.16.3, i64 0, i64 0
  %v27 = ptrtoint ptr %str26 to i64
  %v28 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v28)
  call void @Ownership_Give(i64 %v28)
  %v29 = call i64 @rxm_ImportHost_BeforeMark(i64 %v28, i64 %v25, i64 %v27)
  %v30 = call i64 @Ownership_Keep(i64 %v29)
  %cond31 = icmp eq i64 %v30, 0
  br i1 %cond31, label %own32, label %owned33
own32:
  call void @Ownership_Retain(i64 %v29)
  br label %owned33
owned33:
  %v34 = load i64, ptr %slot14
  call void @Ownership_Release(i64 %v34)
  store i64 %v29, ptr %slot14
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v35 = load i64, ptr %slot14
  call void @Ownership_Retain(i64 %v35)
  call void @Ownership_Give(i64 %v35)
  %v36 = call i64 @Ownership_Keep(i64 %v35)
  %cond37 = icmp eq i64 %v36, 0
  br i1 %cond37, label %own38, label %owned39
own38:
  call void @Ownership_Retain(i64 %v35)
  br label %owned39
owned39:
  %v40 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v40)
  %v41 = load i64, ptr %slot14
  call void @Ownership_Release(i64 %v41)
  %v42 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v42)
  %v43 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v43)
  call void @Ownership_Give(i64 %v35)
  ret i64 %v35
}

define i64 @rxm_ImportHost_BeforeMark(i64 %p_This, i64 %p_text, i64 %p_mark) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_Retain(i64 %p_mark)
  %slot5 = alloca i64
  store i64 %p_mark, ptr %slot5
  call void @Ownership_PushScope()
  %v6 = load i64, ptr %slot5
  call void @Ownership_Retain(i64 %v6)
  call void @Ownership_Give(i64 %v6)
  %v7 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v7)
  call void @Ownership_Give(i64 %v7)
  %v8 = call i64 @Text_IndexOf(i64 %v7, i64 %v6, i64 0)
  %slot9 = alloca i64
  store i64 %v8, ptr %slot9
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v10 = load i64, ptr %slot9
  %v11 = icmp slt i64 %v10, 0
  %v12 = zext i1 %v11 to i64
  %cond13 = icmp ne i64 %v12, 0
  br i1 %cond13, label %then14, label %else15
then14:
  call void @Ownership_PushScope()
  %v17 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v17)
  call void @Ownership_Give(i64 %v17)
  %v18 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v18)
  call void @Ownership_Give(i64 %v18)
  %v19 = call i64 @rxm_ImportHost_TrimEdge(i64 %v18, i64 %v17)
  %v20 = call i64 @Ownership_Keep(i64 %v19)
  %cond21 = icmp eq i64 %v20, 0
  br i1 %cond21, label %own22, label %owned23
own22:
  call void @Ownership_Retain(i64 %v19)
  br label %owned23
owned23:
  %v24 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v24)
  %v25 = load i64, ptr %slot5
  call void @Ownership_Release(i64 %v25)
  %v26 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v26)
  %v27 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v27)
  call void @Ownership_Give(i64 %v19)
  ret i64 %v19
else15:
  br label %merge16
merge16:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v28 = load i64, ptr %slot9
  %v29 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v29)
  call void @Ownership_Give(i64 %v29)
  %v30 = call i64 @Text_Substring(i64 %v29, i64 0, i64 %v28)
  %v31 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v31)
  call void @Ownership_Give(i64 %v31)
  %v32 = call i64 @rxm_ImportHost_TrimEdge(i64 %v31, i64 %v30)
  %v33 = call i64 @Ownership_Keep(i64 %v32)
  %cond34 = icmp eq i64 %v33, 0
  br i1 %cond34, label %own35, label %owned36
own35:
  call void @Ownership_Retain(i64 %v32)
  br label %owned36
owned36:
  %v37 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v37)
  call void @Ownership_Give(i64 %v32)
  ret i64 %v32
}

@.str.18.1 = private unnamed_addr constant [2 x i8] c".\00"
@.str.18.2 = private unnamed_addr constant [10 x i8] c"Packages/\00"
@.str.18.3 = private unnamed_addr constant [2 x i8] c"/\00"
@.str.18.4 = private unnamed_addr constant [4 x i8] c".rx\00"
@.str.18.5 = private unnamed_addr constant [1 x i8] c"\00"
define i64 @rxm_ImportHost_PackagePath(i64 %p_This, i64 %p_packageName) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_packageName)
  %slot4 = alloca i64
  store i64 %p_packageName, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %v6 = call i64 @Ownership_Keep(i64 %v5)
  %cond7 = icmp eq i64 %v6, 0
  br i1 %cond7, label %own8, label %owned9
own8:
  call void @Ownership_Retain(i64 %v5)
  br label %owned9
owned9:
  %slot10 = alloca i64
  store i64 %v5, ptr %slot10
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot11 = alloca i64
  store i64 0, ptr %slot11
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %slot12 = alloca i64
  store i64 0, ptr %slot12
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while13
while13:
  %v16 = load i64, ptr %slot12
  %v17 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v17)
  call void @Ownership_Give(i64 %v17)
  %v18 = call i64 @Text_Length(i64 %v17)
  %v19 = icmp slt i64 %v16, %v18
  %v20 = zext i1 %v19 to i64
  %cond21 = icmp ne i64 %v20, 0
  br i1 %cond21, label %wbody14, label %wend15
wbody14:
  call void @Ownership_PushScope()
  %v22 = load i64, ptr %slot12
  %v23 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v23)
  call void @Ownership_Give(i64 %v23)
  %v24 = call i64 @Text_Substring(i64 %v23, i64 %v22, i64 1)
  %str25 = getelementptr [2 x i8], ptr @.str.18.1, i64 0, i64 0
  %v26 = ptrtoint ptr %str25 to i64
  %v27 = call i64 @Text_Equals(i64 %v24, i64 %v26)
  %cond28 = icmp ne i64 %v27, 0
  br i1 %cond28, label %then29, label %else30
then29:
  call void @Ownership_PushScope()
  %v32 = load i64, ptr %slot12
  store i64 %v32, ptr %slot11
  call void @Ownership_PopScope()
  br label %merge31
else30:
  br label %merge31
merge31:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v33 = load i64, ptr %slot12
  %v34 = add i64 %v33, 1
  store i64 %v34, ptr %slot12
  call void @Ownership_PopScope()
  br label %while13
wend15:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v35 = load i64, ptr %slot11
  %v36 = icmp sgt i64 %v35, 0
  %v37 = zext i1 %v36 to i64
  %cond38 = icmp ne i64 %v37, 0
  br i1 %cond38, label %then39, label %else40
then39:
  call void @Ownership_PushScope()
  %v42 = load i64, ptr %slot11
  %v43 = add i64 %v42, 1
  %v44 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v44)
  call void @Ownership_Give(i64 %v44)
  %v45 = call i64 @Text_Length(i64 %v44)
  %v46 = sub i64 %v45, %v43
  %v47 = call i64 @Text_Substring(i64 %v44, i64 %v43, i64 %v46)
  %v48 = call i64 @Ownership_Keep(i64 %v47)
  %cond49 = icmp eq i64 %v48, 0
  br i1 %cond49, label %own50, label %owned51
own50:
  call void @Ownership_Retain(i64 %v47)
  br label %owned51
owned51:
  %v52 = load i64, ptr %slot10
  call void @Ownership_Release(i64 %v52)
  store i64 %v47, ptr %slot10
  call void @Ownership_PopScope()
  br label %merge41
else40:
  br label %merge41
merge41:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str53 = getelementptr [10 x i8], ptr @.str.18.2, i64 0, i64 0
  %v54 = ptrtoint ptr %str53 to i64
  %v55 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v55)
  call void @Ownership_Give(i64 %v55)
  %v56 = call i64 @Text_Concat(i64 %v54, i64 %v55)
  %str57 = getelementptr [2 x i8], ptr @.str.18.3, i64 0, i64 0
  %v58 = ptrtoint ptr %str57 to i64
  %v59 = call i64 @Text_Concat(i64 %v56, i64 %v58)
  %v60 = load i64, ptr %slot10
  call void @Ownership_Retain(i64 %v60)
  call void @Ownership_Give(i64 %v60)
  %v61 = call i64 @Text_Concat(i64 %v59, i64 %v60)
  %str62 = getelementptr [4 x i8], ptr @.str.18.4, i64 0, i64 0
  %v63 = ptrtoint ptr %str62 to i64
  %v64 = call i64 @Text_Concat(i64 %v61, i64 %v63)
  %v65 = call i64 @Ownership_Keep(i64 %v64)
  %cond66 = icmp eq i64 %v65, 0
  br i1 %cond66, label %own67, label %owned68
own67:
  call void @Ownership_Retain(i64 %v64)
  br label %owned68
owned68:
  %slot69 = alloca i64
  store i64 %v64, ptr %slot69
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v70 = load i64, ptr %slot69
  call void @Ownership_Retain(i64 %v70)
  call void @Ownership_Give(i64 %v70)
  %v71 = call i64 @Files_Exists(i64 %v70)
  %cond72 = icmp ne i64 %v71, 0
  br i1 %cond72, label %then73, label %else74
then73:
  call void @Ownership_PushScope()
  %v76 = load i64, ptr %slot69
  call void @Ownership_Retain(i64 %v76)
  call void @Ownership_Give(i64 %v76)
  %v77 = call i64 @Ownership_Keep(i64 %v76)
  %cond78 = icmp eq i64 %v77, 0
  br i1 %cond78, label %own79, label %owned80
own79:
  call void @Ownership_Retain(i64 %v76)
  br label %owned80
owned80:
  %v81 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v81)
  %v82 = load i64, ptr %slot69
  call void @Ownership_Release(i64 %v82)
  %v83 = load i64, ptr %slot10
  call void @Ownership_Release(i64 %v83)
  %v84 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v84)
  %v85 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v85)
  call void @Ownership_Give(i64 %v76)
  ret i64 %v76
else74:
  br label %merge75
merge75:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str86 = getelementptr [1 x i8], ptr @.str.18.5, i64 0, i64 0
  %v87 = ptrtoint ptr %str86 to i64
  %v88 = call i64 @Ownership_Keep(i64 %v87)
  %cond89 = icmp eq i64 %v88, 0
  br i1 %cond89, label %own90, label %owned91
own90:
  call void @Ownership_Retain(i64 %v87)
  br label %owned91
owned91:
  %v92 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v92)
  call void @Ownership_Give(i64 %v87)
  ret i64 %v87
}

@.str.19.1 = private unnamed_addr constant [2 x i8] c" \00"
@.str.19.2 = private unnamed_addr constant [2 x i8] c"\09\00"
@.str.19.3 = private unnamed_addr constant [2 x i8] c"\0D\00"
@.str.19.4 = private unnamed_addr constant [1 x i8] c"\00"
define i64 @rxm_ImportHost_TrimLeft(i64 %p_This, i64 %p_text) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_PushScope()
  %slot5 = alloca i64
  store i64 0, ptr %slot5
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while6
while6:
  %v9 = load i64, ptr %slot5
  %v10 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v10)
  call void @Ownership_Give(i64 %v10)
  %v11 = call i64 @Text_Length(i64 %v10)
  %v12 = icmp slt i64 %v9, %v11
  %v13 = zext i1 %v12 to i64
  %cond14 = icmp ne i64 %v13, 0
  br i1 %cond14, label %wbody7, label %wend8
wbody7:
  call void @Ownership_PushScope()
  %v15 = load i64, ptr %slot5
  %v16 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v16)
  call void @Ownership_Give(i64 %v16)
  %v17 = call i64 @Text_Substring(i64 %v16, i64 %v15, i64 1)
  %v18 = call i64 @Ownership_Keep(i64 %v17)
  %cond19 = icmp eq i64 %v18, 0
  br i1 %cond19, label %own20, label %owned21
own20:
  call void @Ownership_Retain(i64 %v17)
  br label %owned21
owned21:
  %slot22 = alloca i64
  store i64 %v17, ptr %slot22
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v23 = load i64, ptr %slot22
  call void @Ownership_Retain(i64 %v23)
  call void @Ownership_Give(i64 %v23)
  %str24 = getelementptr [2 x i8], ptr @.str.19.1, i64 0, i64 0
  %v25 = ptrtoint ptr %str24 to i64
  %v26 = call i64 @Text_Equals(i64 %v23, i64 %v25)
  %v27 = xor i64 %v26, 1
  %logic30 = alloca i64
  %cond31 = icmp ne i64 %v27, 0
  store i64 0, ptr %logic30
  br i1 %cond31, label %rhs28, label %logic29
rhs28:
  %v32 = load i64, ptr %slot22
  call void @Ownership_Retain(i64 %v32)
  call void @Ownership_Give(i64 %v32)
  %str33 = getelementptr [2 x i8], ptr @.str.19.2, i64 0, i64 0
  %v34 = ptrtoint ptr %str33 to i64
  %v35 = call i64 @Text_Equals(i64 %v32, i64 %v34)
  %v36 = xor i64 %v35, 1
  %cond37 = icmp ne i64 %v36, 0
  %v38 = zext i1 %cond37 to i64
  store i64 %v38, ptr %logic30
  br label %logic29
logic29:
  %v39 = load i64, ptr %logic30
  %logic42 = alloca i64
  %cond43 = icmp ne i64 %v39, 0
  store i64 0, ptr %logic42
  br i1 %cond43, label %rhs40, label %logic41
rhs40:
  %v44 = load i64, ptr %slot22
  call void @Ownership_Retain(i64 %v44)
  call void @Ownership_Give(i64 %v44)
  %str45 = getelementptr [2 x i8], ptr @.str.19.3, i64 0, i64 0
  %v46 = ptrtoint ptr %str45 to i64
  %v47 = call i64 @Text_Equals(i64 %v44, i64 %v46)
  %v48 = xor i64 %v47, 1
  %cond49 = icmp ne i64 %v48, 0
  %v50 = zext i1 %cond49 to i64
  store i64 %v50, ptr %logic42
  br label %logic41
logic41:
  %v51 = load i64, ptr %logic42
  %cond52 = icmp ne i64 %v51, 0
  br i1 %cond52, label %then53, label %else54
then53:
  call void @Ownership_PushScope()
  %v56 = load i64, ptr %slot5
  %v57 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v57)
  call void @Ownership_Give(i64 %v57)
  %v58 = call i64 @Text_Length(i64 %v57)
  %v59 = sub i64 %v58, %v56
  %v60 = call i64 @Text_Substring(i64 %v57, i64 %v56, i64 %v59)
  %v61 = call i64 @Ownership_Keep(i64 %v60)
  %cond62 = icmp eq i64 %v61, 0
  br i1 %cond62, label %own63, label %owned64
own63:
  call void @Ownership_Retain(i64 %v60)
  br label %owned64
owned64:
  %v65 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v65)
  %v66 = load i64, ptr %slot22
  call void @Ownership_Release(i64 %v66)
  %v67 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v67)
  %v68 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v68)
  call void @Ownership_Give(i64 %v60)
  ret i64 %v60
else54:
  br label %merge55
merge55:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v69 = load i64, ptr %slot5
  %v70 = add i64 %v69, 1
  store i64 %v70, ptr %slot5
  call void @Ownership_PopScope()
  br label %while6
wend8:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str71 = getelementptr [1 x i8], ptr @.str.19.4, i64 0, i64 0
  %v72 = ptrtoint ptr %str71 to i64
  %v73 = call i64 @Ownership_Keep(i64 %v72)
  %cond74 = icmp eq i64 %v73, 0
  br i1 %cond74, label %own75, label %owned76
own75:
  call void @Ownership_Retain(i64 %v72)
  br label %owned76
owned76:
  %v77 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v77)
  call void @Ownership_Give(i64 %v72)
  ret i64 %v72
}

@.str.20.1 = private unnamed_addr constant [2 x i8] c" \00"
@.str.20.2 = private unnamed_addr constant [2 x i8] c"\09\00"
@.str.20.3 = private unnamed_addr constant [2 x i8] c"\0D\00"
@.str.20.4 = private unnamed_addr constant [1 x i8] c"\00"
define i64 @rxm_ImportHost_TrimEdge(i64 %p_This, i64 %p_text) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_This)
  %slot3 = alloca i64
  store i64 %p_This, ptr %slot3
  call void @Ownership_Retain(i64 %p_text)
  %slot4 = alloca i64
  store i64 %p_text, ptr %slot4
  call void @Ownership_PushScope()
  %v5 = load i64, ptr %slot4
  call void @Ownership_Retain(i64 %v5)
  call void @Ownership_Give(i64 %v5)
  %v6 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v6)
  call void @Ownership_Give(i64 %v6)
  %v7 = call i64 @rxm_ImportHost_TrimLeft(i64 %v6, i64 %v5)
  %v8 = call i64 @Ownership_Keep(i64 %v7)
  %cond9 = icmp eq i64 %v8, 0
  br i1 %cond9, label %own10, label %owned11
own10:
  call void @Ownership_Retain(i64 %v7)
  br label %owned11
owned11:
  %slot12 = alloca i64
  store i64 %v7, ptr %slot12
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v13 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v13)
  call void @Ownership_Give(i64 %v13)
  %v14 = call i64 @Text_Length(i64 %v13)
  %slot15 = alloca i64
  store i64 %v14, ptr %slot15
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  br label %while16
while16:
  %v19 = load i64, ptr %slot15
  %v20 = icmp sgt i64 %v19, 0
  %v21 = zext i1 %v20 to i64
  %cond22 = icmp ne i64 %v21, 0
  br i1 %cond22, label %wbody17, label %wend18
wbody17:
  call void @Ownership_PushScope()
  %v23 = load i64, ptr %slot15
  %v24 = sub i64 %v23, 1
  %v25 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v25)
  call void @Ownership_Give(i64 %v25)
  %v26 = call i64 @Text_Substring(i64 %v25, i64 %v24, i64 1)
  %v27 = call i64 @Ownership_Keep(i64 %v26)
  %cond28 = icmp eq i64 %v27, 0
  br i1 %cond28, label %own29, label %owned30
own29:
  call void @Ownership_Retain(i64 %v26)
  br label %owned30
owned30:
  %slot31 = alloca i64
  store i64 %v26, ptr %slot31
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v32 = load i64, ptr %slot31
  call void @Ownership_Retain(i64 %v32)
  call void @Ownership_Give(i64 %v32)
  %str33 = getelementptr [2 x i8], ptr @.str.20.1, i64 0, i64 0
  %v34 = ptrtoint ptr %str33 to i64
  %v35 = call i64 @Text_Equals(i64 %v32, i64 %v34)
  %v36 = xor i64 %v35, 1
  %logic39 = alloca i64
  %cond40 = icmp ne i64 %v36, 0
  store i64 0, ptr %logic39
  br i1 %cond40, label %rhs37, label %logic38
rhs37:
  %v41 = load i64, ptr %slot31
  call void @Ownership_Retain(i64 %v41)
  call void @Ownership_Give(i64 %v41)
  %str42 = getelementptr [2 x i8], ptr @.str.20.2, i64 0, i64 0
  %v43 = ptrtoint ptr %str42 to i64
  %v44 = call i64 @Text_Equals(i64 %v41, i64 %v43)
  %v45 = xor i64 %v44, 1
  %cond46 = icmp ne i64 %v45, 0
  %v47 = zext i1 %cond46 to i64
  store i64 %v47, ptr %logic39
  br label %logic38
logic38:
  %v48 = load i64, ptr %logic39
  %logic51 = alloca i64
  %cond52 = icmp ne i64 %v48, 0
  store i64 0, ptr %logic51
  br i1 %cond52, label %rhs49, label %logic50
rhs49:
  %v53 = load i64, ptr %slot31
  call void @Ownership_Retain(i64 %v53)
  call void @Ownership_Give(i64 %v53)
  %str54 = getelementptr [2 x i8], ptr @.str.20.3, i64 0, i64 0
  %v55 = ptrtoint ptr %str54 to i64
  %v56 = call i64 @Text_Equals(i64 %v53, i64 %v55)
  %v57 = xor i64 %v56, 1
  %cond58 = icmp ne i64 %v57, 0
  %v59 = zext i1 %cond58 to i64
  store i64 %v59, ptr %logic51
  br label %logic50
logic50:
  %v60 = load i64, ptr %logic51
  %cond61 = icmp ne i64 %v60, 0
  br i1 %cond61, label %then62, label %else63
then62:
  call void @Ownership_PushScope()
  %v65 = load i64, ptr %slot15
  %v66 = load i64, ptr %slot12
  call void @Ownership_Retain(i64 %v66)
  call void @Ownership_Give(i64 %v66)
  %v67 = call i64 @Text_Substring(i64 %v66, i64 0, i64 %v65)
  %v68 = call i64 @Ownership_Keep(i64 %v67)
  %cond69 = icmp eq i64 %v68, 0
  br i1 %cond69, label %own70, label %owned71
own70:
  call void @Ownership_Retain(i64 %v67)
  br label %owned71
owned71:
  %v72 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v72)
  %v73 = load i64, ptr %slot31
  call void @Ownership_Release(i64 %v73)
  %v74 = load i64, ptr %slot12
  call void @Ownership_Release(i64 %v74)
  %v75 = load i64, ptr %slot4
  call void @Ownership_Release(i64 %v75)
  %v76 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v76)
  call void @Ownership_Give(i64 %v67)
  ret i64 %v67
else63:
  br label %merge64
merge64:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v77 = load i64, ptr %slot15
  %v78 = sub i64 %v77, 1
  store i64 %v78, ptr %slot15
  call void @Ownership_PopScope()
  br label %while16
wend18:
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %str79 = getelementptr [1 x i8], ptr @.str.20.4, i64 0, i64 0
  %v80 = ptrtoint ptr %str79 to i64
  %v81 = call i64 @Ownership_Keep(i64 %v80)
  %cond82 = icmp eq i64 %v81, 0
  br i1 %cond82, label %own83, label %owned84
own83:
  call void @Ownership_Retain(i64 %v80)
  br label %owned84
owned84:
  %v85 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v85)
  call void @Ownership_Give(i64 %v80)
  ret i64 %v80
}

define i64 @rxf_Main(i64 %p_args) {
Lentry:
  %v1 = call i64 @Ownership_MarkDepth()
  %slot2 = alloca i64
  store i64 %v1, ptr %slot2
  call void @Ownership_Retain(i64 %p_args)
  %slot3 = alloca i64
  store i64 %p_args, ptr %slot3
  call void @Ownership_PushScope()
  %v4 = call i64 @Object_Create(i64 1)
  call void @rxm_ImportHost_new(i64 %v4)
  %v5 = call i64 @Ownership_Keep(i64 %v4)
  %cond6 = icmp eq i64 %v5, 0
  br i1 %cond6, label %own7, label %owned8
own7:
  call void @Ownership_Retain(i64 %v4)
  br label %owned8
owned8:
  %slot9 = alloca i64
  store i64 %v4, ptr %slot9
  call void @Ownership_PopScope()
  call void @Ownership_PushScope()
  %v10 = load i64, ptr %slot3
  call void @Ownership_Retain(i64 %v10)
  call void @Ownership_Give(i64 %v10)
  %v11 = load i64, ptr %slot9
  call void @Ownership_Retain(i64 %v11)
  call void @Ownership_Give(i64 %v11)
  %v12 = call i64 @rxm_ImportHost_Run(i64 %v11, i64 %v10)
  %v13 = call i64 @Ownership_Keep(i64 %v12)
  %cond14 = icmp eq i64 %v13, 0
  br i1 %cond14, label %own15, label %owned16
own15:
  call void @Ownership_Retain(i64 %v12)
  br label %owned16
owned16:
  %v17 = load i64, ptr %slot2
  call void @Ownership_Unwind(i64 %v17)
  %v18 = load i64, ptr %slot9
  call void @Ownership_Release(i64 %v18)
  %v19 = load i64, ptr %slot3
  call void @Ownership_Release(i64 %v19)
  call void @Ownership_Give(i64 %v12)
  ret i64 %v12
}

declare i64 @Process_Arguments(i32, ptr)
declare void @Process_Finish(i64)

define i32 @main(i32 %argc, ptr %argv) {
  %packed = call i64 @Process_Arguments(i32 %argc, ptr %argv)
  %result = call i64 @rxf_Main(i64 %packed)
  %code = trunc i64 %result to i32
  ret i32 %code
}
