Shader "Unlit/Yuv"
{
	Properties
	{
		LumaTexture ("LumaTexture", 2D) = "white" {}
		[Enum(Invalid,0,Greyscale,1,RGBA,3,YYuv_8888_Full,6,YYuv_8888_Ntsc,7)]LumaFormat("LumaFormat",int) = 0
		ChromaUTexture ("ChromaUTexture", 2D) = "black" {}
		[Enum(Debug,999,Invalid,0,ChromaUV_88,11,ChromaVU_88,12,Chroma_U,9,Chroma_V,10)]ChromaUFormat("ChromaUFormat",int) = 0
		ChromaVTexture ("ChromaVTexture", 2D) = "black" {}
		[Enum(Debug,999,Invalid,0,Chroma_U,9,Chroma_V,10)]ChromaVFormat("ChromaVFormat",int) = 0

		[Header(NTSC etc colour settings)]LumaMin("LumaMin", Range(0,255)) = 16
		LumaMax("LumaMax", Range(0,255)) = 253
		ChromaVRed("ChromaVRed", Range(-2,2)) = 1.5958
		ChromaUGreen("ChromaUGreen", Range(-2,2)) = -0.39173
		ChromaVGreen("ChromaVGreen", Range(-2,2)) = -0.81290
		ChromaUBlue("ChromaUBlue", Range(-2,2)) = 2.017
		[Toggle]Flip("Flip", Range(0,1)) = 1
		[Toggle]EnableChroma("EnableChroma", Range(0,1)) = 1
		DepthMaxmm("DepthMaxmm", Range(0,40000)) = 10000
	}
	SubShader
	{
		Tags { "RenderType"="Opaque" }
		LOD 100


		Pass
		{
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			
			#include "UnityCG.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float4 vertex : SV_POSITION;
			};

			sampler2D LumaTexture;
			sampler2D ChromaUTexture;
			sampler2D ChromaVTexture;
			float4 LumaTexture_TexelSize;
			int LumaFormat;
			int ChromaUFormat;
			int ChromaVFormat;

			float LumaMin;
			float LumaMax;
			float ChromaVRed;
			float ChromaUGreen;
			float ChromaVGreen;
			float ChromaUBlue;
			float DepthMaxmm;


			//	SoyPixelsFormat's 
			//	see https://github.com/SoylentGraham/SoyLib/blob/master/src/SoyPixels.h#L16
			//	for their internal usage
			//	match c# PopCameraDevice.SoyPixelsFormat for index
#define Debug			999
#define Invalid			0
#define Greyscale		1
#define RGB				2
#define RGBA			3
#define BGRA			4
#define BGR				5
#define YYuv_8888_Full	6
#define YYuv_8888_Ntsc	7
#define Depth16mm		8
#define Chroma_U		9
#define Chroma_V		10
#define ChromaUV_88		11
#define ChromaVU_88		12

			float Flip;
			float EnableChroma;
			#define FLIP	( Flip > 0.5f )	
			#define ENABLE_CHROMA	( EnableChroma > 0.5f )
			
			v2f vert (appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = v.uv;

				if ( FLIP )
					o.uv.y = 1 - o.uv.y;

				return o;
			}

			float Range(float Min, float Max, float Value)
			{
				return (Value - Min) / (Max - Min);
			}

			float4 NormalToRedGreenBlue(float Normal, float Alpha = 1)
			{
				if (Normal < 0)
					return float4(0, 0, 0, Alpha);
				if (Normal > 1)
					return float4(1, 1, 1, Alpha);

				if (Normal < 0.25)
				{
					Normal = Range(0, 0.25, Normal);
					return float4(1, Normal, 0, Alpha);
				}
				else if (Normal < 0.50)
				{
					Normal = Range(0.25, 0.50, Normal);
					return float4(1 - Normal, 1, 0, Alpha);
				}
				else if (Normal < 0.75)
				{
					Normal = Range(0.50, 0.75, Normal);
					return float4(0, 1, Normal, Alpha);
				}
				else
				{
					Normal = Range(0.75, 1.0, Normal);
					return float4(0, 1- Normal, 1, Alpha);
				}
			}

			float2 GetChromaUv_88(float2 uv)
			{
				//	uv in one plane but organised as 2-component texture
				float2 ChromaUV = tex2D(ChromaUTexture, uv).xy;
				return ChromaUV;
			}

			float2 GetChromaVu_88(float2 uv)
			{
				//	uv in one plane but organised as 2-component texture
				float2 ChromaUV = tex2D(ChromaUTexture, uv).yx;
				return ChromaUV;
			}

			float2 GetChromaUv_Debug(float2 uv)
			{
				return uv;
			}

			float2 GetChromaUv_8_8(float2 uv)
			{
				//	seperate planes
				float ChromaU = tex2D(ChromaUTexture, uv);
				float ChromaV = tex2D(ChromaVTexture, uv);
				return float2(ChromaU, ChromaV);
			}

			void GetLumaChromaUv_8888(float2 uv,out float Luma,out float2 ChromaUV)
			{
				//	data is 
				//	LumaX+0, ChromaU+0, LumaX+1, ChromaV+0
				//	2 lumas for each chroma 
				float2 x = fmod(uv.x * LumaTexture_TexelSize.z, 2.0);
				float uRemainder = x * LumaTexture_TexelSize.x;
				
				//	uv0 = left pixel of pair
				float2 uv0 = uv;
				uv0.x -= uRemainder;
				//	uv1 = right pixel of pair
				float2 uv1 = uv0;
				uv1.x += LumaTexture_TexelSize.x;

				//	just in case, sample from middle of texel!
				uv0.x += LumaTexture_TexelSize.x * 0.5;
				uv1.x += LumaTexture_TexelSize.x * 0.5;

				float ChromaU = tex2D(LumaTexture, uv0).y;
				float ChromaV = tex2D(LumaTexture, uv1).y;
				Luma = tex2D(LumaTexture, uv).x;
				ChromaUV = float2(ChromaU, ChromaV);
			}

			float3 GetDepthColour(float Depthf)
			{
				float Depth = Luma4.x * 65535.0;
				//	detect zero and render black
				if (Depth == 0)
					return NormalToRedGreenBlue(-1);
				float DepthNormal = Depth / DepthMaxmm;
				return NormalToRedGreenBlue(DepthNormal);
			}

			float4 MergeColourAndDepth(float3 Rgb, float2 DepthAndValid)
			{
				float3 DepthRgb = GetDepthColour(Depthf);
				Rgb = lerp(Rgb, DepthRgb, DepthAndValid.y * 0.5);
				return float4(Rgb, 1);
			}

			fixed4 frag (v2f i) : SV_Target
			{
				float4 Luma4 = tex2D(LumaTexture, i.uv);

				float2 DepthAndValid = float2(0, 0);
				if (ChromaUFormat == Depth16mm)
				{
					float Depthf = tex2D(ChromaUTexture, i.uv);
					DepthAndValid = float2(Depthf, 1);
				}

				//	look out for when we have luma+chroma+chroma?
				if (LumaFormat == Greyscale)
					return MergeColourAndDepth(Luma4.xxx, DepthAndValid);
				if (LumaFormat == RGB)
					return MergeColourAndDepth(Luma4, DepthAndValid);
				if (LumaFormat == RGBA)
					return MergeColourAndDepth(Luma4, DepthAndValid);
				if (LumaFormat == BGRA  || LumaFormat == BGR)
					return MergeColourAndDepth(Luma4.yxw, DepthAndValid);//	ARGB data, but is BGRA. z is alpha it seems

				//	gr: this is expected to be 16bit texture format, so 1 component
				if (LumaFormat == Depth16mm)
					return GetDepthColour(Luma4.x);

				// sample the texture
				float Luma = Luma4.x;
				float2 ChromaUV = float2(0, 0);
				if ( LumaFormat == YYuv_8888_Full || LumaFormat == YYuv_8888_Ntsc )
				{
					GetLumaChromaUv_8888(i.uv, Luma, ChromaUV);
				}
				else if ( ChromaUFormat == Debug )
				{
					ChromaUV = GetChromaUv_Debug(i.uv);
				}
				else if ( ChromaUFormat == ChromaUV_88 )
				{
					ChromaUV = GetChromaUv_88(i.uv);
				}
				else if ( ChromaUFormat == Chroma_U && ChromaVFormat == Chroma_V  )
				{
					ChromaUV = GetChromaUv_8_8(i.uv);
				}
				

				//	0..1 to -0.5..0.5
				ChromaUV -= 0.5;
				
				//	override for quick debug
				if ( !ENABLE_CHROMA )
				{
					ChromaUV = float2(0, 0);
				}

				//	set luma range
				Luma = lerp(LumaMin/255, LumaMax/255, Luma);
				float3 Rgb;
				Rgb.x = Luma + (ChromaVRed * ChromaUV.y);
				Rgb.y = Luma + (ChromaUGreen * ChromaUV.x) + (ChromaVGreen * ChromaUV.y);
				Rgb.z = Luma + (ChromaUBlue * ChromaUV.x);

				return float4( Rgb.xyz, 1);
			}
			ENDCG
		}
	}
}
