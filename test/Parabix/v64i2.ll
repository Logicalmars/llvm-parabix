; RUN: llc -mattr=+sse2 < %s | FileCheck %s

define void @test_logic(<64 x i2>* %P) {
entry:
  ;CHECK: 124
  ;CHECK: 251
  ;CHECK: 124
  ;CHECK: 251

  ;CHECK-LABEL: test_logic

  %a = bitcast <2 x i64> <i64 43532, i64 43532> to <64 x i2>
  %b = bitcast <2 x i64> <i64 23420, i64 23420> to <64 x i2>
  %c = and <64 x i2> %a, %b
  %d = xor <64 x i2> %a, %b
  %e = or <64 x i2> %c, %d

  store <64 x i2> %e, <64 x i2>* %P
  ret void
}

define void @test_arith(<64 x i2>* %P) {
entry:
  ;CHECK: 0x5d
  ;CHECK: 0xa5
  ;CHECK-LABEL: test_arith

  %a = bitcast <2 x i64> <i64 12123, i64 12123> to <64 x i2>
  %b = bitcast <2 x i64> <i64 34223, i64 34223> to <64 x i2>
  %c = add <64 x i2> %a, %b
  %d = mul <64 x i2> %a, %b
  %e = sub <64 x i2> %c, %d

  store <64 x i2> %e, <64 x i2>* %P
  ret void
}

define <64 x i2> @test_arith_1(<64 x i2> %a, <64 x i2> %b) {
entry:
  ;CHECK-LABEL: test_arith_1
  %c = add <64 x i2> %a, %b
  %d = mul <64 x i2> %a, %b
  %e = sub <64 x i2> %c, %d

  ret <64 x i2> %e
}
