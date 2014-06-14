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
using namespace llvm;

#define DEBUG_TYPE "x86-isel"

//Parabix LowerSTORE
static SDValue PXLowerSTORE(SDValue Op, SelectionDAG &DAG) {
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

static SDValue PXLowerADD(SDValue Op, SelectionDAG &DAG) {
  //Add for v32i1
  SDLoc dl(Op);
  MVT VT = Op.getSimpleValueType();
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);

  if (VT == MVT::v32i1)
  {
    dbgs() << "LowerADD v32i1" << "\n";
    SDValue transA = DAG.getNode(ISD::BITCAST, dl, MVT::i32, A);
    SDValue transB = DAG.getNode(ISD::BITCAST, dl, MVT::i32, B);
    return DAG.getNode(ISD::XOR, dl, MVT::i32, transA, transB);
  }

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
  MVT ExtVT = VT.getVectorElementType();
  unsigned NumElems = Op.getNumOperands();

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
  dbgs() << "Parabix Lowering" << "\n";
  Op.dumpr();

  switch (Op.getOpcode()) {
  default: llvm_unreachable("Should not custom lower this parabix op!");
  case ISD::STORE:              return PXLowerSTORE(Op, DAG);
  case ISD::LOAD:               return PXLowerLOAD(Op, DAG);
  case ISD::ADD:                return PXLowerADD(Op, DAG);
  case ISD::BUILD_VECTOR:       return PXLowerBUILD_VECTOR(Op, DAG);
  }
}
