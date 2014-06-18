; RUN: llc -march=x86 < %s | FileCheck %s

define void @test(<32 x i1>* %P) {
entry:
  %a = bitcast i32 12123 to <32 x i1>
  %b = bitcast i32 34223 to <32 x i1>
  %c = add <32 x i1> %a, %b
  store <32 x i1> %c, <32 x i1>* %P
  ;CHECK: movl $43764

  ret void
}


