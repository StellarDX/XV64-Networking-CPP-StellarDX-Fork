#pragma once

#ifndef URANDOM_H
#define URANDOM_H

#include "UDef.hh"

template<typename UIntType, size_t w,
    size_t n, size_t m, size_t r, UIntType a,
    UIntType b, UIntType c,  size_t s, size_t t,
    size_t u, UIntType d, size_t l, UIntType f>
class MersenneTwisterEngineLite
{
public:
    /** The type of the generated random value. */
    using result_type = UIntType;

    // parameter values
    static const size_t      WordSize         = w;
    static const size_t      DegOfRecurrence  = n;
    static const size_t      MiddleWord       = m;
    static const size_t      SeparationPoint  = r;
    static const result_type TwisterMatCoeffs = a;
    static const result_type TGFSRMaskB       = b;
    static const result_type TGFSRMaskC       = c;
    static const size_t      TGFSRShiftS      = s;
    static const size_t      TGFSRShiftT      = t;
    static const size_t      AdditionalShift  = u;
    static const result_type AdditionalMask   = d;
    static const size_t      AdditionalShiftL = l;
    static const result_type InitMultiplyer   = f;
    static const result_type DefaultSeed      = 5489u;

    result_type MT[DegOfRecurrence];
    size_t      Index;

    result_type min() {return 0;}
    result_type max() {return ~min();}

    MersenneTwisterEngineLite() : MersenneTwisterEngineLite(DefaultSeed) {}
    MersenneTwisterEngineLite(result_type Seed) {Init(Seed);}

    void Init(result_type Seed)
    {
        MT[0] = Seed & max();
        for (size_t i = 1; i < DegOfRecurrence; i++)
        {
            MT[i] = (InitMultiplyer * (MT[i - 1] ^ (MT[i - 1] >> (WordSize - 2))) + i) & max();
        }
        Index = 0;
    }

    result_type Gen()
    {
        const size_t Index2 = (Index + 1) % DegOfRecurrence;
        const uint64 Mask = SeparationPoint == WordSize ?
            max() : (1ULL << SeparationPoint) - 1ULL;

        const uint64 Yp = (MT[Index] & (~Mask)) | (MT[Index2] & Mask);
        const size_t Index3 = (Index + MiddleWord) % DegOfRecurrence;
        MT[Index] = MT[Index3] ^ (Yp >> 1ULL) ^ (TwisterMatCoeffs * (Yp & 1));
        uint64 Z = MT[Index] ^ ((MT[Index] >> AdditionalShift) & AdditionalMask);
        Index = Index2;
        Z ^= (Z << TGFSRShiftS) & TGFSRMaskB;
        Z ^= (Z << TGFSRShiftT) & TGFSRMaskC;
        return Z ^ (Z >> AdditionalShiftL);
    }

    result_type operator()() {return Gen();}
};

using mt19937l = MersenneTwisterEngineLite
    <DWORD, 32, 624, 397, 31, 0x9908B0DF,
    0x9D2C5680, 0xEFC60000, 7, 15,
    11, 0xFFFFFFFF, 18, 0x6C078965>;

using mt19937l_64 = MersenneTwisterEngineLite
    <QWORD, 64, 312, 156, 31, 0xB5026F5AA96619E9,
    0x71D67FFFEDA60000, 0xFFF7EEE000000000, 17, 37,
    29, 0x5555555555555555, 43, 0x5851F42D4C957F2D>;

#endif // URANDOM_H
