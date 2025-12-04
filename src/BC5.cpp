//-------------------------------------------------------------------------------------
// BC4BC5.cpp
//
// Block-compression (BC) functionality for BC4 and BC5 (DirectX 10 texture compression)
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "BC.h"

#define BLOCK_LEN            4
#define BLOCK_SIZE           (BLOCK_LEN * BLOCK_LEN)
#define NUM_PIXELS_PER_BLOCK 16

struct BC4_UNORM {
	float DecodeFromIndex (size_t uIndex) const noexcept {
		if (uIndex == 0) return float (red_0) / 255.0f;
		if (uIndex == 1) return float (red_1) / 255.0f;
		const float fred_0 = float (red_0) / 255.0f;
		const float fred_1 = float (red_1) / 255.0f;
		if (red_0 > red_1) {
			uIndex -= 1;
			return (fred_0 * float (7u - uIndex) + fred_1 * float (uIndex)) / 7.0f;
		} else {
			if (uIndex == 6) return 0.0f;
			if (uIndex == 7) return 1.0f;
			uIndex -= 1;
			return (fred_0 * float (5u - uIndex) + fred_1 * float (uIndex)) / 5.0f;
		}
	}

	void SetIndex (size_t uOffset, size_t uIndex) noexcept {
		data &= ~(uint64_t (0x07) << (3 * uOffset + 16));
		data |= (uint64_t (uIndex) << (3 * uOffset + 16));
	}

	union {
		struct {
			uint8_t red_0;
			uint8_t red_1;
			uint8_t indices[6];
		};
		uint64_t data;
	};
};

void
FindEndPointsBC4U (const float theTexelsU[], uint8_t &endpointU_0, uint8_t &endpointU_1) noexcept {
	// The boundary of codec for signed/unsigned format
	constexpr float MIN_NORM = 0.f;
	constexpr float MAX_NORM = 1.f;

	// Find max/min of input texels
	float fBlockMax = theTexelsU[0];
	float fBlockMin = theTexelsU[0];
	for (size_t i = 0; i < BLOCK_SIZE; ++i)
		if (theTexelsU[i] < fBlockMin) fBlockMin = theTexelsU[i];
		else if (theTexelsU[i] > fBlockMax) fBlockMax = theTexelsU[i];

	//  If there are boundary values in input texels, should use 4 interpolated color values to guarantee
	//  the exact code of the boundary values.
	const bool bUsing4BlockCodec = (MIN_NORM == fBlockMin || MAX_NORM == fBlockMax);

	// Using Optimize
	float fStart, fEnd;

	if (!bUsing4BlockCodec) {
		// 6 interpolated color values
		OptimizeAlpha<false> (&fStart, &fEnd, theTexelsU, 8);

		auto iStart = static_cast<uint8_t> (fStart * 255.0f);
		auto iEnd   = static_cast<uint8_t> (fEnd * 255.0f);

		endpointU_0 = iEnd;
		endpointU_1 = iStart;
	} else {
		// 4 interpolated color values
		OptimizeAlpha<false> (&fStart, &fEnd, theTexelsU, 6);

		auto iStart = static_cast<uint8_t> (fStart * 255.0f);
		auto iEnd   = static_cast<uint8_t> (fEnd * 255.0f);

		endpointU_1 = iEnd;
		endpointU_0 = iStart;
	}
}

inline void
FindEndPointsBC5U (const float theTexelsU[], const float theTexelsV[], uint8_t &endpointU_0, uint8_t &endpointU_1, uint8_t &endpointV_0,
                   uint8_t &endpointV_1) noexcept {
	// Encoding the U and V channel by BC4 codec separately.
	FindEndPointsBC4U (theTexelsU, endpointU_0, endpointU_1);
	FindEndPointsBC4U (theTexelsV, endpointV_0, endpointV_1);
}

void
FindClosestUNORM (BC4_UNORM *pBC, const float theTexelsU[]) noexcept {
	float rGradient[8];
	for (size_t i = 0; i < 8; ++i)
		rGradient[i] = pBC->DecodeFromIndex (i);

	for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i) {
		size_t uBestIndex = 0;
		float fBestDelta  = 100000;
		for (size_t uIndex = 0; uIndex < 8; uIndex++) {
			const float fCurrentDelta = fabsf (rGradient[uIndex] - theTexelsU[i]);
			if (fCurrentDelta < fBestDelta) {
				uBestIndex = uIndex;
				fBestDelta = fCurrentDelta;
			}
		}
		pBC->SetIndex (i, uBestIndex);
	}
}

void
D3DXEncodeBC5U (u8 *pBC, const XMFLOAT2 *pColor) noexcept {
	assert (pBC && pColor);
	static_assert (sizeof (BC4_UNORM) == 8, "BC4_UNORM should be 8 bytes");

	memset (pBC, 0, sizeof (BC4_UNORM) * 2);
	auto pBCR = reinterpret_cast<BC4_UNORM *> (pBC);
	auto pBCG = reinterpret_cast<BC4_UNORM *> (pBC + sizeof (BC4_UNORM));
	float theTexelsU[NUM_PIXELS_PER_BLOCK];
	float theTexelsV[NUM_PIXELS_PER_BLOCK];

	for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i) {
		theTexelsU[i] = pColor[i].x;
		theTexelsV[i] = pColor[i].y;
	}

	FindEndPointsBC5U (theTexelsU, theTexelsV, pBCR->red_0, pBCR->red_1, pBCG->red_0, pBCG->red_1);

	FindClosestUNORM (pBCR, theTexelsU);
	FindClosestUNORM (pBCG, theTexelsV);
}
