using UnityEngine;
using System.Collections;					// required for Coroutines
using System.Runtime.InteropServices;		// required for DllImport
using System;								// requred for IntPtr
using System.Text;
using System.Collections.Generic;
using PopX;		//	for PopX.PixelFormat, replace this and provide your own pixelformat if you want to remove the dependency


/// <summary>
///	Low level interface
/// </summary>
public static class PopCameraDevice
{
#if UNITY_UWP
	private const string PluginName = "PopCameraDevice.Uwp";
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
	private const string PluginName = "Assets/PopCameraDevice/PopCameraDevice_Osx.framework/Versions/A/PopCameraDevice_Osx";
#elif UNITY_IOS
	private const string PluginName = "__Internal";
#else
	private const string PluginName = "PopCameraDevice";
#endif
	
	//	use byte as System.Char is a unicode char (2 bytes), then convert to Unicode Char
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_EnumCameraDevicesJson([In, Out] byte[] StringBuffer,int StringBufferLength);
	
	//	returns instance id
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern Int32 PopCameraDevice_CreateCameraDevice(byte[] Name, byte[] OptionsJson,[In, Out] byte[] ErrorBuffer, Int32 ErrorBufferLength);

	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_FreeCameraDevice(Int32 Instance);

	//	returns -1 if no new frame
	//	Fills in meta with JSON about frame
	//	Next frame is not deleted. 
	//	Meta will list other frames buffered up, so to skip to latest frame, Pop without buffers
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern Int32 PopCameraDevice_PeekNextFrame(Int32 Instance,[In, Out] byte[] JsonBuffer,int JsonBufferLength);

	//	returns -1 if no new frame
	//	Deletes frame.
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern Int32 PopCameraDevice_PopNextFrame(Int32 Instance, [In, Out] byte[] JsonBuffer, int JsonBufferLength, byte[] Plane0, Int32 Plane0Size, byte[] Plane1, Int32 Plane1Size, byte[] Plane2, Int32 Plane2Size);

	//	returns	version integer as A.BBB.CCCCCC
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern Int32 PopCameraDevice_GetVersion();

	//	DLL cleanup
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_Cleanup();

	//	For ARFrameProxy, call this to pass in an ARFrame pointer (from version 2.0.1)
	[DllImport(PluginName, CallingConvention = CallingConvention.Cdecl)]
	private static extern void PopCameraDevice_ReadNativeHandle(Int32 Instance,System.IntPtr Handle);

	//	PixelFormat^WidthxHeight@FrameRate
	//	eg RGBA^640x480@30
	//		Unknown@60
	//		NV12^1024x1024
	public static string CreateFormatString(string PixelFormat,int Width=0,int Height=0,int FrameRate=0)
	{
		if (String.IsNullOrEmpty(PixelFormat))
			PixelFormat = "Unknown";

		if (Width != 0 || Height != 0)
			PixelFormat += "^" + Width + "x" + Height;
		if (FrameRate != 0)
			PixelFormat += "@" + FrameRate;
		return PixelFormat;
	}

	[System.Serializable]
	public struct PlaneMeta
	{
		//	gr: we don't NEED to have this external dependency of a type, https://github.com/NewChromantics/PopUnityYuvShader
		public PixelFormat	PixelFormat { get { return (PixelFormat)Enum.Parse(typeof(PixelFormat), Format); } }
		public string		Format;
		public int			Width;
		public int			Height;
		public int			DataSize;
		public int			Channels;
	};

	[System.Serializable]
	public struct FrameMeta
	{
		public int				PlaneCount { get { return (Planes != null) ? Planes.Count : 0; } }
		public List<PlaneMeta>	Planes;
	};

	[System.Serializable]
	public struct DeviceMeta
	{
		public string	Serial;		//	unique identifier, sometimes prefixed
		public string[]	Formats;    //	All availible formats, eg. RGBA^640x480@30. Some devices report nothing (so can't pick explicit formats, or are not known until frame arrives)
	};

	[System.Serializable]
	private struct DeviceMetas
	{
		public List<DeviceMeta> Devices;
	};
	
	[System.Serializable]
	public class DeviceParams
	{
		//	these are all optional in the API (if passed with a raw json string)
		//	but in c# we want to document all availible options
		//	todo: constructor with custom struct isntead of this default one
		public bool		SkipFrames = true;
		public int		FrameRate = 30;
		public string	Format = "Yuv_8_88";		//	see Pop.PixelFormat
		public string	DepthFormat = "Depth16mm";	//	see Pop.PixelFormat
		public bool		Debug = false;
		public bool		SplitPlanes = true;
		
		//	arkit
		public bool		HdrColour = false;
		public bool		AutoFocus = true;
		public bool		PlaneTracking = true;
		public bool		FaceTracking = false;
		public bool		LightEstimation = false;
		public bool		BodyTracking = false;
		public bool		Segmentation = false;
		public bool		ResetTracking = false;
		public bool		ResetAnchors = false;
	};


	static public string GetString(byte[] Ascii)
	{
		var String = System.Text.ASCIIEncoding.ASCII.GetString(Ascii);
		var TerminatorPos = String.IndexOf('\0');
		if (TerminatorPos >= 0)
			String = String.Substring(0, TerminatorPos);
		return String;
	}

	public static List<DeviceMeta> EnumCameraDevices()
	{
		var JsonStringBuffer = new byte[1000];
		PopCameraDevice_EnumCameraDevicesJson(JsonStringBuffer, JsonStringBuffer.Length );
		var JsonString = GetString(JsonStringBuffer);
		var Metas = JsonUtility.FromJson<DeviceMetas>(JsonString);
		return Metas.Devices;
	}
	

	public class Device : IDisposable
	{
		int? Instance = null;

		//	cache once to avoid allocating each frame
		List<byte[]> PlaneCaches;
		byte[] UnusedBuffer = new byte[1];

		public Device(string DeviceName,DeviceParams Params=null)
		{
			var ParamsJson = (Params==null) ? "{}" : JSONUtility.stringify(Params);
			var DeviceNameAscii = System.Text.ASCIIEncoding.ASCII.GetBytes(DeviceName+"\0");
			var ParamsJsonAscii = System.Text.ASCIIEncoding.ASCII.GetBytes(ParamsJson + "\0");
			var ErrorBuffer = new byte[100];
			Instance = PopCameraDevice_CreateCameraDevice(DeviceNameAscii, ParamsJsonAscii, ErrorBuffer, ErrorBuffer.Length);
			var Error = GetString(ErrorBuffer);
			if ( Instance.Value <= 0 )
				throw new System.Exception("Failed to create Camera device with name " + DeviceName + "; " + Error);
			if (!String.IsNullOrEmpty(Error))
			{
				Debug.LogWarning("Created PopCameraDevice(" + Instance.Value + ") but error was not empty (length = "+ Error.Length+") " + Error );
			}
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

		static TextureFormat GetTextureFormat(int ComponentCount,PopX.PixelFormat PixelFormat)
		{
			//	special/currently unhandled case, c++ code gives out 16bit, 1 component data
			if (PixelFormat == PixelFormat.Depth16mm)
				return TextureFormat.R16;
				
			if (PixelFormat == PopX.PixelFormat.DepthFloatMetres)
				return TextureFormat.RFloat;

			switch (ComponentCount)
			{
				case 1:	return TextureFormat.R8;
				case 2:	return TextureFormat.RG16;
				case 3:	return TextureFormat.RGB24;
				case 4:	return TextureFormat.ARGB32;
				default:
					throw new System.Exception("Don't know what format to use for component count " + ComponentCount);
			}
		}

		Texture2D AllocTexture(Texture2D Plane,int Width, int Height,int ComponentCount, PixelFormat PixelFormat)
		{
			var Format = GetTextureFormat( ComponentCount, PixelFormat);
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
				Plane.filterMode = FilterMode.Point;
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

		//	returns value if we changed the texture[s]
		public int? GetNextFrame(ref List<Texture2D> Planes,ref List<PixelFormat> PixelFormats)
		{
			var JsonBuffer = new Byte[1000];
			var NextFrameTime = PopCameraDevice_PeekNextFrame( Instance.Value, JsonBuffer, JsonBuffer.Length );
			
			//	no frame pending
			if (NextFrameTime < 0)
				return null;

			var Json = GetString(JsonBuffer);
			//Debug.Log("PopCameraDevice_PeekNextFrame:" + Json);
			var Meta = JsonUtility.FromJson<FrameMeta>(Json);

			var PlaneCount = Meta.Planes.Count;
			//	throw here? should there ALWAYS be a plane?
			if (PlaneCount <= 0)
				throw new System.Exception("Not expecting zero planes");

			AllocListToSize( ref Planes, PlaneCount );
			AllocListToSize( ref PixelFormats, PlaneCount );
			AllocListToSize( ref PlaneCaches, PlaneCount );

			for (var p = 0; p < PlaneCount; p++)
			{
				var PlaneMeta = Meta.Planes[p];
				PixelFormats[p] = PlaneMeta.PixelFormat;
				//	alloc textures so we have data to write to
				var OldTexture = Planes[p];
				Planes[p] = AllocTexture(Planes[p], PlaneMeta.Width, PlaneMeta.Height, PlaneMeta.Channels, PixelFormats[p] );

				//	clear cache
				if (Planes[p] != OldTexture)
					PlaneCaches[p] = null;

				//	setup plane cache that matches texture buffer size (unity is very picky, it even rejects the correct size sometimes :)
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
			var PopFrameTime = PopCameraDevice_PopNextFrame( Instance.Value, null, 0, Plane0Data, Plane0Data.Length, Plane1Data, Plane1Data.Length, Plane2Data, Plane2Data.Length );
			if (PopFrameTime != NextFrameTime)
				throw new System.Exception("Popped frame time(" + PopFrameTime + ") didn't match expected (peeked) time (" + NextFrameTime + ")");
			
			//	update texture
			for ( var p=0;	p<PlaneCount;	p++ )
			{
				if ( !Planes[p] )
					continue;

				Planes[p].LoadRawTextureData( PlaneCaches[p] );
				Planes[p].Apply();
			}

			return NextFrameTime;
		}

		//	todo: be more strict c# side!
		public void ReadNativeHandle(System.IntPtr Handle)
		{
			PopCameraDevice_ReadNativeHandle( Instance.Value, Handle );
		}
	}
}

