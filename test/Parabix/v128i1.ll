; RUN: llc -march=x86 -mattr=+sse,+sse2 < %s | FileCheck %s

define <4 x i32> @ifh_1(<4 x i32> %cond, <4 x i32> %b, <4 x i32> %c) {
entry:
  ;CHECK-LABEL: ifh_1
  %mm = bitcast <4 x i32> %cond to <128 x i1>
  %bb = bitcast <4 x i32> %b to <128 x i1>
  %cc = bitcast <4 x i32> %c to <128 x i1>

  %rr = select <128 x i1> %mm, <128 x i1> %bb, <128 x i1> %cc
  %r  = bitcast <128 x i1> %rr to <4 x i32>
  ;CHECK: andnps
  ;CHECK: orps
  ret <4 x i32> %r
}

define <4 x i32> @ifh_1_ideal(<4 x i32> %cond, <4 x i32> %b, <4 x i32> %c) {
entry:
  ;CHECK-LABEL: ifh_1_ideal
  %not_cond = xor <4 x i32> %cond, <i32 -1, i32 -1, i32 -1, i32 -1>

  %t0 = and <4 x i32> %cond, %b
  %t1 = and <4 x i32> %not_cond, %c
  %r = or <4 x i32> %t0, %t1
  ;CHECK: andnps
  ;CHECK: orps

  ret <4 x i32> %r
}

define <4 x i32> @packh_16(<4 x i32> %a, <4 x i32> %b) {
entry:
  ;CHECK-LABEL: packh_16
  %aa = bitcast <4 x i32> %a to <16 x i8>
  %bb = bitcast <4 x i32> %b to <16 x i8>
  %rr = shufflevector <16 x i8> %bb, <16 x i8> %aa, <16 x i32> <i32 1, i32 3, i32 5, i32 7, i32 9, i32 11, i32 13, i32 15, i32 17, i32 19, i32 21, i32 23, i32 25, i32 27, i32 29, i32 31>

  %rr1 = bitcast <16 x i8> %rr to <4 x i32>
  ret <4 x i32> %rr1
  ;CHECK: psrlw
  ;CHECK: packuswb
}

define <4 x i32> @packl_16(<4 x i32> %a, <4 x i32> %b) {
entry:
  ;CHECK-LABEL: packl_16
  %aa = bitcast <4 x i32> %a to <16 x i8>
  %bb = bitcast <4 x i32> %b to <16 x i8>
  %rr = shufflevector <16 x i8> %bb, <16 x i8> %aa, <16 x i32> <i32 0, i32 2, i32 4, i32 6, i32 8, i32 10, i32 12, i32 14, i32 16, i32 18, i32 20, i32 22, i32 24, i32 26, i32 28, i32 30>

  %rr1 = bitcast <16 x i8> %rr to <4 x i32>
  ret <4 x i32> %rr1
  ;CHECK pand
  ;CHECK packuswb
}
