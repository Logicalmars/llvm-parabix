; RUN: llc -march=x86 < %s | FileCheck %s

define i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %a = alloca <32 x i1>, align 16
  %b = alloca <32 x i1>, align 16
  %c = alloca <32 x i1>, align 16

  store i32 0, i32* %retval
  store <32 x i1> zeroinitializer, <32 x i1>* %a, align 16
  store <32 x i1> zeroinitializer, <32 x i1>* %b, align 16
  %0 = load <32 x i1>* %a, align 16
  %1 = load <32 x i1>* %b, align 16
  ; CHECK-NOT: addl
  %add = add nsw <32 x i1> %0, %1
  store <32 x i1> %add, <32 x i1>* %c, align 16
  ; CHECK: movl 180(%esp), %eax

  %2 = load i32* %retval, align 4
  ret i32 %2
}


