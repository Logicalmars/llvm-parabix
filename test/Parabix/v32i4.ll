; RUN: llc -mattr=+sse2 < %s | FileCheck %s

define <32 x i4> @test_add(<32 x i4> %a, <32 x i4> %b) {
entry:
  ;CHECK-LABEL: test_add

  %c = add <32 x i4> %a, %b
  ret <32 x i4> %c
  ;CHECK: paddb
}

define void @test_logic(<32 x i4>* %P) {
entry:
  ;CHECK: 124
  ;CHECK: 251
  ;CHECK: 124
  ;CHECK: 251

  ;CHECK-LABEL: test_logic

  %a = bitcast <2 x i64> <i64 43532, i64 43532> to <32 x i4>
  %b = bitcast <2 x i64> <i64 23420, i64 23420> to <32 x i4>
  %c = and <32 x i4> %a, %b
  %d = xor <32 x i4> %a, %b
  %e = or <32 x i4> %c, %d

  store <32 x i4> %e, <32 x i4>* %P
  ret void
}

define <16 x i8> @test_mult_8(<16 x i8> %a, <16 x i8> %b) {
entry:
  ;CHECK-LABEL: test_mult_8

  %c = mul <16 x i8> %a, %b
  ret <16 x i8> %c
  ;CHECK: pmullw
  ;CHECK: psrlw
  ;CHECK: psllw
}

define <32 x i4> @test_mult_4(<32 x i4> %a, <32 x i4> %b) {
entry:
  ;CHECK-LABEL: test_mult_4

  %c = mul <32 x i4> %a, %b
  ret <32 x i4> %c
  ;CHECK: pmullw
  ;CHECK: pmullw
}

define <32 x i4> @test_shl(<32 x i4> %a, <32 x i4> %b) {
entry:
  ;CHECK-LABEL: test_shl
  %c = shl <32 x i4> %a, %b
  ret <32 x i4> %c
  ;CHECK: psllw
  ;CHECK: psllw
}

define <32 x i4> @test_srli(<32 x i4> %a) {
entry:
  ;CHECK-LABEL: test_srli
  %c = lshr <32 x i4> %a, <i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2>

  ;CHECK: psrld
  ;CHECK: pand
  ret <32 x i4> %c
}

define <32 x i4> @test_shli(<32 x i4> %a) {
entry:
  ;CHECK-LABEL: test_shli
  %c = shl <32 x i4> %a, <i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2,i4 2, i4 2, i4 2, i4 2>

  ;CHECK: pslld
  ;CHECK: pand
  ret <32 x i4> %c
}

define <16 x i8> @test_shli_8(<16 x i8> %a) {
entry:
  ;CHECK-LABEL: test_shli_8
  %c = shl <16 x i8> %a, <i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2, i8 2>
  ;CHECK: psllw
  ;CHECK: pand

  ret <16 x i8> %c
}

