﻿using UnityEngine;
using System.Collections;					// required for Coroutines
using System.Runtime.InteropServices;		// required for DllImport
using System;								// requred for IntPtr
using System.Text;
using System.Collections.Generic;



/// <summary>
///	Low level interface
/// </summary>
public static class PopCameraDevice
{
	private const string PluginName = "PopCameraDevice";

	//	use byte as System.Char is a unicode char (2 bytes), then convert to Unicode Char
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_EnumCameraDevicesJson([In, Out] byte[] StringBuffer,int StringBufferLength);
	
	//	returns instance id
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern int PopCameraDevice_CreateCameraDeviceWithFormat(byte[] Name, byte[] Format,[In, Out] byte[] ErrorBuffer, int ErrorBufferLength);

	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_FreeCameraDevice(int Instance);

	//	get meta for next frame so buffers can be allocated accordingly
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_GetFrameMetaJson(int Instance,int[] MetaValues,int MetaValuesCount);

	//	returns 0 if no new frame. We split planes because this is sometimes easiest approach, but unity cannot split one big buffer without allocation penalties
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern int PopCameraDevice_PopFrame(int Instance, byte[] Plane0, int Plane0Size, byte[] Plane1, int Plane1Size, byte[] Plane2, int Plane2Size);

	//	returns	version integer as A.BBB.CCCCCC
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern int PopCameraDevice_GetVersion();

	//	DLL cleanup
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_Cleanup();
		

	//	PixelFormat_WidthxHeight@FrameRate
	//	eg RGBA_640x480@30
	//		Unknown@60
	//		NV12_1024x1024
	public static string CreateFormatString(string PixelFormat,int Width=0,int Height=0,int FrameRate=0)
	{
		if (String.IsNullOrEmpty(PixelFormat))
			PixelFormat = "Unknown";

		if (Width != 0 || Height != 0)
			PixelFormat += "_" + Width + "x" + Height;
		if (FrameRate != 0)
			PixelFormat += "@" + FrameRate;
		return PixelFormat;
	}

	[System.Serializable]
	public struct FrameMeta
	{
		public string	PixelFormat;
		public int		TimeMs;
		public int		Width;
		public int		Height;
		public int[]	PlaneSizes;			//	size of each plane in bytes
		public int		FrameRate;
		public int		DeviceFrameRate;	//	the speed the camera is supposed to deliver at
	};

	[System.Serializable]
	public struct DeviceMeta
	{
		public string	Serial;		//	unique identifier, sometimes prefixed
		public string[]	Formats;	//	All availible formats, eg. RGBA_640x480@30
	};

	//	copied directly from https://github.com/SoylentGraham/SoyLib/blob/master/src/SoyPixels.h#L16
	public enum SoyPixelsFormat
	{
		Invalid			= 0,
		Greyscale,
		GreyscaleAlpha,		//	png has this for 2 channel, so why not us!
		RGB,
		RGBA,
		ARGB,
		BGRA,
		BGR,

		//	non integer-based channel counts
		KinectDepth,		//	16 bit, so "two channels". 13 bits of depth, 3 bits of user-index
		FreenectDepth10bit,	//	16 bit
		FreenectDepth11bit,	//	16 bit
		FreenectDepthmm,	//	16 bit
	

		//	http://stackoverflow.com/a/6315159/355753
		//	bi planar is luma followed by chroma.
		//	Full range is 0..255
		//	video LUMA range 16-235 (chroma is still 0-255)	http://stackoverflow.com/a/10129300/355753
		//	Y=luma	uv=ChromaUv
		
		//	gr: I want to remove video/full/ntsc etc and have that explicit in a media format
		//	http://www.chromapure.com/colorscience-decoding.asp
		//	the range counts towards luma & chroma
		//	video = NTSC
		//	full = Rec. 709
		//	SMPTE-C = SMPTE 170M 

		//	gr: naming convention; planes seperated by underscore
		Yuv_8_88_Full,		//	8 bit Luma, interleaved Chroma uv plane (uv is half size... reflect this somehow in the name!)
		Yuv_8_88_Ntsc,		//	8 bit Luma, interleaved Chroma uv plane (uv is half size... reflect this somehow in the name!)
		Yuv_8_88_Smptec,		//	8 bit Luma, interleaved Chroma uv plane (uv is half size... reflect this somehow in the name!)
		Yuv_8_8_8_Full,		//	luma, u, v seperate planes (uv is half size... reflect this somehow in the name!)
		Yuv_8_8_8_Ntsc,	//	luma, u, v seperate planes (uv is half size... reflect this somehow in the name!)
		Yuv_8_8_8_Smptec,   //	luma, u, v seperate planes (uv is half size... reflect this somehow in the name!)

		//	gr: YUY2: LumaX,ChromaU,LumaX+1,ChromaV (4:2:2 ratio, 8 bit)
		//		we still treat it like a 2 component format so dimensions match original
		//		(maybe should be renamed YYuv_88 for this reason)
		Uvy_844_Full,
		Yuv_844_Full,
		Yuv_844_Ntsc,
		Yuv_844_Smptec,

		//	https://stackoverflow.com/a/22793325/355753
		//	4:2:2, apple call this yuvs
		Yuv_844_Full,
		Yuv_844_Ntsc,
		Yuv_844_Smptec,

		ChromaUV_8_8,		//	8 bit plane, 8 bit plane
		ChromaUV_88,		//	16 bit interleaved plane
		ChromaU_8,			//	single plane
		ChromaV_8,			//	single plane
		ChromaUV_44,		//	8 bit interleaved plane
		

		//	https://github.com/ofTheo/ofxKinect/blob/ebb9075bcb5ab2543220b4dec598fd73cec40904/libs/libfreenect/src/cameras.c
		//	kinect (16bit?) yuv. See if its the same as a standard one 
		uyvy,
		/*
		 int u  = raw_buf[2*i];
			int y1 = raw_buf[2*i+1];
			int v  = raw_buf[2*i+2];
			int y2 = raw_buf[2*i+3];
			int r1 = (y1-16)*1164/1000 + (v-128)*1596/1000;
			int g1 = (y1-16)*1164/1000 - (v-128)*813/1000 - (u-128)*391/1000;
			int b1 = (y1-16)*1164/1000 + (u-128)*2018/1000;
			int r2 = (y2-16)*1164/1000 + (v-128)*1596/1000;
			int g2 = (y2-16)*1164/1000 - (v-128)*813/1000 - (u-128)*391/1000;
			int b2 = (y2-16)*1164/1000 + (u-128)*2018/1000;
			CLAMP(r1)
			CLAMP(g1)
			CLAMP(b1)
			CLAMP(r2)
			CLAMP(g2)
			CLAMP(b2)
			proc_buf[3*i]  =r1;
			proc_buf[3*i+1]=g1;
			proc_buf[3*i+2]=b1;
			proc_buf[3*i+3]=r2;
			proc_buf[3*i+4]=g2;
			proc_buf[3*i+5]=b2;		 */

		Luma_Ntsc,			//	ntsc-range luma plane
		Luma_Smptec,		//	Smptec-range luma plane

		//	2 planes, RGB (palette+length8) Greyscale (indexes)
		//	warning, palette's first byte is the size of the palette! need to work out how to auto skip over this when extracting the plane...
		Palettised_RGB_8,
		Palettised_RGBA_8,
		
		//	to distinguish from RGBA etc
		Float1,
		Float2,
		Float3,
		Float4,
		
		
		//	shorthand names
		//	http://www.fourcc.org/yuv.php
		Luma_Full		= Greyscale,	//	Luma plane of a YUV
		Nv12			= Yuv_8_88_Full,
		I420			= Yuv_8_8_8_Full,
				
		Count=99,
	}

	private enum MetaIndex
	{
		PlaneCount = 0,

		Plane0_Width,
		Plane0_Height,
		Plane0_ComponentCount,
		Plane0_SoyPixelsFormat,
		Plane0_PixelDataSize,

		Plane1_Width,
		Plane1_Height,
		Plane1_ComponentCount,
		Plane1_SoyPixelsFormat,
		Plane1_PixelDataSize,

		Plane2_Width,
		Plane2_Height,
		Plane2_ComponentCount,
		Plane2_SoyPixelsFormat,
		Plane2_PixelDataSize,
	};

	public static List<string> EnumCameraDevices()
	{
		var StringBuffer = new byte[1000];
		PopCameraDevice_EnumCameraDevices( StringBuffer, StringBuffer.Length );

		//	split strings
		var Delin = StringBuffer[0];
		string CurrentName = "";
		var Names = new List<string>();
		System.Action FinishCurrentName = ()=>
		{
			if ( String.IsNullOrEmpty(CurrentName) )
				return;
			Names.Add(CurrentName);
			CurrentName = "";
		};

		//	split at delin or when we hit a terminator
		for ( var i=1;	i<StringBuffer.Length;	i++ )
		{
			var Char8 = StringBuffer[i];
			if ( Char8 == '\0' )
			{
				FinishCurrentName();
				break;
			}
			if ( Char8 == Delin )
			{
				FinishCurrentName();
				continue;
			}
			var UnicodeChar = System.Convert.ToChar(Char8);
			CurrentName += UnicodeChar;
		}
		FinishCurrentName();

		return Names;
	}
	

	public class Device : IDisposable
	{
		int? Instance = null;

		//	cache once to avoid allocating each frame
		List<byte[]> PlaneCaches;
		byte[] UnusedBuffer = new byte[1];

		public Device(string DeviceName)
		{
			var DeviceNameAscii = System.Text.ASCIIEncoding.ASCII.GetBytes(DeviceName);
			Instance = PopCameraDevice_CreateCameraDevice(DeviceNameAscii);
			if ( Instance.Value <= 0 )
				throw new System.Exception("Failed to create Camera device with name " + DeviceName);
		}
		~Device()
		{
			Dispose();
		}

		public void Dispose()
		{
			if ( Instance.HasValue )
				PopCameraDevice_FreeCameraDevice( Instance.Value );
			Instance = null;
		}

		TextureFormat GetTextureFormat(int ComponentCount)
		{
			switch(ComponentCount)
			{
				case 1:	return TextureFormat.R8;
				case 2:	return TextureFormat.RG16;
				case 3:	return TextureFormat.RGB24;
				case 4:	return TextureFormat.ARGB32;
				default:
					throw new System.Exception("Don't know what format to use for component count " + ComponentCount);
			}
		}

		Texture2D AllocTexture(Texture2D Plane,int Width, int Height,int ComponentCount)
		{
			var Format = GetTextureFormat( ComponentCount );
			if ( Plane != null )
			{
				if ( Plane.width != Width )
					Plane = null;
				else if ( Plane.height != Height )
					Plane = null;
				else if ( Plane.format != Format )
					Plane = null;
			}

			if ( !Plane )
			{
				var MipMap = false;
				Plane = new Texture2D( Width, Height, Format, MipMap );
			}

			return Plane;
		}

		void AllocListToSize<T>(ref List<T> Array,int Size)
		{
			if ( Array == null )
				Array = new List<T>();
			while ( Array.Count < Size )
				Array.Add( default(T) );
		}

		//	returns if changed
		public bool GetNextFrame(ref List<Texture2D> Planes,ref List<SoyPixelsFormat> PixelFormats)
		{
			var MetaValues = new int[100];
			PopCameraDevice_GetMeta( Instance.Value, MetaValues, MetaValues.Length );
			var PlaneCount = MetaValues[(int)MetaIndex.PlaneCount];
			if ( PlaneCount <= 0 )
			{
				//Debug.Log("No planes (" + PlaneCount +")");
				PixelFormats = null;
				return false;
			}

			AllocListToSize( ref Planes, PlaneCount );
			AllocListToSize( ref PixelFormats, PlaneCount );
			AllocListToSize( ref PlaneCaches, PlaneCount );

			
			if ( PlaneCount >= 1 )	PixelFormats[0] = (SoyPixelsFormat)MetaValues[(int)MetaIndex.Plane0_SoyPixelsFormat];
			if ( PlaneCount >= 2 )	PixelFormats[1] = (SoyPixelsFormat)MetaValues[(int)MetaIndex.Plane1_SoyPixelsFormat];
			if ( PlaneCount >= 3 )	PixelFormats[2] = (SoyPixelsFormat)MetaValues[(int)MetaIndex.Plane2_SoyPixelsFormat];

			//	alloc textures so we have data to write to
			if ( PlaneCount >= 1 )	Planes[0] = AllocTexture( Planes[0], MetaValues[(int)MetaIndex.Plane0_Width], MetaValues[(int)MetaIndex.Plane0_Height], MetaValues[(int)MetaIndex.Plane0_ComponentCount] );
			if ( PlaneCount >= 2 )	Planes[1] = AllocTexture( Planes[1], MetaValues[(int)MetaIndex.Plane1_Width], MetaValues[(int)MetaIndex.Plane1_Height], MetaValues[(int)MetaIndex.Plane1_ComponentCount] );
			if ( PlaneCount >= 3 )	Planes[2] = AllocTexture( Planes[2], MetaValues[(int)MetaIndex.Plane2_Width], MetaValues[(int)MetaIndex.Plane2_Height], MetaValues[(int)MetaIndex.Plane2_ComponentCount] );

			for ( var p=0;	p<PlaneCount;	p++ )
			{
				if ( PlaneCaches[p] != null )
					continue;
				if ( !Planes[p] )
					continue;
				 PlaneCaches[p] = Planes[p].GetRawTextureData();
			}

			//	read frame bytes
			var Plane0Data = (PlaneCaches.Count >=1 && PlaneCaches[0]!=null) ? PlaneCaches[0] : UnusedBuffer;
			var Plane1Data = (PlaneCaches.Count >=2 && PlaneCaches[1]!=null) ? PlaneCaches[1] : UnusedBuffer;
			var Plane2Data = (PlaneCaches.Count >=3 && PlaneCaches[2]!=null) ? PlaneCaches[2] : UnusedBuffer;
			var PopResult = PopCameraDevice_PopFrame( Instance.Value, Plane0Data, Plane0Data.Length, Plane1Data, Plane1Data.Length, Plane2Data, Plane2Data.Length );
			if ( PopResult == 0 )
				return false;
			
			//	update texture
			for ( var p=0;	p<PlaneCount;	p++ )
			{
				if ( !Planes[p] )
					continue;

				Planes[p].LoadRawTextureData( PlaneCaches[p] );
				Planes[p].Apply();
			}

			return true;
		}

	}
}

