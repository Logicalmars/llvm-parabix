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
#include <bitset>
#include <cctype>
#include <map>
using namespace llvm;

#define DEBUG_TYPE "x86-isel"

//Parabix LowerSTORE
static SDValue PXLowerSTORE(SDValue Op, SelectionDAG &DAG) {
  //TODO: expand this strategy to all 32bit/64bit vectors
  MVT ValVT = Op.getOperand(1).getSimpleValueType();
  if (ValVT == MVT::v32i1)
  {
    SDNode *Node = Op.getNode();
    SDLoc dl(Node);
    StoreSDNode *ST = cast<StoreSDNode>(Node);
    SDValue Tmp1 = ST->getChain();
    SDValue Tmp2 = ST->getBasePtr();
    SDValue Tmp3 = ST->getValue();

    SDValue TransTmp3 = DAG.getNode(ISD::BITCAST, dl, MVT::i32, Tmp3);
    return DAG.getStore(Tmp1, dl, TransTmp3, Tmp2, ST->getMemOperand());
  }
  llvm_unreachable("lowering store for unsupported type");
  return SDValue();
}

static SDValue PXLowerLOAD(SDValue Op, SelectionDAG &DAG) {
  //TODO: This strategy works for all 32bit vector and 64bit vectors.
  if (Op.getSimpleValueType() == MVT::v32i1)
  {
    SDLoc dl(Op);
    LoadSDNode *LD = cast<LoadSDNode>(Op);

    SDValue Chain = LD->getChain();
    SDValue BasePtr = LD->getBasePtr();
    MachineMemOperand *MMO = LD->getMemOperand();

    SDValue NewLD = DAG.getLoad(MVT::i32, dl, Chain, BasePtr, MMO);
    SDValue Result = DAG.getNode(ISD::BITCAST, dl, MVT::v32i1, NewLD);
    SDValue Ops[] = { Result, SDValue(NewLD.getNode(), 1) };

    return DAG.getMergeValues(Ops, dl);
  }
  llvm_unreachable("lowering store for unsupported type");
  return SDValue();
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

  //addCastAndOpKind(ISD::VECTOR_SHUFFLE, MVT::v32i1, Expand);
  //addCastAndOpKind(ISD::EXTRACT_VECTOR_ELT, MVT::v32i1,Expand);
  //addCastAndOpKind(ISD::INSERT_VECTOR_ELT, MVT::v32i1, Expand);
  //addCastAndOpKind(ISD::EXTRACT_SUBVECTOR, MVT::v32i1,Expand);
  //addCastAndOpKind(ISD::INSERT_SUBVECTOR, MVT::v32i1, EXpand);
  //SMUL_LOHI
  //MULHS, UMUL_LOHI, MULHU
  //SHL, SRA, SRL, ROTL, ROTR,
  //VSELECT
}

//Bitcast this vector to a full length integer and then do one op
static SDValue lowerWithCastAndOp(SDValue Op, SelectionDAG &DAG) {
  SDLoc dl(Op);
  MVT VT = Op.getSimpleValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);
  CastAndOpKind kind = std::make_pair((ISD::NodeType)Op.getOpcode(), VT);

  assert(CAOops.find(kind) != CAOops.end() && "Undefined cast and op kind");

  MVT castType;
  if (VT.is32BitVector())
    castType = MVT::i32;
  else if (VT.is64BitVector())
    castType = MVT::i64;
  else
    llvm_unreachable("unsupported parabix vector width");

  SDValue transA = DAG.getNode(ISD::BITCAST, dl, castType, A);
  SDValue transB = DAG.getNode(ISD::BITCAST, dl, castType, B);
  SDValue res = DAG.getNode(CAOops[kind], dl, castType, transA, transB);

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

  if (VT == MVT::v32i1 && Op.getOpcode() != ISD::SRA) {
    //SRL or SHL
    SDValue transA = DAG.getNode(ISD::BITCAST, dl, MVT::i32, A);
    SDValue transB = DAG.getNode(ISD::BITCAST, dl, MVT::i32, B);
    SDValue negB = DAG.getNOT(dl, transB, MVT::i32);
    res = DAG.getNode(ISD::AND, dl, MVT::i32, transA, negB);
  }
  else if (VT == MVT::v32i1 && Op.getOpcode() == ISD::SRA) {
    return A;
  }
  else
    llvm_unreachable("lowering undefined parabix shift ops");

  return DAG.getNode(ISD::BITCAST, dl, VT, res);
}

static SDValue PXLowerADD(SDValue Op, SelectionDAG &DAG) {
  llvm_unreachable("lowering add for unsupported type");
  return SDValue();
}

//get zero vector for parabix
static SDValue getPXZeroVector(EVT VT, const X86Subtarget *Subtarget,
                             SelectionDAG &DAG, SDLoc dl) {
  assert(VT.isParabixVector() && "This function only lower parabix vectors");

  SDValue Vec;
  if (VT.isSimple() && VT.getSimpleVT().is32BitVector()) {
    // Careful here, don't use TargetConstant until you are sure.
    Vec = DAG.getConstant(0, MVT::i32);
  } else
    llvm_unreachable("Unexpected vector type");

  return DAG.getNode(ISD::BITCAST, dl, VT, Vec);
}

SDValue
X86TargetLowering::PXLowerBUILD_VECTOR(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);

  MVT VT = Op.getSimpleValueType();
  //MVT ExtVT = VT.getVectorElementType();
  //unsigned NumElems = Op.getNumOperands();

  // Vectors containing all zeros can be matched by pxor and xorps later
  if (ISD::isBuildVectorAllZeros(Op.getNode())) {
    // Canonicalize this to <4 x i32> to 1) ensure the zero vectors are CSE'd
    // and 2) ensure that i64 scalars are eliminated on x86-32 hosts.
    if (VT == MVT::v4i32 || VT == MVT::v8i32 || VT == MVT::v16i32)
      return Op;

    return getPXZeroVector(VT, Subtarget, DAG, dl);
  }

  llvm_unreachable("lowering build_vector for unsupported type");
  return SDValue();
}

///Entrance for parabix lowering.
SDValue X86TargetLowering::LowerParabixOperation(SDValue Op, SelectionDAG &DAG) const {
  //NEED: setOperationAction in target specific lowering (X86ISelLowering.cpp)
  dbgs() << "Parabix Lowering:" << "\n"; Op.dumpr();

  //Only resetOperations for the first time.
  static bool FirstTimeThrough = true;
  if (FirstTimeThrough) {
    resetOperations();
    FirstTimeThrough = false;
  }

  MVT VT = Op.getSimpleValueType();
  //Check if we have registered CastAndOp action
  CastAndOpKind kind = std::make_pair((ISD::NodeType)Op.getOpcode(), VT);
  if (CAOops.find(kind) != CAOops.end())
    return lowerWithCastAndOp(Op, DAG);

  switch (Op.getOpcode()) {
  default: llvm_unreachable("Should not custom lower this parabix op!");
  case ISD::STORE:              return PXLowerSTORE(Op, DAG);
  case ISD::LOAD:               return PXLowerLOAD(Op, DAG);
  case ISD::ADD:                return PXLowerADD(Op, DAG);
  case ISD::BUILD_VECTOR:       return PXLowerBUILD_VECTOR(Op, DAG);
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:                return PXLowerShift(Op, DAG);
  }
}
