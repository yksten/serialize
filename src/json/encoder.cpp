#include "json/encoder.h"
#include <assert.h>


namespace serialize {


#ifndef FASTJSON_UINT64_C2
#define FASTJSON_UINT64_C2(high32, low32) ((static_cast<uint64_t>(high32) << 32) | static_cast<uint64_t>(low32))
#endif

    namespace internal {

        class Double {
        public:
            Double() {}
            Double(double d) : d_(d) {}
            Double(uint64_t u) : u_(u) {}

            double Value() const { return d_; }
            uint64_t Uint64Value() const { return u_; }

            double NextPositiveDouble() const {
                assert(!Sign());
                return Double(u_ + 1).Value();
            }

            bool Sign() const { return (u_ & kSignMask) != 0; }
            uint64_t Significand() const { return u_ & kSignificandMask; }
            int Exponent() const { return static_cast<int>(((u_ & kExponentMask) >> kSignificandSize) - kExponentBias); }

            bool IsNan() const { return (u_ & kExponentMask) == kExponentMask && Significand() != 0; }
            bool IsInf() const { return (u_ & kExponentMask) == kExponentMask && Significand() == 0; }
            bool IsNanOrInf() const { return (u_ & kExponentMask) == kExponentMask; }
            bool IsNormal() const { return (u_ & kExponentMask) != 0 || Significand() == 0; }
            bool IsZero() const { return (u_ & (kExponentMask | kSignificandMask)) == 0; }

            uint64_t IntegerSignificand() const { return IsNormal() ? Significand() | kHiddenBit : Significand(); }
            int IntegerExponent() const { return (IsNormal() ? Exponent() : kDenormalExponent) - kSignificandSize; }
            uint64_t ToBias() const { return (u_ & kSignMask) ? ~u_ + 1 : u_ | kSignMask; }

            static unsigned EffectiveSignificandSize(int order) {
                if (order >= -1021)
                    return 53;
                else if (order <= -1074)
                    return 0;
                else
                    return static_cast<unsigned>(order) + 1074;
            }

        private:
            static const int kSignificandSize = 52;
            static const int kExponentBias = 0x3FF;
            static const int kDenormalExponent = 1 - kExponentBias;
            static const uint64_t kSignMask = FASTJSON_UINT64_C2(0x80000000, 0x00000000);
            static const uint64_t kExponentMask = FASTJSON_UINT64_C2(0x7FF00000, 0x00000000);
            static const uint64_t kSignificandMask = FASTJSON_UINT64_C2(0x000FFFFF, 0xFFFFFFFF);
            static const uint64_t kHiddenBit = FASTJSON_UINT64_C2(0x00100000, 0x00000000);

            union {
                double d_;
                uint64_t u_;
            };
        };

        struct DiyFp {
            DiyFp() : f(), e() {}

            DiyFp(uint64_t fp, int exp) : f(fp), e(exp) {}

            explicit DiyFp(double d) {
                union {
                    double d;
                    uint64_t u64;
                } u = { d };

                int biased_e = static_cast<int>((u.u64 & kDpExponentMask) >> kDpSignificandSize);
                uint64_t significand = (u.u64 & kDpSignificandMask);
                if (biased_e != 0) {
                    f = significand + kDpHiddenBit;
                    e = biased_e - kDpExponentBias;
                }
                else {
                    f = significand;
                    e = kDpMinExponent + 1;
                }
            }

            DiyFp operator-(const DiyFp& rhs) const {
                return DiyFp(f - rhs.f, e);
            }

            DiyFp operator*(const DiyFp& rhs) const {
#if defined(_MSC_VER) && defined(_M_AMD64)
                uint64_t h;
                uint64_t l = _umul128(f, rhs.f, &h);
                if (l & (uint64_t(1) << 63)) // rounding
                    h++;
                return DiyFp(h, e + rhs.e + 64);
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) && defined(__x86_64__)
                __extension__ typedef unsigned __int128 uint128;
                uint128 p = static_cast<uint128>(f) * static_cast<uint128>(rhs.f);
                uint64_t h = static_cast<uint64_t>(p >> 64);
                uint64_t l = static_cast<uint64_t>(p);
                if (l & (uint64_t(1) << 63)) // rounding
                    h++;
                return DiyFp(h, e + rhs.e + 64);
#else
                const uint64_t M32 = 0xFFFFFFFF;
                const uint64_t a = f >> 32;
                const uint64_t b = f & M32;
                const uint64_t c = rhs.f >> 32;
                const uint64_t d = rhs.f & M32;
                const uint64_t ac = a * c;
                const uint64_t bc = b * c;
                const uint64_t ad = a * d;
                const uint64_t bd = b * d;
                uint64_t tmp = (bd >> 32) + (ad & M32) + (bc & M32);
                tmp += 1U << 31;  /// mult_round
                return DiyFp(ac + (ad >> 32) + (bc >> 32) + (tmp >> 32), e + rhs.e + 64);
#endif
            }

            DiyFp Normalize() const {
#if defined(_MSC_VER) && defined(_M_AMD64)
                unsigned long index;
                _BitScanReverse64(&index, f);
                return DiyFp(f << (63 - index), e - (63 - index));
#elif defined(__GNUC__) && __GNUC__ >= 4
                int s = __builtin_clzll(f);
                return DiyFp(f << s, e - s);
#else
                DiyFp res = *this;
                while (!(res.f & (static_cast<uint64_t>(1) << 63))) {
                    res.f <<= 1;
                    res.e--;
                }
                return res;
#endif
            }

            DiyFp NormalizeBoundary() const {
                DiyFp res = *this;
                while (!(res.f & (kDpHiddenBit << 1))) {
                    res.f <<= 1;
                    res.e--;
                }
                res.f <<= (kDiySignificandSize - kDpSignificandSize - 2);
                res.e = res.e - (kDiySignificandSize - kDpSignificandSize - 2);
                return res;
            }

            void NormalizedBoundaries(DiyFp* minus, DiyFp* plus) const {
                DiyFp pl = DiyFp((f << 1) + 1, e - 1).NormalizeBoundary();
                DiyFp mi = (f == kDpHiddenBit) ? DiyFp((f << 2) - 1, e - 2) : DiyFp((f << 1) - 1, e - 1);
                mi.f <<= mi.e - pl.e;
                mi.e = pl.e;
                *plus = pl;
                *minus = mi;
            }

            double ToDouble() const {
                union {
                    double d;
                    uint64_t u64;
                }u;
                const uint64_t be = (e == kDpDenormalExponent && (f & kDpHiddenBit) == 0) ? 0 :
                    static_cast<uint64_t>(e + kDpExponentBias);
                u.u64 = (f & kDpSignificandMask) | (be << kDpSignificandSize);
                return u.d;
            }

            static const int kDiySignificandSize = 64;
            static const int kDpSignificandSize = 52;
            static const int kDpExponentBias = 0x3FF + kDpSignificandSize;
            static const int kDpMaxExponent = 0x7FF - kDpExponentBias;
            static const int kDpMinExponent = -kDpExponentBias;
            static const int kDpDenormalExponent = -kDpExponentBias + 1;
            static const uint64_t kDpExponentMask = FASTJSON_UINT64_C2(0x7FF00000, 0x00000000);
            static const uint64_t kDpSignificandMask = FASTJSON_UINT64_C2(0x000FFFFF, 0xFFFFFFFF);
            static const uint64_t kDpHiddenBit = FASTJSON_UINT64_C2(0x00100000, 0x00000000);

            uint64_t f;
            int e;
        };

        inline DiyFp GetCachedPowerByIndex(size_t index) {
            // 10^-348, 10^-340, ..., 10^340
            static const uint64_t kCachedPowers_F[] = {
                FASTJSON_UINT64_C2(0xfa8fd5a0, 0x081c0288), FASTJSON_UINT64_C2(0xbaaee17f, 0xa23ebf76),
                FASTJSON_UINT64_C2(0x8b16fb20, 0x3055ac76), FASTJSON_UINT64_C2(0xcf42894a, 0x5dce35ea),
                FASTJSON_UINT64_C2(0x9a6bb0aa, 0x55653b2d), FASTJSON_UINT64_C2(0xe61acf03, 0x3d1a45df),
                FASTJSON_UINT64_C2(0xab70fe17, 0xc79ac6ca), FASTJSON_UINT64_C2(0xff77b1fc, 0xbebcdc4f),
                FASTJSON_UINT64_C2(0xbe5691ef, 0x416bd60c), FASTJSON_UINT64_C2(0x8dd01fad, 0x907ffc3c),
                FASTJSON_UINT64_C2(0xd3515c28, 0x31559a83), FASTJSON_UINT64_C2(0x9d71ac8f, 0xada6c9b5),
                FASTJSON_UINT64_C2(0xea9c2277, 0x23ee8bcb), FASTJSON_UINT64_C2(0xaecc4991, 0x4078536d),
                FASTJSON_UINT64_C2(0x823c1279, 0x5db6ce57), FASTJSON_UINT64_C2(0xc2109436, 0x4dfb5637),
                FASTJSON_UINT64_C2(0x9096ea6f, 0x3848984f), FASTJSON_UINT64_C2(0xd77485cb, 0x25823ac7),
                FASTJSON_UINT64_C2(0xa086cfcd, 0x97bf97f4), FASTJSON_UINT64_C2(0xef340a98, 0x172aace5),
                FASTJSON_UINT64_C2(0xb23867fb, 0x2a35b28e), FASTJSON_UINT64_C2(0x84c8d4df, 0xd2c63f3b),
                FASTJSON_UINT64_C2(0xc5dd4427, 0x1ad3cdba), FASTJSON_UINT64_C2(0x936b9fce, 0xbb25c996),
                FASTJSON_UINT64_C2(0xdbac6c24, 0x7d62a584), FASTJSON_UINT64_C2(0xa3ab6658, 0x0d5fdaf6),
                FASTJSON_UINT64_C2(0xf3e2f893, 0xdec3f126), FASTJSON_UINT64_C2(0xb5b5ada8, 0xaaff80b8),
                FASTJSON_UINT64_C2(0x87625f05, 0x6c7c4a8b), FASTJSON_UINT64_C2(0xc9bcff60, 0x34c13053),
                FASTJSON_UINT64_C2(0x964e858c, 0x91ba2655), FASTJSON_UINT64_C2(0xdff97724, 0x70297ebd),
                FASTJSON_UINT64_C2(0xa6dfbd9f, 0xb8e5b88f), FASTJSON_UINT64_C2(0xf8a95fcf, 0x88747d94),
                FASTJSON_UINT64_C2(0xb9447093, 0x8fa89bcf), FASTJSON_UINT64_C2(0x8a08f0f8, 0xbf0f156b),
                FASTJSON_UINT64_C2(0xcdb02555, 0x653131b6), FASTJSON_UINT64_C2(0x993fe2c6, 0xd07b7fac),
                FASTJSON_UINT64_C2(0xe45c10c4, 0x2a2b3b06), FASTJSON_UINT64_C2(0xaa242499, 0x697392d3),
                FASTJSON_UINT64_C2(0xfd87b5f2, 0x8300ca0e), FASTJSON_UINT64_C2(0xbce50864, 0x92111aeb),
                FASTJSON_UINT64_C2(0x8cbccc09, 0x6f5088cc), FASTJSON_UINT64_C2(0xd1b71758, 0xe219652c),
                FASTJSON_UINT64_C2(0x9c400000, 0x00000000), FASTJSON_UINT64_C2(0xe8d4a510, 0x00000000),
                FASTJSON_UINT64_C2(0xad78ebc5, 0xac620000), FASTJSON_UINT64_C2(0x813f3978, 0xf8940984),
                FASTJSON_UINT64_C2(0xc097ce7b, 0xc90715b3), FASTJSON_UINT64_C2(0x8f7e32ce, 0x7bea5c70),
                FASTJSON_UINT64_C2(0xd5d238a4, 0xabe98068), FASTJSON_UINT64_C2(0x9f4f2726, 0x179a2245),
                FASTJSON_UINT64_C2(0xed63a231, 0xd4c4fb27), FASTJSON_UINT64_C2(0xb0de6538, 0x8cc8ada8),
                FASTJSON_UINT64_C2(0x83c7088e, 0x1aab65db), FASTJSON_UINT64_C2(0xc45d1df9, 0x42711d9a),
                FASTJSON_UINT64_C2(0x924d692c, 0xa61be758), FASTJSON_UINT64_C2(0xda01ee64, 0x1a708dea),
                FASTJSON_UINT64_C2(0xa26da399, 0x9aef774a), FASTJSON_UINT64_C2(0xf209787b, 0xb47d6b85),
                FASTJSON_UINT64_C2(0xb454e4a1, 0x79dd1877), FASTJSON_UINT64_C2(0x865b8692, 0x5b9bc5c2),
                FASTJSON_UINT64_C2(0xc83553c5, 0xc8965d3d), FASTJSON_UINT64_C2(0x952ab45c, 0xfa97a0b3),
                FASTJSON_UINT64_C2(0xde469fbd, 0x99a05fe3), FASTJSON_UINT64_C2(0xa59bc234, 0xdb398c25),
                FASTJSON_UINT64_C2(0xf6c69a72, 0xa3989f5c), FASTJSON_UINT64_C2(0xb7dcbf53, 0x54e9bece),
                FASTJSON_UINT64_C2(0x88fcf317, 0xf22241e2), FASTJSON_UINT64_C2(0xcc20ce9b, 0xd35c78a5),
                FASTJSON_UINT64_C2(0x98165af3, 0x7b2153df), FASTJSON_UINT64_C2(0xe2a0b5dc, 0x971f303a),
                FASTJSON_UINT64_C2(0xa8d9d153, 0x5ce3b396), FASTJSON_UINT64_C2(0xfb9b7cd9, 0xa4a7443c),
                FASTJSON_UINT64_C2(0xbb764c4c, 0xa7a44410), FASTJSON_UINT64_C2(0x8bab8eef, 0xb6409c1a),
                FASTJSON_UINT64_C2(0xd01fef10, 0xa657842c), FASTJSON_UINT64_C2(0x9b10a4e5, 0xe9913129),
                FASTJSON_UINT64_C2(0xe7109bfb, 0xa19c0c9d), FASTJSON_UINT64_C2(0xac2820d9, 0x623bf429),
                FASTJSON_UINT64_C2(0x80444b5e, 0x7aa7cf85), FASTJSON_UINT64_C2(0xbf21e440, 0x03acdd2d),
                FASTJSON_UINT64_C2(0x8e679c2f, 0x5e44ff8f), FASTJSON_UINT64_C2(0xd433179d, 0x9c8cb841),
                FASTJSON_UINT64_C2(0x9e19db92, 0xb4e31ba9), FASTJSON_UINT64_C2(0xeb96bf6e, 0xbadf77d9),
                FASTJSON_UINT64_C2(0xaf87023b, 0x9bf0ee6b)
            };
            static const int16_t kCachedPowers_E[] = {
                -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007,  -980,
                -954,  -927,  -901,  -874,  -847,  -821,  -794,  -768,  -741,  -715,
                -688,  -661,  -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,
                -422,  -396,  -369,  -343,  -316,  -289,  -263,  -236,  -210,  -183,
                -157,  -130,  -103,   -77,   -50,   -24,     3,    30,    56,    83,
                109,   136,   162,   189,   216,   242,   269,   295,   322,   348,
                375,   402,   428,   455,   481,   508,   534,   561,   588,   614,
                641,   667,   694,   720,   747,   774,   800,   827,   853,   880,
                907,   933,   960,   986,  1013,  1039,  1066
            };
            return DiyFp(kCachedPowers_F[index], kCachedPowers_E[index]);
        }

        inline DiyFp GetCachedPower(int e, int* K) {

            //int k = static_cast<int>(ceil((-61 - e) * 0.30102999566398114)) + 374;
            double dk = (-61 - e) * 0.30102999566398114 + 347;  // dk must be positive, so can do ceiling in positive
            int k = static_cast<int>(dk);
            if (dk - k > 0.0)
                k++;

            unsigned index = static_cast<unsigned>((k >> 3) + 1);
            *K = -(-348 + static_cast<int>(index << 3));    // decimal exponent no need lookup table

            return GetCachedPowerByIndex(index);
        }


        inline const char* GetDigitsLut() {
            static const char cDigitsLut[200] = {
                '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
                '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
                '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
                '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
                '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
                '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
                '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
                '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
                '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
                '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
            };
            return cDigitsLut;
        }

        inline char* u32toa(uint32_t value, char* buffer) {
            const char* cDigitsLut = GetDigitsLut();

            if (value < 10000) {
                const uint32_t d1 = (value / 100) << 1;
                const uint32_t d2 = (value % 100) << 1;

                if (value >= 1000)
                    *buffer++ = cDigitsLut[d1];
                if (value >= 100)
                    *buffer++ = cDigitsLut[d1 + 1];
                if (value >= 10)
                    *buffer++ = cDigitsLut[d2];
                *buffer++ = cDigitsLut[d2 + 1];
            }
            else if (value < 100000000) {
                // value = bbbbcccc
                const uint32_t b = value / 10000;
                const uint32_t c = value % 10000;

                const uint32_t d1 = (b / 100) << 1;
                const uint32_t d2 = (b % 100) << 1;

                const uint32_t d3 = (c / 100) << 1;
                const uint32_t d4 = (c % 100) << 1;

                if (value >= 10000000)
                    *buffer++ = cDigitsLut[d1];
                if (value >= 1000000)
                    *buffer++ = cDigitsLut[d1 + 1];
                if (value >= 100000)
                    *buffer++ = cDigitsLut[d2];
                *buffer++ = cDigitsLut[d2 + 1];

                *buffer++ = cDigitsLut[d3];
                *buffer++ = cDigitsLut[d3 + 1];
                *buffer++ = cDigitsLut[d4];
                *buffer++ = cDigitsLut[d4 + 1];
            }
            else {
                // value = aabbbbcccc in decimal

                const uint32_t a = value / 100000000; // 1 to 42
                value %= 100000000;

                if (a >= 10) {
                    const unsigned i = a << 1;
                    *buffer++ = cDigitsLut[i];
                    *buffer++ = cDigitsLut[i + 1];
                }
                else
                    *buffer++ = static_cast<char>('0' + static_cast<char>(a));

                const uint32_t b = value / 10000; // 0 to 9999
                const uint32_t c = value % 10000; // 0 to 9999

                const uint32_t d1 = (b / 100) << 1;
                const uint32_t d2 = (b % 100) << 1;

                const uint32_t d3 = (c / 100) << 1;
                const uint32_t d4 = (c % 100) << 1;

                *buffer++ = cDigitsLut[d1];
                *buffer++ = cDigitsLut[d1 + 1];
                *buffer++ = cDigitsLut[d2];
                *buffer++ = cDigitsLut[d2 + 1];
                *buffer++ = cDigitsLut[d3];
                *buffer++ = cDigitsLut[d3 + 1];
                *buffer++ = cDigitsLut[d4];
                *buffer++ = cDigitsLut[d4 + 1];
            }
            return buffer;
        }

        inline char* i32toa(int32_t value, char* buffer) {
            uint32_t u = static_cast<uint32_t>(value);
            if (value < 0) {
                *buffer++ = '-';
                u = ~u + 1;
            }

            return u32toa(u, buffer);
        }

        inline char* u64toa(uint64_t value, char* buffer) {
            const char* cDigitsLut = GetDigitsLut();
            const uint64_t  kTen8 = 100000000;
            const uint64_t  kTen9 = kTen8 * 10;
            const uint64_t kTen10 = kTen8 * 100;
            const uint64_t kTen11 = kTen8 * 1000;
            const uint64_t kTen12 = kTen8 * 10000;
            const uint64_t kTen13 = kTen8 * 100000;
            const uint64_t kTen14 = kTen8 * 1000000;
            const uint64_t kTen15 = kTen8 * 10000000;
            const uint64_t kTen16 = kTen8 * kTen8;

            if (value < kTen8) {
                uint32_t v = static_cast<uint32_t>(value);
                if (v < 10000) {
                    const uint32_t d1 = (v / 100) << 1;
                    const uint32_t d2 = (v % 100) << 1;

                    if (v >= 1000)
                        *buffer++ = cDigitsLut[d1];
                    if (v >= 100)
                        *buffer++ = cDigitsLut[d1 + 1];
                    if (v >= 10)
                        *buffer++ = cDigitsLut[d2];
                    *buffer++ = cDigitsLut[d2 + 1];
                }
                else {
                    // value = bbbbcccc
                    const uint32_t b = v / 10000;
                    const uint32_t c = v % 10000;

                    const uint32_t d1 = (b / 100) << 1;
                    const uint32_t d2 = (b % 100) << 1;

                    const uint32_t d3 = (c / 100) << 1;
                    const uint32_t d4 = (c % 100) << 1;

                    if (value >= 10000000)
                        *buffer++ = cDigitsLut[d1];
                    if (value >= 1000000)
                        *buffer++ = cDigitsLut[d1 + 1];
                    if (value >= 100000)
                        *buffer++ = cDigitsLut[d2];
                    *buffer++ = cDigitsLut[d2 + 1];

                    *buffer++ = cDigitsLut[d3];
                    *buffer++ = cDigitsLut[d3 + 1];
                    *buffer++ = cDigitsLut[d4];
                    *buffer++ = cDigitsLut[d4 + 1];
                }
            }
            else if (value < kTen16) {
                const uint32_t v0 = static_cast<uint32_t>(value / kTen8);
                const uint32_t v1 = static_cast<uint32_t>(value % kTen8);

                const uint32_t b0 = v0 / 10000;
                const uint32_t c0 = v0 % 10000;

                const uint32_t d1 = (b0 / 100) << 1;
                const uint32_t d2 = (b0 % 100) << 1;

                const uint32_t d3 = (c0 / 100) << 1;
                const uint32_t d4 = (c0 % 100) << 1;

                const uint32_t b1 = v1 / 10000;
                const uint32_t c1 = v1 % 10000;

                const uint32_t d5 = (b1 / 100) << 1;
                const uint32_t d6 = (b1 % 100) << 1;

                const uint32_t d7 = (c1 / 100) << 1;
                const uint32_t d8 = (c1 % 100) << 1;

                if (value >= kTen15)
                    *buffer++ = cDigitsLut[d1];
                if (value >= kTen14)
                    *buffer++ = cDigitsLut[d1 + 1];
                if (value >= kTen13)
                    *buffer++ = cDigitsLut[d2];
                if (value >= kTen12)
                    *buffer++ = cDigitsLut[d2 + 1];
                if (value >= kTen11)
                    *buffer++ = cDigitsLut[d3];
                if (value >= kTen10)
                    *buffer++ = cDigitsLut[d3 + 1];
                if (value >= kTen9)
                    *buffer++ = cDigitsLut[d4];
                if (value >= kTen8)
                    *buffer++ = cDigitsLut[d4 + 1];

                *buffer++ = cDigitsLut[d5];
                *buffer++ = cDigitsLut[d5 + 1];
                *buffer++ = cDigitsLut[d6];
                *buffer++ = cDigitsLut[d6 + 1];
                *buffer++ = cDigitsLut[d7];
                *buffer++ = cDigitsLut[d7 + 1];
                *buffer++ = cDigitsLut[d8];
                *buffer++ = cDigitsLut[d8 + 1];
            }
            else {
                const uint32_t a = static_cast<uint32_t>(value / kTen16); // 1 to 1844
                value %= kTen16;

                if (a < 10)
                    *buffer++ = static_cast<char>('0' + static_cast<char>(a));
                else if (a < 100) {
                    const uint32_t i = a << 1;
                    *buffer++ = cDigitsLut[i];
                    *buffer++ = cDigitsLut[i + 1];
                }
                else if (a < 1000) {
                    *buffer++ = static_cast<char>('0' + static_cast<char>(a / 100));

                    const uint32_t i = (a % 100) << 1;
                    *buffer++ = cDigitsLut[i];
                    *buffer++ = cDigitsLut[i + 1];
                }
                else {
                    const uint32_t i = (a / 100) << 1;
                    const uint32_t j = (a % 100) << 1;
                    *buffer++ = cDigitsLut[i];
                    *buffer++ = cDigitsLut[i + 1];
                    *buffer++ = cDigitsLut[j];
                    *buffer++ = cDigitsLut[j + 1];
                }

                const uint32_t v0 = static_cast<uint32_t>(value / kTen8);
                const uint32_t v1 = static_cast<uint32_t>(value % kTen8);

                const uint32_t b0 = v0 / 10000;
                const uint32_t c0 = v0 % 10000;

                const uint32_t d1 = (b0 / 100) << 1;
                const uint32_t d2 = (b0 % 100) << 1;

                const uint32_t d3 = (c0 / 100) << 1;
                const uint32_t d4 = (c0 % 100) << 1;

                const uint32_t b1 = v1 / 10000;
                const uint32_t c1 = v1 % 10000;

                const uint32_t d5 = (b1 / 100) << 1;
                const uint32_t d6 = (b1 % 100) << 1;

                const uint32_t d7 = (c1 / 100) << 1;
                const uint32_t d8 = (c1 % 100) << 1;

                *buffer++ = cDigitsLut[d1];
                *buffer++ = cDigitsLut[d1 + 1];
                *buffer++ = cDigitsLut[d2];
                *buffer++ = cDigitsLut[d2 + 1];
                *buffer++ = cDigitsLut[d3];
                *buffer++ = cDigitsLut[d3 + 1];
                *buffer++ = cDigitsLut[d4];
                *buffer++ = cDigitsLut[d4 + 1];
                *buffer++ = cDigitsLut[d5];
                *buffer++ = cDigitsLut[d5 + 1];
                *buffer++ = cDigitsLut[d6];
                *buffer++ = cDigitsLut[d6 + 1];
                *buffer++ = cDigitsLut[d7];
                *buffer++ = cDigitsLut[d7 + 1];
                *buffer++ = cDigitsLut[d8];
                *buffer++ = cDigitsLut[d8 + 1];
            }

            return buffer;
        }

        inline char* i64toa(int64_t value, char* buffer) {
            uint64_t u = static_cast<uint64_t>(value);
            if (value < 0) {
                *buffer++ = '-';
                u = ~u + 1;
            }

            return u64toa(u, buffer);
        }

        inline char* WriteExponent(int K, char* buffer) {
            if (K < 0) {
                *buffer++ = '-';
                K = -K;
            }

            if (K >= 100) {
                *buffer++ = static_cast<char>('0' + static_cast<char>(K / 100));
                K %= 100;
                const char* d = GetDigitsLut() + K * 2;
                *buffer++ = d[0];
                *buffer++ = d[1];
            }
            else if (K >= 10) {
                const char* d = GetDigitsLut() + K * 2;
                *buffer++ = d[0];
                *buffer++ = d[1];
            }
            else
                *buffer++ = static_cast<char>('0' + static_cast<char>(K));

            return buffer;
        }


        inline char* Prettify(char* buffer, int length, int k, int maxDecimalPlaces) {
            const int kk = length + k;  // 10^(kk-1) <= v < 10^kk

            if (0 <= k && kk <= 21) {
                // 1234e7 -> 12340000000
                for (int i = length; i < kk; i++)
                    buffer[i] = '0';
                buffer[kk] = '.';
                buffer[kk + 1] = '0';
                return &buffer[kk + 2];
            }
            else if (0 < kk && kk <= 21) {
                // 1234e-2 -> 12.34
                std::memmove(&buffer[kk + 1], &buffer[kk], static_cast<size_t>(length - kk));
                buffer[kk] = '.';
                if (0 > k + maxDecimalPlaces) {
                    // When maxDecimalPlaces = 2, 1.2345 -> 1.23, 1.102 -> 1.1
                    // Remove extra trailing zeros (at least one) after truncation.
                    for (int i = kk + maxDecimalPlaces; i > kk + 1; i--)
                        if (buffer[i] != '0')
                            return &buffer[i + 1];
                    return &buffer[kk + 2]; // Reserve one zero
                }
                else
                    return &buffer[length + 1];
            }
            else if (-6 < kk && kk <= 0) {
                // 1234e-6 -> 0.001234
                const int offset = 2 - kk;
                std::memmove(&buffer[offset], &buffer[0], static_cast<size_t>(length));
                buffer[0] = '0';
                buffer[1] = '.';
                for (int i = 2; i < offset; i++)
                    buffer[i] = '0';
                if (length - kk > maxDecimalPlaces) {
                    // When maxDecimalPlaces = 2, 0.123 -> 0.12, 0.102 -> 0.1
                    // Remove extra trailing zeros (at least one) after truncation.
                    for (int i = maxDecimalPlaces + 1; i > 2; i--)
                        if (buffer[i] != '0')
                            return &buffer[i + 1];
                    return &buffer[3]; // Reserve one zero
                }
                else
                    return &buffer[length + offset];
            }
            else if (kk < -maxDecimalPlaces) {
                // Truncate to zero
                buffer[0] = '0';
                buffer[1] = '.';
                buffer[2] = '0';
                return &buffer[3];
            }
            else if (length == 1) {
                // 1e30
                buffer[1] = 'e';
                return WriteExponent(kk - 1, &buffer[2]);
            }
            else {
                // 1234e30 -> 1.234e33
                std::memmove(&buffer[2], &buffer[1], static_cast<size_t>(length - 1));
                buffer[1] = '.';
                buffer[length + 1] = 'e';
                return WriteExponent(kk - 1, &buffer[0 + length + 2]);
            }
        }

        inline void GrisuRound(char* buffer, int len, uint64_t delta, uint64_t rest, uint64_t ten_kappa, uint64_t wp_w) {
            while (rest < wp_w && delta - rest >= ten_kappa &&
                (rest + ten_kappa < wp_w ||  /// closer
                    wp_w - rest > rest + ten_kappa - wp_w)) {
                buffer[len - 1]--;
                rest += ten_kappa;
            }
        }

        inline unsigned CountDecimalDigit32(uint32_t n) {
            // Simple pure C++ implementation was faster than __builtin_clz version in this situation.
            if (n < 10) return 1;
            if (n < 100) return 2;
            if (n < 1000) return 3;
            if (n < 10000) return 4;
            if (n < 100000) return 5;
            if (n < 1000000) return 6;
            if (n < 10000000) return 7;
            if (n < 100000000) return 8;
            // Will not reach 10 digits in DigitGen()
            //if (n < 1000000000) return 9;
            //return 10;
            return 9;
        }

        inline void DigitGen(const DiyFp& W, const DiyFp& Mp, uint64_t delta, char* buffer, int* len, int* K) {
            static const uint32_t kPow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };
            const DiyFp one(uint64_t(1) << -Mp.e, Mp.e);
            const DiyFp wp_w = Mp - W;
            uint32_t p1 = static_cast<uint32_t>(Mp.f >> -one.e);
            uint64_t p2 = Mp.f & (one.f - 1);
            unsigned kappa = CountDecimalDigit32(p1); // kappa in [0, 9]
            *len = 0;

            while (kappa > 0) {
                uint32_t d = 0;
                switch (kappa) {
                case  9: d = p1 / 100000000; p1 %= 100000000; break;
                case  8: d = p1 / 10000000; p1 %= 10000000; break;
                case  7: d = p1 / 1000000; p1 %= 1000000; break;
                case  6: d = p1 / 100000; p1 %= 100000; break;
                case  5: d = p1 / 10000; p1 %= 10000; break;
                case  4: d = p1 / 1000; p1 %= 1000; break;
                case  3: d = p1 / 100; p1 %= 100; break;
                case  2: d = p1 / 10; p1 %= 10; break;
                case  1: d = p1;              p1 = 0; break;
                default:;
                }
                if (d || *len)
                    buffer[(*len)++] = static_cast<char>('0' + static_cast<char>(d));
                kappa--;
                uint64_t tmp = (static_cast<uint64_t>(p1) << -one.e) + p2;
                if (tmp <= delta) {
                    *K += kappa;
                    GrisuRound(buffer, *len, delta, tmp, static_cast<uint64_t>(kPow10[kappa]) << -one.e, wp_w.f);
                    return;
                }
            }

            // kappa = 0
            for (;;) {
                p2 *= 10;
                delta *= 10;
                char d = static_cast<char>(p2 >> -one.e);
                if (d || *len)
                    buffer[(*len)++] = static_cast<char>('0' + d);
                p2 &= one.f - 1;
                kappa--;
                if (p2 < delta) {
                    *K += kappa;
                    int index = -static_cast<int>(kappa);
                    GrisuRound(buffer, *len, delta, p2, one.f, wp_w.f * (index < 9 ? kPow10[-static_cast<int>(kappa)] : 0));
                    return;
                }
            }
        }

        inline void Grisu2(double value, char* buffer, int* length, int* K) {
            const DiyFp v(value);
            DiyFp w_m, w_p;
            v.NormalizedBoundaries(&w_m, &w_p);

            const DiyFp c_mk = GetCachedPower(w_p.e, K);
            const DiyFp W = v.Normalize() * c_mk;
            DiyFp Wp = w_p * c_mk;
            DiyFp Wm = w_m * c_mk;
            Wm.f++;
            Wp.f--;
            DigitGen(W, Wp, Wp.f - Wm.f, buffer, length, K);
        }

        inline char* dtoa(double value, char* buffer, int maxDecimalPlaces = 324) {
            assert(maxDecimalPlaces >= 1);
            Double d(value);
            if (d.IsZero()) {
                if (d.Sign())
                    *buffer++ = '-';     // -0.0, Issue #289
                buffer[0] = '0';
                buffer[1] = '.';
                buffer[2] = '0';
                return &buffer[3];
            }
            else {
                if (value < 0) {
                    *buffer++ = '-';
                    value = -value;
                }
                int length, K;
                Grisu2(value, buffer, &length, &K);
                return Prettify(buffer, length, K, maxDecimalPlaces);
            }
        }

    } // namespace internal

    GenericWriter::GenericWriter(std::string& str) :_str(str) {
    }

    void GenericWriter::Null() {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        _str.append("null");
        vt.first = kValueType;
    }

    void GenericWriter::Bool(bool b) {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        if (b) {
            _str.append("true");
        }
        else {
            _str.append("false");
        }
        vt.first = kValueType;
    }

    void GenericWriter::Int(int32_t i) {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        char buffer[11];
        const char* end = internal::i32toa(i, buffer);
        _str.append(buffer, end - buffer);
        vt.first = kValueType;
    }

    void GenericWriter::Uint(uint32_t u) {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        char buffer[11];
        const char* end = internal::u32toa(u, buffer);
        _str.append(buffer, end - buffer);
        vt.first = kValueType;
    }

    void GenericWriter::Int64(int64_t i64) {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        char buffer[21];
        const char* end = internal::i64toa(i64, buffer);
        _str.append(buffer, end - buffer);
        vt.first = kValueType;
    }

    void GenericWriter::Uint64(uint64_t u64) {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        char buffer[21];
        const char* end = internal::i64toa(u64, buffer);
        _str.append(buffer, end - buffer);
        vt.first = kValueType;
    }

    void GenericWriter::Double(double d) {
        if (internal::Double(d).IsNanOrInf()) {
            if (internal::Double(d).IsNan()) {
                _str.append(":NaN");
                return;
            }
            _str.append(1, ':');
            if (internal::Double(d).Sign()) {
                _str.append(1, '-');
            }
            _str.append("Infinity");
            return;
        }

        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        } else if (vt.first == kkeyType) {
            _str.append(1, ':');
        } else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        char buffer[25];
        char* end = internal::dtoa(d, buffer);
        _str.append(buffer, end - buffer);
    }

    void GenericWriter::Key(const char* szKey) {
        value_type& vt = _stack.top();
        if (vt.second) {
            _str.append(1, ',');
        }
        _str.append(1, '"').append(szKey).append(1, '\"');
        vt.second++;
        vt.first = kkeyType;
    }

    void GenericWriter::String(const char* szValue) {
        value_type& vt = _stack.top();
        if (vt.first == kNullType) {
            ;
        }
        else if (vt.first == kkeyType) {
            _str.append(1, ':');
        }
        else if (vt.first == kValueType) {
            _str.append(1, ',');
        }
        _str.append(1, '\"');
        for (const char* ptr = szValue; *ptr; ++ptr) {
            if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') {
                _str.append(1, *ptr);
            } else {
                _str.append(1, '\\');
                switch (*ptr) {
                    case '\\':    _str.append(1, '\\');    break;
                    case '\"':    _str.append(1, '\"');    break;
                    case '\b':    _str.append(1, 'b');    break;
                    case '\f':    _str.append(1, 'f');    break;
                    case '\n':    _str.append(1, 'n');    break;
                    case '\r':    _str.append(1, 'r');    break;
                    case '\t':    _str.append(1, 't');    break;
                    default: {
                        size_t nSize = _str.size();
                        _str.resize(nSize + 5, '\0');
                        snprintf(&_str.at(nSize), 5, "u%04x", *ptr);
                        break;
                    }
                }
            }
        }
        _str.append(1, '\"');
        vt.first = kValueType;
    }

    void GenericWriter::StartObject() {
        if (!_stack.empty() && _stack.top().first == kkeyType) {
            _str.append(1, ':');
        }
        _stack.push(value_type(kNullType, 0));
        _str.append(1, '{');
    }

    void GenericWriter::EndObject() {
        _str.append(1, '}');
        _stack.pop();
    }

    void GenericWriter::StartArray() {
        _stack.push(value_type(kNullType, 0));
        _str.append(":[");
    }

    void GenericWriter::EndArray() {
        _str.append(1, ']');
        _stack.pop();
    }

    void GenericWriter::Separation() {
        _str.append(1, ',');
    }

    bool GenericWriter::result() const {
        return _stack.empty();
    }

    /*------------------------------------------------------------------------------*/
    JSONEncoder::JSONEncoder(std::string& str) : _writer(str) {
    }

    JSONEncoder::~JSONEncoder() {
    }

    void JSONEncoder::encodeValue(const char* sz, bool value, bool* pHas) {
        _writer.Key(sz);
        _writer.Bool(value);
    }

    void JSONEncoder::encodeValue(const char* sz, uint32_t value, bool* pHas) {
        _writer.Key(sz);
        _writer.Uint(value);
    }

    void JSONEncoder::encodeValue(const char* sz, int32_t value, bool* pHas) {
        _writer.Key(sz);
        _writer.Int(value);
    }

    void JSONEncoder::encodeValue(const char* sz, uint64_t value, bool* pHas) {
        _writer.Key(sz);
        _writer.Uint64(value);
    }

    void JSONEncoder::encodeValue(const char* sz, int64_t value, bool* pHas) {
        _writer.Key(sz);
        _writer.Int64(value);
    }

    void JSONEncoder::encodeValue(const char* sz, float value, bool* pHas) {
        _writer.Key(sz);
        _writer.Double(value);
    }

    void JSONEncoder::encodeValue(const char* sz, double value, bool* pHas) {
        _writer.Key(sz);
        _writer.Double(value);
    }

    void JSONEncoder::encodeValue(const char* sz, const std::string& value, bool* pHas) {
        _writer.Key(sz);
        _writer.String(value.c_str());
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<bool>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Bool(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<uint32_t>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Uint(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<int32_t>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Int(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<uint64_t>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Uint64(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<int64_t>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Int64(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<float>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Double(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<double>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.Double(value.at(i));
        }
        _writer.EndArray();
    }

    void JSONEncoder::encodeValue(const char* sz, const std::vector<std::string>& value, bool* pHas) {
        _writer.Key(sz);
        _writer.StartArray();
        int32_t count = (int32_t)value.size();
        for (int32_t i = 0; i < count; i++) {
            _writer.String(value.at(i).c_str());
        }
        _writer.EndArray();
    }

    void JSONEncoder::StartObject(const char* sz) {
        if (sz)
            _writer.Key(sz);
        _writer.StartObject();
    }

    void JSONEncoder::EndObject() {
        _writer.EndObject();
    }

    void JSONEncoder::StartArray(const char* sz) {
        if (sz)
            _writer.Key(sz);
        _writer.StartArray();
    }

    void JSONEncoder::EndArray() {
        _writer.EndArray();
    }

}
