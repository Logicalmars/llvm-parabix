//===- ValueTypes.td - ValueType definitions ---------------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Value types - These values correspond to the register types defined in the
// ValueTypes.h file.  If you update anything here, you must update it there as
// well!
//
//===----------------------------------------------------------------------===//

class ValueType<int size, int value> {
  string Namespace = "MVT";
  int Size = size;
  int Value = value;
}

def OtherVT: ValueType<0  ,  0>;   // "Other" value
def i1	: ValueType<1 , 1>;
def i8	: ValueType<8 , 2>;
def i16	: ValueType<16 , 3>;
def i32	: ValueType<32 , 4>;
def i64	: ValueType<64 , 5>;
def i128	: ValueType<128 , 6>;
def i256	: ValueType<256 , 7>;
def i512	: ValueType<512 , 8>;
def i1024	: ValueType<1024 , 9>;

def f16    : ValueType<16 , 10>;   // 16-bit floating point value
def f32    : ValueType<32 , 11>;   // 32-bit floating point value
def f64    : ValueType<64 , 12>;   // 64-bit floating point value
def f80    : ValueType<80 , 13>;   // 80-bit floating point value
def f128   : ValueType<128, 14>;   // 128-bit floating point value
def ppcf128: ValueType<128, 15>;   // PPC 128-bit floating point value

def v1i1	: ValueType<1 , 16>;
def v2i1	: ValueType<2 , 17>;
def v4i1	: ValueType<4 , 18>;
def v8i1	: ValueType<8 , 19>;
def v16i1	: ValueType<16 , 20>;
def v32i1	: ValueType<32 , 21>;
def v64i1	: ValueType<64 , 22>;
def v128i1	: ValueType<128 , 23>;
def v256i1	: ValueType<256 , 24>;
def v512i1	: ValueType<512 , 25>;
def v1024i1	: ValueType<1024 , 26>;
def v1i2	: ValueType<2 , 27>;
def v2i2	: ValueType<4 , 28>;
def v4i2	: ValueType<8 , 29>;
def v8i2	: ValueType<16 , 30>;
def v16i2	: ValueType<32 , 31>;
def v32i2	: ValueType<64 , 32>;
def v64i2	: ValueType<128 , 33>;
def v128i2	: ValueType<256 , 34>;
def v256i2	: ValueType<512 , 35>;
def v512i2	: ValueType<1024 , 36>;
def v1i4	: ValueType<4 , 37>;
def v2i4	: ValueType<8 , 38>;
def v4i4	: ValueType<16 , 39>;
def v8i4	: ValueType<32 , 40>;
def v16i4	: ValueType<64 , 41>;
def v32i4	: ValueType<128 , 42>;
def v64i4	: ValueType<256 , 43>;
def v128i4	: ValueType<512 , 44>;
def v256i4	: ValueType<1024 , 45>;
def v1i8	: ValueType<8 , 46>;
def v2i8	: ValueType<16 , 47>;
def v4i8	: ValueType<32 , 48>;
def v8i8	: ValueType<64 , 49>;
def v16i8	: ValueType<128 , 50>;
def v32i8	: ValueType<256 , 51>;
def v64i8	: ValueType<512 , 52>;
def v128i8	: ValueType<1024 , 53>;
def v1i16	: ValueType<16 , 54>;
def v2i16	: ValueType<32 , 55>;
def v4i16	: ValueType<64 , 56>;
def v8i16	: ValueType<128 , 57>;
def v16i16	: ValueType<256 , 58>;
def v32i16	: ValueType<512 , 59>;
def v64i16	: ValueType<1024 , 60>;
def v1i32	: ValueType<32 , 61>;
def v2i32	: ValueType<64 , 62>;
def v4i32	: ValueType<128 , 63>;
def v8i32	: ValueType<256 , 64>;
def v16i32	: ValueType<512 , 65>;
def v32i32	: ValueType<1024 , 66>;
def v1i64	: ValueType<64 , 67>;
def v2i64	: ValueType<128 , 68>;
def v4i64	: ValueType<256 , 69>;
def v8i64	: ValueType<512 , 70>;
def v16i64	: ValueType<1024 , 71>;
def v1i128	: ValueType<128 , 72>;
def v2i128	: ValueType<256 , 73>;
def v4i128	: ValueType<512 , 74>;
def v8i128	: ValueType<1024 , 75>;
def v1i256	: ValueType<256 , 76>;
def v2i256	: ValueType<512 , 77>;
def v4i256	: ValueType<1024 , 78>;
def v1i512	: ValueType<512 , 79>;
def v2i512	: ValueType<1024 , 80>;
def v1i1024	: ValueType<1024 , 81>;


def v1f16	: ValueType<16 , 82>;
def v2f16	: ValueType<32 , 83>;
def v4f16	: ValueType<64 , 84>;
def v8f16	: ValueType<128 , 85>;
def v16f16	: ValueType<256 , 86>;
def v32f16	: ValueType<512 , 87>;
def v64f16	: ValueType<1024 , 88>;
def v1f32	: ValueType<32 , 89>;
def v2f32	: ValueType<64 , 90>;
def v4f32	: ValueType<128 , 91>;
def v8f32	: ValueType<256 , 92>;
def v16f32	: ValueType<512 , 93>;
def v32f32	: ValueType<1024 , 94>;
def v1f64	: ValueType<64 , 95>;
def v2f64	: ValueType<128 , 96>;
def v4f64	: ValueType<256 , 97>;
def v8f64	: ValueType<512 , 98>;
def v16f64	: ValueType<1024 , 99>;


def x86mmx : ValueType<64 , 100>;   // X86 MMX value
def FlagVT : ValueType<0  , 101>;   // Pre-RA sched glue
def isVoid : ValueType<0  , 102>;   // Produces no value
def untyped: ValueType<8  , 103>;   // Produces an untyped value

def MetadataVT: ValueType<0, 250>; // Metadata

// Pseudo valuetype mapped to the current pointer size to any address space.
// Should only be used in TableGen.
def iPTRAny   : ValueType<0, 251>;

// Pseudo valuetype to represent "vector of any size"
def vAny   : ValueType<0  , 252>;

// Pseudo valuetype to represent "float of any format"
def fAny   : ValueType<0  , 253>;

// Pseudo valuetype to represent "integer of any bit width"
def iAny   : ValueType<0  , 254>;

// Pseudo valuetype mapped to the current pointer size.
def iPTR   : ValueType<0  , 255>;
