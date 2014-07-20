; RUN: llc -march=x86 < %s | FileCheck %s

define void @test(<32 x i1>* %P) {
entry:
  %a = bitcast i32 12123 to <32 x i1>
  %b = bitcast i32 34223 to <32 x i1>
  %c = add <32 x i1> %a, %b
  store <32 x i1> %c, <32 x i1>* %P
  ;CHECK: movl $43764

  ret void
}

define void @test1(i8* %P) {
entry:
  ;CHECK-LABEL: test1
  %a = bitcast <8 x i1> <i1 0, i1 1, i1 0, i1 1, i1 0, i1 1, i1 0, i1 1> to i8

  store i8 %a, i8* %P
  ;CHECK: movb $-86
  ret void
}

define void @test2(i8* %P) {
entry:
  ;CHECK-LABEL: test2
  %a = bitcast <2 x i1> <i1 0, i1 1> to i2
  %b = zext i2 %a to i8

  store i8 %b, i8* %P
  ;CHECK: movb $2
  ret void
}

define void @test3(i2* %P) {
entry:
  ;CHECK-LABEL: test3
  %a = bitcast <2 x i1> <i1 0, i1 1> to i2
  store i2 %a, i2* %P

  ;CHECK: movb $2
  ret void
}

