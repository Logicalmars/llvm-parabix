//===-- X86ParabixISelLowering.h - X86 DAG Lowering Interface ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interface that Parabix uses on X86
//
//===----------------------------------------------------------------------===//

#ifndef X86PARABIXISELLOWERING_H
#define X86PARABIXISELLOWERING_H

#include "X86Subtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/TargetOptions.h"

// WARN: THIS FILE IS NOT USED FOR NOW
// May use it as a general parabix lower interface for many targets.
namespace llvm {
  namespace parabix {
    //SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const;
  }
}

#endif

