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

define void @test_shuffle(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_shuffle

  %a = bitcast i32 0 to <32 x i1>
  %b = bitcast i32 -1 to <32 x i1>

  %c = shufflevector <32 x i1> %a, <32 x i1> %b,
                    <8 x i32> <i32 0, i32 32, i32 1, i32 33, i32 2, i32 34, i32 3, i32 35>
  %d = bitcast <8 x i1> %c to i8
  %e = zext i8 %d to i32
  %f = bitcast i32 %e to <32 x i1>

  store <32 x i1> %f, <32 x i1>* %P
  ;CHECK: movl $170
  ret void
}

define void @test_icmp(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_icmp
  %a = bitcast i32 14123232 to <32 x i1>
  %b = bitcast i32 10101223 to <32 x i1>
  %one = bitcast i32 -1 to <32 x i1>

  %d = icmp ult <32 x i1> %a, %b
  %e = icmp slt <32 x i1> %a, %b

  %x = xor <32 x i1> %e, %one
  %eq = icmp eq <32 x i1> %d, %x

  store <32 x i1> %eq, <32 x i1>* %P
  ;CHECK: movl $-1

  ret void
}

define void @test_select(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_select
  %a = bitcast i32 14123232 to <32 x i1>
  %b = bitcast i32 10101223 to <32 x i1>
  %mask = bitcast i32 -2 to <32 x i1>

  %bb = select i1 0, <32 x i1> %a, <32 x i1> %b
  %aa = select <32 x i1> %mask, <32 x i1> %a, <32 x i1> %b

  %res = and <32 x i1> %aa, %bb
  store <32 x i1> %res, <32 x i1>* %P
  ;CHECK: movl $9568481

  ret void
}

