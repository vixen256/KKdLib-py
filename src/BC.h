//-------------------------------------------------------------------------------------
// BC.h
//
// Block-compression (BC) functionality
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "helpers.h"

#define NUM_PIXELS_PER_BLOCK 16

struct alignas (16) XMFLOAT2 {
	f32 x;
	f32 y;
};

constexpr int32_t BC67_WEIGHT_MAX    = 64;
constexpr uint32_t BC67_WEIGHT_SHIFT = 6;
constexpr int32_t BC67_WEIGHT_ROUND  = 32;

const int g_aWeights2[] = {0, 21, 43, 64};
const int g_aWeights3[] = {0, 9, 18, 27, 37, 46, 55, 64};
const int g_aWeights4[] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

class LDRColorA {
public:
	uint8_t r, g, b, a;

	const uint8_t &operator[] (size_t uElement) const noexcept {
		switch (uElement) {
		case 0: return r;
		case 1: return g;
		case 2: return b;
		case 3: return a;
		default: assert (false); return r;
		}
	}

	uint8_t &operator[] (size_t uElement) noexcept {
		switch (uElement) {
		case 0: return r;
		case 1: return g;
		case 2: return b;
		case 3: return a;
		default: assert (false); return r;
		}
	}

	static void InterpolateRGB (const LDRColorA &c0, const LDRColorA &c1, size_t wc, size_t wcprec, LDRColorA &out) noexcept {
		const int *aWeights = nullptr;
		switch (wcprec) {
		case 2:
			aWeights = g_aWeights2;
			assert (wc < 4);
			break;
		case 3:
			aWeights = g_aWeights3;
			assert (wc < 8);
			break;
		case 4:
			aWeights = g_aWeights4;
			assert (wc < 16);
			break;
		default:
			assert (false);
			out.r = out.g = out.b = 0;
			return;
		}
		out.r =
		    uint8_t ((uint32_t (c0.r) * uint32_t (BC67_WEIGHT_MAX - aWeights[wc]) + uint32_t (c1.r) * uint32_t (aWeights[wc]) + BC67_WEIGHT_ROUND) >>
		             BC67_WEIGHT_SHIFT);
		out.g =
		    uint8_t ((uint32_t (c0.g) * uint32_t (BC67_WEIGHT_MAX - aWeights[wc]) + uint32_t (c1.g) * uint32_t (aWeights[wc]) + BC67_WEIGHT_ROUND) >>
		             BC67_WEIGHT_SHIFT);
		out.b =
		    uint8_t ((uint32_t (c0.b) * uint32_t (BC67_WEIGHT_MAX - aWeights[wc]) + uint32_t (c1.b) * uint32_t (aWeights[wc]) + BC67_WEIGHT_ROUND) >>
		             BC67_WEIGHT_SHIFT);
	}

	static void InterpolateA (const LDRColorA &c0, const LDRColorA &c1, size_t wa, size_t waprec, LDRColorA &out) noexcept {
		const int *aWeights = nullptr;
		switch (waprec) {
		case 2:
			aWeights = g_aWeights2;
			assert (wa < 4);
			break;
		case 3:
			aWeights = g_aWeights3;
			assert (wa < 8);
			break;
		case 4:
			aWeights = g_aWeights4;
			assert (wa < 16);
			break;
		default:
			assert (false);
			out.a = 0;
			return;
		}
		out.a =
		    uint8_t ((uint32_t (c0.a) * uint32_t (BC67_WEIGHT_MAX - aWeights[wa]) + uint32_t (c1.a) * uint32_t (aWeights[wa]) + BC67_WEIGHT_ROUND) >>
		             BC67_WEIGHT_SHIFT);
	}

	static void Interpolate (const LDRColorA &c0, const LDRColorA &c1, size_t wc, size_t wa, size_t wcprec, size_t waprec, LDRColorA &out) noexcept {
		InterpolateRGB (c0, c1, wc, wcprec, out);
		InterpolateA (c0, c1, wa, waprec, out);
	}
};

struct alignas (16) HDRColorA {
	f32 r;
	f32 g;
	f32 b;
	f32 a;

	HDRColorA () = default;
	HDRColorA (float _r, float _g, float _b, float _a) noexcept : r (_r), g (_g), b (_b), a (_a) {}

	HDRColorA (HDRColorA const &)            = default;
	HDRColorA &operator= (const HDRColorA &) = default;

	HDRColorA (HDRColorA &&)            = default;
	HDRColorA &operator= (HDRColorA &&) = default;

	HDRColorA (LDRColorA c) {
		r = c.r;
		g = c.g;
		b = c.b;
		a = c.a;
	}

	HDRColorA operator+ (const HDRColorA &c) const noexcept { return HDRColorA (r + c.r, g + c.g, b + c.b, a + c.a); }

	HDRColorA operator- (const HDRColorA &c) const noexcept { return HDRColorA (r - c.r, g - c.g, b - c.b, a - c.a); }

	HDRColorA operator* (float f) const noexcept { return HDRColorA (r * f, g * f, b * f, a * f); }

	HDRColorA operator/ (float f) const noexcept {
		const float fInv = 1.0f / f;
		return HDRColorA (r * fInv, g * fInv, b * fInv, a * fInv);
	}

	float operator* (const HDRColorA &c) const noexcept { return r * c.r + g * c.g + b * c.b + a * c.a; }

	HDRColorA &operator+= (const HDRColorA &c) noexcept {
		r += c.r;
		g += c.g;
		b += c.b;
		a += c.a;
		return *this;
	}

	HDRColorA &operator-= (const HDRColorA &c) noexcept {
		r -= c.r;
		g -= c.g;
		b -= c.b;
		a -= c.a;
		return *this;
	}

	HDRColorA &operator*= (float f) noexcept {
		r *= f;
		g *= f;
		b *= f;
		a *= f;
		return *this;
	}

	HDRColorA &operator/= (float f) noexcept {
		const float fInv = 1.0f / f;
		r *= fInv;
		g *= fInv;
		b *= fInv;
		a *= fInv;
		return *this;
	}

	HDRColorA &Clamp (_In_ float fMin, _In_ float fMax) noexcept {
		r = std::min<float> (fMax, std::max<float> (fMin, r));
		g = std::min<float> (fMax, std::max<float> (fMin, g));
		b = std::min<float> (fMax, std::max<float> (fMin, b));
		a = std::min<float> (fMax, std::max<float> (fMin, a));
		return *this;
	}

	LDRColorA ToLDRColorA () const noexcept;
};

inline LDRColorA
HDRColorA::ToLDRColorA () const noexcept {
	return LDRColorA (static_cast<uint8_t> (r + 0.01f), static_cast<uint8_t> (g + 0.01f), static_cast<uint8_t> (b + 0.01f),
	                  static_cast<uint8_t> (a + 0.01f));
}

inline HDRColorA *
HDRColorALerp (HDRColorA *pOut, const HDRColorA *pC1, const HDRColorA *pC2, f32 s) noexcept {
	pOut->r = pC1->r + s * (pC2->r - pC1->r);
	pOut->g = pC1->g + s * (pC2->g - pC1->g);
	pOut->b = pC1->b + s * (pC2->b - pC1->b);
	pOut->a = pC1->a + s * (pC2->a - pC1->a);
	return pOut;
}

enum BC_FLAGS : u32 {
	BC_FLAGS_NONE = 0x0,

	BC_FLAGS_DITHER_RGB = 0x10000,
	// Enables dithering for RGB colors for BC1-3

	BC_FLAGS_DITHER_A = 0x20000,
	// Enables dithering for Alpha channel for BC1-3

	BC_FLAGS_UNIFORM = 0x40000,
	// By default, uses perceptual weighting for BC1-3; this flag makes it a uniform weighting

	BC_FLAGS_USE_3SUBSETS = 0x80000,
	// By default, BC7 skips mode 0 & 2; this flag adds those modes back

	BC_FLAGS_FORCE_BC7_MODE6 = 0x100000,
	// BC7 should only use mode 6; skip other modes
};

template <bool bRange>
void
OptimizeAlpha (float *pX, float *pY, const float *pPoints, uint32_t cSteps) noexcept {
	static const float pC6[] = {5.0f / 5.0f, 4.0f / 5.0f, 3.0f / 5.0f, 2.0f / 5.0f, 1.0f / 5.0f, 0.0f / 5.0f};
	static const float pD6[] = {0.0f / 5.0f, 1.0f / 5.0f, 2.0f / 5.0f, 3.0f / 5.0f, 4.0f / 5.0f, 5.0f / 5.0f};
	static const float pC8[] = {7.0f / 7.0f, 6.0f / 7.0f, 5.0f / 7.0f, 4.0f / 7.0f, 3.0f / 7.0f, 2.0f / 7.0f, 1.0f / 7.0f, 0.0f / 7.0f};
	static const float pD8[] = {0.0f / 7.0f, 1.0f / 7.0f, 2.0f / 7.0f, 3.0f / 7.0f, 4.0f / 7.0f, 5.0f / 7.0f, 6.0f / 7.0f, 7.0f / 7.0f};

	const float *pC = (6 == cSteps) ? pC6 : pC8;
	const float *pD = (6 == cSteps) ? pD6 : pD8;

	constexpr float MAX_VALUE = 1.0f;
	constexpr float MIN_VALUE = (bRange) ? -1.0f : 0.0f;

	// Find Min and Max points, as starting point
	float fX = MAX_VALUE;
	float fY = MIN_VALUE;

	if (8 == cSteps) {
		for (size_t iPoint = 0; iPoint < NUM_PIXELS_PER_BLOCK; iPoint++) {
			if (pPoints[iPoint] < fX) fX = pPoints[iPoint];

			if (pPoints[iPoint] > fY) fY = pPoints[iPoint];
		}
	} else {
		for (size_t iPoint = 0; iPoint < NUM_PIXELS_PER_BLOCK; iPoint++) {
			if (pPoints[iPoint] < fX && pPoints[iPoint] > MIN_VALUE) fX = pPoints[iPoint];

			if (pPoints[iPoint] > fY && pPoints[iPoint] < MAX_VALUE) fY = pPoints[iPoint];
		}

		if (fX == fY) fY = MAX_VALUE;
	}

	// Use Newton's Method to find local minima of sum-of-squares error.
	const auto fSteps = static_cast<float> (cSteps - 1);

	for (size_t iIteration = 0; iIteration < 8; iIteration++) {
		if ((fY - fX) < (1.0f / 256.0f)) break;

		float const fScale = fSteps / (fY - fX);

		// Calculate new steps
		float pSteps[8];

		for (size_t iStep = 0; iStep < cSteps; iStep++)
			pSteps[iStep] = pC[iStep] * fX + pD[iStep] * fY;

		if (6 == cSteps) {
			pSteps[6] = MIN_VALUE;
			pSteps[7] = MAX_VALUE;
		}

		// Evaluate function, and derivatives
		float dX  = 0.0f;
		float dY  = 0.0f;
		float d2X = 0.0f;
		float d2Y = 0.0f;

		for (size_t iPoint = 0; iPoint < NUM_PIXELS_PER_BLOCK; iPoint++) {
			const float fDot = (pPoints[iPoint] - fX) * fScale;

			uint32_t iStep;
			if (fDot <= 0.0f) {
				// D3DX10 / D3DX11 didn't take into account the proper minimum value for the bRange (BC4S/BC5S) case
				iStep = ((6 == cSteps) && (pPoints[iPoint] <= (fX + MIN_VALUE) * 0.5f)) ? 6u : 0u;
			} else if (fDot >= fSteps) {
				iStep = ((6 == cSteps) && (pPoints[iPoint] >= (fY + MAX_VALUE) * 0.5f)) ? 7u : (cSteps - 1);
			} else {
				iStep = uint32_t (fDot + 0.5f);
			}

			if (iStep < cSteps) {
				// D3DX had this computation backwards (pPoints[iPoint] - pSteps[iStep])
				// this fix improves RMS of the alpha component
				const float fDiff = pSteps[iStep] - pPoints[iPoint];

				dX += pC[iStep] * fDiff;
				d2X += pC[iStep] * pC[iStep];

				dY += pD[iStep] * fDiff;
				d2Y += pD[iStep] * pD[iStep];
			}
		}

		// Move endpoints
		if (d2X > 0.0f) fX -= dX / d2X;

		if (d2Y > 0.0f) fY -= dY / d2Y;

		if (fX > fY) {
			const float f = fX;
			fX            = fY;
			fY            = f;
		}

		if ((dX * dX < (1.0f / 64.0f)) && (dY * dY < (1.0f / 64.0f))) break;
	}

	*pX = (fX < MIN_VALUE) ? MIN_VALUE : (fX > MAX_VALUE) ? MAX_VALUE : fX;
	*pY = (fY < MIN_VALUE) ? MIN_VALUE : (fY > MAX_VALUE) ? MAX_VALUE : fY;
}

void D3DXEncodeBC3 (u8 *pBC, const HDRColorA *pColor, uint32_t flags) noexcept;
void D3DXEncodeBC5U (u8 *pBC, const XMFLOAT2 *pColor) noexcept;
void D3DXEncodeBC7 (u8 *pBC, const HDRColorA *pColor, uint32_t flags) noexcept;
