//-------------------------------------------------------------------------------------
// BC6HBC7.cpp
//
// Block-compression (BC) functionality for BC6H and BC7 (DirectX 11 texture compression)
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//-------------------------------------------------------------------------------------

#include "BC.h"

#define BC7_MAX_REGIONS 3
#define BC7_MAX_INDICES 16

constexpr size_t BC7_NUM_CHANNELS = 4;
constexpr size_t BC7_MAX_SHAPES   = 64;

constexpr float fEpsilon = (0.25f / 64.0f) * (0.25f / 64.0f);
constexpr float pC3[]    = {2.0f / 2.0f, 1.0f / 2.0f, 0.0f / 2.0f};
constexpr float pD3[]    = {0.0f / 2.0f, 1.0f / 2.0f, 2.0f / 2.0f};
constexpr float pC4[]    = {3.0f / 3.0f, 2.0f / 3.0f, 1.0f / 3.0f, 0.0f / 3.0f};
constexpr float pD4[]    = {0.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f, 3.0f / 3.0f};

const uint8_t g_aPartitionTable[3][64][16] = {{// 1 Region case has no subsets (all 0)
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},

                                              {
                                                  // BC6H/BC7 Partition Set for 2 Subsets
                                                  {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}, // Shape 0
                                                  {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}, // Shape 1
                                                  {0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1}, // Shape 2
                                                  {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1}, // Shape 3
                                                  {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1}, // Shape 4
                                                  {0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1}, // Shape 5
                                                  {0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1}, // Shape 6
                                                  {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1}, // Shape 7
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1}, // Shape 8
                                                  {0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Shape 9
                                                  {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1}, // Shape 10
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1}, // Shape 11
                                                  {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Shape 12
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}, // Shape 13
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, // Shape 14
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1}, // Shape 15
                                                  {0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1}, // Shape 16
                                                  {0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // Shape 17
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0}, // Shape 18
                                                  {0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0}, // Shape 19
                                                  {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, // Shape 20
                                                  {0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0}, // Shape 21
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, // Shape 22
                                                  {0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1}, // Shape 23
                                                  {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0}, // Shape 24
                                                  {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, // Shape 25
                                                  {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0}, // Shape 26
                                                  {0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0}, // Shape 27
                                                  {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0}, // Shape 28
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0}, // Shape 29
                                                  {0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0}, // Shape 30
                                                  {0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0}, // Shape 31

                                                  // BC7 Partition Set for 2 Subsets (second-half)
                                                  {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}, // Shape 32
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1}, // Shape 33
                                                  {0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0}, // Shape 34
                                                  {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0}, // Shape 35
                                                  {0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0}, // Shape 36
                                                  {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0}, // Shape 37
                                                  {0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1}, // Shape 38
                                                  {0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1}, // Shape 39
                                                  {0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0}, // Shape 40
                                                  {0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0}, // Shape 41
                                                  {0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0}, // Shape 42
                                                  {0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0}, // Shape 43
                                                  {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0}, // Shape 44
                                                  {0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1}, // Shape 45
                                                  {0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1}, // Shape 46
                                                  {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0}, // Shape 47
                                                  {0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0}, // Shape 48
                                                  {0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0}, // Shape 49
                                                  {0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0}, // Shape 50
                                                  {0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0}, // Shape 51
                                                  {0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1}, // Shape 52
                                                  {0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1}, // Shape 53
                                                  {0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0}, // Shape 54
                                                  {0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0}, // Shape 55
                                                  {0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1}, // Shape 56
                                                  {0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1}, // Shape 57
                                                  {0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1}, // Shape 58
                                                  {0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1}, // Shape 59
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}, // Shape 60
                                                  {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0}, // Shape 61
                                                  {0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0}, // Shape 62
                                                  {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1}  // Shape 63
                                              },

                                              {
                                                  // BC7 Partition Set for 3 Subsets
                                                  {0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 1, 2, 2, 2, 2}, // Shape 0
                                                  {0, 0, 0, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1}, // Shape 1
                                                  {0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1}, // Shape 2
                                                  {0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 1, 1}, // Shape 3
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2}, // Shape 4
                                                  {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 2, 2}, // Shape 5
                                                  {0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}, // Shape 6
                                                  {0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1}, // Shape 7
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2}, // Shape 8
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2}, // Shape 9
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2}, // Shape 10
                                                  {0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2}, // Shape 11
                                                  {0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2}, // Shape 12
                                                  {0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2}, // Shape 13
                                                  {0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2, 1, 2, 2, 2}, // Shape 14
                                                  {0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0, 2, 2, 2, 0}, // Shape 15
                                                  {0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2}, // Shape 16
                                                  {0, 1, 1, 1, 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0}, // Shape 17
                                                  {0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2}, // Shape 18
                                                  {0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1}, // Shape 19
                                                  {0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2, 0, 2, 2, 2}, // Shape 20
                                                  {0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 1, 2, 2, 2, 1}, // Shape 21
                                                  {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2}, // Shape 22
                                                  {0, 0, 0, 0, 1, 1, 0, 0, 2, 2, 1, 0, 2, 2, 1, 0}, // Shape 23
                                                  {0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0}, // Shape 24
                                                  {0, 0, 1, 2, 0, 0, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2}, // Shape 25
                                                  {0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1, 0, 1, 1, 0}, // Shape 26
                                                  {0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1}, // Shape 27
                                                  {0, 0, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 0, 0, 2, 2}, // Shape 28
                                                  {0, 1, 1, 0, 0, 1, 1, 0, 2, 0, 0, 2, 2, 2, 2, 2}, // Shape 29
                                                  {0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1}, // Shape 30
                                                  {0, 0, 0, 0, 2, 0, 0, 0, 2, 2, 1, 1, 2, 2, 2, 1}, // Shape 31
                                                  {0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 2, 2, 2}, // Shape 32
                                                  {0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 2, 0, 0, 1, 1}, // Shape 33
                                                  {0, 0, 1, 1, 0, 0, 1, 2, 0, 0, 2, 2, 0, 2, 2, 2}, // Shape 34
                                                  {0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0}, // Shape 35
                                                  {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0}, // Shape 36
                                                  {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0}, // Shape 37
                                                  {0, 1, 2, 0, 2, 0, 1, 2, 1, 2, 0, 1, 0, 1, 2, 0}, // Shape 38
                                                  {0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1}, // Shape 39
                                                  {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1}, // Shape 40
                                                  {0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2}, // Shape 41
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1}, // Shape 42
                                                  {0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 2, 2}, // Shape 43
                                                  {0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1}, // Shape 44
                                                  {0, 2, 2, 0, 1, 2, 2, 1, 0, 2, 2, 0, 1, 2, 2, 1}, // Shape 45
                                                  {0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 0, 1}, // Shape 46
                                                  {0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1}, // Shape 47
                                                  {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2}, // Shape 48
                                                  {0, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 2, 0, 1, 1, 1}, // Shape 49
                                                  {0, 0, 0, 2, 1, 1, 1, 2, 0, 0, 0, 2, 1, 1, 1, 2}, // Shape 50
                                                  {0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2}, // Shape 51
                                                  {0, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2}, // Shape 52
                                                  {0, 0, 0, 2, 1, 1, 1, 2, 1, 1, 1, 2, 0, 0, 0, 2}, // Shape 53
                                                  {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2}, // Shape 54
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2}, // Shape 55
                                                  {0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2}, // Shape 56
                                                  {0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2}, // Shape 57
                                                  {0, 0, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2}, // Shape 58
                                                  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2}, // Shape 59
                                                  {0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1}, // Shape 60
                                                  {0, 2, 2, 2, 1, 2, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2}, // Shape 61
                                                  {0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, // Shape 62
                                                  {0, 1, 1, 1, 2, 0, 1, 1, 2, 2, 0, 1, 2, 2, 2, 0}  // Shape 63
                                              }};

// Partition, Shape, Fixup
const uint8_t g_aFixUp[3][64][3] = {
    {// No fix-ups for 1st subset for BC6H or BC7
     {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
     {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
     {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
     {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
     {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},

    {// BC6H/BC7 Partition Set Fixups for 2 Subsets
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 2, 0},
     {0, 8, 0},
     {0, 2, 0},
     {0, 2, 0},
     {0, 8, 0},
     {0, 8, 0},
     {0, 15, 0},
     {0, 2, 0},
     {0, 8, 0},
     {0, 2, 0},
     {0, 2, 0},
     {0, 8, 0},
     {0, 8, 0},
     {0, 2, 0},
     {0, 2, 0},

     // BC7 Partition Set Fixups for 2 Subsets (second-half)
     {0, 15, 0},
     {0, 15, 0},
     {0, 6, 0},
     {0, 8, 0},
     {0, 2, 0},
     {0, 8, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 2, 0},
     {0, 8, 0},
     {0, 2, 0},
     {0, 2, 0},
     {0, 2, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 6, 0},
     {0, 6, 0},
     {0, 2, 0},
     {0, 6, 0},
     {0, 8, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 2, 0},
     {0, 2, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 15, 0},
     {0, 2, 0},
     {0, 2, 0},
     {0, 15, 0}},

    {// BC7 Partition Set Fixups for 3 Subsets
     {0, 3, 15},  {0, 3, 8},  {0, 15, 8},  {0, 15, 3}, {0, 8, 15},  {0, 3, 15}, {0, 15, 3},  {0, 15, 8}, {0, 8, 15},  {0, 8, 15}, {0, 6, 15},
     {0, 6, 15},  {0, 6, 15}, {0, 5, 15},  {0, 3, 15}, {0, 3, 8},   {0, 3, 15}, {0, 3, 8},   {0, 8, 15}, {0, 15, 3},  {0, 3, 15}, {0, 3, 8},
     {0, 6, 15},  {0, 10, 8}, {0, 5, 3},   {0, 8, 15}, {0, 8, 6},   {0, 6, 10}, {0, 8, 15},  {0, 5, 15}, {0, 15, 10}, {0, 15, 8}, {0, 8, 15},
     {0, 15, 3},  {0, 3, 15}, {0, 5, 10},  {0, 6, 10}, {0, 10, 8},  {0, 8, 9},  {0, 15, 10}, {0, 15, 6}, {0, 3, 15},  {0, 15, 8}, {0, 5, 15},
     {0, 15, 3},  {0, 15, 6}, {0, 15, 6},  {0, 15, 8}, {0, 3, 15},  {0, 15, 3}, {0, 5, 15},  {0, 5, 15}, {0, 5, 15},  {0, 8, 15}, {0, 5, 15},
     {0, 10, 15}, {0, 5, 15}, {0, 10, 15}, {0, 8, 15}, {0, 13, 15}, {0, 15, 3}, {0, 12, 15}, {0, 3, 15}, {0, 3, 8}}};

struct LDREndPntPair {
	LDRColorA A;
	LDRColorA B;
};

template <size_t SizeInBytes>
class CBits {
public:
	uint8_t GetBit (_Inout_ size_t &uStartBit) const noexcept {
		assert (uStartBit < 128);
		const size_t uIndex = uStartBit >> 3;
		const auto ret      = static_cast<uint8_t> ((m_uBits[uIndex] >> (uStartBit - (uIndex << 3))) & 0x01);
		uStartBit++;
		return ret;
	}

	uint8_t GetBits (_Inout_ size_t &uStartBit, _In_ size_t uNumBits) const noexcept {
		if (uNumBits == 0) return 0;
		assert (uStartBit + uNumBits <= 128 && uNumBits <= 8);
		uint8_t ret;
		const size_t uIndex = uStartBit >> 3;
		const size_t uBase  = uStartBit - (uIndex << 3);
		if (uBase + uNumBits > 8) {
			const size_t uFirstIndexBits = 8 - uBase;
			const size_t uNextIndexBits  = uNumBits - uFirstIndexBits;
			ret                          = static_cast<uint8_t> ((unsigned (m_uBits[uIndex]) >> uBase) |
			                                                     ((unsigned (m_uBits[uIndex + 1]) & ((1u << uNextIndexBits) - 1)) << uFirstIndexBits));
		} else {
			ret = static_cast<uint8_t> ((m_uBits[uIndex] >> uBase) & ((1 << uNumBits) - 1));
		}
		assert (ret < (1 << uNumBits));
		uStartBit += uNumBits;
		return ret;
	}

	void SetBit (_Inout_ size_t &uStartBit, _In_ uint8_t uValue) noexcept {
		assert (uStartBit < 128 && uValue < 2);
		size_t uIndex      = uStartBit >> 3;
		const size_t uBase = uStartBit - (uIndex << 3);
		m_uBits[uIndex] &= ~(1 << uBase);
		m_uBits[uIndex] |= uValue << uBase;
		uStartBit++;
	}

	void SetBits (_Inout_ size_t &uStartBit, _In_ size_t uNumBits, _In_ uint8_t uValue) noexcept {
		if (uNumBits == 0) return;
		assert (uStartBit + uNumBits <= 128 && uNumBits <= 8);
		assert (uValue < (1 << uNumBits));
		size_t uIndex      = uStartBit >> 3;
		const size_t uBase = uStartBit - (uIndex << 3);
		if (uBase + uNumBits > 8) {
			const size_t uFirstIndexBits = 8 - uBase;
			const size_t uNextIndexBits  = uNumBits - uFirstIndexBits;
			m_uBits[uIndex] &= ~(((1 << uFirstIndexBits) - 1) << uBase);
			m_uBits[uIndex] |= uValue << uBase;
			m_uBits[uIndex + 1] &= ~((1 << uNextIndexBits) - 1);
			m_uBits[uIndex + 1] |= uValue >> uFirstIndexBits;
		} else {
			m_uBits[uIndex] &= ~(((1 << uNumBits) - 1) << uBase);
			m_uBits[uIndex] |= uValue << uBase;
		}
		uStartBit += uNumBits;
	}

private:
	uint8_t m_uBits[SizeInBytes];
};

class D3DX_BC7 : private CBits<16> {
public:
	void Encode (uint32_t flags, const HDRColorA *const pIn) noexcept;

private:
	struct ModeInfo {
		uint8_t uPartitions;
		uint8_t uPartitionBits;
		uint8_t uPBits;
		uint8_t uRotationBits;
		uint8_t uIndexModeBits;
		uint8_t uIndexPrec;
		uint8_t uIndexPrec2;
		LDRColorA RGBAPrec;
		LDRColorA RGBAPrecWithP;
	};

	struct EncodeParams {
		uint8_t uMode;
		LDREndPntPair aEndPts[BC7_MAX_SHAPES][BC7_MAX_REGIONS];
		LDRColorA aLDRPixels[NUM_PIXELS_PER_BLOCK];
		const HDRColorA *const aHDRPixels;

		EncodeParams (const HDRColorA *const aOriginal) noexcept : uMode (0), aEndPts{}, aLDRPixels{}, aHDRPixels (aOriginal) {}
	};

	static uint8_t Quantize (_In_ uint8_t comp, _In_ uint8_t uPrec) noexcept {
		assert (0 < uPrec && uPrec <= 8);
		const uint8_t rnd = std::min<uint8_t> (255u, static_cast<uint8_t> (unsigned (comp) + (1u << (7 - uPrec))));
		return uint8_t (rnd >> (8u - uPrec));
	}

	static LDRColorA Quantize (_In_ const LDRColorA &c, _In_ const LDRColorA &RGBAPrec) noexcept {
		LDRColorA q;
		q.r = Quantize (c.r, RGBAPrec.r);
		q.g = Quantize (c.g, RGBAPrec.g);
		q.b = Quantize (c.b, RGBAPrec.b);
		if (RGBAPrec.a) q.a = Quantize (c.a, RGBAPrec.a);
		else q.a = 255;
		return q;
	}

	static uint8_t Unquantize (_In_ uint8_t comp, _In_ size_t uPrec) noexcept {
		assert (0 < uPrec && uPrec <= 8);
		comp = static_cast<uint8_t> (unsigned (comp) << (8 - uPrec));
		return uint8_t (comp | (comp >> uPrec));
	}

	static LDRColorA Unquantize (_In_ const LDRColorA &c, _In_ const LDRColorA &RGBAPrec) noexcept {
		LDRColorA q;
		q.r = Unquantize (c.r, RGBAPrec.r);
		q.g = Unquantize (c.g, RGBAPrec.g);
		q.b = Unquantize (c.b, RGBAPrec.b);
		q.a = RGBAPrec.a > 0 ? Unquantize (c.a, RGBAPrec.a) : 255u;
		return q;
	}

	void GeneratePaletteQuantized (const EncodeParams *pEP, size_t uIndexMode, const LDREndPntPair &endpts, LDRColorA aPalette[]) const noexcept;
	float PerturbOne (const EncodeParams *pEP, const LDRColorA colors[], size_t np, _In_ size_t uIndexMode, size_t ch,
	                  const LDREndPntPair &old_endpts, LDREndPntPair &new_endpts, float old_err, uint8_t do_b) const noexcept;
	void Exhaustive (const EncodeParams *pEP, const LDRColorA aColors[], size_t np, size_t uIndexMode, size_t ch, float &fOrgErr,
	                 LDREndPntPair &optEndPt) const noexcept;
	void OptimizeOne (const EncodeParams *pEP, const LDRColorA colors[], size_t np, size_t uIndexMode, float orig_err,
	                  const LDREndPntPair &orig_endpts, LDREndPntPair &opt_endpts) const noexcept;
	void OptimizeEndPoints (const EncodeParams *pEP, size_t uShape, size_t uIndexMode, const float orig_err[], const LDREndPntPair orig_endpts[],
	                        LDREndPntPair opt_endpts[]) const noexcept;
	void AssignIndices (const EncodeParams *pEP, size_t uShape, size_t uIndexMode, LDREndPntPair endpts[], size_t aIndices[], size_t aIndices2[],
	                    float afTotErr[]) const noexcept;
	void EmitBlock (const EncodeParams *pEP, size_t uShape, size_t uRotation, size_t uIndexMode, const LDREndPntPair aEndPts[], const size_t aIndex[],
	                const size_t aIndex2[]) noexcept;

	void FixEndpointPBits (_In_ const EncodeParams *pEP, const LDREndPntPair *pOrigEndpoints, LDREndPntPair *pFixedEndpoints) noexcept;
	float Refine (const EncodeParams *pEP, size_t uShape, size_t uRotation, size_t uIndexMode) noexcept;

	float MapColors (const EncodeParams *pEP, const LDRColorA aColors[], size_t np, size_t uIndexMode, const LDREndPntPair &endPts,
	                 float fMinErr) const noexcept;
	static float RoughMSE (EncodeParams *pEP, size_t uShape, size_t uIndexMode) noexcept;

private:
	static constexpr uint8_t c_NumModes = 8;

	static const ModeInfo ms_aInfo[c_NumModes];
};

const D3DX_BC7::ModeInfo D3DX_BC7::ms_aInfo[D3DX_BC7::c_NumModes] = {
    {2, 4, 6, 0, 0, 3, 0, LDRColorA (4, 4, 4, 0), LDRColorA (5, 5, 5, 0)},
    // Mode 0: Color only, 3 Subsets, RGBP 4441 (unique P-bit), 3-bit indecies, 16 partitions
    {1, 6, 2, 0, 0, 3, 0, LDRColorA (6, 6, 6, 0), LDRColorA (7, 7, 7, 0)},
    // Mode 1: Color only, 2 Subsets, RGBP 6661 (shared P-bit), 3-bit indecies, 64 partitions
    {2, 6, 0, 0, 0, 2, 0, LDRColorA (5, 5, 5, 0), LDRColorA (5, 5, 5, 0)},
    // Mode 2: Color only, 3 Subsets, RGB 555, 2-bit indecies, 64 partitions
    {1, 6, 4, 0, 0, 2, 0, LDRColorA (7, 7, 7, 0), LDRColorA (8, 8, 8, 0)},
    // Mode 3: Color only, 2 Subsets, RGBP 7771 (unique P-bit), 2-bits indecies, 64 partitions
    {0, 0, 0, 2, 1, 2, 3, LDRColorA (5, 5, 5, 6), LDRColorA (5, 5, 5, 6)},
    // Mode 4: Color w/ Separate Alpha, 1 Subset, RGB 555, A6, 16x2/16x3-bit indices, 2-bit rotation, 1-bit index selector
    {0, 0, 0, 2, 0, 2, 2, LDRColorA (7, 7, 7, 8), LDRColorA (7, 7, 7, 8)},
    // Mode 5: Color w/ Separate Alpha, 1 Subset, RGB 777, A8, 16x2/16x2-bit indices, 2-bit rotation
    {0, 0, 2, 0, 0, 4, 0, LDRColorA (7, 7, 7, 7), LDRColorA (8, 8, 8, 8)},
    // Mode 6: Color+Alpha, 1 Subset, RGBAP 77771 (unique P-bit), 16x4-bit indecies
    {1, 6, 4, 0, 0, 2, 0, LDRColorA (5, 5, 5, 5), LDRColorA (6, 6, 6, 6)}
    // Mode 7: Color+Alpha, 2 Subsets, RGBAP 55551 (unique P-bit), 2-bit indices, 64 partitions
};

inline bool
IsFixUpOffset (size_t uPartitions, size_t uShape, size_t uOffset) noexcept {
	assert (uPartitions < 3 && uShape < 64 && uOffset < 16);
	for (size_t p = 0; p <= uPartitions; p++)
		if (uOffset == g_aFixUp[uPartitions][uShape][p]) return true;
	return false;
}

void
OptimizeRGB (const HDRColorA *const pPoints, HDRColorA *pX, HDRColorA *pY, uint32_t cSteps, size_t cPixels, const size_t *pIndex) noexcept {
	const float *pC = (3 == cSteps) ? pC3 : pC4;
	const float *pD = (3 == cSteps) ? pD3 : pD4;

	// Find Min and Max points, as starting point
	HDRColorA X (FLT_MAX, FLT_MAX, FLT_MAX, 0.0f);
	HDRColorA Y (-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.0f);

	for (size_t iPoint = 0; iPoint < cPixels; iPoint++) {
		if (pPoints[pIndex[iPoint]].r < X.r) X.r = pPoints[pIndex[iPoint]].r;
		if (pPoints[pIndex[iPoint]].g < X.g) X.g = pPoints[pIndex[iPoint]].g;
		if (pPoints[pIndex[iPoint]].b < X.b) X.b = pPoints[pIndex[iPoint]].b;
		if (pPoints[pIndex[iPoint]].r > Y.r) Y.r = pPoints[pIndex[iPoint]].r;
		if (pPoints[pIndex[iPoint]].g > Y.g) Y.g = pPoints[pIndex[iPoint]].g;
		if (pPoints[pIndex[iPoint]].b > Y.b) Y.b = pPoints[pIndex[iPoint]].b;
	}

	// Diagonal axis
	HDRColorA AB;
	AB.r = Y.r - X.r;
	AB.g = Y.g - X.g;
	AB.b = Y.b - X.b;

	const float fAB = AB.r * AB.r + AB.g * AB.g + AB.b * AB.b;

	// Single color block.. no need to root-find
	if (fAB < FLT_MIN) {
		pX->r = X.r;
		pX->g = X.g;
		pX->b = X.b;
		pY->r = Y.r;
		pY->g = Y.g;
		pY->b = Y.b;
		return;
	}

	// Try all four axis directions, to determine which diagonal best fits data
	const float fABInv = 1.0f / fAB;

	HDRColorA Dir;
	Dir.r = AB.r * fABInv;
	Dir.g = AB.g * fABInv;
	Dir.b = AB.b * fABInv;

	HDRColorA Mid;
	Mid.r = (X.r + Y.r) * 0.5f;
	Mid.g = (X.g + Y.g) * 0.5f;
	Mid.b = (X.b + Y.b) * 0.5f;

	float fDir[4];
	fDir[0] = fDir[1] = fDir[2] = fDir[3] = 0.0f;

	for (size_t iPoint = 0; iPoint < cPixels; iPoint++) {
		HDRColorA Pt;
		Pt.r = (pPoints[pIndex[iPoint]].r - Mid.r) * Dir.r;
		Pt.g = (pPoints[pIndex[iPoint]].g - Mid.g) * Dir.g;
		Pt.b = (pPoints[pIndex[iPoint]].b - Mid.b) * Dir.b;

		float f;
		f = Pt.r + Pt.g + Pt.b;
		fDir[0] += f * f;
		f = Pt.r + Pt.g - Pt.b;
		fDir[1] += f * f;
		f = Pt.r - Pt.g + Pt.b;
		fDir[2] += f * f;
		f = Pt.r - Pt.g - Pt.b;
		fDir[3] += f * f;
	}

	float fDirMax  = fDir[0];
	size_t iDirMax = 0;

	for (size_t iDir = 1; iDir < 4; iDir++) {
		if (fDir[iDir] > fDirMax) {
			fDirMax = fDir[iDir];
			iDirMax = iDir;
		}
	}

	if (iDirMax & 2) std::swap (X.g, Y.g);
	if (iDirMax & 1) std::swap (X.b, Y.b);

	// Two color block.. no need to root-find
	if (fAB < 1.0f / 4096.0f) {
		pX->r = X.r;
		pX->g = X.g;
		pX->b = X.b;
		pY->r = Y.r;
		pY->g = Y.g;
		pY->b = Y.b;
		return;
	}

	// Use Newton's Method to find local minima of sum-of-squares error.
	const auto fSteps = static_cast<float> (cSteps - 1);

	for (size_t iIteration = 0; iIteration < 8; iIteration++) {
		// Calculate new steps
		HDRColorA pSteps[4] = {};

		for (size_t iStep = 0; iStep < cSteps; iStep++) {
			pSteps[iStep].r = X.r * pC[iStep] + Y.r * pD[iStep];
			pSteps[iStep].g = X.g * pC[iStep] + Y.g * pD[iStep];
			pSteps[iStep].b = X.b * pC[iStep] + Y.b * pD[iStep];
		}

		// Calculate color direction
		Dir.r = Y.r - X.r;
		Dir.g = Y.g - X.g;
		Dir.b = Y.b - X.b;

		const float fLen = (Dir.r * Dir.r + Dir.g * Dir.g + Dir.b * Dir.b);

		if (fLen < (1.0f / 4096.0f)) break;

		const float fScale = fSteps / fLen;

		Dir.r *= fScale;
		Dir.g *= fScale;
		Dir.b *= fScale;

		// Evaluate function, and derivatives
		float d2X = 0.0f, d2Y = 0.0f;
		HDRColorA dX (0.0f, 0.0f, 0.0f, 0.0f), dY (0.0f, 0.0f, 0.0f, 0.0f);

		for (size_t iPoint = 0; iPoint < cPixels; iPoint++) {
			const float fDot =
			    (pPoints[pIndex[iPoint]].r - X.r) * Dir.r + (pPoints[pIndex[iPoint]].g - X.g) * Dir.g + (pPoints[pIndex[iPoint]].b - X.b) * Dir.b;

			uint32_t iStep;
			if (fDot <= 0.0f) iStep = 0;
			else if (fDot >= fSteps) iStep = cSteps - 1;
			else iStep = uint32_t (fDot + 0.5f);

			HDRColorA Diff;
			Diff.r = pSteps[iStep].r - pPoints[pIndex[iPoint]].r;
			Diff.g = pSteps[iStep].g - pPoints[pIndex[iPoint]].g;
			Diff.b = pSteps[iStep].b - pPoints[pIndex[iPoint]].b;

			const float fC = pC[iStep] * (1.0f / 8.0f);
			const float fD = pD[iStep] * (1.0f / 8.0f);

			d2X += fC * pC[iStep];
			dX.r += fC * Diff.r;
			dX.g += fC * Diff.g;
			dX.b += fC * Diff.b;

			d2Y += fD * pD[iStep];
			dY.r += fD * Diff.r;
			dY.g += fD * Diff.g;
			dY.b += fD * Diff.b;
		}

		// Move endpoints
		if (d2X > 0.0f) {
			const float f = -1.0f / d2X;

			X.r += dX.r * f;
			X.g += dX.g * f;
			X.b += dX.b * f;
		}

		if (d2Y > 0.0f) {
			const float f = -1.0f / d2Y;

			Y.r += dY.r * f;
			Y.g += dY.g * f;
			Y.b += dY.b * f;
		}

		if ((dX.r * dX.r < fEpsilon) && (dX.g * dX.g < fEpsilon) && (dX.b * dX.b < fEpsilon) && (dY.r * dY.r < fEpsilon) &&
		    (dY.g * dY.g < fEpsilon) && (dY.b * dY.b < fEpsilon)) {
			break;
		}
	}

	pX->r = X.r;
	pX->g = X.g;
	pX->b = X.b;
	pY->r = Y.r;
	pY->g = Y.g;
	pY->b = Y.b;
}

void
OptimizeRGBA (const HDRColorA *const pPoints, HDRColorA *pX, HDRColorA *pY, uint32_t cSteps, size_t cPixels, const size_t *pIndex) noexcept {
	const float *pC = (3 == cSteps) ? pC3 : pC4;
	const float *pD = (3 == cSteps) ? pD3 : pD4;

	// Find Min and Max points, as starting point
	HDRColorA X (1.0f, 1.0f, 1.0f, 1.0f);
	HDRColorA Y (0.0f, 0.0f, 0.0f, 0.0f);

	for (size_t iPoint = 0; iPoint < cPixels; iPoint++) {
		if (pPoints[pIndex[iPoint]].r < X.r) X.r = pPoints[pIndex[iPoint]].r;
		if (pPoints[pIndex[iPoint]].g < X.g) X.g = pPoints[pIndex[iPoint]].g;
		if (pPoints[pIndex[iPoint]].b < X.b) X.b = pPoints[pIndex[iPoint]].b;
		if (pPoints[pIndex[iPoint]].a < X.a) X.a = pPoints[pIndex[iPoint]].a;
		if (pPoints[pIndex[iPoint]].r > Y.r) Y.r = pPoints[pIndex[iPoint]].r;
		if (pPoints[pIndex[iPoint]].g > Y.g) Y.g = pPoints[pIndex[iPoint]].g;
		if (pPoints[pIndex[iPoint]].b > Y.b) Y.b = pPoints[pIndex[iPoint]].b;
		if (pPoints[pIndex[iPoint]].a > Y.a) Y.a = pPoints[pIndex[iPoint]].a;
	}

	// Diagonal axis
	const HDRColorA AB = Y - X;
	const float fAB    = AB * AB;

	// Single color block.. no need to root-find
	if (fAB < FLT_MIN) {
		*pX = X;
		*pY = Y;
		return;
	}

	// Try all four axis directions, to determine which diagonal best fits data
	const float fABInv  = 1.0f / fAB;
	HDRColorA Dir       = AB * fABInv;
	const HDRColorA Mid = (X + Y) * 0.5f;

	float fDir[8];
	fDir[0] = fDir[1] = fDir[2] = fDir[3] = fDir[4] = fDir[5] = fDir[6] = fDir[7] = 0.0f;

	for (size_t iPoint = 0; iPoint < cPixels; iPoint++) {
		HDRColorA Pt;
		Pt.r = (pPoints[pIndex[iPoint]].r - Mid.r) * Dir.r;
		Pt.g = (pPoints[pIndex[iPoint]].g - Mid.g) * Dir.g;
		Pt.b = (pPoints[pIndex[iPoint]].b - Mid.b) * Dir.b;
		Pt.a = (pPoints[pIndex[iPoint]].a - Mid.a) * Dir.a;

		float f;
		f = Pt.r + Pt.g + Pt.b + Pt.a;
		fDir[0] += f * f;
		f = Pt.r + Pt.g + Pt.b - Pt.a;
		fDir[1] += f * f;
		f = Pt.r + Pt.g - Pt.b + Pt.a;
		fDir[2] += f * f;
		f = Pt.r + Pt.g - Pt.b - Pt.a;
		fDir[3] += f * f;
		f = Pt.r - Pt.g + Pt.b + Pt.a;
		fDir[4] += f * f;
		f = Pt.r - Pt.g + Pt.b - Pt.a;
		fDir[5] += f * f;
		f = Pt.r - Pt.g - Pt.b + Pt.a;
		fDir[6] += f * f;
		f = Pt.r - Pt.g - Pt.b - Pt.a;
		fDir[7] += f * f;
	}

	float fDirMax  = fDir[0];
	size_t iDirMax = 0;

	for (size_t iDir = 1; iDir < 8; iDir++) {
		if (fDir[iDir] > fDirMax) {
			fDirMax = fDir[iDir];
			iDirMax = iDir;
		}
	}

	if (iDirMax & 4) std::swap (X.g, Y.g);
	if (iDirMax & 2) std::swap (X.b, Y.b);
	if (iDirMax & 1) std::swap (X.a, Y.a);

	// Two color block.. no need to root-find
	if (fAB < 1.0f / 4096.0f) {
		*pX = X;
		*pY = Y;
		return;
	}

	// Use Newton's Method to find local minima of sum-of-squares error.
	const auto fSteps = static_cast<float> (cSteps - 1u);

	for (size_t iIteration = 0; iIteration < 8; iIteration++) {
		// Calculate new steps
		HDRColorA pSteps[BC7_MAX_INDICES];

		LDRColorA lX, lY;
		lX = (X * 255.0f).ToLDRColorA ();
		lY = (Y * 255.0f).ToLDRColorA ();

		for (size_t iStep = 0; iStep < cSteps; iStep++) {
			pSteps[iStep] = X * pC[iStep] + Y * pD[iStep];
			// LDRColorA::Interpolate(lX, lY, i, i, wcprec, waprec, aSteps[i]);
		}

		// Calculate color direction
		Dir              = Y - X;
		const float fLen = Dir * Dir;
		if (fLen < (1.0f / 4096.0f)) break;

		const float fScale = fSteps / fLen;
		Dir *= fScale;

		// Evaluate function, and derivatives
		float d2X = 0.0f, d2Y = 0.0f;
		HDRColorA dX (0.0f, 0.0f, 0.0f, 0.0f), dY (0.0f, 0.0f, 0.0f, 0.0f);

		for (size_t iPoint = 0; iPoint < cPixels; ++iPoint) {
			const float fDot = (pPoints[pIndex[iPoint]] - X) * Dir;

			uint32_t iStep;
			if (fDot <= 0.0f) iStep = 0;
			else if (fDot >= fSteps) iStep = cSteps - 1;
			else iStep = uint32_t (fDot + 0.5f);

			const HDRColorA Diff = pSteps[iStep] - pPoints[pIndex[iPoint]];
			const float fC       = pC[iStep] * (1.0f / 8.0f);
			const float fD       = pD[iStep] * (1.0f / 8.0f);

			d2X += fC * pC[iStep];
			dX += Diff * fC;

			d2Y += fD * pD[iStep];
			dY += Diff * fD;
		}

		// Move endpoints
		if (d2X > 0.0f) {
			const float f = -1.0f / d2X;
			X += dX * f;
		}

		if (d2Y > 0.0f) {
			const float f = -1.0f / d2Y;
			Y += dY * f;
		}

		if ((dX * dX < fEpsilon) && (dY * dY < fEpsilon)) break;
	}

	*pX = X;
	*pY = Y;
}

float
ComputeError (const LDRColorA &pixel, const LDRColorA aPalette[], uint8_t uIndexPrec, uint8_t uIndexPrec2, size_t *pBestIndex = nullptr,
              size_t *pBestIndex2 = nullptr) noexcept {
	const size_t uNumIndices  = size_t (1) << uIndexPrec;
	const size_t uNumIndices2 = size_t (1) << uIndexPrec2;
	float fTotalErr           = 0;
	float fBestErr            = FLT_MAX;

	if (pBestIndex) *pBestIndex = 0;
	if (pBestIndex2) *pBestIndex2 = 0;

	const HDRColorA vpixel = pixel;

	if (uIndexPrec2 == 0) {
		for (size_t i = 0; i < uNumIndices && fBestErr > 0; i++) {
			HDRColorA tpixel = aPalette[i];
			// Compute ErrorMetric
			tpixel           = vpixel - tpixel;
			const float fErr = tpixel.r * tpixel.r + tpixel.g * tpixel.g + tpixel.b * tpixel.b + tpixel.a * tpixel.a;
			if (fErr > fBestErr) // error increased, so we're done searching
				break;
			if (fErr < fBestErr) {
				fBestErr = fErr;
				if (pBestIndex) *pBestIndex = i;
			}
		}
		fTotalErr += fBestErr;
	} else {
		for (size_t i = 0; i < uNumIndices && fBestErr > 0; i++) {
			HDRColorA tpixel = aPalette[i];
			// Compute ErrorMetricRGB
			tpixel           = vpixel - tpixel;
			const float fErr = tpixel.r * tpixel.r + tpixel.g * tpixel.g + tpixel.b * tpixel.b;
			if (fErr > fBestErr) // error increased, so we're done searching
				break;
			if (fErr < fBestErr) {
				fBestErr = fErr;
				if (pBestIndex) *pBestIndex = i;
			}
		}
		fTotalErr += fBestErr;
		fBestErr = FLT_MAX;
		for (size_t i = 0; i < uNumIndices2 && fBestErr > 0; i++) {
			// Compute ErrorMetricAlpha
			const float ea   = float (pixel.a) - float (aPalette[i].a);
			const float fErr = ea * ea;
			if (fErr > fBestErr) // error increased, so we're done searching
				break;
			if (fErr < fBestErr) {
				fBestErr = fErr;
				if (pBestIndex2) *pBestIndex2 = i;
			}
		}
		fTotalErr += fBestErr;
	}

	return fTotalErr;
}

float
D3DX_BC7::MapColors (const EncodeParams *pEP, const LDRColorA aColors[], size_t np, size_t uIndexMode, const LDREndPntPair &endPts,
                     float fMinErr) const noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const uint8_t uIndexPrec  = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec2 : ms_aInfo[pEP->uMode].uIndexPrec;
	const uint8_t uIndexPrec2 = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec : ms_aInfo[pEP->uMode].uIndexPrec2;
	LDRColorA aPalette[BC7_MAX_INDICES];
	float fTotalErr = 0;

	GeneratePaletteQuantized (pEP, uIndexMode, endPts, aPalette);
	for (size_t i = 0; i < np; ++i) {
		fTotalErr += ComputeError (aColors[i], aPalette, uIndexPrec, uIndexPrec2);
		if (fTotalErr > fMinErr) // check for early exit
		{
			fTotalErr = FLT_MAX;
			break;
		}
	}

	return fTotalErr;
}

float
D3DX_BC7::RoughMSE (EncodeParams *pEP, size_t uShape, size_t uIndexMode) noexcept {
	assert (pEP);
	assert (uShape < BC7_MAX_SHAPES);
	assert (pEP->uMode < c_NumModes);

	LDREndPntPair *aEndPts = pEP->aEndPts[uShape];

	const uint8_t uPartitions = ms_aInfo[pEP->uMode].uPartitions;
	assert (uPartitions < BC7_MAX_REGIONS);

	const uint8_t uIndexPrec  = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec2 : ms_aInfo[pEP->uMode].uIndexPrec;
	const uint8_t uIndexPrec2 = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec : ms_aInfo[pEP->uMode].uIndexPrec2;
	const auto uNumIndices    = static_cast<const uint8_t> (1u << uIndexPrec);
	const auto uNumIndices2   = static_cast<const uint8_t> (1u << uIndexPrec2);
	size_t auPixIdx[NUM_PIXELS_PER_BLOCK];
	LDRColorA aPalette[BC7_MAX_REGIONS][BC7_MAX_INDICES];

	for (size_t p = 0; p <= uPartitions; p++) {
		size_t np = 0;
		for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
			if (g_aPartitionTable[uPartitions][uShape][i] == p) auPixIdx[np++] = i;

		// handle simple cases
		assert (np > 0);
		if (np == 1) {
			aEndPts[p].A = pEP->aLDRPixels[auPixIdx[0]];
			aEndPts[p].B = pEP->aLDRPixels[auPixIdx[0]];
			continue;
		} else if (np == 2) {
			aEndPts[p].A = pEP->aLDRPixels[auPixIdx[0]];
			aEndPts[p].B = pEP->aLDRPixels[auPixIdx[1]];
			continue;
		}

		if (uIndexPrec2 == 0) {
			HDRColorA epA, epB;
			OptimizeRGBA (pEP->aHDRPixels, &epA, &epB, 4, np, auPixIdx);
			epA.Clamp (0.0f, 1.0f);
			epB.Clamp (0.0f, 1.0f);
			epA *= 255.0f;
			epB *= 255.0f;
			aEndPts[p].A = epA.ToLDRColorA ();
			aEndPts[p].B = epB.ToLDRColorA ();
		} else {
			uint8_t uMinAlpha = 255, uMaxAlpha = 0;
			for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i) {
				uMinAlpha = std::min<uint8_t> (uMinAlpha, pEP->aLDRPixels[auPixIdx[i]].a);
				uMaxAlpha = std::max<uint8_t> (uMaxAlpha, pEP->aLDRPixels[auPixIdx[i]].a);
			}

			HDRColorA epA, epB;
			OptimizeRGB (pEP->aHDRPixels, &epA, &epB, 4, np, auPixIdx);
			epA.Clamp (0.0f, 1.0f);
			epB.Clamp (0.0f, 1.0f);
			epA *= 255.0f;
			epB *= 255.0f;
			aEndPts[p].A   = epA.ToLDRColorA ();
			aEndPts[p].B   = epB.ToLDRColorA ();
			aEndPts[p].A.a = uMinAlpha;
			aEndPts[p].B.a = uMaxAlpha;
		}
	}

	if (uIndexPrec2 == 0) {
		for (size_t p = 0; p <= uPartitions; p++)
			for (size_t i = 0; i < uNumIndices; i++)
				LDRColorA::Interpolate (aEndPts[p].A, aEndPts[p].B, i, i, uIndexPrec, uIndexPrec, aPalette[p][i]);
	} else {
		for (size_t p = 0; p <= uPartitions; p++) {
			for (size_t i = 0; i < uNumIndices; i++)
				LDRColorA::InterpolateRGB (aEndPts[p].A, aEndPts[p].B, i, uIndexPrec, aPalette[p][i]);
			for (size_t i = 0; i < uNumIndices2; i++)
				LDRColorA::InterpolateA (aEndPts[p].A, aEndPts[p].B, i, uIndexPrec2, aPalette[p][i]);
		}
	}

	float fTotalErr = 0;
	for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++) {
		const uint8_t uRegion = g_aPartitionTable[uPartitions][uShape][i];
		fTotalErr += ComputeError (pEP->aLDRPixels[i], aPalette[uRegion], uIndexPrec, uIndexPrec2);
	}

	return fTotalErr;
}

void
D3DX_BC7::GeneratePaletteQuantized (const EncodeParams *pEP, size_t uIndexMode, const LDREndPntPair &endPts, LDRColorA aPalette[]) const noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const size_t uIndexPrec   = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec2 : ms_aInfo[pEP->uMode].uIndexPrec;
	const size_t uIndexPrec2  = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec : ms_aInfo[pEP->uMode].uIndexPrec2;
	const size_t uNumIndices  = size_t (1) << uIndexPrec;
	const size_t uNumIndices2 = size_t (1) << uIndexPrec2;
	assert (uNumIndices > 0 && uNumIndices2 > 0);
	assert ((uNumIndices <= BC7_MAX_INDICES) && (uNumIndices2 <= BC7_MAX_INDICES));

	const LDRColorA a = Unquantize (endPts.A, ms_aInfo[pEP->uMode].RGBAPrecWithP);
	const LDRColorA b = Unquantize (endPts.B, ms_aInfo[pEP->uMode].RGBAPrecWithP);
	if (uIndexPrec2 == 0) {
		for (size_t i = 0; i < uNumIndices; i++)
			LDRColorA::Interpolate (a, b, i, i, uIndexPrec, uIndexPrec, aPalette[i]);
	} else {
		for (size_t i = 0; i < uNumIndices; i++)
			LDRColorA::InterpolateRGB (a, b, i, uIndexPrec, aPalette[i]);
		for (size_t i = 0; i < uNumIndices2; i++)
			LDRColorA::InterpolateA (a, b, i, uIndexPrec2, aPalette[i]);
	}
}

float
D3DX_BC7::PerturbOne (const EncodeParams *pEP, const LDRColorA aColors[], size_t np, size_t uIndexMode, size_t ch, const LDREndPntPair &oldEndPts,
                      LDREndPntPair &newEndPts, float fOldErr, uint8_t do_b) const noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const int prec           = ms_aInfo[pEP->uMode].RGBAPrecWithP[ch];
	LDREndPntPair tmp_endPts = newEndPts = oldEndPts;
	float fMinErr                        = fOldErr;
	uint8_t *pnew_c                      = (do_b ? &newEndPts.B[ch] : &newEndPts.A[ch]);
	uint8_t *ptmp_c                      = (do_b ? &tmp_endPts.B[ch] : &tmp_endPts.A[ch]);

	// do a logarithmic search for the best error for this endpoint (which)
	for (int step = 1 << (prec - 1); step; step >>= 1) {
		bool bImproved = false;
		int beststep   = 0;
		for (int sign = -1; sign <= 1; sign += 2) {
			int tmp = int (*pnew_c) + sign * step;
			if (tmp < 0 || tmp >= (1 << prec)) continue;
			else *ptmp_c = static_cast<uint8_t> (tmp);

			const float fTotalErr = MapColors (pEP, aColors, np, uIndexMode, tmp_endPts, fMinErr);
			if (fTotalErr < fMinErr) {
				bImproved = true;
				fMinErr   = fTotalErr;
				beststep  = sign * step;
			}
		}

		// if this was an improvement, move the endpoint and continue search from there
		if (bImproved) *pnew_c = uint8_t (int (*pnew_c) + beststep);
	}
	return fMinErr;
}

// perturb the endpoints at least -3 to 3.
// always ensure endpoint ordering is preserved (no need to overlap the scan)
void
D3DX_BC7::Exhaustive (const EncodeParams *pEP, const LDRColorA aColors[], size_t np, size_t uIndexMode, size_t ch, float &fOrgErr,
                      LDREndPntPair &optEndPt) const noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const uint8_t uPrec = ms_aInfo[pEP->uMode].RGBAPrecWithP[ch];
	LDREndPntPair tmpEndPt;
	if (fOrgErr == 0) return;

	constexpr int delta = 5;

	// ok figure out the range of A and B
	tmpEndPt        = optEndPt;
	const int alow  = std::max<int> (0, int (optEndPt.A[ch]) - delta);
	const int ahigh = std::min<int> ((1 << uPrec) - 1, int (optEndPt.A[ch]) + delta);
	const int blow  = std::max<int> (0, int (optEndPt.B[ch]) - delta);
	const int bhigh = std::min<int> ((1 << uPrec) - 1, int (optEndPt.B[ch]) + delta);
	int amin        = 0;
	int bmin        = 0;

	float fBestErr = fOrgErr;
	if (optEndPt.A[ch] <= optEndPt.B[ch]) {
		// keep a <= b
		for (int a = alow; a <= ahigh; ++a) {
			for (int b = std::max<int> (a, blow); b < bhigh; ++b) {
				tmpEndPt.A[ch] = static_cast<uint8_t> (a);
				tmpEndPt.B[ch] = static_cast<uint8_t> (b);

				const float fErr = MapColors (pEP, aColors, np, uIndexMode, tmpEndPt, fBestErr);
				if (fErr < fBestErr) {
					amin     = a;
					bmin     = b;
					fBestErr = fErr;
				}
			}
		}
	} else {
		// keep b <= a
		for (int b = blow; b < bhigh; ++b) {
			for (int a = std::max<int> (b, alow); a <= ahigh; ++a) {
				tmpEndPt.A[ch] = static_cast<uint8_t> (a);
				tmpEndPt.B[ch] = static_cast<uint8_t> (b);

				const float fErr = MapColors (pEP, aColors, np, uIndexMode, tmpEndPt, fBestErr);
				if (fErr < fBestErr) {
					amin     = a;
					bmin     = b;
					fBestErr = fErr;
				}
			}
		}
	}

	if (fBestErr < fOrgErr) {
		optEndPt.A[ch] = static_cast<uint8_t> (amin);
		optEndPt.B[ch] = static_cast<uint8_t> (bmin);
		fOrgErr        = fBestErr;
	}
}

void
D3DX_BC7::OptimizeOne (const EncodeParams *pEP, const LDRColorA aColors[], size_t np, size_t uIndexMode, float fOrgErr, const LDREndPntPair &org,
                       LDREndPntPair &opt) const noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	float fOptErr = fOrgErr;
	opt           = org;

	LDREndPntPair new_a, new_b;
	LDREndPntPair newEndPts;
	uint8_t do_b;

	// now optimize each channel separately
	for (size_t ch = 0; ch < BC7_NUM_CHANNELS; ++ch) {
		if (ms_aInfo[pEP->uMode].RGBAPrecWithP[ch] == 0) continue;

		// figure out which endpoint when perturbed gives the most improvement and start there
		// if we just alternate, we can easily end up in a local minima
		const float fErr0 = PerturbOne (pEP, aColors, np, uIndexMode, ch, opt, new_a, fOptErr, 0); // perturb endpt A
		const float fErr1 = PerturbOne (pEP, aColors, np, uIndexMode, ch, opt, new_b, fOptErr, 1); // perturb endpt B

		uint8_t &copt_a = opt.A[ch];
		uint8_t &copt_b = opt.B[ch];
		uint8_t &cnew_a = new_a.A[ch];
		uint8_t &cnew_b = new_a.B[ch];

		if (fErr0 < fErr1) {
			if (fErr0 >= fOptErr) continue;
			copt_a  = cnew_a;
			fOptErr = fErr0;
			do_b    = 1; // do B next
		} else {
			if (fErr1 >= fOptErr) continue;
			copt_b  = cnew_b;
			fOptErr = fErr1;
			do_b    = 0; // do A next
		}

		// now alternate endpoints and keep trying until there is no improvement
		for (;;) {
			const float fErr = PerturbOne (pEP, aColors, np, uIndexMode, ch, opt, newEndPts, fOptErr, do_b);
			if (fErr >= fOptErr) break;
			if (do_b == 0) copt_a = cnew_a;
			else copt_b = cnew_b;
			fOptErr = fErr;
			do_b    = 1u - do_b; // now move the other endpoint
		}
	}

	// finally, do a small exhaustive search around what we think is the global minima to be sure
	for (size_t ch = 0; ch < BC7_NUM_CHANNELS; ch++)
		Exhaustive (pEP, aColors, np, uIndexMode, ch, fOptErr, opt);
}

void
D3DX_BC7::OptimizeEndPoints (const EncodeParams *pEP, size_t uShape, size_t uIndexMode, const float afOrgErr[], const LDREndPntPair aOrgEndPts[],
                             LDREndPntPair aOptEndPts[]) const noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const uint8_t uPartitions = ms_aInfo[pEP->uMode].uPartitions;
	assert (uPartitions < BC7_MAX_REGIONS && uShape < BC7_MAX_SHAPES);

	LDRColorA aPixels[NUM_PIXELS_PER_BLOCK];

	for (size_t p = 0; p <= uPartitions; ++p) {
		// collect the pixels in the region
		size_t np = 0;
		for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i)
			if (g_aPartitionTable[uPartitions][uShape][i] == p) aPixels[np++] = pEP->aLDRPixels[i];

		OptimizeOne (pEP, aPixels, np, uIndexMode, afOrgErr[p], aOrgEndPts[p], aOptEndPts[p]);
	}
}

void
D3DX_BC7::AssignIndices (const EncodeParams *pEP, size_t uShape, size_t uIndexMode, LDREndPntPair endPts[], size_t aIndices[], size_t aIndices2[],
                         float afTotErr[]) const noexcept {
	assert (pEP);
	assert (uShape < BC7_MAX_SHAPES);
	assert (pEP->uMode < c_NumModes);

	const uint8_t uPartitions = ms_aInfo[pEP->uMode].uPartitions;
	assert (uPartitions < BC7_MAX_REGIONS);

	const uint8_t uIndexPrec  = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec2 : ms_aInfo[pEP->uMode].uIndexPrec;
	const uint8_t uIndexPrec2 = uIndexMode ? ms_aInfo[pEP->uMode].uIndexPrec : ms_aInfo[pEP->uMode].uIndexPrec2;
	const auto uNumIndices    = static_cast<const uint8_t> (1u << uIndexPrec);
	const auto uNumIndices2   = static_cast<const uint8_t> (1u << uIndexPrec2);

	assert ((uNumIndices <= BC7_MAX_INDICES) && (uNumIndices2 <= BC7_MAX_INDICES));

	const uint8_t uHighestIndexBit  = uint8_t (uNumIndices >> 1);
	const uint8_t uHighestIndexBit2 = uint8_t (uNumIndices2 >> 1);
	LDRColorA aPalette[BC7_MAX_REGIONS][BC7_MAX_INDICES];

	// build list of possibles
	for (size_t p = 0; p <= uPartitions; p++) {
		GeneratePaletteQuantized (pEP, uIndexMode, endPts[p], aPalette[p]);
		afTotErr[p] = 0;
	}

	for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++) {
		uint8_t uRegion = g_aPartitionTable[uPartitions][uShape][i];
		assert (uRegion < BC7_MAX_REGIONS);
		afTotErr[uRegion] += ComputeError (pEP->aLDRPixels[i], aPalette[uRegion], uIndexPrec, uIndexPrec2, &(aIndices[i]), &(aIndices2[i]));
	}

	// swap endpoints as needed to ensure that the indices at index_positions have a 0 high-order bit
	if (uIndexPrec2 == 0) {
		for (size_t p = 0; p <= uPartitions; p++) {
			if (aIndices[g_aFixUp[uPartitions][uShape][p]] & uHighestIndexBit) {
				std::swap (endPts[p].A, endPts[p].B);
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					if (g_aPartitionTable[uPartitions][uShape][i] == p) aIndices[i] = uNumIndices - 1 - aIndices[i];
			}
			assert ((aIndices[g_aFixUp[uPartitions][uShape][p]] & uHighestIndexBit) == 0);
		}
	} else {
		for (size_t p = 0; p <= uPartitions; p++) {
			if (aIndices[g_aFixUp[uPartitions][uShape][p]] & uHighestIndexBit) {
				std::swap (endPts[p].A.r, endPts[p].B.r);
				std::swap (endPts[p].A.g, endPts[p].B.g);
				std::swap (endPts[p].A.b, endPts[p].B.b);
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					if (g_aPartitionTable[uPartitions][uShape][i] == p) aIndices[i] = uNumIndices - 1 - aIndices[i];
			}
			assert ((aIndices[g_aFixUp[uPartitions][uShape][p]] & uHighestIndexBit) == 0);

			if (aIndices2[0] & uHighestIndexBit2) {
				std::swap (endPts[p].A.a, endPts[p].B.a);
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					aIndices2[i] = uNumIndices2 - 1 - aIndices2[i];
			}
			assert ((aIndices2[0] & uHighestIndexBit2) == 0);
		}
	}
}

void
D3DX_BC7::EmitBlock (const EncodeParams *pEP, size_t uShape, size_t uRotation, size_t uIndexMode, const LDREndPntPair aEndPts[],
                     const size_t aIndex[], const size_t aIndex2[]) noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const uint8_t uPartitions = ms_aInfo[pEP->uMode].uPartitions;
	assert (uPartitions < BC7_MAX_REGIONS);

	const size_t uPBits           = ms_aInfo[pEP->uMode].uPBits;
	const size_t uIndexPrec       = ms_aInfo[pEP->uMode].uIndexPrec;
	const size_t uIndexPrec2      = ms_aInfo[pEP->uMode].uIndexPrec2;
	const LDRColorA RGBAPrec      = ms_aInfo[pEP->uMode].RGBAPrec;
	const LDRColorA RGBAPrecWithP = ms_aInfo[pEP->uMode].RGBAPrecWithP;
	size_t i;
	size_t uStartBit = 0;
	SetBits (uStartBit, pEP->uMode, 0);
	SetBits (uStartBit, 1, 1);
	SetBits (uStartBit, ms_aInfo[pEP->uMode].uRotationBits, static_cast<uint8_t> (uRotation));
	SetBits (uStartBit, ms_aInfo[pEP->uMode].uIndexModeBits, static_cast<uint8_t> (uIndexMode));
	SetBits (uStartBit, ms_aInfo[pEP->uMode].uPartitionBits, static_cast<uint8_t> (uShape));

	if (uPBits) {
		const size_t uNumEP                  = (size_t (uPartitions) + 1) << 1;
		uint8_t aPVote[BC7_MAX_REGIONS << 1] = {0, 0, 0, 0, 0, 0};
		uint8_t aCount[BC7_MAX_REGIONS << 1] = {0, 0, 0, 0, 0, 0};
		for (uint8_t ch = 0; ch < BC7_NUM_CHANNELS; ch++) {
			uint8_t ep = 0;
			for (i = 0; i <= uPartitions; i++) {
				if (RGBAPrec[ch] == RGBAPrecWithP[ch]) {
					SetBits (uStartBit, RGBAPrec[ch], aEndPts[i].A[ch]);
					SetBits (uStartBit, RGBAPrec[ch], aEndPts[i].B[ch]);
				} else {
					SetBits (uStartBit, RGBAPrec[ch], uint8_t (aEndPts[i].A[ch] >> 1));
					SetBits (uStartBit, RGBAPrec[ch], uint8_t (aEndPts[i].B[ch] >> 1));
					size_t idx = ep++ * uPBits / uNumEP;
					assert (idx < (BC7_MAX_REGIONS << 1));
					aPVote[idx] += aEndPts[i].A[ch] & 0x01;
					aCount[idx]++;
					idx = ep++ * uPBits / uNumEP;
					assert (idx < (BC7_MAX_REGIONS << 1));
					aPVote[idx] += aEndPts[i].B[ch] & 0x01;
					aCount[idx]++;
				}
			}
		}

		for (i = 0; i < uPBits; i++)
			SetBits (uStartBit, 1, (aPVote[i] > (aCount[i] >> 1)) ? 1u : 0u);
	} else {
		for (size_t ch = 0; ch < BC7_NUM_CHANNELS; ch++) {
			for (i = 0; i <= uPartitions; i++) {
				SetBits (uStartBit, RGBAPrec[ch], aEndPts[i].A[ch]);
				SetBits (uStartBit, RGBAPrec[ch], aEndPts[i].B[ch]);
			}
		}
	}

	const size_t *aI1 = uIndexMode ? aIndex2 : aIndex;
	const size_t *aI2 = uIndexMode ? aIndex : aIndex2;
	for (i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
		if (IsFixUpOffset (ms_aInfo[pEP->uMode].uPartitions, uShape, i)) SetBits (uStartBit, uIndexPrec - 1, static_cast<uint8_t> (aI1[i]));
		else SetBits (uStartBit, uIndexPrec, static_cast<uint8_t> (aI1[i]));
	if (uIndexPrec2)
		for (i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
			SetBits (uStartBit, i ? uIndexPrec2 : uIndexPrec2 - 1, static_cast<uint8_t> (aI2[i]));

	assert (uStartBit == 128);
}

void
D3DX_BC7::FixEndpointPBits (const EncodeParams *pEP, const LDREndPntPair *pOrigEndpoints, LDREndPntPair *pFixedEndpoints) noexcept {
	assert (pEP);
	assert (pEP->uMode < c_NumModes);

	const size_t uPartitions = ms_aInfo[pEP->uMode].uPartitions;
	assert (uPartitions < BC7_MAX_REGIONS);

	pFixedEndpoints[0] = pOrigEndpoints[0];
	pFixedEndpoints[1] = pOrigEndpoints[1];
	pFixedEndpoints[2] = pOrigEndpoints[2];

	const size_t uPBits = ms_aInfo[pEP->uMode].uPBits;

	if (uPBits) {
		const size_t uNumEP                  = size_t (1 + uPartitions) << 1;
		uint8_t aPVote[BC7_MAX_REGIONS << 1] = {0, 0, 0, 0, 0, 0};
		uint8_t aCount[BC7_MAX_REGIONS << 1] = {0, 0, 0, 0, 0, 0};

		const LDRColorA RGBAPrec      = ms_aInfo[pEP->uMode].RGBAPrec;
		const LDRColorA RGBAPrecWithP = ms_aInfo[pEP->uMode].RGBAPrecWithP;

		for (uint8_t ch = 0; ch < BC7_NUM_CHANNELS; ch++) {
			uint8_t ep = 0;
			for (size_t i = 0; i <= uPartitions; i++) {
				if (RGBAPrec[ch] == RGBAPrecWithP[ch]) {
					pFixedEndpoints[i].A[ch] = pOrigEndpoints[i].A[ch];
					pFixedEndpoints[i].B[ch] = pOrigEndpoints[i].B[ch];
				} else {
					pFixedEndpoints[i].A[ch] = uint8_t (pOrigEndpoints[i].A[ch] >> 1);
					pFixedEndpoints[i].B[ch] = uint8_t (pOrigEndpoints[i].B[ch] >> 1);

					size_t idx = ep++ * uPBits / uNumEP;
					assert (idx < (BC7_MAX_REGIONS << 1));
					aPVote[idx] += pOrigEndpoints[i].A[ch] & 0x01;
					aCount[idx]++;
					idx = ep++ * uPBits / uNumEP;
					assert (idx < (BC7_MAX_REGIONS << 1));
					aPVote[idx] += pOrigEndpoints[i].B[ch] & 0x01;
					aCount[idx]++;
				}
			}
		}

		// Compute the actual pbits we'll use when we encode block. Note this is not
		// rounding the component indices correctly in cases the pbits != a component's LSB.
		int pbits[BC7_MAX_REGIONS << 1];
		for (size_t i = 0; i < uPBits; i++)
			pbits[i] = aPVote[i] > (aCount[i] >> 1) ? 1 : 0;

		// Now calculate the actual endpoints with proper pbits, so error calculations are accurate.
		if (pEP->uMode == 1) {
			// shared pbits
			for (uint8_t ch = 0; ch < BC7_NUM_CHANNELS; ch++) {
				for (size_t i = 0; i <= uPartitions; i++) {
					pFixedEndpoints[i].A[ch] = static_cast<uint8_t> ((pFixedEndpoints[i].A[ch] << 1) | pbits[i]);
					pFixedEndpoints[i].B[ch] = static_cast<uint8_t> ((pFixedEndpoints[i].B[ch] << 1) | pbits[i]);
				}
			}
		} else {
			for (uint8_t ch = 0; ch < BC7_NUM_CHANNELS; ch++) {
				for (size_t i = 0; i <= uPartitions; i++) {
					pFixedEndpoints[i].A[ch] = static_cast<uint8_t> ((pFixedEndpoints[i].A[ch] << 1) | pbits[i * 2 + 0]);
					pFixedEndpoints[i].B[ch] = static_cast<uint8_t> ((pFixedEndpoints[i].B[ch] << 1) | pbits[i * 2 + 1]);
				}
			}
		}
	}
}

float
D3DX_BC7::Refine (const EncodeParams *pEP, size_t uShape, size_t uRotation, size_t uIndexMode) noexcept {
	assert (pEP);
	assert (uShape < BC7_MAX_SHAPES);
	assert (pEP->uMode < c_NumModes);

	const size_t uPartitions = ms_aInfo[pEP->uMode].uPartitions;
	assert (uPartitions < BC7_MAX_REGIONS);

	LDREndPntPair aOrgEndPts[BC7_MAX_REGIONS];
	LDREndPntPair aOptEndPts[BC7_MAX_REGIONS];
	size_t aOrgIdx[NUM_PIXELS_PER_BLOCK];
	size_t aOrgIdx2[NUM_PIXELS_PER_BLOCK];
	size_t aOptIdx[NUM_PIXELS_PER_BLOCK];
	size_t aOptIdx2[NUM_PIXELS_PER_BLOCK];
	float aOrgErr[BC7_MAX_REGIONS];
	float aOptErr[BC7_MAX_REGIONS];

	const LDREndPntPair *aEndPts = &pEP->aEndPts[uShape][0];

	for (size_t p = 0; p <= uPartitions; p++) {
		aOrgEndPts[p].A = Quantize (aEndPts[p].A, ms_aInfo[pEP->uMode].RGBAPrecWithP);
		aOrgEndPts[p].B = Quantize (aEndPts[p].B, ms_aInfo[pEP->uMode].RGBAPrecWithP);
	}

	LDREndPntPair newEndPts1[BC7_MAX_REGIONS];
	FixEndpointPBits (pEP, aOrgEndPts, newEndPts1);

	AssignIndices (pEP, uShape, uIndexMode, newEndPts1, aOrgIdx, aOrgIdx2, aOrgErr);

	OptimizeEndPoints (pEP, uShape, uIndexMode, aOrgErr, newEndPts1, aOptEndPts);

	LDREndPntPair newEndPts2[BC7_MAX_REGIONS];
	FixEndpointPBits (pEP, aOptEndPts, newEndPts2);

	AssignIndices (pEP, uShape, uIndexMode, newEndPts2, aOptIdx, aOptIdx2, aOptErr);

	float fOrgTotErr = 0, fOptTotErr = 0;
	for (size_t p = 0; p <= uPartitions; p++) {
		fOrgTotErr += aOrgErr[p];
		fOptTotErr += aOptErr[p];
	}
	if (fOptTotErr < fOrgTotErr) {
		EmitBlock (pEP, uShape, uRotation, uIndexMode, newEndPts2, aOptIdx, aOptIdx2);
		return fOptTotErr;
	} else {
		EmitBlock (pEP, uShape, uRotation, uIndexMode, newEndPts1, aOrgIdx, aOrgIdx2);
		return fOrgTotErr;
	}
}

void
D3DX_BC7::Encode (uint32_t flags, const HDRColorA *const pIn) noexcept {
	assert (pIn);

	D3DX_BC7 final = *this;
	EncodeParams EP (pIn);
	float fMSEBest     = FLT_MAX;
	uint32_t alphaMask = 0xFF;

	for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; ++i) {
		EP.aLDRPixels[i].r = uint8_t (std::max<float> (0.0f, std::min<float> (255.0f, pIn[i].r * 255.0f + 0.01f)));
		EP.aLDRPixels[i].g = uint8_t (std::max<float> (0.0f, std::min<float> (255.0f, pIn[i].g * 255.0f + 0.01f)));
		EP.aLDRPixels[i].b = uint8_t (std::max<float> (0.0f, std::min<float> (255.0f, pIn[i].b * 255.0f + 0.01f)));
		EP.aLDRPixels[i].a = uint8_t (std::max<float> (0.0f, std::min<float> (255.0f, pIn[i].a * 255.0f + 0.01f)));
		alphaMask &= EP.aLDRPixels[i].a;
	}

	const bool bHasAlpha = (alphaMask != 0xFF);

	for (EP.uMode = 0; EP.uMode < 8 && fMSEBest > 0; ++EP.uMode) {
		if (!(flags & BC_FLAGS_USE_3SUBSETS) && (EP.uMode == 0 || EP.uMode == 2)) {
			// 3 subset modes tend to be used rarely and add significant compression time
			continue;
		}

		if ((flags & BC_FLAGS_FORCE_BC7_MODE6) && (EP.uMode != 6)) {
			// Use only mode 6
			continue;
		}

		if ((!bHasAlpha) && (EP.uMode == 7)) {
			// There is no value in using mode 7 for completely opaque blocks (the other 2 subset modes handle this case for opaque blocks), so skip
			// it for a small perf win.
			continue;
		}

		const size_t uShapes = size_t (1) << ms_aInfo[EP.uMode].uPartitionBits;
		assert (uShapes <= BC7_MAX_SHAPES);

		const size_t uNumRots    = size_t (1) << ms_aInfo[EP.uMode].uRotationBits;
		const size_t uNumIdxMode = size_t (1) << ms_aInfo[EP.uMode].uIndexModeBits;
		// Number of rough cases to look at. reasonable values of this are 1, uShapes/4, and uShapes
		// uShapes/4 gets nearly all the cases; you can increase that a bit (say by 3 or 4) if you really want to squeeze the last bit out
		const size_t uItems = std::max<size_t> (1, uShapes >> 2);
		float afRoughMSE[BC7_MAX_SHAPES];
		size_t auShape[BC7_MAX_SHAPES];

		for (size_t r = 0; r < uNumRots && fMSEBest > 0; ++r) {
			switch (r) {
			case 1:
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					std::swap (EP.aLDRPixels[i].r, EP.aLDRPixels[i].a);
				break;
			case 2:
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					std::swap (EP.aLDRPixels[i].g, EP.aLDRPixels[i].a);
				break;
			case 3:
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					std::swap (EP.aLDRPixels[i].b, EP.aLDRPixels[i].a);
				break;
			default: break;
			}

			for (size_t im = 0; im < uNumIdxMode && fMSEBest > 0; ++im) {
				// pick the best uItems shapes and refine these.
				for (size_t s = 0; s < uShapes; s++) {
					afRoughMSE[s] = RoughMSE (&EP, s, im);
					auShape[s]    = s;
				}

				// Bubble up the first uItems items
				for (size_t i = 0; i < uItems; i++) {
					for (size_t j = i + 1; j < uShapes; j++) {
						if (afRoughMSE[i] > afRoughMSE[j]) {
							std::swap (afRoughMSE[i], afRoughMSE[j]);
							std::swap (auShape[i], auShape[j]);
						}
					}
				}

				for (size_t i = 0; i < uItems && fMSEBest > 0; i++) {
					const float fMSE = Refine (&EP, auShape[i], r, im);
					if (fMSE < fMSEBest) {
						final    = *this;
						fMSEBest = fMSE;
					}
				}
			}

			switch (r) {
			case 1:
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					std::swap (EP.aLDRPixels[i].r, EP.aLDRPixels[i].a);
				break;
			case 2:
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					std::swap (EP.aLDRPixels[i].g, EP.aLDRPixels[i].a);
				break;
			case 3:
				for (size_t i = 0; i < NUM_PIXELS_PER_BLOCK; i++)
					std::swap (EP.aLDRPixels[i].b, EP.aLDRPixels[i].a);
				break;
			default: break;
			}
		}
	}

	*this = final;
}

void
D3DXEncodeBC7 (u8 *pBC, const HDRColorA *pColor, uint32_t flags) noexcept {
	assert (pBC && pColor);
	static_assert (sizeof (D3DX_BC7) == 16, "D3DX_BC7 should be 16 bytes");
	reinterpret_cast<D3DX_BC7 *> (pBC)->Encode (flags, pColor);
}
