// Copyright (C) 2013 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "CWGLManager.h"

#ifdef _IRR_COMPILE_WITH_WGL_MANAGER_

#include "os.h"

#include <GL/gl.h>
#include <GL/wglext.h>

namespace video
{

CWGLManager::CWGLManager() :
		PrimaryContext(SExposedVideoData(0)), PixelFormat(0), libHandle(NULL)
{
	memset(FunctionPointers, 0, sizeof(FunctionPointers));
}

CWGLManager::~CWGLManager()
{
}

bool CWGLManager::initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &videodata)
{
	// store params, videoData is set later as it would be overwritten else
	Params = params;

	// Create a window to test antialiasing support
	const fschar_t *ClassName = __TEXT("CWGLManager");
	HINSTANCE lhInstance = GetModuleHandle(0);

	// Register Class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)DefWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = lhInstance;
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = ClassName;
	wcex.hIconSm = 0;
	RegisterClassEx(&wcex);

	RECT clientSize;
	clientSize.top = 0;
	clientSize.left = 0;
	clientSize.right = Params.WindowSize.Width;
	clientSize.bottom = Params.WindowSize.Height;

	DWORD style = WS_POPUP;
	if (!Params.Fullscreen)
		style = WS_SYSMENU | WS_BORDER | WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	AdjustWindowRect(&clientSize, style, FALSE);

	const s32 realWidth = clientSize.right - clientSize.left;
	const s32 realHeight = clientSize.bottom - clientSize.top;

	const s32 windowLeft = (GetSystemMetrics(SM_CXSCREEN) - realWidth) / 2;
	const s32 windowTop = (GetSystemMetrics(SM_CYSCREEN) - realHeight) / 2;

	HWND temporary_wnd = CreateWindow(ClassName, __TEXT(""), style, windowLeft,
			windowTop, realWidth, realHeight, NULL, NULL, lhInstance, NULL);

	if (!temporary_wnd) {
		os::Printer::log("Cannot create a temporary window.", ELL_ERROR);
		UnregisterClass(ClassName, lhInstance);
		return false;
	}

	HDC HDc = GetDC(temporary_wnd);

	// Set up pixel format descriptor with desired parameters
	PIXELFORMATDESCRIPTOR tmp_pfd = {
			sizeof(PIXELFORMATDESCRIPTOR),                         // Size Of This Pixel Format Descriptor
			1,                                                     // Version Number
			(DWORD)(PFD_DRAW_TO_WINDOW |                           // Format Must Support Window
					PFD_SUPPORT_OPENGL |                           // Format Must Support OpenGL
					(Params.Doublebuffer ? PFD_DOUBLEBUFFER : 0) | // Must Support Double Buffering
					(Params.Stereobuffer ? PFD_STEREO : 0)),       // Must Support Stereo Buffer
			PFD_TYPE_RGBA,                                         // Request An RGBA Format
			Params.Bits,                                           // Select Our Color Depth
			0, 0, 0, 0, 0, 0,                                      // Color Bits Ignored
			0,                                                     // No Alpha Buffer
			0,                                                     // Shift Bit Ignored
			0,                                                     // No Accumulation Buffer
			0, 0, 0, 0,                                            // Accumulation Bits Ignored
			Params.ZBufferBits,                                    // Z-Buffer (Depth Buffer)
			BYTE(Params.Stencilbuffer ? 1 : 0),                    // Stencil Buffer Depth
			0,                                                     // No Auxiliary Buffer
			PFD_MAIN_PLANE,                                        // Main Drawing Layer
			0,                                                     // Reserved
			0, 0, 0                                                // Layer Masks Ignored
	};
	pfd = tmp_pfd;

	for (u32 i = 0; i < 6; ++i) {
		if (i == 1) {
			if (Params.Stencilbuffer) {
				os::Printer::log("Cannot create a GL device with stencil buffer, disabling stencil shadows.", ELL_WARNING);
				Params.Stencilbuffer = false;
				pfd.cStencilBits = 0;
			} else
				continue;
		} else if (i == 2) {
			pfd.cDepthBits = 24;
		} else if (i == 3) {
			if (Params.Bits != 16)
				pfd.cDepthBits = 16;
			else
				continue;
		} else if (i == 4) {
			// try single buffer
			if (Params.Doublebuffer)
				pfd.dwFlags &= ~PFD_DOUBLEBUFFER;
			else
				continue;
		} else if (i == 5) {
			os::Printer::log("Cannot create a GL device context", "No suitable format for temporary window.", ELL_ERROR);
			ReleaseDC(temporary_wnd, HDc);
			DestroyWindow(temporary_wnd);
			UnregisterClass(ClassName, lhInstance);
			return false;
		}

		// choose pixelformat
		PixelFormat = ChoosePixelFormat(HDc, &pfd);
		if (PixelFormat)
			break;
	}

	SetPixelFormat(HDc, PixelFormat, &pfd);
	os::Printer::log("Create temporary GL rendering context", ELL_DEBUG);
	HGLRC hrc = wglCreateContext(HDc);
	if (!hrc) {
		os::Printer::log("Cannot create a temporary GL rendering context.", ELL_ERROR);
		ReleaseDC(temporary_wnd, HDc);
		DestroyWindow(temporary_wnd);
		UnregisterClass(ClassName, lhInstance);
		return false;
	}

	CurrentContext.OpenGLWin32.HDc = HDc;
	CurrentContext.OpenGLWin32.HRc = hrc;
	CurrentContext.OpenGLWin32.HWnd = temporary_wnd;

	if (!activateContext(CurrentContext, false)) {
		os::Printer::log("Cannot activate a temporary GL rendering context.", ELL_ERROR);
		wglDeleteContext(hrc);
		ReleaseDC(temporary_wnd, HDc);
		DestroyWindow(temporary_wnd);
		UnregisterClass(ClassName, lhInstance);
		return false;
	}

	core::stringc wglExtensions;
#ifdef WGL_ARB_extensions_string
	PFNWGLGETEXTENSIONSSTRINGARBPROC irrGetExtensionsString = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	if (irrGetExtensionsString)
		wglExtensions = irrGetExtensionsString(HDc);
#elif defined(WGL_EXT_extensions_string)
	PFNWGLGETEXTENSIONSSTRINGEXTPROC irrGetExtensionsString = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");
	if (irrGetExtensionsString)
		wglExtensions = irrGetExtensionsString(HDc);
#endif
	const bool pixel_format_supported = (wglExtensions.find("WGL_ARB_pixel_format") != -1);
	const bool multi_sample_supported = ((wglExtensions.find("WGL_ARB_multisample") != -1) ||
										 (wglExtensions.find("WGL_EXT_multisample") != -1) || (wglExtensions.find("WGL_3DFX_multisample") != -1));
	if (params.DriverDebug)
		os::Printer::log("WGL_extensions", wglExtensions);

	// Without a GL context we can't call wglGetProcAddress so store this for later
	FunctionPointers[0] = (void *)wglGetProcAddress("wglCreateContextAttribsARB");

#ifdef WGL_ARB_pixel_format
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormat_ARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	if (pixel_format_supported && wglChoosePixelFormat_ARB) {
		// This value determines the number of samples used for antialiasing
		// My experience is that 8 does not show a big
		// improvement over 4, but 4 shows a big improvement
		// over 2.

		if (Params.AntiAlias > 32)
			Params.AntiAlias = 32;

		f32 fAttributes[] = {0.0, 0.0};
		s32 iAttributes[] = {
				WGL_DRAW_TO_WINDOW_ARB, 1,
				WGL_SUPPORT_OPENGL_ARB, 1,
				WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
				WGL_COLOR_BITS_ARB, (Params.Bits == 32) ? 24 : 15,
				WGL_ALPHA_BITS_ARB, (Params.Bits == 32) ? 8 : 1,
				WGL_DEPTH_BITS_ARB, Params.ZBufferBits, // 10,11
				WGL_STENCIL_BITS_ARB, Params.Stencilbuffer ? 1 : 0,
				WGL_DOUBLE_BUFFER_ARB, Params.Doublebuffer ? 1 : 0,
				WGL_STEREO_ARB, Params.Stereobuffer ? 1 : 0,
				WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
#ifdef WGL_ARB_multisample
				WGL_SAMPLES_ARB, Params.AntiAlias, // 20,21
				WGL_SAMPLE_BUFFERS_ARB, (Params.AntiAlias > 0) ? 1 : 0,
#elif defined(WGL_EXT_multisample)
				WGL_SAMPLES_EXT, AntiAlias, // 20,21
				WGL_SAMPLE_BUFFERS_EXT, (Params.AntiAlias > 0) ? 1 : 0,
#elif defined(WGL_3DFX_multisample)
				WGL_SAMPLES_3DFX, AntiAlias, // 20,21
				WGL_SAMPLE_BUFFERS_3DFX, (Params.AntiAlias > 0) ? 1 : 0,
#endif
				//			WGL_DEPTH_FLOAT_EXT, 1,
				0, 0, 0, 0,
			};
		int iAttrSize = sizeof(iAttributes) / sizeof(int);
		if (!multi_sample_supported) {
			memmove(&iAttributes[20], &iAttributes[24], sizeof(int) * (iAttrSize - 24));
			iAttrSize -= 4;
		}

		s32 rv = 0;
		// Try to get an acceptable pixel format
		do {
			int pixelFormat = 0;
			UINT numFormats = 0;
			const BOOL valid = wglChoosePixelFormat_ARB(HDc, iAttributes, fAttributes, 1, &pixelFormat, &numFormats);

			if (valid && numFormats)
				rv = pixelFormat;
			else
				iAttributes[21] -= 1;
		} while (rv == 0 && iAttributes[21] > 1);
		if (rv) {
			PixelFormat = rv;
			Params.AntiAlias = iAttributes[21];
		}
	} else
#endif
		Params.AntiAlias = 0;

	// this only terminates the temporary HRc
	destroyContext();
	destroySurface();
	terminate();
	DestroyWindow(temporary_wnd);
	UnregisterClass(ClassName, lhInstance);

	// now get new window
	CurrentContext.OpenGLWin32.HWnd = videodata.OpenGLWin32.HWnd;
	// get hdc
	if (!(CurrentContext.OpenGLWin32.HDc = GetDC((HWND)videodata.OpenGLWin32.HWnd))) {
		os::Printer::log("Cannot create a GL device context.", ELL_ERROR);
		return false;
	}
	if (!PrimaryContext.OpenGLWin32.HWnd) {
		PrimaryContext.OpenGLWin32.HWnd = CurrentContext.OpenGLWin32.HWnd;
		PrimaryContext.OpenGLWin32.HDc = CurrentContext.OpenGLWin32.HDc;
	}

	return true;
}

void CWGLManager::terminate()
{
	if (CurrentContext.OpenGLWin32.HDc)
		ReleaseDC((HWND)CurrentContext.OpenGLWin32.HWnd, (HDC)CurrentContext.OpenGLWin32.HDc);
	if (PrimaryContext.OpenGLWin32.HDc && PrimaryContext.OpenGLWin32.HDc == CurrentContext.OpenGLWin32.HDc)
		memset(&PrimaryContext, 0, sizeof(PrimaryContext));
	memset(&CurrentContext, 0, sizeof(CurrentContext));
	if (libHandle)
		FreeLibrary(libHandle);
}

bool CWGLManager::generateSurface()
{
	HDC HDc = (HDC)CurrentContext.OpenGLWin32.HDc;
	// search for pixel format the simple way
	if (PixelFormat == 0 || (!SetPixelFormat(HDc, PixelFormat, &pfd))) {
		for (u32 i = 0; i < 5; ++i) {
			if (i == 1) {
				if (Params.Stencilbuffer) {
					os::Printer::log("Cannot create a GL device with stencil buffer, disabling stencil shadows.", ELL_WARNING);
					Params.Stencilbuffer = false;
					pfd.cStencilBits = 0;
				} else
					continue;
			} else if (i == 2) {
				pfd.cDepthBits = 24;
			}
			if (i == 3) {
				if (Params.Bits != 16)
					pfd.cDepthBits = 16;
				else
					continue;
			} else if (i == 4) {
				os::Printer::log("Cannot create a GL device context", "No suitable format.", ELL_ERROR);
				return false;
			}

			// choose pixelformat
			PixelFormat = ChoosePixelFormat(HDc, &pfd);
			if (PixelFormat)
				break;
		}

		// set pixel format
		if (!SetPixelFormat(HDc, PixelFormat, &pfd)) {
			os::Printer::log("Cannot set the pixel format.", ELL_ERROR);
			return false;
		}
	}

	if (pfd.cAlphaBits != 0) {
		if (pfd.cRedBits == 8)
			ColorFormat = ECF_A8R8G8B8;
		else
			ColorFormat = ECF_A1R5G5B5;
	} else {
		if (pfd.cRedBits == 8)
			ColorFormat = ECF_R8G8B8;
		else
			ColorFormat = ECF_R5G6B5;
	}
	os::Printer::log("Pixel Format", core::stringc(PixelFormat).c_str(), ELL_DEBUG);
	return true;
}

void CWGLManager::destroySurface()
{
}

bool CWGLManager::generateContext()
{
	HDC HDc = (HDC)CurrentContext.OpenGLWin32.HDc;
	HGLRC hrc;
	// create rendering context
#ifdef WGL_ARB_create_context
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs_ARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)FunctionPointers[0];
	if (wglCreateContextAttribs_ARB) {
		// with 3.0 all available profiles should be usable, higher versions impose restrictions
		// we need at least 1.1
		const int iAttribs[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, 1,
				WGL_CONTEXT_MINOR_VERSION_ARB, 1,
				//			WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,	// enable to get a debug context (depends on driver if that does anything)
				0,
			};
		hrc = wglCreateContextAttribs_ARB(HDc, 0, iAttribs);
	} else
#endif
		hrc = wglCreateContext(HDc);
	os::Printer::log("Irrlicht context");

	if (!hrc) {
		os::Printer::log("Cannot create a GL rendering context.", ELL_ERROR);
		return false;
	}

	// set exposed data
	CurrentContext.OpenGLWin32.HRc = hrc;
	if (!PrimaryContext.OpenGLWin32.HRc)
		PrimaryContext.OpenGLWin32.HRc = CurrentContext.OpenGLWin32.HRc;

	return true;
}

const SExposedVideoData &CWGLManager::getContext() const
{
	return CurrentContext;
}

bool CWGLManager::activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero)
{
	if (videoData.OpenGLWin32.HWnd && videoData.OpenGLWin32.HDc && videoData.OpenGLWin32.HRc) {
		if (!wglMakeCurrent((HDC)videoData.OpenGLWin32.HDc, (HGLRC)videoData.OpenGLWin32.HRc)) {
			os::Printer::log("Render Context switch failed.");
			return false;
		}
		CurrentContext = videoData;
	} else if (!restorePrimaryOnZero && !videoData.OpenGLWin32.HDc && !videoData.OpenGLWin32.HRc) {
		if (!wglMakeCurrent((HDC)0, (HGLRC)0)) {
			os::Printer::log("Render Context reset failed.");
			return false;
		}
		CurrentContext = videoData;
	}
	// set back to main context
	else if (!videoData.OpenGLWin32.HWnd && CurrentContext.OpenGLWin32.HDc != PrimaryContext.OpenGLWin32.HDc) {
		if (!wglMakeCurrent((HDC)PrimaryContext.OpenGLWin32.HDc, (HGLRC)PrimaryContext.OpenGLWin32.HRc)) {
			os::Printer::log("Render Context switch (back to main) failed.");
			return false;
		}
		CurrentContext = PrimaryContext;
	}
	return true;
}

void CWGLManager::destroyContext()
{
	if (CurrentContext.OpenGLWin32.HRc) {
		if (!wglMakeCurrent((HDC)CurrentContext.OpenGLWin32.HDc, 0))
			os::Printer::log("Release of render context failed.", ELL_WARNING);

		if (!wglDeleteContext((HGLRC)CurrentContext.OpenGLWin32.HRc))
			os::Printer::log("Deletion of render context failed.", ELL_WARNING);
		if (PrimaryContext.OpenGLWin32.HRc == CurrentContext.OpenGLWin32.HRc)
			PrimaryContext.OpenGLWin32.HRc = 0;
		CurrentContext.OpenGLWin32.HRc = 0;
	}
}

void *CWGLManager::getProcAddress(const std::string &procName)
{
	void *proc = NULL;
	proc = (void *)wglGetProcAddress(procName.c_str());
	if (!proc) { // Fallback
		if (!libHandle)
			libHandle = LoadLibraryA("opengl32.dll");
		if (libHandle)
			proc = (void *)GetProcAddress(libHandle, procName.c_str());
	}
	return proc;
}

bool CWGLManager::swapBuffers()
{
	return SwapBuffers((HDC)CurrentContext.OpenGLWin32.HDc) == TRUE;
}

}

#endif
