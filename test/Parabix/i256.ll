; RUN: llc < %s -mtriple=x86_64-apple-darwin -mcpu=core-avx2 | FileCheck %s

define <32 x i8> @constant_i256_avx2() {
; CHECK-LABEL: constant_i256_avx2:
  ret <32 x i8> <i8 0, i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7, i8 0, i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7, i8 0, i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7,i8 0, i8 1, i8 2, i8 3, i8 4, i8 5, i8 6, i8 7>
}

define void @sign_ext(i8* %a, i8* %b, i8* %c, i8* %d) {
  ;CHECK-LABEL: sign_ext
  %o = bitcast i32 123345 to <32 x i1>
  %e = sext <32 x i1> %o to <32 x i8>

  %a1 = extractelement <32 x i8> %e, i32 0
  %b1 = extractelement <32 x i8> %e, i32 1
  %c1 = extractelement <32 x i8> %e, i32 2
  %d1 = extractelement <32 x i8> %e, i32 3

  store i8 %a1, i8* %a
  store i8 %b1, i8* %b
  store i8 %c1, i8* %c
  store i8 %d1, i8* %d

  ;CHECK: movb $-1
  ;CHECK: movb $0
  ;CHECK: movb $0
  ;CHECK: movb $0

  ret void
}

