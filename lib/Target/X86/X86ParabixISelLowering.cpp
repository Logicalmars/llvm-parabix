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

#define DEBUG_TYPE "x86-isel"

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

//Parabix LowerSTORE
static SDValue PXLowerSTORE(SDValue Op, SelectionDAG &DAG) {
  MVT ValVT = Op.getOperand(1).getSimpleValueType();
  MVT CastType = getFullRegisterType(ValVT);

  SDNode *Node = Op.getNode();
  SDLoc dl(Node);
  StoreSDNode *ST = cast<StoreSDNode>(Node);
  SDValue Tmp1 = ST->getChain();
  SDValue Tmp2 = ST->getBasePtr();
  SDValue Tmp3 = ST->getValue();

  SDValue TransTmp3 = DAG.getNode(ISD::BITCAST, dl, CastType, Tmp3);
  return DAG.getStore(Tmp1, dl, TransTmp3, Tmp2, ST->getMemOperand());
}

static SDValue PXLowerLOAD(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT CastType = getFullRegisterType(VT);

  SDLoc dl(Op);
  LoadSDNode *LD = cast<LoadSDNode>(Op);

  SDValue Chain = LD->getChain();
  SDValue BasePtr = LD->getBasePtr();
  MachineMemOperand *MMO = LD->getMemOperand();

  SDValue NewLD = DAG.getLoad(CastType, dl, Chain, BasePtr, MMO);
  SDValue Result = DAG.getNode(ISD::BITCAST, dl, VT, NewLD);
  SDValue Ops[] = { Result, SDValue(NewLD.getNode(), 1) };

  return DAG.getMergeValues(Ops, dl);
}

typedef std::pair<ISD::NodeType, MVT> CastAndOpKind;
static std::map<CastAndOpKind, ISD::NodeType> CAOops;

static void addCastAndOpKind(ISD::NodeType Op, MVT VT, ISD::NodeType ReplaceOp)
{
  CAOops[std::make_pair(Op, VT)] = ReplaceOp;
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

  //cast v64i2 to v2i64 to lower logic ops.
  addCastAndOpKind(ISD::AND, MVT::v64i2, ISD::AND);
  addCastAndOpKind(ISD::XOR, MVT::v64i2, ISD::XOR);
  addCastAndOpKind(ISD::OR,  MVT::v64i2, ISD::OR);
}

static SDValue getFullRegister(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  SDLoc dl(Op);

  return DAG.getNode(ISD::BITCAST, dl, getFullRegisterType(VT), Op);
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
  if (Op.getSimpleValueType() == MVT::v64i2) {
    return GENLowerADD(Op, DAG);
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

static SDValue PXLowerINSERT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);

  SDLoc dl(Op);
  SDValue N0 = Op.getOperand(0); // vector <val>
  SDValue N1 = Op.getOperand(1); // elt
  SDValue N2 = Op.getOperand(2); // idx

  if (VT.getVectorElementType() == MVT::i1) {
    //VT is v32i1 or v64i1
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

  llvm_unreachable("lowering insert_vector_elt for unsupported type");
  return SDValue();
}

static SDValue PXLowerEXTRACT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) {
  SDLoc dl(Op);
  SDValue Vec = Op.getOperand(0);
  MVT VecVT = Vec.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VecVT);
  SDValue Idx = Op.getOperand(1);

  if (VecVT.getVectorElementType() == MVT::i1) {
    //VecVT is v32i1 or v64i1
    //TRUNC(AND(1, SRL(FULL_REG(VecVT), Idx)), i8)
    SDValue TransV = DAG.getNode(ISD::BITCAST, dl, FullVT, Vec);
    SDValue ShiftV = DAG.getNode(ISD::SRL, dl, FullVT, TransV, Idx);
    return DAG.getNode(ISD::TRUNCATE, dl, MVT::i8,
                       DAG.getNode(ISD::AND, dl, FullVT, ShiftV, DAG.getConstant(1, FullVT)));
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

  assert(EltVT.isInteger() && Val.getValueType().bitsGE(EltVT) &&
         "incorrect scalar_to_vector parameters");
  if (VecVT == MVT::v32i1 || VecVT == MVT::v64i1) {
    SDValue Trunc = DAG.getNode(ISD::AND, dl, MVT::i8, DAG.getConstant(1, MVT::i8),
                                DAG.getNode(ISD::TRUNCATE, dl, MVT::i8, Val));
    SDValue Ext = DAG.getNode(ISD::ANY_EXTEND, dl, FullVT, Trunc);
    return DAG.getNode(ISD::BITCAST, dl, VecVT, Ext);
  }

  llvm_unreachable("lowering unsupported scalar_to_vector");
}

//get zero vector for parabix
static SDValue getPXZeroVector(EVT VT, const X86Subtarget *Subtarget,
                             SelectionDAG &DAG, SDLoc dl) {
  assert(VT.isParabixVector() && "This function only lower parabix vectors");

  SDValue Vec;
  if (VT.isSimple() && VT.getSimpleVT().is32BitVector()) {
    // Careful here, don't use TargetConstant until you are sure.
    Vec = DAG.getConstant(0, MVT::i32);
  } else if (VT.isSimple() && VT.getSimpleVT().is64BitVector()) {
    Vec = DAG.getConstant(0, MVT::i64);
  } else
    llvm_unreachable("Unexpected vector type");

  return DAG.getNode(ISD::BITCAST, dl, VT, Vec);
}

static SDValue getPXOnesVector(EVT VT, const X86Subtarget *Subtarget,
                             SelectionDAG &DAG, SDLoc dl) {
  assert(VT.isParabixVector() && "This function only lower parabix vectors");

  SDValue Vec;
  if (VT.isSimple() && VT.getSimpleVT().is32BitVector()) {
    // Careful here, don't use TargetConstant until you are sure.
    Vec = DAG.getConstant(-1, MVT::i32);
  } else if (VT.isSimple() && VT.getSimpleVT().is64BitVector()) {
    Vec = DAG.getConstant(-1, MVT::i64);
  } else
    llvm_unreachable("Unexpected vector type");

  return DAG.getNode(ISD::BITCAST, dl, VT, Vec);
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

    return getPXZeroVector(VT, Subtarget, DAG, dl);
  }

  if (ISD::isBuildVectorAllOnes(Op.getNode())) {
    return getPXOnesVector(VT, Subtarget, DAG, dl);
  }

  if (VT == MVT::v32i1 || VT == MVT::v64i1) {
    //Brutely insert element
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

  if (VT == MVT::v32i1 || VT == MVT::v64i1) {
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
  dbgs() << "Parabix Lowering:" << "\n"; Op.dump();

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

  switch (Op.getOpcode()) {
  default: llvm_unreachable("[ROOT SWITCH] Should not custom lower this parabix op!");
  case ISD::STORE:              return PXLowerSTORE(Op, DAG);
  case ISD::LOAD:               return PXLowerLOAD(Op, DAG);
  case ISD::ADD:                return PXLowerADD(Op, DAG);
  case ISD::SUB:                return PXLowerSUB(Op, DAG);
  case ISD::MUL:                return PXLowerMUL(Op, DAG);
  case ISD::BUILD_VECTOR:       return PXLowerBUILD_VECTOR(Op, DAG);
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:                return PXLowerShift(Op, DAG);
  case ISD::INSERT_VECTOR_ELT:  return PXLowerINSERT_VECTOR_ELT(Op, DAG);
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
      dbgs() << "Combining select v128i1 \n";
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
    dbgs() << "Parabix combine: \n";
    N->dumpr();

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
    dbgs() << "Parabix combine: \n";
    N->dumpr();

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
    dbgs() << "Parabix combine: \n";
    N->dumpr();

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

SDValue X86TargetLowering::PerformParabixDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const
{
  //For now, only combine simple value type.
  if (!N->getValueType(0).isSimple()) return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  switch (N->getOpcode()) {
  default: break;
  case ISD::VSELECT:            return PXPerformVSELECTCombine(N, DAG, DCI, Subtarget);
  case ISD::VECTOR_SHUFFLE:     return PXPerformVECTOR_SHUFFLECombine(N, DAG, DCI, Subtarget);
  }

  return SDValue();
}

