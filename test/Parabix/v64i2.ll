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
