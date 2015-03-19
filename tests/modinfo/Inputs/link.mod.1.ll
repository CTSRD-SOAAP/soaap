;extern void b();
;
;int a() {
;  b();
;  return 1;
;}
;
;int main(int argc, char** argv) {
;  a();
;}

; ModuleID = 'link.mod.1.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-freebsd10.0"

; Function Attrs: nounwind uwtable
define i32 @a() #0 {
  call void (...)* @b(), !dbg !17
  ret i32 1, !dbg !18
}

declare void @b(...) #1

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
  %1 = alloca i32, align 4
  %2 = alloca i8**, align 8
  store i32 %argc, i32* %1, align 4
  call void @llvm.dbg.declare(metadata i32* %1, metadata !19, metadata !20), !dbg !21
  store i8** %argv, i8*** %2, align 8
  call void @llvm.dbg.declare(metadata i8*** %2, metadata !22, metadata !20), !dbg !23
  %3 = call i32 @a(), !dbg !24
  ret i32 0, !dbg !25
}

; Function Attrs: nounwind readnone
declare void @llvm.dbg.declare(metadata, metadata, metadata) #2

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind readnone }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!14, !15}
!llvm.ident = !{!16}

!0 = !MDCompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 3.7.0 (git@github.com:CTSRD-SOAAP/clang.git 3187867722e27ca247cc02c521081a5281c9faee) (git@github.com:CTSRD-SOAAP/llvm.git 56c9390e49696b1f414c8e35bac2436f6829e697)", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, retainedTypes: !2, subprograms: !3, globals: !2, imports: !2)
!1 = !MDFile(filename: "link.mod.1.c", directory: "/home/kg365/workspace/soaap/tests/modinfo")
!2 = !{}
!3 = !{!4, !8}
!4 = !MDSubprogram(name: "a", scope: !1, file: !1, line: 3, type: !5, isLocal: false, isDefinition: true, scopeLine: 3, isOptimized: false, function: i32 ()* @a, variables: !2)
!5 = !MDSubroutineType(types: !6)
!6 = !{!7}
!7 = !MDBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
!8 = !MDSubprogram(name: "main", scope: !1, file: !1, line: 8, type: !9, isLocal: false, isDefinition: true, scopeLine: 8, flags: DIFlagPrototyped, isOptimized: false, function: i32 (i32, i8**)* @main, variables: !2)
!9 = !MDSubroutineType(types: !10)
!10 = !{!7, !7, !11}
!11 = !MDDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64, align: 64)
!12 = !MDDerivedType(tag: DW_TAG_pointer_type, baseType: !13, size: 64, align: 64)
!13 = !MDBasicType(name: "char", size: 8, align: 8, encoding: DW_ATE_signed_char)
!14 = !{i32 2, !"Dwarf Version", i32 2}
!15 = !{i32 2, !"Debug Info Version", i32 3}
!16 = !{!"clang version 3.7.0 (git@github.com:CTSRD-SOAAP/clang.git 3187867722e27ca247cc02c521081a5281c9faee) (git@github.com:CTSRD-SOAAP/llvm.git 56c9390e49696b1f414c8e35bac2436f6829e697)"}
!17 = !MDLocation(line: 4, column: 3, scope: !4)
!18 = !MDLocation(line: 5, column: 3, scope: !4)
!19 = !MDLocalVariable(tag: DW_TAG_arg_variable, name: "argc", arg: 1, scope: !8, file: !1, line: 8, type: !7)
!20 = !MDExpression()
!21 = !MDLocation(line: 8, column: 14, scope: !8)
!22 = !MDLocalVariable(tag: DW_TAG_arg_variable, name: "argv", arg: 2, scope: !8, file: !1, line: 8, type: !11)
!23 = !MDLocation(line: 8, column: 27, scope: !8)
!24 = !MDLocation(line: 9, column: 3, scope: !8)
!25 = !MDLocation(line: 10, column: 1, scope: !8)
