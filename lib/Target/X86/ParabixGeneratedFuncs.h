/***************** Python template file ******************
 * Lower v64i2 operations into logic / shiftings.
 * AUTO GENERATED FILE
 */
#ifndef PARABIX_GENERATED_FUNCS
#define PARABIX_GENERATED_FUNCS

#include "llvm/CodeGen/SelectionDAG.h"
using namespace llvm;

MVT getFullRegisterType(MVT VT);

/* Generated function */
static SDValue GENLowerADD(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.XOR(A1, A2);
    return b.IFH1(b.HiMask(128, 2), b.XOR(Tmp, b.SHL<1>(b.AND(A1, A2))), Tmp);
  }

  llvm_unreachable("GENLower of add is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerSUB(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.XOR(A1, A2);
    return b.IFH1(b.HiMask(128, 2), b.XOR(Tmp, b.SHL<1>(b.AND(b.NOT(A1), A2))), Tmp);
  }

  llvm_unreachable("GENLower of sub is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerMUL(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp1 = b.SHL<1>(A1);
    SDValue Tmp2 = b.SHL<1>(A2);
    return b.IFH1(b.HiMask(128, 2), b.OR(b.AND(Tmp1, b.AND(A2, b.OR(b.NOT(A1), b.NOT(Tmp2)))), b.AND(A1, b.AND(Tmp2, b.OR(b.NOT(Tmp1), b.NOT(A2))))), b.AND(A1, A2));
  }

  llvm_unreachable("GENLower of mul is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerICMP_EQ(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.XOR(A1, A2);
    SDValue TmpAns = b.AND(b.NOT(b.SHL<1>(Tmp)), b.NOT(Tmp));
    return b.IFH1(b.HiMask(128, 2), TmpAns, b.SRL<1>(TmpAns));
  }

  llvm_unreachable("GENLower of icmp eq is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerICMP_SLT(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.NOT(A2);
    SDValue TmpAns = b.OR(b.AND(A1, Tmp), b.AND(b.SHL<1>(b.AND(b.NOT(A1), A2)), b.OR(A1, Tmp)));
    return b.IFH1(b.HiMask(128, 2), TmpAns, b.SRL<1>(TmpAns));
  }

  llvm_unreachable("GENLower of icmp slt is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerICMP_SGT(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.NOT(A1);
    SDValue TmpAns = b.OR(b.AND(Tmp, A2), b.AND(b.SHL<1>(b.AND(A1, b.NOT(A2))), b.OR(Tmp, A2)));
    return b.IFH1(b.HiMask(128, 2), TmpAns, b.SRL<1>(TmpAns));
  }

  llvm_unreachable("GENLower of icmp sgt is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerICMP_ULT(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.NOT(A1);
    SDValue TmpAns = b.OR(b.AND(Tmp, A2), b.AND(b.SHL<1>(b.AND(Tmp, A2)), b.OR(Tmp, A2)));
    return b.IFH1(b.HiMask(128, 2), TmpAns, b.SRL<1>(TmpAns));
  }

  llvm_unreachable("GENLower of icmp ult is misused.");
  return SDValue();
}

/* Generated function */
static SDValue GENLowerICMP_UGT(SDValue Op, SelectionDAG &DAG) {
  MVT VT = Op.getSimpleValueType();
  MVT FullVT = getFullRegisterType(VT);
  SDNodeTreeBuilder b(Op, &DAG);

  if (VT == MVT::v64i2) {
    SDValue A1 = b.BITCAST(Op.getOperand(0), FullVT);
    SDValue A2 = b.BITCAST(Op.getOperand(1), FullVT);

    SDValue Tmp = b.NOT(A2);
    SDValue TmpAns = b.OR(b.AND(A1, Tmp), b.AND(b.SHL<1>(b.AND(A1, Tmp)), b.OR(A1, Tmp)));
    return b.IFH1(b.HiMask(128, 2), TmpAns, b.SRL<1>(TmpAns));
  }

  llvm_unreachable("GENLower of icmp ugt is misused.");
  return SDValue();
}


#endif