; RUN: llc -march=x86-64 -mattr=-sse < %s | FileCheck %s

define void @test_shuffle(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_shuffle

  %a = bitcast i64 0 to <64 x i1>
  %b = bitcast i64 -1 to <64 x i1>

  %c = shufflevector <64 x i1> %a, <64 x i1> %b,
                    <8 x i32> <i32 0, i32 64, i32 1, i32 65, i32 2, i32 66, i32 3, i32 67>
  %d = bitcast <8 x i1> %c to i8
  %e = zext i8 %d to i64
  %f = bitcast i64 %e to <64 x i1>

  store <64 x i1> %f, <64 x i1>* %P
  ;CHECK: movq $170
  ret void
}
