; RUN: llc -march=x86-64 -mattr=+sse2 < %s | FileCheck %s

define <2 x i64> @shl_1(<2 x i64> %a) {
entry:
  ;CHECK-LABEL: shl_1
  %aa = bitcast <2 x i64> %a to i128

  %r = shl i128 %aa, 1
  %rr = bitcast i128 %r to <2 x i64>

  ret <2 x i64> %rr
  ;CHECK: psllq $1
  ;CHECK: psrlq $63
  ;CHECK: pslldq
  ;CHECK: por
}
