#include "JsonFunctions.h"


#if defined(TARGET_IOS)|| defined(TARGET_OSX)
matrix_float3x3 Get3x3(const matrix_float4x3& FourThree)
{
	matrix_float3x3 ThreeThree;
	for ( auto r=0;	r<3;	r++ )
		for ( auto c=0;	c<3;	c++ )
 			ThreeThree.columns[c][r] = FourThree.columns[c][r];
 	return ThreeThree;
}
#endif


json11::Json::array GetJsonArray(vec3f Values)
{
	json11::Json::array Array;
	Array.push_back( Values.x );
	Array.push_back( Values.y );
	Array.push_back( Values.z );
	return Array;
}

json11::Json::array GetJsonArray(const float4x4& Values)
{
	json11::Json::array Array;
	auto ValuesArray = Values.GetArray();
	for ( auto i=0;	i<ValuesArray.GetSize();	i++ )
	{
		Array.push_back( ValuesArray[i] );
	}
	return Array;
}

json11::Json::array GetJsonArray(const ArrayBridge<float>&& Values)
{
	json11::Json::array Array;
	for ( auto i=0;	i<Values.GetSize();	i++ )
	{
		Array.push_back( Values[i] );
	}
	return Array;
}

#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(simd_float2 Values)
{
	json11::Json::array Array;
	Array.push_back( Values[0] );
	Array.push_back( Values[1] );
	return Array;
}
#endif

#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(simd_float3 Values)
{
	json11::Json::array Array;
	Array.push_back( Values[0] );
	Array.push_back( Values[1] );
	Array.push_back( Values[2] );
	return Array;
}
#endif


#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(simd_float4 Values)
{
	json11::Json::array Array;
	Array.push_back( Values[0] );
	Array.push_back( Values[1] );
	Array.push_back( Values[2] );
	Array.push_back( Values[3] );
	return Array;
}
#endif


#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(simd_float4x4 Values)
{
	json11::Json::array Array;
	for ( auto r=0;	r<4;	r++ )
		for ( auto c=0;	c<4;	c++ )
			Array.push_back( Values.columns[c][r] );
	return Array;
}
#endif

#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArrayAs4x4(simd_float4x3 Values)
{
	json11::Json::array Array;
	for ( auto r=0;	r<3;	r++ )
		for ( auto c=0;	c<4;	c++ )
			Array.push_back( Values.columns[c][r] );
	Array.push_back(0);
	Array.push_back(0);
	Array.push_back(0);
	Array.push_back(1);
	return Array;
}
#endif


#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(simd_float3x3 Values)
{
	json11::Json::array Array;
	for ( auto r=0;	r<3;	r++ )
		for ( auto c=0;	c<3;	c++ )
			Array.push_back( Values.columns[c][r] );
	return Array;
}
#endif


#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(CGSize Values)
{
	//	gr: should this return a object with Width/Height?
	json11::Json::array Array;
	Array.push_back( Values.width );
	Array.push_back( Values.height );
	return Array;
}
#endif



#if defined(TARGET_IOS)|| defined(TARGET_OSX)
json11::Json::array GetJsonArray(CGPoint Values)
{
	json11::Json::array Array;
	Array.push_back( Values.x );
	Array.push_back( Values.y );
	return Array;
}
#endif 
