; RUN: llc -march=x86 < %s | FileCheck %s

define void @test2(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test2

  %a = bitcast i32 14123232 to <32 x i1>
  %b = bitcast i32 12321423 to <32 x i1>
  %c = shl <32 x i1> %a, %b

  store <32 x i1> %c, <32 x i1>* %P
  ret void

  ; 0b110101111000000011100000
  ; 0b101111000000001010001111
  ; 0b010000111000000001100000 : (4423776)
  ; CHECK: movl $4423776
}

define void @test3(<32 x i1>* %P) {
entry:
  ;CHECK-LABEL: test3

  %a = bitcast i32 14123232 to <32 x i1>
  %b = bitcast i32 12321423 to <32 x i1>
  %c = lshr <32 x i1> %a, %b
  %d = ashr <32 x i1> %a, %b
  %e = shl <32 x i1> %c, %d

  store <32 x i1> %e, <32 x i1>* %P
  ret void

  ; CHECK: movl $0
}

