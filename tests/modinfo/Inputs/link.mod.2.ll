;void b() {
;}
; ModuleID = 'link.mod.2.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-freebsd10.0"

; Function Attrs: nounwind uwtable
define void @b() #0 {
  ret void, !dbg !10
}

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!7, !8}
!llvm.ident = !{!9}

!0 = !MDCompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 3.7.0 (git@github.com:CTSRD-SOAAP/clang.git 3187867722e27ca247cc02c521081a5281c9faee) (git@github.com:CTSRD-SOAAP/llvm.git 56c9390e49696b1f414c8e35bac2436f6829e697)", isOptimized: false, runtimeVersion: 0, emissionKind: 1, enums: !2, retainedTypes: !2, subprograms: !3, globals: !2, imports: !2)
!1 = !MDFile(filename: "link.mod.2.c", directory: "/home/kg365/workspace/soaap/tests/modinfo")
!2 = !{}
!3 = !{!4}
!4 = !MDSubprogram(name: "b", scope: !1, file: !1, line: 1, type: !5, isLocal: false, isDefinition: true, scopeLine: 1, isOptimized: false, function: void ()* @b, variables: !2)
!5 = !MDSubroutineType(types: !6)
!6 = !{null}
!7 = !{i32 2, !"Dwarf Version", i32 2}
!8 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{!"clang version 3.7.0 (git@github.com:CTSRD-SOAAP/clang.git 3187867722e27ca247cc02c521081a5281c9faee) (git@github.com:CTSRD-SOAAP/llvm.git 56c9390e49696b1f414c8e35bac2436f6829e697)"}
!10 = !MDLocation(line: 2, column: 1, scope: !4)
