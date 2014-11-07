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

define <2 x i64> @shl_8(<2 x i64> %a) {
entry:
  ;CHECK-LABEL: shl_8
  %aa = bitcast <2 x i64> %a to i128

  %r = shl i128 %aa, 8
  %rr = bitcast i128 %r to <2 x i64>

  ret <2 x i64> %rr
  ;CHECK: pslldq $1
}

define <2 x i64> @lshr_1(<2 x i64> %a) {
entry:
  ;CHECK-LABEL: lshr_1
  %aa = bitcast <2 x i64> %a to i128

  %r = lshr i128 %aa, 1
  %rr = bitcast i128 %r to <2 x i64>

  ret <2 x i64> %rr
  ;CHECK: psrlq $1
  ;CHECK: psllq $63
  ;CHECK: psrldq
  ;CHECK: por
}

define <2 x i64> @lshr_8(<2 x i64> %a) {
entry:
  ;CHECK-LABEL: lshr_8
  %aa = bitcast <2 x i64> %a to i128

  %r = lshr i128 %aa, 8
  %rr = bitcast i128 %r to <2 x i64>

  ret <2 x i64> %rr
  ;CHECK: psrldq $1
}

define <2 x i64> @dslli(<2 x i64> %a, <2 x i64> %b) {
entry:
  ;CHECK-LABEL: dslli
  %aa = bitcast <2 x i64> %a to i128
  %bb = bitcast <2 x i64> %b to i128

  %a1 = shl i128 %aa, 1
  %b1 = lshr i128 %bb, 127
  %cc = or i128 %a1, %b1

  %c = bitcast i128 %cc to <2 x i64>
  ret <2 x i64> %c
  ;CHECK: shufpd
  ;CHECK: psrlq
  ;CHECK: psllq
  ;CHECK: por
}


define void @advance_with_carry(<2 x i64> %strm, <2 x i64> %carry, <2 x i64>* %r, <2 x i64>* %carry_out) {
entry:
  ;CHECK-LABEL: advance_with_carry
  %a = bitcast <2 x i64> %strm to i128
  %c = bitcast <2 x i64> %carry to i128

  %a_shift = shl i128 %a, 1
  %adv = or i128 %a_shift, %c

  %adv_r = bitcast i128 %adv to <2 x i64>
  store <2 x i64> %adv_r, <2 x i64>* %r

  %strm_out = lshr i128 %a, 127
  %strm_out1 = bitcast i128 %strm_out to <2 x i64>
  store <2 x i64> %strm_out1, <2 x i64>* %carry_out

  ret void
}

; long stream add/shift
declare {i128, i1} @llvm.uadd.with.overflow.i128(i128 %a, i128 %b)

define <2 x i64> @uadd_with_overflow_i128(<2 x i64> %a, <2 x i64> %b) {
entry:
  ;CHECK-LABEL: uadd_with_overflow_i128
  %aa = bitcast <2 x i64> %a to i128
  %bb = bitcast <2 x i64> %b to i128

  %res = call {i128, i1} @llvm.uadd.with.overflow.i128(i128 %aa, i128 %bb)
  %sum = extractvalue {i128, i1} %res, 0

  %sum1 = bitcast i128 %sum to <2 x i64>

  ret <2 x i64> %sum1
}
