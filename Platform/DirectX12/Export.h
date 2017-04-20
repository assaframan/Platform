#ifndef SIMUL_DIRECTX12_EXPORT_H
#define SIMUL_DIRECTX12_EXPORT_H
#if defined(_MSC_VER) && !defined(SIMUL_DIRECTX12_DLL)
	#ifdef _DEBUG
		#ifdef _DLL
			#pragma comment(lib,"SimulDirectX12_MDd")
		#else
			#pragma comment(lib,"SimulDirectX12_MTd")
		#endif
	#else
		#ifdef _DLL
			#pragma comment(lib,"SimulDirectX12_MD")
		#else
			#pragma comment(lib,"SimulDirectX12_MT")
		#endif
	#endif
#endif

#if defined(SIMUL_DYNAMIC_LINK) && !defined(DOXYGEN)
// In this lib:
	#if !defined(SIMUL_DIRECTX12_DLL) 
	// If we're building dll libraries but not in this library IMPORT the classes
		#define SIMUL_DIRECTX12_EXPORT __declspec(dllimport)
	#else
	// In ALL OTHER CASES we EXPORT the classes!
		#define SIMUL_DIRECTX12_EXPORT __declspec(dllexport)
	#endif
#else
	#define SIMUL_DIRECTX12_EXPORT
#endif

#ifdef _MSC_VER
	#define SIMUL_DIRECTX12_EXPORT_FN SIMUL_DIRECTX12_EXPORT __cdecl
#else
	#define SIMUL_DIRECTX12_EXPORT_FN SIMUL_DIRECTX12_EXPORT
#endif

#define SIMUL_DIRECTX12_EXPORT_CLASS class SIMUL_DIRECTX12_EXPORT
#define SIMUL_DIRECTX12_EXPORT_STRUCT struct SIMUL_DIRECTX12_EXPORT

#endif