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
#include "llvm/ADT/SmallVector.h"

namespace llvm {
  class SDNodeTreeBuilder
  {
    SDValue Op;
    SelectionDAG *DAG;
    SDLoc dl;
  public:
    SDNodeTreeBuilder(SDValue BaseOp, SelectionDAG *_DAG)
      : Op(BaseOp), DAG(_DAG), dl(BaseOp) {}

    SDValue Constant(int Num, MVT NumType = MVT::i32) {
      return DAG->getConstant(Num, NumType);
    }

    SDValue Undef(EVT VT) {
      return DAG->getUNDEF(VT);
    }

    SDValue BUILD_VECTOR(MVT VT, SmallVectorImpl<SDValue> &Elements) {
      assert(VT.isVector() && "not building vector");

      unsigned NumElems = VT.getVectorNumElements();
      assert(Elements.size() == NumElems);

      return DAG->getNode(ISD::BUILD_VECTOR, dl, VT, Elements);
    }

    SDValue EXTRACT_VECTOR_ELT(SDValue Vec, SDValue Idx) {
      MVT VT = Vec.getSimpleValueType();
      MVT EltVT = VT.getVectorElementType();
      if (EltVT == MVT::i1)
        EltVT = MVT::i8;

      return DAG->getNode(ISD::EXTRACT_VECTOR_ELT, dl, EltVT, Vec, Idx);
    }

    SDValue EXTRACT_VECTOR_ELT(SDValue Vec, unsigned Idx) {
      return EXTRACT_VECTOR_ELT(Vec, Constant(Idx));
    }

    // int Mask[] = {...}, call with &Mask[0]
    SDValue VECTOR_SHUFFLE(SDValue Vec0, SDValue Vec1, const int * Mask) {
      MVT VT = Vec0.getSimpleValueType();
      return DAG->getVectorShuffle(VT, dl, Vec0, Vec1, Mask);
    }

    SDValue SIGN_EXTEND_INREG(SDValue Val, MVT FromVT, MVT ToVT) {
      return DAG->getNode(ISD::SIGN_EXTEND_INREG, dl, ToVT, Val,
                          DAG->getValueType(FromVT));
    }

    SDValue SELECT(SDValue Cond, SDValue TrueVal, SDValue FalseVal) {
      MVT VT = TrueVal.getSimpleValueType();
      return DAG->getNode(ISD::SELECT, dl, VT, Cond, TrueVal, FalseVal);
    }
  };
}

#endif

