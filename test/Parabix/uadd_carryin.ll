; RUN: llc -march=x86 -mattr=+sse,+sse2 < %s | FileCheck %s

declare {i128, i1} @llvm.uadd.with.overflow.carryin.i128(i128 %a, i128 %b, i1 %carryin)

define void @add_with_carry_ir(<2 x i64> %a, <2 x i64> %b, <2 x i64> %carry_in, <2 x i64>* %carry_out, <2 x i64>* %sum) {
entry:
  ;CHECK-LABEL: add_with_carry_ir
  %aa = bitcast <2 x i64> %a to i128
  %bb = bitcast <2 x i64> %b to i128
  %cc = bitcast <2 x i64> %carry_in to i128
  %cin = trunc i128 %cc to i1

  %res1 = call {i128, i1} @llvm.uadd.with.overflow.carryin.i128(i128 %aa, i128 %bb, i1 %cin)
  %sum1 = extractvalue {i128, i1} %res1, 0
  %obit = extractvalue {i128, i1} %res1, 1

  %ret_sum = bitcast i128 %sum1 to <2 x i64>
  %obit_64 = zext i1 %obit to i64
  %obit_2x64 = insertelement <2 x i64> zeroinitializer, i64 %obit_64, i32 0

  store <2 x i64> %ret_sum, <2 x i64>* %sum
  store <2 x i64> %obit_2x64, <2 x i64>* %carry_out

  ret void
}
