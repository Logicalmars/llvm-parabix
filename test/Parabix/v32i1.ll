; RUN: llc -march=x86 < %s | FileCheck %s

define void @test(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test

  %a = bitcast i32 14123232 to <32 x i1>
  %b = extractelement <32 x i1> %a, i32 0
  %c = insertelement <32 x i1> %a, i1 %b, i32 20

  store <32 x i1> %c, <32 x i1>* %P
  ;CHECK: movl $13074656
  ret void
}

define void @test1(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test1

  %a = bitcast i32 14123232 to <32 x i1>
  %b = insertelement <32 x i1> %a, i1 0, i32 20

  store <32 x i1> %b, <32 x i1>* %P
  ;CHECK: movl $13074656
  ret void
}

define void @test2(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test2

  %a = bitcast i32 14123232 to <32 x i1>
  %b = insertelement <32 x i1> %a, i1 1, i32 20

  store <32 x i1> %b, <32 x i1>* %P
  ;CHECK: movl $14123232
  ret void
}

define void @test3(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test3

  %a = bitcast i32 14123232 to <32 x i1>
  %b = extractelement <32 x i1> %a, i32 20
  %c = xor i1 %b, 1
  %d = insertelement <32 x i1> %a, i1 %c, i32 20

  store <32 x i1> %d, <32 x i1>* %P
  ;CHECK: movl $13074656
  ret void
}

