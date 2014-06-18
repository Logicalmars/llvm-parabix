; RUN: llc -march=x86 < %s | FileCheck %s

define void @test(<32 x i1>* %P) {
entry:

  %a = bitcast i32 12123 to <32 x i1>
  %b = bitcast i32 34223 to <32 x i1>
  %c = add <32 x i1> %a, %b
  %d = mul <32 x i1> %a, %b
  %e = sub <32 x i1> %c, %d

  store <32 x i1> %e, <32 x i1>* %P
  ;proper lowering will cause constant combine.
  ;CHECK-LABEL: test
  ;CHECK: movl $45055

  ret void
}

define void @test1(<32 x i1>* %P) {
entry:

  %a = bitcast i32 43532 to <32 x i1>
  %b = bitcast i32 23420 to <32 x i1>
  %c = and <32 x i1> %a, %b
  %d = xor <32 x i1> %a, %b
  %e = or <32 x i1> %c, %d

  store <32 x i1> %e, <32 x i1>* %P
  ;CHECK-LABEL: test1
  ;CHECK: movl $64380

  ret void
}



