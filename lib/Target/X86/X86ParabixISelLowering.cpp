//===-- X86ParabixISelLowering.cpp - X86 Parabix DAG Lowering Implementation ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------------===//
//
// This file defines the interfaces that Parabix uses to lower LLVM code into a
// selection DAG on X86.
//
// Lowering Strategy Sequence:
// For LowerParabixOperation, we check if the op fits in CastAndOpKind first. If fits,
// lowering process is done and terminated. Otherwise, we would check those general
// policies (like in-place vector promotion). If fail again, custom lowering code will
// be executed (like PXLowerADD).
//
//===----------------------------------------------------------------------------===//

#include "X86ISelLowering.h"
#include "Utils/X86ShuffleDecode.h"
#include "X86CallingConv.h"
#include "X86InstrBuilder.h"
#include "X86MachineFunctionInfo.h"
#include "X86TargetMachine.h"
#include "X86TargetObjectFile.h"
#include "X86ParabixISelLowering.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/VariadicFunction.h"
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetOptions.h"
#include "ParabixGeneratedFuncs.h"
#include <bitset>
#include <cctype>
#include <map>
#include <string>
using namespace llvm;

#define DEBUG_TYPE "parabix"

//v32i1 => i32, v32i2 => i64, etc
MVT getFullRegisterType(MVT VT) {
  MVT castType;
  if (VT.is32BitVector())
    castType = MVT::i32;
  else if (VT.is64BitVector())
    castType = MVT::i64;
  else if (VT.is128BitVector())
    castType = MVT::v2i64;
  else
    llvm_unreachable("unsupported parabix vector width");

  return castType;
}

typedef std::pair<ISD::NodeType, MVT> CastAndOpKind;
static std::map<CastAndOpKind, ISD::NodeType> CAOops;

enum PXLegalizeAction {
  InPlacePromote
};

typedef CastAndOpKind OpKind;
static std::map<OpKind, PXLegalizeAction> OpKindActions;

static void addCastAndOpKind(ISD::NodeType Op, MVT VT, ISD::NodeType ReplaceOp)
{
  CAOops[std::make_pair(Op, VT)] = ReplaceOp;
}

static void addOpKindAction(ISD::NodeType Op, MVT VT, PXLegalizeAction action)
{
  OpKindActions[std::make_pair(Op, VT)] = action;
}

static void resetOperations()
{
  //NEED: setOperationAction in X86ISelLowering with Custom
  CAOops.clear();

  //use XOR to simulate ADD on v32i1
  addCastAndOpKind(ISD::ADD, MVT::v32i1, ISD::XOR);
  addCastAndOpKind(ISD::SUB, MVT::v32i1, ISD::XOR);
  addCastAndOpKind(ISD::MUL, MVT::v32i1, ISD::AND);
  addCastAndOpKind(ISD::AND, MVT::v32i1, ISD::AND);
  addCastAndOpKind(ISD::XOR, MVT::v32i1, ISD::XOR);
  addCastAndOpKind(ISD::OR,  MVT::v32i1, ISD::OR);
  addCastAndOpKind(ISD::MULHU,  MVT::v32i1, ISD::AND);

  addCastAndOpKind(ISD::ADD, MVT::v64i1, ISD::XOR);
  addCastAndOpKind(ISD::SUB, MVT::v64i1, ISD::XOR);
  addCastAndOpKind(ISD::MUL, MVT::v64i1, ISD::AND);
  addCastAndOpKind(ISD::AND, MVT::v64i1, ISD::AND);
  addCastAndOpKind(ISD::XOR, MVT::v64i1, ISD::XOR);
  addCastAndOpKind(ISD::OR,  MVT::v64i1, ISD::OR);
  addCastAndOpKind(ISD::MULHU,  MVT::v64i1, ISD::AND);

  addCastAndOpKind(ISD::ADD, MVT::v128i1, ISD::XOR);
  addCastAndOpKind(ISD::SUB, MVT::v128i1, ISD::XOR);
  addCastAndOpKind(ISD::MUL, MVT::v128i1, ISD::AND);
  addCastAndOpKind(ISD::AND, MVT::v128i1, ISD::AND);
  addCastAndOpKind(ISD::XOR, MVT::v128i1, ISD::XOR);
  addCastAndOpKind(ISD::OR,  MVT::v128i1, ISD::OR);
  addCastAndOpKind(ISD::MULHU,  MVT::v128i1, ISD::AND);

  //cast v64i2 to v2i64 to lower logic ops.
  addCastAndOpKind(ISD::AND, MVT::v64i2, ISD::AND);
  addCastAndOpKind(ISD::XOR, MVT::v64i2, ISD::XOR);
  addCastAndOpKind(ISD::OR,  MVT::v64i2, ISD::OR);

  addCastAndOpKind(ISD::AND, MVT::v32i4, ISD::AND);
  addCastAndOpKind(ISD::XOR, MVT::v32i4, ISD::XOR);
  addCastAndOpKind(ISD::OR,  MVT::v32i4, ISD::OR);

  //A custom lowering for v32i4 add is implmented. So ADD is not here.
  addOpKindAction(ISD::SUB, MVT::v32i4, InPlacePromote);
  addOpKindAction(ISD::MUL, MVT::v32i4, InPlacePromote);
  addOpKindAction(ISD::SHL, MVT::v32i4, InPlacePromote);
  addOpKindAction(ISD::SRL, MVT::v32i4, InPlacePromote);
  addOpKindAction(ISD::SRA, MVT::v32i4, InPlacePromote);
  addOpKindAction(ISD::SETCC, MVT::v32i4, InPlacePromote);

  addOpKindAction(ISD::MUL, MVT::v16i8, InPlacePromote);
}

static SDValue getFullRegister(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  SDLoc dl(Op);

  return DAG.getNode(ISD::BITCAST, dl, getFullRegisterType(VT), Op);
}

//Promote vector type in place, doubling fieldwidth within the same register
//e.g. v32i4 => v16i8
static MVT PromoteTypeDouble(MVT VT) {
  unsigned RegisterWidth = VT.getSizeInBits();
  unsigned FieldWidth = VT.getScalarSizeInBits();
  unsigned NumElems = RegisterWidth / FieldWidth;

  MVT ToVT = MVT::getVectorVT(MVT::getIntegerVT(FieldWidth * 2), NumElems / 2);
  return ToVT;
}

//Root function for general policy lowering. Register the OpKind in resetOperations,
//then the policy will be executed here.
static SDValue lowerWithOpAction(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  OpKind kind = std::make_pair((ISD::NodeType)Op.getOpcode(), VT);
  SDNodeTreeBuilder b(Op, &DAG);
  unsigned RegisterWidth = VT.getSizeInBits();
  unsigned FieldWidth = VT.getScalarSizeInBits();
  SDValue Op0 = Op.getOperand(0);
  SDValue Op1 = Op.getOperand(1);

  switch (OpKindActions[kind]) {
  default: llvm_unreachable("Unknown OpAction to lower parabix op");
  case InPlacePromote:
    MVT DoubleVT = PromoteTypeDouble(VT);
    SDValue Himask = b.HiMask(RegisterWidth, FieldWidth * 2);
    SDValue Lowmask = b.NOT(Himask);
    SDValue HiBits, LowBits;

    Op0 = getFullRegister(Op0, DAG);
    Op1 = getFullRegister(Op1, DAG);

    if (Op.getOpcode() == ISD::MUL) {
      //MUL is a little different, needs to shift right high bits before calc
      HiBits = b.SHL(FieldWidth, b.DoOp(DoubleVT,
                                        b.SRL(FieldWidth, b.BITCAST(Op0, DoubleVT)),
                                        b.SRL(FieldWidth, b.BITCAST(Op1, DoubleVT))));
    } else if (Op.getOpcode() == ISD::SHL) {
      // shift left
      HiBits = b.DoOp(DoubleVT, b.AND(Op0, Himask),
                      b.SRL(FieldWidth, b.BITCAST(Op1, DoubleVT)));
    } else if (Op.getOpcode() == ISD::SRL || Op.getOpcode() == ISD::SRA) {
      // shift right, logic or arithmetic are the same
      HiBits = b.DoOp(DoubleVT,
                      Op0, b.SRL(FieldWidth, b.BITCAST(Op1, DoubleVT)));
    } else {
      HiBits = b.DoOp(DoubleVT, b.AND(Op0, Himask), b.AND(Op1, Himask));
    }

    if (Op.getOpcode() == ISD::SETCC) {
      //SETCC needs to shift the lowbits left, to properly set the sign bit.
      LowBits = b.DoOp(DoubleVT,
                       b.SHL(FieldWidth, b.BITCAST(Op0, DoubleVT)),
                       b.SHL(FieldWidth, b.BITCAST(Op1, DoubleVT)));
    } else if (Op.getOpcode() == ISD::SHL) {
      //shift left
      LowBits = b.DoOp(DoubleVT, Op0, b.AND(Op1, Lowmask));
    } else if (Op.getOpcode() == ISD::SRL) {
      //shift right logic
      LowBits = b.DoOp(DoubleVT, b.AND(Op0, Lowmask), b.AND(Op1, Lowmask));
    } else if (Op.getOpcode() == ISD::SRA) {
      //shift right arithmetic. Need to shift left low half to high half to set sign bit
      LowBits = b.SRL(FieldWidth,
                      b.DoOp(DoubleVT, b.SHL(FieldWidth, b.BITCAST(Op0, DoubleVT)),
                             b.AND(Op1, Lowmask)));

    } else {
      LowBits = b.DoOp(DoubleVT, Op0, Op1);
    }

    SDValue R = b.IFH1(Himask, HiBits, LowBits);
    return b.BITCAST(R, VT);
  }

  llvm_unreachable("Reach the end of lowerWithOpAction");
  return SDValue();
}

//Bitcast this vector to a full length integer and then do one op
//Lookup op in CAOops map
static SDValue lowerWithCastAndOp(SDValue Op, SelectionDAG &DAG) {
  SDLoc dl(Op);
  MVT VT = Op.getSimpleValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);
  CastAndOpKind kind = std::make_pair((ISD::NodeType)Op.getOpcode(), VT);

  assert(CAOops.find(kind) != CAOops.end() && "Undefined cast and op kind");

  MVT castType = getFullRegisterType(VT);
  SDValue transA = DAG.getNode(ISD::BITCAST, dl, castType, A);
  SDValue transB = DAG.getNode(ISD::BITCAST, dl, castType, B);
  SDValue res = DAG.getNode(CAOops[kind], dl, castType, transA, transB);

  return DAG.getNode(ISD::BITCAST, dl, VT, res);
}

//Don't do the lookup, use NewOp on casted operands.
//If swap is set true, will swap operands
static SDValue lowerWithCastAndOp(SDValue Op, SelectionDAG &DAG,
                                  ISD::NodeType NewOp, bool Swap=false) {
  SDLoc dl(Op);
  MVT VT = Op.getSimpleValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);

  MVT castType = getFullRegisterType(VT);
  SDValue transA = DAG.getNode(ISD::BITCAST, dl, castType, A);
  SDValue transB = DAG.getNode(ISD::BITCAST, dl, castType, B);
  SDValue res;
  if (!Swap)
    res = DAG.getNode(NewOp, dl, castType, transA, transB);
  else
    res = DAG.getNode(NewOp, dl, castType, transB, transA);

  return DAG.getNode(ISD::BITCAST, dl, VT, res);
}

static SDValue PXLowerShift(SDValue Op, SelectionDAG &DAG) {
  assert((Op.getOpcode() == ISD::SHL || Op.getOpcode() == ISD::SRA ||
          Op.getOpcode() == ISD::SRL) && "Only lower shift ops here");

  SDLoc dl(Op);
  MVT VT = Op.getSimpleValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);
  SDValue res;
  SDNodeTreeBuilder b(Op, &DAG);

  MVT VectorEleType = VT.getVectorElementType();

  if (VectorEleType == MVT::i1 && Op.getOpcode() != ISD::SRA) {
    //SRL or SHL
    res = b.AND(getFullRegister(A, DAG), b.NOT(getFullRegister(B, DAG)));
  }
  else if (VectorEleType == MVT::i1 && Op.getOpcode() == ISD::SRA) {
    return A;
  }
  else if (VT == MVT::v64i2) {
    if (Op.getOpcode() == ISD::SHL)
      return GENLowerSHL(Op, DAG);
    else if (Op.getOpcode() == ISD::SRL)
      return GENLowerLSHR(Op, DAG);
    else
      return GENLowerASHR(Op, DAG);
  } else
    llvm_unreachable("lowering undefined parabix shift ops");

  return DAG.getNode(ISD::BITCAST, dl, VT, res);
}

static SDValue PXLowerADD(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  SDNodeTreeBuilder b(Op, &DAG);
  SDValue Op0 = Op.getOperand(0);
  SDValue Op1 = Op.getOperand(1);

  if (VT == MVT::v64i2) {
    return GENLowerADD(Op, DAG);
  }
  else if (VT == MVT::v32i4) {
    // Use mask = 0x8888... to mask out high bits and then we can do the i4 add
    // with only one paddb.
    std::string mask = "";
    for (unsigned int i = 0; i < 16; i++) mask += "1000";
    SDValue Mask = b.Constant(mask, MVT::v2i64);

    MVT DoubleVT = MVT::v16i8;

    SDValue Ah = b.AND(Mask, b.BITCAST(Op0, MVT::v2i64));
    SDValue Bh = b.AND(Mask, b.BITCAST(Op1, MVT::v2i64));
    SDValue R = b.DoOp(DoubleVT,
                       b.AND(b.BITCAST(Op0, MVT::v2i64), b.NOT(Ah)),
                       b.AND(b.BITCAST(Op1, MVT::v2i64), b.NOT(Bh)));
    R = b.XOR(R, b.BITCAST(b.XOR(Ah, Bh), DoubleVT));

    return b.BITCAST(R, VT);
  }

  llvm_unreachable("lowering add for unsupported type");
  return SDValue();
}

static SDValue PXLowerSUB(SDValue Op, SelectionDAG &DAG) {
  if (Op.getSimpleValueType() == MVT::v64i2) {
    return GENLowerSUB(Op, DAG);
  }

  llvm_unreachable("lowering sub for unsupported type");
  return SDValue();
}

static SDValue PXLowerMUL(SDValue Op, SelectionDAG &DAG) {
  if (Op.getSimpleValueType() == MVT::v64i2) {
    return GENLowerMUL(Op, DAG);
  }

  llvm_unreachable("only lowering parabix MUL");
  return SDValue();
}

static SDValue getTruncateOrZeroExtend(SDValue V, SelectionDAG &DAG, MVT ToVT)
{
  SDNodeTreeBuilder b(V, &DAG);
  MVT VT = V.getSimpleValueType();
  if (VT.bitsLT(ToVT))
    return b.ZERO_EXTEND(V, ToVT);
  else if (VT.bitsGT(ToVT))
    return b.TRUNCATE(V, ToVT);

  return V;
}

static SDValue PXLowerINSERT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG, const X86Subtarget* Subtarget) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  SDLoc dl(Op);
  SDValue N0 = Op.getOperand(0); // vector <val>
  SDValue N1 = Op.getOperand(1); // elt
  SDValue N2 = Op.getOperand(2); // idx

  int RegisterWidth = VT.getSizeInBits();
  int NumElts = VT.getVectorNumElements();
  int FieldWidth = RegisterWidth / NumElts;

  if (VT == MVT::v32i1 || VT == MVT::v64i1) {
    //Cast VT into full register and do bit manipulation.
    SDValue TransN0 = getFullRegister(N0, DAG);
    SDValue Res;

    if (isa<ConstantSDNode>(N1)) {
      if (cast<ConstantSDNode>(N1)->isNullValue()) {
        //insert zero
        SDValue Mask = DAG.getNode(ISD::SHL, dl, FullVT, DAG.getConstant(1, FullVT), N2);
        SDValue NegMask = DAG.getNOT(dl, Mask, FullVT);
        Res = DAG.getNode(ISD::AND, dl, FullVT, NegMask, TransN0);
      } else {
        //insert one
        SDValue Mask = DAG.getNode(ISD::SHL, dl, FullVT, DAG.getConstant(1, FullVT), N2);
        Res = DAG.getNode(ISD::OR, dl, FullVT, Mask, TransN0);
      }
    } else {
      // Elt is not a constant node
      // Mask = NOT(SHL(ZEXT(NOT(elt, i1), i32), idx))
      // return AND(Vector, Mask)
      // NOT is sensitive of bit width
      SDValue NotV = DAG.getNode(ISD::AND, dl, MVT::i8, DAG.getConstant(1, MVT::i8),
                                 DAG.getNOT(dl, N1, MVT::i8));
      SDValue Zext = DAG.getNode(ISD::ZERO_EXTEND, dl, FullVT, NotV);
      SDValue Mask = DAG.getNOT(dl, DAG.getNode(ISD::SHL, dl, FullVT, Zext, N2),
                                FullVT);
      Res = DAG.getNode(ISD::AND, dl, FullVT, Mask, TransN0);
    }

    // Cast back
    return DAG.getNode(ISD::BITCAST, dl, VT, Res);
  }
  else {
    //General strategy here
    //extract an i16 from the vector and insert N1 into proper location.
    //then, insert the modified i16 back
    //SSE2 don't have extract i32, only have extract i16
    assert(VT.getVectorElementType().bitsLE(MVT::i8) &&
           "general INSERT_VECTOR_ELT only works with FieldWidth <= 8");

    N2 = getTruncateOrZeroExtend(N2, DAG, MVT::i16);

    int I16VecNumElts = RegisterWidth / 16;
    MVT I16VecType = MVT::getVectorVT(MVT::i16, I16VecNumElts);
    int lowbitsMask = (1 << FieldWidth) - 1;

    SDValue IdxVec = b.UDIV(N2, b.Constant(NumElts / I16VecNumElts, MVT::i16));
    SDValue IdxInside = b.UREM(N2, b.Constant(NumElts / I16VecNumElts, MVT::i16));

    SDValue TransVal = b.BITCAST(N0, I16VecType);
    SDValue ExtVal = b.EXTRACT_VECTOR_ELT(TransVal, IdxVec);

    SDValue NewElt = b.SHL(b.ZERO_EXTEND(b.AND(N1, b.Constant(lowbitsMask, MVT::i8)),
                                         MVT::i16),
                           b.MUL(IdxInside, b.Constant(FieldWidth, MVT::i16)));
    SDValue Mask   = b.SHL(b.Constant(lowbitsMask, MVT::i16),
                           b.MUL(IdxInside, b.Constant(FieldWidth, MVT::i16)));

    ExtVal = b.OR(b.AND(ExtVal, b.NOT(Mask)), NewElt);

    // idx for insert_vector_elt should match the subtarget
    MVT IdxValueType = MVT::i32;
    if (Subtarget->is64Bit())
        IdxValueType = MVT::i64;

    return b.BITCAST(b.INSERT_VECTOR_ELT(TransVal, ExtVal, b.ZERO_EXTEND(IdxVec, IdxValueType)), VT);
  }

  llvm_unreachable("lowering insert_vector_elt for unsupported type");
  return SDValue();
}

static SDValue PXLowerEXTRACT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) {
  SDLoc dl(Op);
  SDValue Vec = Op.getOperand(0);
  MVT VecVT = Vec.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VecVT);
  SDValue Idx = Op.getOperand(1);
  SDNodeTreeBuilder b(Op, &DAG);

  int RegisterWidth = VecVT.getSizeInBits();
  int NumElts = VecVT.getVectorNumElements();
  int FieldWidth = RegisterWidth / NumElts;

  if (VecVT == MVT::v32i1 || VecVT == MVT::v64i1) {
    //TRUNC(AND(1, SRL(FULL_REG(VecVT), Idx)), i8)
    SDValue TransV = DAG.getNode(ISD::BITCAST, dl, FullVT, Vec);
    SDValue ShiftV = DAG.getNode(ISD::SRL, dl, FullVT, TransV, Idx);
    return DAG.getNode(ISD::TRUNCATE, dl, MVT::i8,
                       DAG.getNode(ISD::AND, dl, FullVT, ShiftV, DAG.getConstant(1, FullVT)));
  }
  else {
    //General strategy here, extract i16 from the vector and then do shifting
    //and truncate.
    assert(VecVT.getVectorElementType().bitsLE(MVT::i8) &&
           "general EXTRACT_VECTOR_ELT only works with FieldWidth <= 8");

    Idx = getTruncateOrZeroExtend(Idx, DAG, MVT::i16);

    int I16VecNumElts = RegisterWidth / 16;
    MVT I16VecType = MVT::getVectorVT(MVT::i16, I16VecNumElts);

    SDValue IdxVec = b.UDIV(Idx, b.Constant(NumElts / I16VecNumElts, MVT::i16));
    SDValue IdxInside = b.UREM(Idx, b.Constant(NumElts / I16VecNumElts, MVT::i16));

    SDValue TransVal = b.BITCAST(Vec, I16VecType);
    SDValue ExtVal = b.EXTRACT_VECTOR_ELT(TransVal, IdxVec);

    return b.TRUNCATE(b.AND(b.SRL(ExtVal, b.MUL(IdxInside, b.Constant(FieldWidth, MVT::i16))),
                            b.Constant( (1 << FieldWidth) - 1, MVT::i16)),
                      MVT::i8);
  }

  llvm_unreachable("lowering extract_vector_elt for unsupported type");
  return SDValue();
}

static SDValue PXLowerSCALAR_TO_VECTOR(SDValue Op, SelectionDAG &DAG) {
  SDLoc dl(Op);
  MVT VecVT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VecVT);
  SDValue Val = Op.getOperand(0);
  EVT EltVT = VecVT.getVectorElementType();
  SDNodeTreeBuilder b(Op, &DAG);

  assert(EltVT.isInteger() && Val.getValueType().bitsGE(EltVT) &&
         "incorrect scalar_to_vector parameters");

  //Lowering assumes i8 is the smallest legal integer, which is true on X86
  if (VecVT == MVT::v32i1 || VecVT == MVT::v64i1) {
    SDValue Trunc = DAG.getNode(ISD::AND, dl, MVT::i8, DAG.getConstant(1, MVT::i8),
                                DAG.getNode(ISD::TRUNCATE, dl, MVT::i8, Val));
    SDValue Ext = DAG.getNode(ISD::ANY_EXTEND, dl, FullVT, Trunc);
    return DAG.getNode(ISD::BITCAST, dl, VecVT, Ext);
  }
  else {
    assert(Val.getSimpleValueType().bitsLE(MVT::i8) &&
           "GetVectorFromScalarInteger only work with i2 or i4");

    MVT I32VecType = MVT::getVectorVT(MVT::i32, VecVT.getSizeInBits() / 32);
    int mask = (1 << EltVT.getSizeInBits()) - 1;

    SDValue R1 = b.ANY_EXTEND(b.AND(b.TRUNCATE(Val, MVT::i8),
                                    b.Constant(mask, MVT::i8)), MVT::i32);
    SDValue R2 = DAG.getNode(ISD::SCALAR_TO_VECTOR, dl, I32VecType, R1);
    return b.BITCAST(R2, VecVT);
  }

  llvm_unreachable("lowering unsupported scalar_to_vector");
  return SDValue();
}

//get zero vector for parabix
static SDValue getPXZeroVector(EVT VT, SDNodeTreeBuilder b) {
  SDValue Vec;
  if (VT.isSimple() && VT.getSimpleVT().is32BitVector()) {
    // Careful here, don't use TargetConstant until you are sure.
    Vec = b.Constant(0, MVT::i32);
  } else if (VT.isSimple() && VT.getSimpleVT().is64BitVector()) {
    Vec = b.Constant(0, MVT::i64);
  } else if (VT.isSimple() && VT.getSimpleVT().is128BitVector()) {
    Vec = b.ConstantVector(MVT::v4i32, 0);
  } else if (VT.isSimple() && VT.getSimpleVT().is256BitVector()) {
    Vec = b.ConstantVector(MVT::v8i32, 0);
  } else
    llvm_unreachable("Unexpected vector type");

  return b.BITCAST(Vec, VT.getSimpleVT());
}

static SDValue getPXOnesVector(EVT VT, SDNodeTreeBuilder b) {
  SDValue Vec;
  if (VT.isSimple() && VT.getSimpleVT().is32BitVector()) {
    // Careful here, don't use TargetConstant until you are sure.
    Vec = b.Constant(-1, MVT::i32);
  } else if (VT.isSimple() && VT.getSimpleVT().is64BitVector()) {
    Vec = b.Constant(-1, MVT::i64);
  } else if (VT.isSimple() && VT.getSimpleVT().is128BitVector()) {
    Vec = b.ConstantVector(MVT::v4i32, -1);
  } else if (VT.isSimple() && VT.getSimpleVT().is256BitVector()) {
    Vec = b.ConstantVector(MVT::v8i32, -1);
  } else
    llvm_unreachable("Unexpected vector type");

  return b.BITCAST(Vec, VT.getSimpleVT());
}

SDValue
X86TargetLowering::PXLowerBUILD_VECTOR(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);

  MVT VT = Op.getSimpleValueType();
  unsigned NumElems = Op.getNumOperands();
  SDNodeTreeBuilder b(Op, &DAG);

  // Vectors containing all zeros can be matched by pxor and xorps later
  if (ISD::isBuildVectorAllZeros(Op.getNode())) {
    // Canonicalize this to <4 x i32> to 1) ensure the zero vectors are CSE'd
    // and 2) ensure that i64 scalars are eliminated on x86-32 hosts.
    if (VT == MVT::v4i32 || VT == MVT::v8i32 || VT == MVT::v16i32)
      return Op;

    return getPXZeroVector(VT, b);
  }

  if (ISD::isBuildVectorAllOnes(Op.getNode())) {
    return getPXOnesVector(VT, b);
  }

  if (VT == MVT::v32i1 || VT == MVT::v64i1 || VT == MVT::v128i1) {
    //Brutely insert element
    //TODO: improve efficiency of v128i1
    MVT FullVT = getFullRegisterType(VT);
    SDValue Base = DAG.getNode(ISD::BITCAST, dl, VT,
                               DAG.getConstant(0, FullVT));
    for (unsigned i = 0; i < NumElems; ++i) {
      SDValue Elt = Op.getOperand(i);
      if (Elt.getOpcode() == ISD::UNDEF)
        continue;
      Base = DAG.getNode(ISD::INSERT_VECTOR_ELT, dl, VT, Base, Elt,
                         DAG.getConstant(i, MVT::i32));
    }

    return Base;
  }

  if (VT == MVT::v64i2) {
    //Rearrange index and do 4 shifts and or
    SmallVector<SmallVector<SDValue, 16>, 4> RearrangedVectors;
    SmallVector<SDValue, 16> RV;
    for (unsigned vi = 0; vi < 4; vi++) {
      RV.clear();
      for (unsigned i = vi; i < NumElems; i += 4) {
        RV.push_back(Op.getOperand(i));
      }

      RearrangedVectors.push_back(RV);
    }

    //i2 is not legal on X86, so the 64 operands are all i8
    SDValue V0 = b.BUILD_VECTOR(MVT::v16i8, RearrangedVectors[0]);
    SDValue V1 = b.BUILD_VECTOR(MVT::v16i8, RearrangedVectors[1]);
    SDValue V2 = b.BUILD_VECTOR(MVT::v16i8, RearrangedVectors[2]);
    SDValue V3 = b.BUILD_VECTOR(MVT::v16i8, RearrangedVectors[3]);

    return b.BITCAST(b.OR(b.OR(b.OR(V0, b.SHL<2>(V1)), b.SHL<4>(V2)), b.SHL<6>(V3)),
                     MVT::v64i2);
  }

  if (VT == MVT::v32i4) {
    //Rearrange index and do 2 shifts and or
    //We have 32 x i8 as build_vector oprand, we build 2 v16i8, V0 and V1, then
    //we can return the result as V0 | (V1 << 4), where
    //V0 = build_vector(Op0, Op2, Op4, ... , Op30)
    //V1 = build_vector(Op1, Op3, Op5, ... , Op31)
    SmallVector<SDValue, 2> V;
    SmallVector<SDValue, 16> RowV;
    for (unsigned vi = 0; vi < 2; vi ++) {
      RowV.clear();
      for (unsigned i = vi; i < NumElems; i += 2) {
        RowV.push_back(Op.getOperand(i));
      }

      V.push_back(b.BUILD_VECTOR(MVT::v16i8, RowV));
    }

    return b.BITCAST(b.OR(V[0], b.SHL<4>(V[1])), VT);
  }

  llvm_unreachable("lowering build_vector for unsupported type");
  return SDValue();
}

static SDValue PXLowerSETCC(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDLoc dl(Op);
  SDValue Op0 = Op.getOperand(0);
  SDValue Op1 = Op.getOperand(1);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(2))->get();
  SDValue NEVec, TransA, TransB, Res, NotOp1, NotOp0;
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v32i1 || VT == MVT::v64i1 || VT == MVT::v128i1) {
    switch (CC) {
    default: llvm_unreachable("Can't lower this parabix SETCC");
    case ISD::SETNE:    return lowerWithCastAndOp(Op, DAG, ISD::XOR);
    case ISD::SETEQ:
      NEVec = lowerWithCastAndOp(Op, DAG, ISD::XOR);
      return DAG.getNOT(dl, NEVec, VT);
    case ISD::SETLT:
    case ISD::SETUGT:
      NotOp1 = DAG.getNOT(dl, Op1, VT);
      TransA = DAG.getNode(ISD::BITCAST, dl, FullVT, Op0);
      TransB = DAG.getNode(ISD::BITCAST, dl, FullVT, NotOp1);
      Res = DAG.getNode(ISD::AND, dl, FullVT, TransA, TransB);
      return DAG.getNode(ISD::BITCAST, dl, VT, Res);
    case ISD::SETGT:
    case ISD::SETULT:
      NotOp0 = DAG.getNOT(dl, getFullRegister(Op0, DAG), FullVT);
      Res = DAG.getNode(ISD::AND, dl, FullVT, NotOp0,
                                getFullRegister(Op1, DAG));
      return DAG.getNode(ISD::BITCAST, dl, VT, Res);
    case ISD::SETLE:
    case ISD::SETUGE:
      Res = DAG.getNode(ISD::SETCC, dl, VT, Op0, Op1, DAG.getCondCode(ISD::SETGT));
      return DAG.getNOT(dl, Res, VT);
    case ISD::SETGE:
    case ISD::SETULE:
      Res = DAG.getNode(ISD::SETCC, dl, VT, Op0, Op1, DAG.getCondCode(ISD::SETLT));
      return DAG.getNOT(dl, Res, VT);
    }
  }
  else if (VT == MVT::v64i2) {
    switch (CC) {
    default: llvm_unreachable("Can't lower this parabix SETCC");
    case ISD::SETEQ:  return GENLowerICMP_EQ(Op, DAG);
    case ISD::SETLT:  return GENLowerICMP_SLT(Op, DAG);
    case ISD::SETGT:  return GENLowerICMP_SGT(Op, DAG);
    case ISD::SETULT: return GENLowerICMP_ULT(Op, DAG);
    case ISD::SETUGT: return GENLowerICMP_UGT(Op, DAG);
    case ISD::SETNE:
      Res = GENLowerICMP_EQ(Op, DAG);
      return b.NOT(Res);
    case ISD::SETGE:
      Res = GENLowerICMP_SLT(Op, DAG);
      return b.NOT(Res);
    case ISD::SETLE:
      Res = GENLowerICMP_SGT(Op, DAG);
      return b.NOT(Res);
    case ISD::SETUGE:
      Res = GENLowerICMP_ULT(Op, DAG);
      return b.NOT(Res);
    case ISD::SETULE:
      Res = GENLowerICMP_UGT(Op, DAG);
      return b.NOT(Res);
    }
  }

  llvm_unreachable("only lowering parabix SETCC");
  return SDValue();
}

///Entrance for parabix lowering.
SDValue X86TargetLowering::LowerParabixOperation(SDValue Op, SelectionDAG &DAG) const {
  //NEED: setOperationAction in target specific lowering (X86ISelLowering.cpp)
  DEBUG(dbgs() << "Parabix Lowering:" << "\n"; Op.dump());

  //Only resetOperations for the first time.
  static bool FirstTimeThrough = true;
  if (FirstTimeThrough) {
    //dbgs() << "Parabix Lowering:" << "\n"; Op.dump();
    resetOperations();
    FirstTimeThrough = false;
  }

  MVT VT = Op.getSimpleValueType();
  //Check if we have registered CastAndOp action
  CastAndOpKind kind = std::make_pair((ISD::NodeType)Op.getOpcode(), VT);
  if (CAOops.find(kind) != CAOops.end())
    return lowerWithCastAndOp(Op, DAG);
  //Check general policy
  if (OpKindActions.find((OpKind) kind) != OpKindActions.end())
    return lowerWithOpAction(Op, DAG);

  switch (Op.getOpcode()) {
  default: llvm_unreachable("[ROOT SWITCH] Should not custom lower this parabix op!");
  case ISD::ADD:                return PXLowerADD(Op, DAG);
  case ISD::SUB:                return PXLowerSUB(Op, DAG);
  case ISD::MUL:                return PXLowerMUL(Op, DAG);
  case ISD::BUILD_VECTOR:       return PXLowerBUILD_VECTOR(Op, DAG);
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:                return PXLowerShift(Op, DAG);
  case ISD::INSERT_VECTOR_ELT:  return PXLowerINSERT_VECTOR_ELT(Op, DAG, Subtarget);
  case ISD::EXTRACT_VECTOR_ELT: return PXLowerEXTRACT_VECTOR_ELT(Op, DAG);
  case ISD::SCALAR_TO_VECTOR:   return PXLowerSCALAR_TO_VECTOR(Op, DAG);
  case ISD::SETCC:              return PXLowerSETCC(Op, DAG);
  }
}

static SDValue PXPerformVSELECTCombine(SDNode *N, SelectionDAG &DAG,
                                    TargetLowering::DAGCombinerInfo &DCI,
                                    const X86Subtarget *Subtarget) {
  MVT VT = N->getSimpleValueType(0);
  SDValue Mask = N->getOperand(0);
  MVT MaskTy = Mask.getSimpleValueType();
  SDLoc dl(N);

  SDNodeTreeBuilder b(&DAG, dl);

  if (DCI.isBeforeLegalize()) {
    //v128i1 (select v128i1, v128i1, v128i1) can be combined into logical ops
    if (MaskTy == MVT::v128i1 && VT == MVT::v128i1) {
      DEBUG(dbgs() << "Combining select v128i1 \n");
      return b.IFH1(Mask, N->getOperand(0), N->getOperand(1));
   }
  }

  //v32i8 (select v32i1, v32i8, v32i8) don't have proper lowering on AVX2, so
  //we convert the mask to v32i8
  if (MaskTy == MVT::v32i1 && VT == MVT::v32i8 &&
      (Subtarget->hasAVX2() || Subtarget->hasAVX())) {
    Mask = DAG.getNode(ISD::SIGN_EXTEND, dl, VT, Mask);
    DCI.AddToWorklist(Mask.getNode());
    return DAG.getNode(N->getOpcode(), dl, VT, Mask, N->getOperand(1), N->getOperand(2));
  }

  return SDValue();
}

/// isUndefOrEqual - Val is either less than zero (undef) or equal to the
/// specified value.
static bool isUndefOrEqual(int Val, int CmpVal) {
  return (Val < 0 || Val == CmpVal);
}

//Check whether the shuffle node is same as IDISA simd<16>::packl
//Convert packed 16-bit integers from a and b to packed 8-bit integers
//Collect all the low parts of vectors a and b
static bool isPackLowMask(ShuffleVectorSDNode *SVOp) {
  EVT VT = SVOp->getValueType(0);
  unsigned NumElems = VT.getVectorNumElements();

  //v16i8 (shufflevector v16i8, v16i8, <0, 2, 4, 6, 8, ..., 30>)
  for (unsigned i = 0; i < NumElems; i++) {
    if (!isUndefOrEqual(SVOp->getMaskElt(i), i * 2))
      return false;
  }

  return true;
}

//Check whether the shuffle node is same as IDISA simd<16>::packh
//Collect all the high parts of vectors
static bool isPackHighMask(ShuffleVectorSDNode *SVOp) {
  EVT VT = SVOp->getValueType(0);
  unsigned NumElems = VT.getVectorNumElements();

  //v16i8 (shufflevector v16i8, v16i8, <1, 3, 5, 7, ..., 31>)
  for (unsigned i = 0; i < NumElems; i++) {
    if (!isUndefOrEqual(SVOp->getMaskElt(i), i * 2 + 1))
      return false;
  }

  return true;
}

static SDValue PXPerformVECTOR_SHUFFLECombine(SDNode *N, SelectionDAG &DAG,
                                    TargetLowering::DAGCombinerInfo &DCI,
                                    const X86Subtarget *Subtarget) {
  MVT VT = N->getSimpleValueType(0);
  SDLoc dl(N);
  ShuffleVectorSDNode *SVOp = cast<ShuffleVectorSDNode>(N);
  SDValue V1 = SVOp->getOperand(0);
  SDValue V2 = SVOp->getOperand(1);
  SDNodeTreeBuilder b(&DAG, dl);

  //v16i8 (vector_shuffle v16i8, v16i8, v16i32) can be combined into
  //X86ISD::PACKUS
  //simd<16>::packl
  if (Subtarget->hasSSE2() && VT == MVT::v16i8 && isPackLowMask(SVOp) &&
      V1.getOpcode() != ISD::UNDEF && V2.getOpcode() != ISD::UNDEF) {
    DEBUG(dbgs() << "Parabix combine: \n"; N->dumpr());

    //00000000111111110000000011111111
    SDValue LowMaskInteger = b.Constant(16711935, MVT::i32);
    SDValue VPool[] = {LowMaskInteger, LowMaskInteger, LowMaskInteger, LowMaskInteger};
    SDValue LowMask16 = b.BITCAST(b.BUILD_VECTOR(MVT::v4i32, VPool), MVT::v8i16);

    SDValue NewV1 = b.AND(LowMask16, b.BITCAST(V1, MVT::v8i16));
    SDValue NewV2 = b.AND(LowMask16, b.BITCAST(V2, MVT::v8i16));

    DCI.AddToWorklist(LowMask16.getNode());
    DCI.AddToWorklist(NewV1.getNode());
    DCI.AddToWorklist(NewV2.getNode());

    return DAG.getNode(X86ISD::PACKUS, dl, MVT::v16i8, NewV1, NewV2);
  }

  //X86ISD::PACKUS cont.
  //For simd<16>::packh
  if (Subtarget->hasSSE2() && VT == MVT::v16i8 && isPackHighMask(SVOp) &&
      V1.getOpcode() != ISD::UNDEF && V2.getOpcode() != ISD::UNDEF) {
    DEBUG(dbgs() << "Parabix combine: \n"; N->dumpr());

    SDValue Cst = b.Constant(8, MVT::i16);
    SDValue VPool[] = {Cst, Cst, Cst, Cst, Cst, Cst, Cst, Cst};
    SDValue Shift = b.BUILD_VECTOR(MVT::v8i16, VPool);

    SDValue NewV1 = b.SRL(b.BITCAST(V1, MVT::v8i16), Shift);
    SDValue NewV2 = b.SRL(b.BITCAST(V2, MVT::v8i16), Shift);

    DCI.AddToWorklist(Shift.getNode());
    DCI.AddToWorklist(NewV1.getNode());
    DCI.AddToWorklist(NewV2.getNode());

    return DAG.getNode(X86ISD::PACKUS, dl, MVT::v16i8, NewV1, NewV2);
  }

  //PEXT for simd<2, 4, 8>::packl or packh
  //the Mask is the only thing different
  if (Subtarget->hasBMI2() && Subtarget->is64Bit() &&
      (isPackLowMask(SVOp) || isPackHighMask(SVOp)) &&
      (VT == MVT::v32i4 || VT == MVT::v64i2 || VT == MVT::v128i1)) {
    DEBUG(dbgs() << "Parabix combine: \n"; N->dumpr());

    std::string Mask;
    if (isPackLowMask(SVOp)) {
      switch (VT.SimpleTy) {
      default: llvm_unreachable("unsupported type");
      case MVT::v32i4:
        //simd<8>::packl
        Mask = "0000111100001111000011110000111100001111000011110000111100001111";
        break;
      case MVT::v64i2:
        //simd<4>::packl
        Mask = "0011001100110011001100110011001100110011001100110011001100110011";
        break;
      case MVT::v128i1:
        //simd<2>::packl
        Mask = "0101010101010101010101010101010101010101010101010101010101010101";
        break;
      }
    } else if (isPackHighMask(SVOp)) {
      switch (VT.SimpleTy) {
      default: llvm_unreachable("unsupported type");
      case MVT::v32i4:
        //simd<8>::packl
        Mask = "1111000011110000111100001111000011110000111100001111000011110000";
        break;
      case MVT::v64i2:
        //simd<4>::packl
        Mask = "1100110011001100110011001100110011001100110011001100110011001100";
        break;
      case MVT::v128i1:
        //simd<2>::packl
        Mask = "1010101010101010101010101010101010101010101010101010101010101010";
        break;
      }
    }

    APInt MaskInt(64, Mask, 2);
    SDValue MaskNode = DAG.getConstant(MaskInt, MVT::i64);

    SDValue A = b.BITCAST(V1, MVT::v2i64);
    SDValue A0 = b.EXTRACT_VECTOR_ELT(A, b.Constant(0));
    SDValue A1 = b.EXTRACT_VECTOR_ELT(A, b.Constant(1));

    SDValue B = b.BITCAST(V2, MVT::v2i64);
    SDValue B0 = b.EXTRACT_VECTOR_ELT(B, b.Constant(0));
    SDValue B1 = b.EXTRACT_VECTOR_ELT(B, b.Constant(1));

    //There are 2 ways of implementation at this point. OR/SHL is the first one.
    //It will generate 3 more ops for each packh/l, but have better performance
    //for whole transposition.
    SDValue P0 = b.OR(b.PEXT64(A0, MaskNode),
                      b.SHL(b.PEXT64(A1, MaskNode), b.Constant(32, MVT::i64)));
    SDValue P1 = b.OR(b.PEXT64(B0, MaskNode),
                      b.SHL(b.PEXT64(B1, MaskNode), b.Constant(32, MVT::i64)));
    SDValue P[] = {P0, P1};
    return b.BITCAST(b.BUILD_VECTOR(MVT::v2i64, P), VT);

    //////////////////////////////////////
    //Below is the second implementation. Less instructions will be generated,
    //but hurt the whole performance.

    //SDValue P0 = b.TRUNCATE(b.PEXT64(A0, MaskNode), MVT::i32);
    //SDValue P1 = b.TRUNCATE(b.PEXT64(A1, MaskNode), MVT::i32);
    //SDValue P2 = b.TRUNCATE(b.PEXT64(B0, MaskNode), MVT::i32);
    //SDValue P3 = b.TRUNCATE(b.PEXT64(B1, MaskNode), MVT::i32);

    //SDValue P[] = {P0, P1, P2, P3};
    //return b.BITCAST(b.BUILD_VECTOR(MVT::v4i32, P), VT);
  }

  return SDValue();
}

static bool isImmediateShiftingMask(SDValue Mask, int &imm) {
  if (isa<ConstantSDNode>(Mask)) {
    // immediate shift for long integer. e.g. i128, i256
    imm = (int) (cast<ConstantSDNode>(Mask)->getZExtValue());
    return true;
  }

  if (Mask.getOpcode() != ISD::BUILD_VECTOR)
    return false;

  bool FirstImmediate = true;
  uint64_t ImmNumber;

  for (unsigned i = 0, e = Mask.getNumOperands(); i != e; ++i) {
    SDValue Op = Mask.getOperand(i);
    if (Op.getOpcode() == ISD::UNDEF)
      continue;
    if (!isa<ConstantSDNode>(Op))
      return false;

    if (FirstImmediate) {
      FirstImmediate = false;
      ImmNumber = cast<ConstantSDNode>(Op)->getZExtValue();
    }
    else if (cast<ConstantSDNode>(Op)->getZExtValue() != ImmNumber) {
      return false;
    }
  }

  imm = (int) ImmNumber;
  return true;
}

static SDValue PXPerformShiftCombine(SDNode *N, SelectionDAG &DAG,
                                     TargetLowering::DAGCombinerInfo &DCI,
                                     const X86Subtarget *Subtarget) {
  MVT VT = N->getSimpleValueType(0);
  SDLoc dl(N);
  SDValue V1 = N->getOperand(0);
  SDValue V2 = N->getOperand(1);
  SDNodeTreeBuilder b(&DAG, dl);

  assert((N->getOpcode() == ISD::SHL || N->getOpcode() == ISD::SRL ||
          N->getOpcode() == ISD::SRA) && "Only lowering shift");

  int imm;

  //Optimize immediate shiftings.
  if (Subtarget->hasSSE2() && VT == MVT::v32i4 && isImmediateShiftingMask(V2, imm)) {

    MVT I32VecType = MVT::v4i32;

    if (N->getOpcode() == ISD::SHL) {
      DEBUG(dbgs() << "Parabix combining: "; N->dump());

      SDValue R = b.AND(b.SHL(imm, b.BITCAST(V1, I32VecType)),
                        b.BITCAST(b.ConstantVector(MVT::v32i4, (15 << imm) & 15), I32VecType));
      return b.BITCAST(R, VT);
    }
    else if (N->getOpcode() == ISD::SRL) {
      DEBUG(dbgs() << "Parabix combining: "; N->dump());

      SDValue R = b.AND(b.SRL(imm, b.BITCAST(V1, I32VecType)),
                        b.BITCAST(b.ConstantVector(MVT::v32i4, 15 >> imm), I32VecType));
      return b.BITCAST(R, VT);
    }
  }

  // long integer shift for MVT::i128
  if (Subtarget->hasSSE2() && VT == MVT::i128 && isImmediateShiftingMask(V2, imm)) {
    if (imm % 8 == 0)
    {
      // for 128 bit SIMD register, there is pslldq and psrldq intrinsic
      if (N->getOpcode() == ISD::SHL)
      {
        SDValue B = b.ShiftLeftInByte(V1, imm / 8);
        return b.BITCAST(B, VT);
      }
      else if (N->getOpcode() == ISD::SRL)
      {
        return b.BITCAST(b.ShiftRightLogicInByte(V1, imm / 8), VT);
      }

      return SDValue();
    }

    if (N->getOpcode() == ISD::SHL) {
      DEBUG(dbgs() << "Parabix combining: "; N->dump());

      if (imm >= 64) {
        int mask[] = {2, 0};
        SDValue Shift64 = b.VECTOR_SHUFFLE(b.BITCAST(V1, MVT::v2i64),
                                           b.ConstantVector(MVT::v2i64, 0), mask);
        if (imm > 64) Shift64 = b.SHL(imm - 64, Shift64);
        return b.BITCAST(Shift64, VT);
      }
      else {
        //Double shift
        SDValue ShiftField = b.SHL(imm, b.BITCAST(V1, MVT::v2i64));
        SDValue ShiftInPart = b.SRL(64 - imm, b.BITCAST(V1, MVT::v2i64));
        int mask[] = {2, 0};
        ShiftInPart = b.VECTOR_SHUFFLE(ShiftInPart, b.ConstantVector(MVT::v2i64, 0), mask);
        SDValue R = b.OR(ShiftInPart, ShiftField);

        return b.BITCAST(R, VT);
      }
    }
    else if (N->getOpcode() == ISD::SRL) {
      DEBUG(dbgs() << "Parabix combining: "; N->dump());

      if (imm >= 64) {
        int mask[] = {3, 0};
        SDValue Shift64 = b.VECTOR_SHUFFLE(b.ConstantVector(MVT::v2i64, 0),
                                           b.BITCAST(V1, MVT::v2i64), mask);
        if (imm > 64) Shift64 = b.SRL(imm - 64, Shift64);
        return b.BITCAST(Shift64, VT);
      }
      else {
        //Double shift
        SDValue ShiftField = b.SRL(imm, b.BITCAST(V1, MVT::v2i64));
        SDValue ShiftInPart = b.SHL(64 - imm, b.BITCAST(V1, MVT::v2i64));
        int mask[] = {1, 2};
        ShiftInPart = b.VECTOR_SHUFFLE(ShiftInPart, b.ConstantVector(MVT::v2i64, 0), mask);
        SDValue R = b.OR(ShiftInPart, ShiftField);

        return b.BITCAST(R, VT);
      }
    }
  }

  return SDValue();
}

static SDValue LongStreamAddition(MVT VT, SDValue V1, SDValue V2, SDValue Vcarryin, SDNodeTreeBuilder &b) {
  //general logic for uadd.with.overflow.iXXX
  int RegisterWidth = VT.getSizeInBits();
  int f = RegisterWidth / 64;
  MVT VXi64Ty = MVT::getVectorVT(MVT::i64, f);
  MVT MaskTy = MVT::getIntegerVT(f);
  MVT MaskVecTy = MVT::getVectorVT(MVT::i1, f);

  SDValue X = b.BITCAST(V1, VXi64Ty);
  SDValue Y = b.BITCAST(V2, VXi64Ty);
  SDValue R = b.ADD(X, Y);

  SDValue Ones = getPXOnesVector(VXi64Ty, b);

  //x = hsimd<64>::signmask(X), x, y, r are all i32 type
  SDValue x, y, r, bubble;
  if (f == 2) {
    //i128, v2i1 to i2 seems to be problematic
    x = b.SignMask2x64(X);
    y = b.SignMask2x64(Y);
    r = b.SignMask2x64(R);
    bubble = b.SignMask2x64(b.SIGN_EXTEND(b.SETCC(R, Ones, ISD::SETEQ), VXi64Ty));
  }
  else if (f == 4) {
    //i256
    x = b.SignMask4x64(X);
    y = b.SignMask4x64(Y);
    r = b.SignMask4x64(R);
    bubble = b.SignMask4x64(b.SIGN_EXTEND(b.SETCC(R, Ones, ISD::SETEQ), VXi64Ty));
  }
  else
  {
    //i512, i1024, ..., i4096
    SDValue Zero = getPXZeroVector(VXi64Ty, b);
    x = b.ZERO_EXTEND(b.BITCAST(b.SETCC(X, Zero, ISD::SETLT), MaskTy), MVT::i32);
    y = b.ZERO_EXTEND(b.BITCAST(b.SETCC(Y, Zero, ISD::SETLT), MaskTy), MVT::i32);
    r = b.ZERO_EXTEND(b.BITCAST(b.SETCC(R, Zero, ISD::SETLT), MaskTy), MVT::i32);
    bubble = b.ZERO_EXTEND(b.BITCAST(b.SETCC(R, Ones, ISD::SETEQ), MaskTy), MVT::i32);
  }

  SDValue carry = b.OR(b.AND(x, y), b.AND(b.OR(x, y), b.NOT(r)));

  SDValue increments;
  if (Vcarryin.getNode()) {
    //carryin is not empty
    increments = b.MatchStar(b.OR(b.ZERO_EXTEND(Vcarryin, MVT::i32), b.SHL(carry, b.Constant(1, MVT::i32))),
                             bubble);
  } else {
    increments = b.MatchStar(b.SHL(carry, b.Constant(1, MVT::i32)), bubble);
  }

  SDValue carry_out = b.TRUNCATE(b.SRL(increments, b.Constant(f, MVT::i32)), MVT::i1);

  SDValue spread;

  if (f == 2) {
    // <2 x i1> increments to <2 x i64>
    SDValue spreadV2I16 = b.BITCAST(b.AND(b.MUL(increments, b.Constant(0x8001, MVT::i32)),
                                          b.Constant(0x10001, MVT::i32)), MVT::v2i16);
    spread = b.ZERO_EXTEND(spreadV2I16, VXi64Ty);
  }
  else if (f == 4) {
    SDValue increments64 = b.ZERO_EXTEND(increments, MVT::i64);
    SDValue spreadV4I16 = b.BITCAST(b.AND(b.MUL(increments64,
                                                b.Constant(0x0000200040008001ull, MVT::i64)),
                                          b.Constant(0x0001000100010001ull, MVT::i64)),
                                    MVT::v4i16);
    spread = b.ZERO_EXTEND(spreadV4I16, VXi64Ty);
  }
  else
  {
    // calc spread with zero extend: e.g. <4 x i1> to <4 x i64>
    spread = b.ZERO_EXTEND(b.BITCAST(b.TRUNCATE(increments, MaskTy), MaskVecTy), VXi64Ty);
  }
  SDValue sum = b.BITCAST(b.ADD(R, spread), VT);

  SDValue Pool[] = {sum, carry_out};
  return b.MergeValues(Pool);
}

//Perform combine for @llvm.uadd.with.overflow
static SDValue PXPerformUADDO(SDNode *N, SelectionDAG &DAG,
                                     TargetLowering::DAGCombinerInfo &DCI,
                                     const X86Subtarget *Subtarget) {
  MVT VT = N->getSimpleValueType(0);
  SDLoc dl(N);
  SDValue V1 = N->getOperand(0);
  SDValue V2 = N->getOperand(1);
  SDNodeTreeBuilder b(&DAG, dl);

  if (DCI.isBeforeLegalize() &&
      ((Subtarget->hasSSE2() && VT == MVT::i128) || (Subtarget->hasAVX() && VT == MVT::i256))) {
    DEBUG(dbgs() << "Parabix combining: "; N->dump());

    SDValue Ret = LongStreamAddition(VT, V1, V2, SDValue(), b);

    DEBUG(dbgs() << "Combined into: \n"; Ret.dumpr());
    return Ret;
  }

  return SDValue();
}

//Perform combine for @llvm.uadd.with.overflow.carryin
static SDValue PXPerformUADDE(SDNode *N, SelectionDAG &DAG,
                                     TargetLowering::DAGCombinerInfo &DCI,
                                     const X86Subtarget *Subtarget) {
  MVT VT = N->getSimpleValueType(0);
  SDLoc dl(N);
  SDValue V1 = N->getOperand(0);
  SDValue V2 = N->getOperand(1);
  SDValue Vcarryin = N->getOperand(2);
  SDNodeTreeBuilder b(&DAG, dl);

  if (DCI.isBeforeLegalize() &&
      ((Subtarget->hasSSE2() && VT == MVT::i128) || (Subtarget->hasAVX() && VT == MVT::i256))) {
    DEBUG(dbgs() << "Parabix combining: "; N->dump());

    SDValue Ret = LongStreamAddition(VT, V1, V2, Vcarryin, b);

    DEBUG(dbgs() << "Combined into: \n"; Ret.dumpr());
    return Ret;
  }

  return SDValue();
}

static SDValue PXPerformLogic(SDNode *N, SelectionDAG &DAG,
                                     TargetLowering::DAGCombinerInfo &DCI,
                                     const X86Subtarget *Subtarget) {
  MVT VT = N->getSimpleValueType(0);
  SDLoc dl(N);
  SDValue V1 = N->getOperand(0);
  SDValue V2 = N->getOperand(1);
  SDNodeTreeBuilder b(&DAG, dl);

  assert(((N->getOpcode() == ISD::OR) || (N->getOpcode() == ISD::AND) ||
          (N->getOpcode() == ISD::XOR)) &&
         "PXPerformLogic only works for AND / OR / XOR");

  /* Support for simd<128>::dslli<n>(A, B): long shift left for A, with shift-in bits in B
   * Pattern: OR(SHL(i128 A, n), SHR(i128 B, 128 - n))
   */
  if (N->getOpcode() == ISD::OR && VT == MVT::i128 &&
     ((V1.getOpcode() == ISD::SHL && V2.getOpcode() == ISD::SRL) ||
      (V1.getOpcode() == ISD::SRL && V2.getOpcode() == ISD::SHL))) {
    //possibly it's advance with carry
    //now check if the shl amount + shr amount is 128
    if (V1.getOpcode() != ISD::SHL) {
      std::swap(V1, V2);
    }

    int immLeft, immRight;
    if (isImmediateShiftingMask(V1.getOperand(1), immLeft) &&
        isImmediateShiftingMask(V2.getOperand(1), immRight))
    {
      if (immLeft + immRight == 128) {
        //finally, it is advance with carry
        SDValue A = b.BITCAST(V1.getOperand(0), MVT::v2i64);
        SDValue B = b.BITCAST(V2.getOperand(0), MVT::v2i64);

        int pool[] = {1, 2};
        SDValue C = b.VECTOR_SHUFFLE(B, A, pool);
        if (immLeft < 64) {
          SDValue D = b.SHL(immLeft, A);
          SDValue E = b.SRL(64 - immLeft, C);
          SDValue R = b.OR(D, E);
          return b.BITCAST(R, VT);
        }
        else
        {
          SDValue D = b.SHL(immLeft - 64, C);
          SDValue E = b.SRL(128 - immLeft, B);
          SDValue R = b.OR(D, E);
          return b.BITCAST(R, VT);
        }
      }
    }
  }

  /* Support for simd<256>::dslli<n>(A, B): long shift left for A, with shift-in bits in B
   * Pattern: OR(SHL(i256 A, n), SHR(i256 B, 256 - n))
   */
  if (N->getOpcode() == ISD::OR && VT == MVT::i256 &&
     ((V1.getOpcode() == ISD::SHL && V2.getOpcode() == ISD::SRL) ||
      (V1.getOpcode() == ISD::SRL && V2.getOpcode() == ISD::SHL))) {
    //possibly it's advance with carry
    //now check if the shl amount + shr amount is 256
    if (V1.getOpcode() != ISD::SHL) {
      std::swap(V1, V2);
    }

    int immLeft, immRight;
    if (isImmediateShiftingMask(V1.getOperand(1), immLeft) &&
        isImmediateShiftingMask(V2.getOperand(1), immRight))
    {
      if (immLeft + immRight == 256) {
        //finally, it is advance with carry
        SDValue A = b.BITCAST(V1.getOperand(0), MVT::v4i64);
        SDValue B = b.BITCAST(V2.getOperand(0), MVT::v4i64);

        if (immLeft < 64) {
          int pool[] = {3, 4, 5, 6};
          SDValue C = b.VECTOR_SHUFFLE(B, A, pool);

          SDValue D = b.SHL(immLeft, A);
          SDValue E = b.SRL(64 - immLeft, C);
          SDValue R = b.OR(D, E);
          return b.BITCAST(R, VT);
        }
        else if (immLeft >= 64 && immLeft < 128)
        {
          int apool[] = {3, 4, 5, 6};
          SDValue NA = b.VECTOR_SHUFFLE(B, A, apool);

          int pool[] = {2, 3, 4, 5};
          SDValue C = b.VECTOR_SHUFFLE(B, A, pool);

          SDValue D = b.SHL(immLeft - 64, NA);
          SDValue E = b.SRL(128 - immLeft, C);
          SDValue R = b.OR(D, E);
          return b.BITCAST(R, VT);
        }
        else if (immLeft >= 128 && immLeft < 192)
        {
          int apool[] = {2, 3, 4, 5};
          SDValue NA = b.VECTOR_SHUFFLE(B, A, apool);

          int pool[] = {1, 2, 3, 4};
          SDValue C = b.VECTOR_SHUFFLE(B, A, pool);

          SDValue D = b.SHL(immLeft - 128, NA);
          SDValue E = b.SRL(192 - immLeft, C);
          SDValue R = b.OR(D, E);

          return b.BITCAST(R, VT);
        }
        else
        {
          //immLeft >= 192 && immLeft <= 256
          int apool[] = {1, 2, 3, 4};
          A = b.VECTOR_SHUFFLE(B, A, apool);

          SDValue D = b.SHL(immLeft - 192, A);
          SDValue E = b.SRL(256 - immLeft, B);
          SDValue R = b.OR(D, E);

          return b.BITCAST(R, VT);
        }
      }
    }
  }

  if (VT == MVT::i128 && Subtarget->hasSSE2()) {
    return b.BITCAST(DAG.getNode(N->getOpcode(), dl, MVT::v2i64,
                                 b.BITCAST(V1, MVT::v2i64),
                                 b.BITCAST(V2, MVT::v2i64)), VT);
  }
  else if (VT == MVT::i256 && Subtarget->hasAVX2()) {
    return b.BITCAST(DAG.getNode(N->getOpcode(), dl, MVT::v4i64,
                                 b.BITCAST(V1, MVT::v4i64),
                                 b.BITCAST(V2, MVT::v4i64)), VT);
  }

  return SDValue();
}

SDValue X86TargetLowering::PerformParabixDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const
{
  //For now, only combine simple value type.
  if (!N->getValueType(0).isSimple()) return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  switch (N->getOpcode()) {
  default: break;
  case ISD::VSELECT:            return PXPerformVSELECTCombine(N, DAG, DCI, Subtarget);
  case ISD::VECTOR_SHUFFLE:     return PXPerformVECTOR_SHUFFLECombine(N, DAG, DCI, Subtarget);
  case ISD::SHL:
  case ISD::SRL:                return PXPerformShiftCombine(N, DAG, DCI, Subtarget);
  case ISD::UADDO:              return PXPerformUADDO(N, DAG, DCI, Subtarget);
  case ISD::UADDE:              return PXPerformUADDE(N, DAG, DCI, Subtarget);
  case ISD::AND:
  case ISD::XOR:
  case ISD::OR:                 return PXPerformLogic(N, DAG, DCI, Subtarget);
  }

  return SDValue();
}

