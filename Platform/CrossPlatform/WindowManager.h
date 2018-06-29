#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/GraphicsDeviceInterface.h"
#include "Simul/Platform/CrossPlatform/Window.h"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  

namespace simul
{
	namespace crossplatform
	{
		//! A class for multiple swap chains (i.e. rendering windows) to share the same device.
		//! With each graphics window it manages (identified by HWND's), WindowManager creates and manages a SwapChain instance.
		class SIMUL_CROSSPLATFORM_EXPORT WindowManager: public crossplatform::WindowManagerInterface
		{
		public:
			WindowManager();
			~WindowManager();
			void Initialize(RenderPlatform *r);
			void Shutdown();
			// Implementing Window Manager, which associates Hwnd's with renderers and view ids:
			//! Add a window. Creates a new Swap Chain.
			void AddWindow(cp_hwnd h);
			//! Removes the window and destroys its associated Swap Chain.
			void RemoveWindow(cp_hwnd h);
			SwapChain *GetSwapChain(cp_hwnd hwnd);
			void Render(cp_hwnd hwnd);
			void SetRenderer(cp_hwnd hwnd,crossplatform::PlatformRendererInterface *ci,int view_id);
			void SetFullScreen(cp_hwnd hwnd,bool fullscreen,int which_output);
			void ResizeSwapChain(cp_hwnd hwnd);
			int GetViewId(cp_hwnd hwnd);
			Window *GetWindow(cp_hwnd hwnd);
		protected:
			RenderPlatform* renderPlatform;
			typedef std::map<cp_hwnd,Window*> WindowMap;
			WindowMap windows;
		};
	}
}

	#pragma warning(pop)  
#endif