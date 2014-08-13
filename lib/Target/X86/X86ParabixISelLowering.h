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
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/MathExtras.h"
#include <string>

namespace llvm {
  class SDNodeTreeBuilder
  {
    SDValue Op;
    SelectionDAG *DAG;
    SDLoc dl;
  public:
    SDNodeTreeBuilder(SDValue BaseOp, SelectionDAG *_DAG)
      : Op(BaseOp), DAG(_DAG), dl(BaseOp) {}

    SDNodeTreeBuilder(SelectionDAG *_DAG, SDLoc _dl)
      : DAG(_DAG), dl(_dl) {}

    //use the same OpCode with Op
    SDValue DoOp(MVT VT, SDValue Op0, SDValue Op1) {
      if (Op0.getSimpleValueType() != VT)
        Op0 = BITCAST(Op0, VT);
      if (Op1.getSimpleValueType() != VT)
        Op1 = BITCAST(Op1, VT);

      return DAG->getNode(Op.getOpcode(), dl, VT, Op0, Op1);
    }

    SDValue BITCAST(SDValue A, MVT VT) {
      return DAG->getNode(ISD::BITCAST, dl, VT, A);
    }

    SDValue TRUNCATE(SDValue A, MVT VT) {
      return DAG->getNode(ISD::TRUNCATE, dl, VT, A);
    }

    SDValue Constant(int Num, MVT NumType = MVT::i32) {
      return DAG->getConstant(Num, NumType);
    }

    SDValue Undef(EVT VT) {
      return DAG->getUNDEF(VT);
    }

    SDValue BUILD_VECTOR(MVT VT, ArrayRef<SDValue> Elements) {
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

    SDValue INSERT_VECTOR_ELT(SDValue Vec, SDValue Elt, SDValue Idx) {
      MVT VT = Vec.getSimpleValueType();
      return DAG->getNode(ISD::INSERT_VECTOR_ELT, dl, VT, Vec, Elt, Idx);
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

    SDValue ANY_EXTEND(SDValue Val, MVT ToVT) {
      return DAG->getNode(ISD::ANY_EXTEND, dl, ToVT, Val);
    }

    SDValue ZERO_EXTEND(SDValue Val, MVT ToVT) {
      return DAG->getNode(ISD::ZERO_EXTEND, dl, ToVT, Val);
    }

    SDValue SIGN_EXTEND(SDValue Val, MVT ToVT) {
      return DAG->getNode(ISD::SIGN_EXTEND, dl, ToVT, Val);
    }

    SDValue SELECT(SDValue Cond, SDValue TrueVal, SDValue FalseVal) {
      MVT VT = TrueVal.getSimpleValueType();
      return DAG->getNode(ISD::SELECT, dl, VT, Cond, TrueVal, FalseVal);
    }

    SDValue AND(SDValue A, SDValue B) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNode(ISD::AND, dl, VT, A, B);
    }

    SDValue OR(SDValue A, SDValue B) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNode(ISD::OR, dl, VT, A, B);
    }

    SDValue XOR(SDValue A, SDValue B) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNode(ISD::XOR, dl, VT, A, B);
    }

    SDValue NOT(SDValue A) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNOT(dl, A, VT);
    }

    SDValue MUL(SDValue A, SDValue B) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNode(ISD::MUL, dl, VT, A, B);
    }

    SDValue UDIV(SDValue A, SDValue B) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNode(ISD::UDIV, dl, VT, A, B);
    }

    SDValue UREM(SDValue A, SDValue B) {
      MVT VT = A.getSimpleValueType();
      return DAG->getNode(ISD::UREM, dl, VT, A, B);
    }

    SDValue SRL(SDValue A, SDValue Shift) {
      assert(A.getSimpleValueType().SimpleTy == Shift.getSimpleValueType().SimpleTy &&
             "SRL value type doesn't match");
      return DAG->getNode(ISD::SRL, dl, A.getSimpleValueType(), A, Shift);
    }

    SDValue SHL(SDValue A, SDValue Shift) {
      assert(A.getSimpleValueType().SimpleTy == Shift.getSimpleValueType().SimpleTy &&
             "SHL value type doesn't match");
      return DAG->getNode(ISD::SHL, dl, A.getSimpleValueType(), A, Shift);
    }

    template <int sh>
    SDValue SHL(SDValue A) {
      MVT VT = A.getSimpleValueType();
      int NumElts = VT.getVectorNumElements();

      SmallVector<SDValue, 16> Pool;
      SDValue Cst = Constant(sh, VT.getScalarType());
      for (int i = 0; i < NumElts; i++)
        Pool.push_back(Cst);

      return SHL(A, BUILD_VECTOR(VT, Pool));
    }

    SDValue SHL(int sh, SDValue A) {
      MVT VT = A.getSimpleValueType();
      int NumElts = VT.getVectorNumElements();

      SmallVector<SDValue, 16> Pool;
      SDValue Cst = Constant(sh, VT.getScalarType());
      for (int i = 0; i < NumElts; i++)
        Pool.push_back(Cst);

      return SHL(A, BUILD_VECTOR(VT, Pool));
    }

    template <int sh>
    SDValue SRL(SDValue A) {
      MVT VT = A.getSimpleValueType();
      int NumElts = VT.getVectorNumElements();

      SmallVector<SDValue, 16> Pool;
      SDValue Cst = Constant(sh, VT.getScalarType());
      for (int i = 0; i < NumElts; i++)
        Pool.push_back(Cst);

      return SRL(A, BUILD_VECTOR(VT, Pool));
    }

    SDValue SRL(int sh, SDValue A) {
      MVT VT = A.getSimpleValueType();
      int NumElts = VT.getVectorNumElements();

      SmallVector<SDValue, 16> Pool;
      SDValue Cst = Constant(sh, VT.getScalarType());
      for (int i = 0; i < NumElts; i++)
        Pool.push_back(Cst);

      return SRL(A, BUILD_VECTOR(VT, Pool));
    }

    //High mask of RegisterWidth bits vectors with different field width.
    //e.g. fieldwidth = 8, mask is 1111000011110000...
    //return <XX x i64>
    SDValue HiMask(unsigned RegisterWidth, unsigned FieldWidth) {
      assert(isPowerOf2_32(FieldWidth) &&
             isPowerOf2_32(RegisterWidth) &&
             "all width can only be the power of 2");
      assert(FieldWidth <= 32 && "HiMask FieldWidth greater than 32");

      //Get i64 mask node
      std::string mask, finalMask;
      for (unsigned i = 0; i < FieldWidth/2; i++) mask += "1";
      for (unsigned i = 0; i < FieldWidth/2; i++) mask += "0";
      for (unsigned i = 0; i < 64 / mask.size(); i++) finalMask += mask;
      APInt MaskInt64(64, finalMask, 2);
      SDValue MaskNode64 = DAG->getConstant(MaskInt64, MVT::i64);

      if (RegisterWidth == 64) {
        return MaskNode64;
      }
      else if (RegisterWidth == 128) {
        SDValue Pool[] = {MaskNode64, MaskNode64};
        return BUILD_VECTOR(MVT::v2i64, Pool);
      }
      else if (RegisterWidth == 256) {
        SDValue Pool[] = {MaskNode64, MaskNode64, MaskNode64, MaskNode64};
        return BUILD_VECTOR(MVT::v4i64, Pool);
      }
      else
        llvm_unreachable("Wrong RegisterWidth in HiMask");

      return SDValue();
    }

    SDValue PEXT64(SDValue A, SDValue B) {
      assert(A.getSimpleValueType() == MVT::i64 &&
             B.getSimpleValueType() == MVT::i64 &&
             "PEXT64 only take i64 operands");

      SDValue V = DAG->getNode(ISD::INTRINSIC_WO_CHAIN, dl,
                              MVT::i64,
                              DAG->getConstant(Intrinsic::x86_bmi_pext_64, MVT::i32),
                              A,B);
      return V;
    }

    //simd<1>::ifh
    //just like SELECT, but for v128i1 vectors. if Mask[i] == 1, A[i] is chosen.
    //return with the same ValueType of A.
    SDValue IFH1(SDValue Mask, SDValue A, SDValue B) {
      assert(A.getValueType().is128BitVector() &&
             B.getValueType().is128BitVector() &&
             Mask.getValueType().is128BitVector() &&
             "IFH1 only take 128 bit vectors");

      MVT VT = A.getSimpleValueType();
      SDValue NewMask = BITCAST(Mask, MVT::v4i32);
      SDValue NewOp1  = BITCAST(A, MVT::v4i32);
      SDValue NewOp2  = BITCAST(B, MVT::v4i32);

      // (NewMask & NewOp1) || (~NewMask & NewOp2)
      SDValue R = OR(AND(NewMask, NewOp1), AND(NOT(NewMask), NewOp2));
      return BITCAST(R, VT);
    }
  };
}

#endif

