#pragma once

#include "Json11/json11.hpp"

#include <simd/simd.h>
#include <SoyVector.h>

#if defined(TARGET_IOS)|| defined(TARGET_OSX)
#import <CoreGraphics/CoreGraphics.h>
#endif

json11::Json::array GetJsonArray(simd_float2 Values);
json11::Json::array GetJsonArray(simd_float3 Values);
json11::Json::array GetJsonArray(vec3f Values);
json11::Json::array GetJsonArray(simd_float4 Values);
json11::Json::array GetJsonArray(matrix_float3x3 Values);
json11::Json::array GetJsonArray(simd_float4x4 Values);
json11::Json::array GetJsonArrayAs4x4(simd_float4x3 Values);
json11::Json::array GetJsonArray(simd_float3x3 Values);
json11::Json::array GetJsonArray(CGSize Values);
json11::Json::array GetJsonArray(CGPoint Values);
matrix_float3x3 Get3x3(const matrix_float4x3& FourThree);
