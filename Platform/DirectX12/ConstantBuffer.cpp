#include "Simul/Platform/DirectX12/ConstantBuffer.h"
#include "Utilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "SimulDirectXHeader.h"

#include <string>

using namespace simul;
using namespace dx12;
#pragma optimize("",off)

inline bool IsPowerOfTwo( UINT64 n )
{
	return ( ( n & (n-1) ) == 0 && (n) != 0 );
}

inline UINT64 NextMultiple( UINT64 value, UINT64 multiple )
{
   SIMUL_ASSERT( IsPowerOfTwo(multiple) );

	return (value + multiple - 1) & ~(multiple - 1);
}

template< class T > UINT64 BytePtrToUint64( _In_ T* ptr )
{
	return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}



PlatformConstantBuffer::PlatformConstantBuffer() :
	mSlots(0),
	mMaxDescriptors(0),
	mLastFrameIndex(UINT_MAX),
	mCurApplyCount(0)
{
	for (unsigned int i = 0; i < kNumBuffers; i++)
	{
		mUploadHeap[i] = nullptr;
	}
}

PlatformConstantBuffer::~PlatformConstantBuffer()
{
	InvalidateDeviceObjects();
}

static float megas = 0.0f;
void PlatformConstantBuffer::CreateBuffers(crossplatform::RenderPlatform* r, void *addr) 
{
	renderPlatform = r;
	if (addr)
		SIMUL_BREAK("Nacho has to check this");

	// Create the heaps (storage for our descriptors)
	// mBufferSize / 256 (this is the max number of descriptors we can hold with this upload heaps
	// 	Pc alignment != XBOX alignment (?)
	mMaxDescriptors = mBufferSize / (kBufferAlign * mSlots);
	for (unsigned int i = 0; i < 3; i++)
	{
		mHeaps[i].Restore((dx12::RenderPlatform*)renderPlatform, mMaxDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "CBHeap", false);
	}

	// Create the upload heap (the gpu memory that will hold the constant buffer)
	// Each upload heap has 64KB.  (64KB aligned)
	for (unsigned int i = 0; i < 3; i++)
	{
		HRESULT res;
		res = renderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
#ifdef _XBOX_ONE
			IID_GRAPHICS_PPV_ARGS(&mUploadHeap[i])
#else
			IID_PPV_ARGS(&mUploadHeap[i])
#endif	
		);
		SIMUL_ASSERT(res == S_OK);

		// Just debug memory usage
		megas += (float)(mBufferSize) / 1048576.0f;
		
		// If it is an UPLOAD buffer, will be the memory allocated in VRAM too?
		// as far as we know, an UPLOAD buffer has CPU read/write access and GPU read access.

		//SIMUL_COUT << "Total MB Allocated in the GPU: " << std::to_string(megas) << std::endl;
	}
}

void PlatformConstantBuffer::SetNumBuffers(crossplatform::RenderPlatform *r, UINT nContexts,  UINT nMapsPerFrame, UINT nFramesBuffering )
{
	CreateBuffers( r, nullptr);
}


D3D12_CPU_DESCRIPTOR_HANDLE simul::dx12::PlatformConstantBuffer::AsD3D12ConstantBuffer()
{
	// This method is a bit hacky, it basically returns the CPU handle of the last
	// "current" descriptor we will increase the curApply after the aplly that's why 
	// we substract 1 here
	if (mCurApplyCount == 0)
		SIMUL_BREAK("We must apply this cb first");
	return D3D12_CPU_DESCRIPTOR_HANDLE(mHeaps[mLastFrameIndex].GetCpuHandleFromStartAfOffset(mCurApplyCount - 1));
}

void PlatformConstantBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t size,void *addr)
{
	renderPlatform = r;
	
	// Calculate the number of slots this constant buffer will use:
	// mSlots = 256 Aligned size / 256
	mSlots = ((size + (kBufferAlign - 1)) & ~ (kBufferAlign - 1)) / kBufferAlign;
	InvalidateDeviceObjects();
	if(!r)
		return;
	SetNumBuffers( r,1, 64, 2 );
}

void PlatformConstantBuffer::LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex)
{

}

void PlatformConstantBuffer::InvalidateDeviceObjects()
{
	auto rPlat = (dx12::RenderPlatform*)renderPlatform;
	for (unsigned int i = 0; i < kNumBuffers; i++)
	{
		mHeaps[i].Release(rPlat);
		rPlat->PushToReleaseManager(mUploadHeap[i], "ConstantBufferUpload");
	}
}

void  PlatformConstantBuffer::Apply(simul::crossplatform::DeviceContext &deviceContext, size_t size, void *addr)
{
	if (mCurApplyCount >= mMaxDescriptors)
	{
		// This should really be solved by having like some kind of pool? Or allocating more space, something like that
		SIMUL_BREAK("Nacho has to fix this");
	}

	auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	auto curFrameIndex = rPlat->GetIdx();
	// If new frame, update current frame index and reset the apply count
	if (mLastFrameIndex != curFrameIndex)
	{
		mLastFrameIndex = curFrameIndex;
		mCurApplyCount = 0;
	}

	// pDest points at the begining of the uploadHeap, we can offset it! (we created 64KB and each Constart buffer
	// has a minimum size of kBufferAlign)
	UINT8* pDest = nullptr;
	UINT64 offset = (kBufferAlign * mSlots) * mCurApplyCount;	
	const CD3DX12_RANGE mapRange(0, 0);
	HRESULT hResult=mUploadHeap[curFrameIndex]->Map(0, &mapRange, reinterpret_cast<void**>(&pDest));
	if(hResult==S_OK)
	{
		if (pDest)
		{
			memset(pDest + offset, 0, kBufferAlign * mSlots);
			memcpy(pDest + offset, addr, size);
		}
		const CD3DX12_RANGE unMapRange(0, 1);
		mUploadHeap[curFrameIndex]->Unmap(0, &unMapRange);
	}

	// Create a CBV
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation					= mUploadHeap[curFrameIndex]->GetGPUVirtualAddress() + offset;
	cbvDesc.SizeInBytes						= kBufferAlign * mSlots;
	// If we applied this CB more than once this frame, we will be appending another descriptor (thats why we offset)
	D3D12_CPU_DESCRIPTOR_HANDLE handle		= mHeaps[curFrameIndex].GetCpuHandleFromStartAfOffset(mCurApplyCount);
	rPlat->AsD3D12Device()->CreateConstantBufferView(&cbvDesc, handle);

	mCurApplyCount++;
}

void  PlatformConstantBuffer::ActualApply(crossplatform::DeviceContext &deviceContext, crossplatform::EffectPass *currentEffectPass, int slot)
{

}

void PlatformConstantBuffer::Unbind(simul::crossplatform::DeviceContext &)
{

}