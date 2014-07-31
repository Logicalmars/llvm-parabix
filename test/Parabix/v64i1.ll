; RUN: llc -march=x86-64 < %s | FileCheck %s

define void @test_arith(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_arith

  %a = bitcast i64 12123 to <64 x i1>
  %b = bitcast i64 34223 to <64 x i1>
  %c = add <64 x i1> %a, %b
  %d = mul <64 x i1> %a, %b
  %e = sub <64 x i1> %c, %d

  store <64 x i1> %e, <64 x i1>* %P
  ;CHECK: movq $45055
  ret void
}

define void @test_logic(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_logic

  %a = bitcast i64 43532 to <64 x i1>
  %b = bitcast i64 23420 to <64 x i1>
  %c = and <64 x i1> %a, %b
  %d = xor <64 x i1> %a, %b
  %e = or <64 x i1> %c, %d

  store <64 x i1> %e, <64 x i1>* %P
  ;CHECK: movq $64380

  ret void
}

define void @test_ins_ext(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_ins_ext

  %a = bitcast i64 14123232 to <64 x i1>
  %b = extractelement <64 x i1> %a, i32 0
  %c = insertelement <64 x i1> %a, i1 %b, i32 20

  store <64 x i1> %c, <64 x i1>* %P
  ;CHECK: movq $13074656
  ret void
}

define void @test_ins_0(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_ins_0

  %a = bitcast i64 14123232 to <64 x i1>
  %b = insertelement <64 x i1> %a, i1 0, i32 20

  store <64 x i1> %b, <64 x i1>* %P
  ;CHECK: movq $13074656
  ret void
}

define void @test_ins_1(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_ins_1

  %a = bitcast i64 14123232 to <64 x i1>
  %b = insertelement <64 x i1> %a, i1 1, i32 20

  store <64 x i1> %b, <64 x i1>* %P
  ;CHECK: movq $14123232
  ret void
}

define void @test_ext_ins_1(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_ext_ins_1

  %a = bitcast i64 14123232 to <64 x i1>
  %b = extractelement <64 x i1> %a, i32 20
  %c = xor i1 %b, 1
  %d = insertelement <64 x i1> %a, i1 %c, i32 20

  store <64 x i1> %d, <64 x i1>* %P
  ;CHECK: movq $13074656
  ret void
}

define void @test_icmp(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_icmp
  %a = bitcast i64 14123232 to <64 x i1>
  %b = bitcast i64 10101223 to <64 x i1>
  %one = bitcast i64 -1 to <64 x i1>

  %d = icmp ult <64 x i1> %a, %b
  %e = icmp sle <64 x i1> %a, %b

  %x = xor <64 x i1> %e, %one
  %eq = icmp eq <64 x i1> %d, %x

  store <64 x i1> %eq, <64 x i1>* %P
  ;CHECK: movq $-1

  ret void
}

define void @test_select(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_select
  %a = bitcast i64 14123232 to <64 x i1>
  %b = bitcast i64 10101223 to <64 x i1>
  %mask = bitcast i64 -2 to <64 x i1>

  %bb = select i1 0, <64 x i1> %a, <64 x i1> %b
  %aa = select <64 x i1> %mask, <64 x i1> %a, <64 x i1> %b

  %res = and <64 x i1> %aa, %bb
  store <64 x i1> %res, <64 x i1>* %P
  ;CHECK: movq $9568481

  ret void
}

define void @test_shl(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_shl

  %a = bitcast i64 14123232 to <64 x i1>
  %b = bitcast i64 12321423 to <64 x i1>
  %c = shl <64 x i1> %a, %b

  store <64 x i1> %c, <64 x i1>* %P
  ret void

  ; 0b110101111000000011100000
  ; 0b101111000000001010001111
  ; 0b010000111000000001100000 : (4423776)
  ; CHECK: movq $4423776
}

define void @test_shift(<64 x i1>* %P) {
entry:
  ;CHECK-LABEL: test_shift

  %a = bitcast i64 14123232 to <64 x i1>
  %b = bitcast i64 12321423 to <64 x i1>
  %c = lshr <64 x i1> %a, %b
  %d = ashr <64 x i1> %a, %b
  %e = shl <64 x i1> %c, %d

  store <64 x i1> %e, <64 x i1>* %P
  ret void

  ; CHECK: movq $0
}
