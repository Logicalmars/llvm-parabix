; RUN: llc -march=x86 < %s | FileCheck %s

define void @test(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test

  %a = bitcast i32 14123232 to <32 x i1>
  %b = extractelement <32 x i1> %a, i32 0
  %c = insertelement <32 x i1> %a, i1 %b, i32 20

  store <32 x i1> %c, <32 x i1>* %P
  ret void
}

