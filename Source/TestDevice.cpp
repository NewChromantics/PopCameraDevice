#include "TestDevice.h"
#include "SoyLib/src/SoyMedia.h"
#include "PopCameraDevice.h"


TTestDeviceParams::TTestDeviceParams(json11::Json& Options)
{
	Read( Options, POPCAMERADEVICE_KEY_FORMAT, mColourFormat );
	Read( Options, POPCAMERADEVICE_KEY_FRAMERATE, mFrameRate );
	Read( Options, "Width", mWidth );
	Read( Options, "Height", mHeight );
	Read( Options, POPCAMERADEVICE_KEY_DEBUG, mVerboseDebug );
	
	mRenderSphere |= Read( Options, "SphereX", mSpherePosition.x ); 
	mRenderSphere |= Read( Options, "SphereY", mSpherePosition.y ); 
	mRenderSphere |= Read( Options, "SphereZ", mSpherePosition.z ); 
	mRenderSphere |= Read( Options, "SphereRadius", mSphereRadius ); 
	mRenderSphere |= Read( Options, "VerticalFov", mVerticalFov ); 
	mRenderSphere |= Read( Options, "NearDepth", mNearDepth ); 
	mRenderSphere |= Read( Options, "FarDepth", mFarDepth ); 
	mRenderSphere |= Read( Options, "InvalidDepth", mInvalidDepth ); 
	mRenderSphere |= Read( Options, "ProjectionMatrix", GetArrayBridge(mProjectionMatrix) );
	
	if ( mProjectionMatrix.GetSize() != 0 && mProjectionMatrix.GetSize() != 16 )
	{
		std::stringstream Error;
		Error << "ProjectionMatrix x" << mProjectionMatrix.GetSize() << " but expecting 4x4; ";
		throw Soy::AssertException(Error);
	}
}


void TestDevice::EnumDeviceNames(std::function<void(const std::string&)> Enum)
{
	Enum(TestDevice::DeviceName);
}

TestDevice::TestDevice(json11::Json& Options) :
	mParams	( Options )
{
	GenerateFrame();

	auto Iteration = [this]()
	{
		this->GenerateFrame();
		
		auto Sleepf = 1000.0f / static_cast<float>(this->mParams.mFrameRate);
		auto Sleepms = static_cast<int>(Sleepf);
		std::this_thread::sleep_for( std::chrono::milliseconds(Sleepms));
		
		return this->mRunning;
	};

	mThread.reset( new SoyThreadLambda( std::string(DeviceName), Iteration ) );
}

TestDevice::~TestDevice()
{
	mRunning = false;
	if ( mThread )
	{
		mThread->Stop(true);
		mThread.reset();
	}
}

void TestDevice::GenerateFrame()
{
	if ( mParams.mRenderSphere )
	{
		GenerateSphereFrame(mParams.mSpherePosition.x,mParams.mSpherePosition.y,mParams.mSpherePosition.z,mParams.mSphereRadius);
		return;
	}

	std::shared_ptr<TPixelBuffer> pPixelBuffer(new TDumbPixelBuffer());
	auto& PixelBuffer = dynamic_cast<TDumbPixelBuffer&>(*pPixelBuffer);
	auto& Pixels = PixelBuffer.mPixels;

	SoyTime FrameTime = SoyTime::UpTime();

	//	set the type, alloc pixels, then fill the test planes
	Pixels.mMeta = SoyPixelsMeta(mParams.mWidth,mParams.mHeight,mParams.mColourFormat);
	Pixels.mArray.SetSize(Pixels.mMeta.GetDataSize());

	BufferArray<std::shared_ptr<SoyPixelsImpl>,3> Planes;
	Pixels.SplitPlanes(GetArrayBridge(Planes));

	BufferArray<uint8_t, 4> Components;
	Components.PushBack( mFrameNumber % 256 );
	Components.PushBack(0);
	Components.PushBack(255);
	Components.PushBack(255);
	for (auto p=0;	p<Planes.GetSize();	p++ )
	{
		auto& Plane = *Planes[p];
		Plane.SetPixels(GetArrayBridge(Components));
	}

	json11::Json::object Meta;
	Meta["Hello"] = "World";
	Meta["GeneratedFrameNumer"] = static_cast<int>(mFrameNumber);

	this->PushFrame(pPixelBuffer, FrameTime, Meta);
	mFrameNumber++;
}

void TestDevice::EnableFeature(PopCameraDevice::TFeature::Type Feature,bool Enable)
{
	std::stringstream Error;
	Error << "Test device doesn't support feature " << Feature;
	throw Soy::AssertException(Error.str());
}


json11::Json::array GetJsonArray(ArrayBridge<float>&& Floats)
{
	json11::Json::array Array;
	for ( auto i=0;	i<Floats.GetSize();	i++ )
	{
		Array.push_back( Floats[i] );
	}
	return Array;
}

float4x4 MatrixMultiply4x4(float4x4 a,float4x4 b)
{
	auto a00 = a[0],
	a01 = a[1],
	a02 = a[2],
	a03 = a[3];
	auto a10 = a[4],
	a11 = a[5],
	a12 = a[6],
	a13 = a[7];
	auto a20 = a[8],
	a21 = a[9],
	a22 = a[10],
	a23 = a[11];
	auto a30 = a[12],
	a31 = a[13],
	a32 = a[14],
	a33 = a[15];
	
	// Cache only the current line of the second matrix
	auto b0 = b[0],
	b1 = b[1],
	b2 = b[2],
	b3 = b[3];

	float out[16];
	out[0] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out[1] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out[2] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out[3] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
	
	b0 = b[4];b1 = b[5];b2 = b[6];b3 = b[7];
	out[4] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out[5] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out[6] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out[7] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
	
	b0 = b[8];b1 = b[9];b2 = b[10];b3 = b[11];
	out[8] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out[9] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out[10] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out[11] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
	
	b0 = b[12];b1 = b[13];b2 = b[14];b3 = b[15];
	out[12] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
	out[13] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
	out[14] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
	out[15] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
	return out;
}

float4x4 CreateTranslationMatrix(float x,float y,float z,float w)
{
	return float4x4( 1,0,0,0,	0,1,0,0,	0,0,1,0,	x,y,z,1	);
}

vec3f GetMatrixTranslation(float4x4 Matrix,float DivW)
{
	vec3f xyz;
	xyz.x = Matrix[12];
	xyz.y = Matrix[13];
	xyz.z = Matrix[14];
	if ( DivW )
	{
		auto w = Matrix[15];
		xyz.x /= w;
		xyz.y /= w;
		xyz.z /= w;
	}
	return xyz;
}


float length(const vec3f& Vector)
{
	auto x = Vector.x;
	auto y = Vector.y;
	auto z = Vector.z;
	auto LengthSq = (x*x) + (y*y) + (z*z);
	return sqrtf(LengthSq);
}

#define PI 3.14159265358979323846f
float radians(float Degrees)
{
	auto Rad = (Degrees * PI) / 180.0f;
	return Rad;
}

vec3f normalize(vec3f From,vec3f To)
{
	vec3f Delta;
	Delta.x = To.x - From.x;
	Delta.y = To.y - From.y;
	Delta.z = To.z - From.z;
	float Length = length(Delta);
	Delta *= 1.0f / Length;
	return Delta;
}

//	return hit position, w = hit or miss
vec4f Raymarch(float4x4 ScreenToCameraMatrix,float u,float v,std::function<float(vec3f&)>& GetSceneDistance)
{
	//	part of me thinks i shouldn't have to do this...
	u -= 0.5f;	u *= 2.0f;
	v -= 0.5f;	v *= 2.0f;

	auto ScreenToCamera = [&](float z)
	{
		auto ViewPosMtx = CreateTranslationMatrix(u,v,z,1.0f);
		auto CamPosMtx = MatrixMultiply4x4( ScreenToCameraMatrix, ViewPosMtx );
		
		//	gr: somewhere we're missing perspective correction... need this extra multiply?
		//auto CameraToWorldMtx = float4x4();
		//auto WorldPosMtx = MatrixMultiply4x4( CameraToWorldMtx, CamPosMtx );
		//vec3f WorldPos = GetMatrixTranslation(WorldPosMtx,true);	
		//return WorldPos;
		//	gr: this works, but no perspective correction...
		vec3f CamPos = GetMatrixTranslation(CamPosMtx,true);	
		return CamPos;
	};

	auto RayStart = ScreenToCamera(0.01f);
	auto RayEnd = ScreenToCamera(10.f);
	auto RayDir = normalize(RayEnd,RayStart);
	
	auto GetRayPosition = [&](float WorldZ)
	{
		vec3f Pos = RayDir;
		Pos.x *= WorldZ;
		Pos.y *= WorldZ;
		Pos.z *= WorldZ;
		Pos.x += RayStart.x;
		Pos.y += RayStart.y;
		Pos.z += RayStart.z;
		return Pos;
	};
		
	float RayWorldz = 0.0f;
	auto MaxSteps = 10;
	auto MinDistance = 0.01f;	
	
	for ( auto i=0;	i<MaxSteps;	i++ )
	{
		auto WorldPos = GetRayPosition(RayWorldz);
		auto Distance = GetSceneDistance(WorldPos);
		if ( Distance <= MinDistance )
		{
			return vec4f( WorldPos.x, WorldPos.y, WorldPos.z, 1.0f );
		}
		RayWorldz += Distance;
	}
	
	//	missed
	return vec4f(0,0,0,0);		
}


//	taken from PopEngineCommon js
float4x4 GetProjectionMatrix(float FovVertical,vec2f FocalCenter,float NearDistance,float FarDistance,vec4f ViewRect)
{
	auto GetOpenglFocalLengths = [&](float& fx,float& fy,float& cx,float& cy,float& s)
	{
		auto Aspect = ViewRect[2] / ViewRect[3];
		fy = 1.0f / tanf( radians(FovVertical) / 2.0f);
		//OpenglFocal.fx = 1.0 / Math.tan( Math.radians(FovHorizontal) / 2);
		fx = fy / Aspect;
		cx = FocalCenter.x;
		cy = FocalCenter.y;
		s = 0;
	};

	float OpenglFocal_fx,OpenglFocal_fy,OpenglFocal_cx,OpenglFocal_cy,OpenglFocal_s;
	GetOpenglFocalLengths(OpenglFocal_fx,OpenglFocal_fy,OpenglFocal_cx,OpenglFocal_cy,OpenglFocal_s);
		
	float4x4 Matrix;
	Matrix[0] = OpenglFocal_fx;
	Matrix[1] = OpenglFocal_s;
	Matrix[2] = OpenglFocal_cx;
	Matrix[3] = 0;
	
	Matrix[4] = 0;
	Matrix[5] = OpenglFocal_fy;
	Matrix[6] = OpenglFocal_cy;
	Matrix[7] = 0;
	
	auto Far = FarDistance;
	auto Near = NearDistance;
	
	//	near...far in opengl needs to resovle to -1...1
	//	gr: glDepthRange suggests programmable opengl pipeline is 0...1
	//		not sure about this, but matrix has changed below so 1 is forward on z
	//		which means we can now match the opencv pose (roll is wrong though!)
	//	http://ogldev.atspace.co.uk/www/tutorial12/tutorial12.html
	Matrix[8] = 0;
	Matrix[9] = 0;
	//	gr: this should now work in both ways, but one of them is mirrored.
	//		false SHOULD match old engine style... but is directx
	auto ZForwardIsNegative = false;
	if ( ZForwardIsNegative )
	{
		//	opengl
		Matrix[10] = -(-Near-Far) / (Near-Far);
		Matrix[11] = -1;
	}
	else
	{
		//	directx (also, our old setup!)
		Matrix[10] = (-Near-Far) / (Near-Far);
		Matrix[11] = 1;
	}
	Matrix[12] = 0;
	Matrix[13] = 0;
	Matrix[14] = (2*Far*Near) / (Near-Far);
	Matrix[15] = 0;
	
	return Matrix;
}


float4x4 MatrixInverse4x4(float4x4 Matrix)
{
	auto& m = Matrix;
	float4x4 r;
	
	r[0] = m[5]*m[10]*m[15] - m[5]*m[14]*m[11] - m[6]*m[9]*m[15] + m[6]*m[13]*m[11] + m[7]*m[9]*m[14] - m[7]*m[13]*m[10];
	r[1] = -m[1]*m[10]*m[15] + m[1]*m[14]*m[11] + m[2]*m[9]*m[15] - m[2]*m[13]*m[11] - m[3]*m[9]*m[14] + m[3]*m[13]*m[10];
	r[2] = m[1]*m[6]*m[15] - m[1]*m[14]*m[7] - m[2]*m[5]*m[15] + m[2]*m[13]*m[7] + m[3]*m[5]*m[14] - m[3]*m[13]*m[6];
	r[3] = -m[1]*m[6]*m[11] + m[1]*m[10]*m[7] + m[2]*m[5]*m[11] - m[2]*m[9]*m[7] - m[3]*m[5]*m[10] + m[3]*m[9]*m[6];
	
	r[4] = -m[4]*m[10]*m[15] + m[4]*m[14]*m[11] + m[6]*m[8]*m[15] - m[6]*m[12]*m[11] - m[7]*m[8]*m[14] + m[7]*m[12]*m[10];
	r[5] = m[0]*m[10]*m[15] - m[0]*m[14]*m[11] - m[2]*m[8]*m[15] + m[2]*m[12]*m[11] + m[3]*m[8]*m[14] - m[3]*m[12]*m[10];
	r[6] = -m[0]*m[6]*m[15] + m[0]*m[14]*m[7] + m[2]*m[4]*m[15] - m[2]*m[12]*m[7] - m[3]*m[4]*m[14] + m[3]*m[12]*m[6];
	r[7] = m[0]*m[6]*m[11] - m[0]*m[10]*m[7] - m[2]*m[4]*m[11] + m[2]*m[8]*m[7] + m[3]*m[4]*m[10] - m[3]*m[8]*m[6];
	
	r[8] = m[4]*m[9]*m[15] - m[4]*m[13]*m[11] - m[5]*m[8]*m[15] + m[5]*m[12]*m[11] + m[7]*m[8]*m[13] - m[7]*m[12]*m[9];
	r[9] = -m[0]*m[9]*m[15] + m[0]*m[13]*m[11] + m[1]*m[8]*m[15] - m[1]*m[12]*m[11] - m[3]*m[8]*m[13] + m[3]*m[12]*m[9];
	r[10] = m[0]*m[5]*m[15] - m[0]*m[13]*m[7] - m[1]*m[4]*m[15] + m[1]*m[12]*m[7] + m[3]*m[4]*m[13] - m[3]*m[12]*m[5];
	r[11] = -m[0]*m[5]*m[11] + m[0]*m[9]*m[7] + m[1]*m[4]*m[11] - m[1]*m[8]*m[7] - m[3]*m[4]*m[9] + m[3]*m[8]*m[5];
	
	r[12] = -m[4]*m[9]*m[14] + m[4]*m[13]*m[10] + m[5]*m[8]*m[14] - m[5]*m[12]*m[10] - m[6]*m[8]*m[13] + m[6]*m[12]*m[9];
	r[13] = m[0]*m[9]*m[14] - m[0]*m[13]*m[10] - m[1]*m[8]*m[14] + m[1]*m[12]*m[10] + m[2]*m[8]*m[13] - m[2]*m[12]*m[9];
	r[14] = -m[0]*m[5]*m[14] + m[0]*m[13]*m[6] + m[1]*m[4]*m[14] - m[1]*m[12]*m[6] - m[2]*m[4]*m[13] + m[2]*m[12]*m[5];
	r[15] = m[0]*m[5]*m[10] - m[0]*m[9]*m[6] - m[1]*m[4]*m[10] + m[1]*m[8]*m[6] + m[2]*m[4]*m[9] - m[2]*m[8]*m[5];
	
	auto det = m[0]*r[0] + m[1]*r[4] + m[2]*r[8] + m[3]*r[12];
	for ( auto i=0;	i<16;	i++ )
		r[i] /= det;
	
	return r;
}


void TestDevice::GenerateSphereFrame(float x,float y,float z,float Radius)
{
	Soy::TScopeTimerPrint Timer(__PRETTY_FUNCTION__,10);
	SoyTime FrameTime = SoyTime::UpTime();


	auto Format = SoyPixelsFormat::DepthFloatMetres;
	auto w = mParams.mWidth;
	auto h = mParams.mHeight;
	Array<float> Pixelsf;
	Pixelsf.SetSize( w * h * 1);

	vec4f ViewRect(0,0,w,h);
	float4x4 ProjectionMatrix = GetProjectionMatrix( mParams.mVerticalFov, vec2f(0,0), mParams.mNearDepth, mParams.mFarDepth, ViewRect );
	if ( !mParams.mProjectionMatrix.IsEmpty() )
		ProjectionMatrix = float4x4(mParams.mProjectionMatrix.GetArray());
	
	auto ScreenToCamera = MatrixInverse4x4(ProjectionMatrix);

	auto SetPixel = [&](int x,int y,float Depth)
	{
		int Index = x + (y*w);
		Pixelsf[Index] = Depth;
	};
		
	std::function<float(vec3f&)> SdfSceneDistance = [&](vec3f& Position)
	{
		//	get position in sphere space
		vec3f PositionLocal = Position;
		PositionLocal.x -= x;
		PositionLocal.y -= y;
		PositionLocal.z -= z;
		float Distance = length(PositionLocal);
		return Distance - Radius;
	};
	
	
	for ( int x=0;	x<w;	x++ )
	{
		for ( int y=0;	y<h;	y++ )
		{
			auto u = (x-ViewRect.x) / ViewRect.z;
			auto v = (y-ViewRect.y) / ViewRect.w;
			auto HitPosition4 = Raymarch( ScreenToCamera, u, v, SdfSceneDistance );
			float Depth = mParams.mInvalidDepth;
			if ( HitPosition4.w > 0.0f )
			{
				Depth = length( HitPosition4.xyz() );
			}			 
			SetPixel(x,y,Depth);
		}
	}

	//	
	auto Pixels8 = reinterpret_cast<uint8_t*>(Pixelsf.GetArray());
	SoyPixelsRemote Pixels( Pixels8, w, h, Pixelsf.GetDataSize(), Format );

	std::shared_ptr<TPixelBuffer> pPixelBuffer(new TDumbPixelBuffer());
	auto& PixelBuffer = dynamic_cast<TDumbPixelBuffer&>(*pPixelBuffer);
	PixelBuffer.mPixels.Copy(Pixels);

	json11::Json::object Meta;
	BufferArray<float,4> Sphere4{x,y,z,Radius};
	Meta["Sphere"] = GetJsonArray( GetArrayBridge(Sphere4) );
	Meta["GeneratedFrameNumer"] = static_cast<int>(mFrameNumber);
	Meta["ProjectionMatrix"] = GetJsonArray( GetArrayBridge( ProjectionMatrix.GetArray() ) );

	this->PushFrame(pPixelBuffer, FrameTime, Meta);
	mFrameNumber++;
}
