// Copyright (C) 2013 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "CGLXManager.h"

#ifdef _IRR_COMPILE_WITH_GLX_MANAGER_

#include "os.h"

#define GL_GLEXT_LEGACY 1
#define GLX_GLXEXT_LEGACY 1
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <GL/glxext.h>

namespace video
{

CGLXManager::CGLXManager(const SIrrlichtCreationParameters &params, const SExposedVideoData &videodata, int screennr) :
		Params(params), PrimaryContext(videodata), VisualInfo(0), glxFBConfig(0), GlxWin(0)
{
	CurrentContext.OpenGLLinux.X11Display = PrimaryContext.OpenGLLinux.X11Display;

	int major, minor;
	Display *display = (Display *)PrimaryContext.OpenGLLinux.X11Display;
	const bool isAvailableGLX = glXQueryExtension(display, &major, &minor);

	if (isAvailableGLX && glXQueryVersion(display, &major, &minor)) {
#if defined(GLX_VERSION_1_3)
		typedef GLXFBConfig *(*PFNGLXCHOOSEFBCONFIGPROC)(Display *dpy, int screen, const int *attrib_list, int *nelements);

		PFNGLXCHOOSEFBCONFIGPROC glxChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXChooseFBConfig"));
		if (major == 1 && minor > 2 && glxChooseFBConfig) {
			os::Printer::log("GLX >= 1.3", ELL_DEBUG);
			// attribute array for the draw buffer
			int visualAttrBuffer[] = {
					GLX_RENDER_TYPE,
					GLX_RGBA_BIT,
					GLX_RED_SIZE,
					4,
					GLX_GREEN_SIZE,
					4,
					GLX_BLUE_SIZE,
					4,
					GLX_ALPHA_SIZE,
					Params.WithAlphaChannel ? 1 : 0,
					GLX_DEPTH_SIZE,
					Params.ZBufferBits, // 10,11
					GLX_DOUBLEBUFFER,
					Params.Doublebuffer ? True : False,
					GLX_STENCIL_SIZE,
					Params.Stencilbuffer ? 1 : 0,
#if defined(GLX_VERSION_1_4) && defined(GLX_SAMPLE_BUFFERS) // we need to check the extension string!
					GLX_SAMPLE_BUFFERS,
					1,
					GLX_SAMPLES,
					Params.AntiAlias, // 18,19
#elif defined(GLX_ARB_multisample)
					GLX_SAMPLE_BUFFERS_ARB,
					1,
					GLX_SAMPLES_ARB,
					Params.AntiAlias, // 18,19
#elif defined(GLX_SGIS_multisample)
					GLX_SAMPLE_BUFFERS_SGIS,
					1,
					GLX_SAMPLES_SGIS,
					Params.AntiAlias, // 18,19
#endif
					GLX_STEREO,
					Params.Stereobuffer ? True : False,
					None,
				};

			GLXFBConfig *configList = 0;
			int nitems = 0;
			if (Params.AntiAlias < 2) {
				visualAttrBuffer[17] = 0;
				visualAttrBuffer[19] = 0;
			}
			// first round with unchanged values
			{
				configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
				if (!configList && Params.AntiAlias) {
					while (!configList && (visualAttrBuffer[19] > 1)) {
						visualAttrBuffer[19] -= 1;
						configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
					}
					if (!configList) {
						visualAttrBuffer[17] = 0;
						visualAttrBuffer[19] = 0;
						configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
						if (configList) {
							os::Printer::log("No FSAA available.", ELL_WARNING);
							Params.AntiAlias = 0;
						} else {
							// reenable multisampling
							visualAttrBuffer[17] = 1;
							visualAttrBuffer[19] = Params.AntiAlias;
						}
					}
				}
			}
			// Next try with flipped stencil buffer value
			// If the first round was with stencil flag it's now without
			// Other way round also makes sense because some configs
			// only have depth buffer combined with stencil buffer
			if (!configList) {
				if (Params.Stencilbuffer)
					os::Printer::log("No stencilbuffer available, disabling stencil shadows.", ELL_WARNING);
				Params.Stencilbuffer = !Params.Stencilbuffer;
				visualAttrBuffer[15] = Params.Stencilbuffer ? 1 : 0;

				configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
				if (!configList && Params.AntiAlias) {
					while (!configList && (visualAttrBuffer[19] > 1)) {
						visualAttrBuffer[19] -= 1;
						configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
					}
					if (!configList) {
						visualAttrBuffer[17] = 0;
						visualAttrBuffer[19] = 0;
						configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
						if (configList) {
							os::Printer::log("No FSAA available.", ELL_WARNING);
							Params.AntiAlias = 0;
						} else {
							// reenable multisampling
							visualAttrBuffer[17] = 1;
							visualAttrBuffer[19] = Params.AntiAlias;
						}
					}
				}
			}
			// Next try without double buffer
			if (!configList && Params.Doublebuffer) {
				os::Printer::log("No doublebuffering available.", ELL_WARNING);
				Params.Doublebuffer = false;
				visualAttrBuffer[13] = GLX_DONT_CARE;
				Params.Stencilbuffer = false;
				visualAttrBuffer[15] = 0;
				configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
				if (!configList && Params.AntiAlias) {
					while (!configList && (visualAttrBuffer[19] > 1)) {
						visualAttrBuffer[19] -= 1;
						configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
					}
					if (!configList) {
						visualAttrBuffer[17] = 0;
						visualAttrBuffer[19] = 0;
						configList = glxChooseFBConfig(display, screennr, visualAttrBuffer, &nitems);
						if (configList) {
							os::Printer::log("No FSAA available.", ELL_WARNING);
							Params.AntiAlias = 0;
						} else {
							// reenable multisampling
							visualAttrBuffer[17] = 1;
							visualAttrBuffer[19] = Params.AntiAlias;
						}
					}
				}
			}
			if (configList) {
				glxFBConfig = configList[0];
				XFree(configList);
				typedef XVisualInfo *(*PFNGLXGETVISUALFROMFBCONFIGPROC)(Display *dpy, GLXFBConfig config);
				PFNGLXGETVISUALFROMFBCONFIGPROC glxGetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXGetVisualFromFBConfig"));
				if (glxGetVisualFromFBConfig)
					VisualInfo = glxGetVisualFromFBConfig(display, (GLXFBConfig)glxFBConfig);
			}
		} else
#endif
		{
			// attribute array for the draw buffer
			int visualAttrBuffer[] = {
					GLX_RGBA, GLX_USE_GL,
					GLX_RED_SIZE, 4,
					GLX_GREEN_SIZE, 4,
					GLX_BLUE_SIZE, 4,
					GLX_ALPHA_SIZE, Params.WithAlphaChannel ? 1 : 0,
					GLX_DEPTH_SIZE, Params.ZBufferBits,
					GLX_STENCIL_SIZE, Params.Stencilbuffer ? 1 : 0, // 12,13
					// The following attributes have no flags, but are
					// either present or not. As a no-op we use
					// GLX_USE_GL, which is silently ignored by glXChooseVisual
					Params.Doublebuffer ? GLX_DOUBLEBUFFER : GLX_USE_GL, // 14
					Params.Stereobuffer ? GLX_STEREO : GLX_USE_GL,       // 15
					None,
				};

			VisualInfo = glXChooseVisual(display, screennr, visualAttrBuffer);
			if (!VisualInfo) {
				if (Params.Stencilbuffer)
					os::Printer::log("No stencilbuffer available, disabling.", ELL_WARNING);
				Params.Stencilbuffer = !Params.Stencilbuffer;
				visualAttrBuffer[13] = Params.Stencilbuffer ? 1 : 0;

				VisualInfo = glXChooseVisual(display, screennr, visualAttrBuffer);
				if (!VisualInfo && Params.Doublebuffer) {
					os::Printer::log("No doublebuffering available.", ELL_WARNING);
					Params.Doublebuffer = false;
					visualAttrBuffer[14] = GLX_USE_GL;
					VisualInfo = glXChooseVisual(display, screennr, visualAttrBuffer);
				}
			}
		}
	} else
		os::Printer::log("No GLX support available. OpenGL driver will not work.", ELL_WARNING);
}

CGLXManager::~CGLXManager()
{
}

bool CGLXManager::initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &videodata)
{
	// store params
	Params = params;

	// set display
	CurrentContext.OpenGLLinux.X11Display = videodata.OpenGLLinux.X11Display;

	// now get new window
	CurrentContext.OpenGLLinux.X11Window = videodata.OpenGLLinux.X11Window;
	if (!PrimaryContext.OpenGLLinux.X11Window) {
		PrimaryContext.OpenGLLinux.X11Window = CurrentContext.OpenGLLinux.X11Window;
	}

	return true;
}

void CGLXManager::terminate()
{
	memset((void *)&CurrentContext, 0, sizeof(CurrentContext));
}

bool CGLXManager::generateSurface()
{
	if (glxFBConfig) {
		GlxWin = glXCreateWindow((Display *)CurrentContext.OpenGLLinux.X11Display, (GLXFBConfig)glxFBConfig, CurrentContext.OpenGLLinux.X11Window, NULL);
		if (!GlxWin) {
			os::Printer::log("Could not create GLX window.", ELL_WARNING);
			return false;
		}

		CurrentContext.OpenGLLinux.GLXWindow = GlxWin;
	} else {
		CurrentContext.OpenGLLinux.GLXWindow = CurrentContext.OpenGLLinux.X11Window;
	}
	return true;
}

void CGLXManager::destroySurface()
{
	if (GlxWin)
		glXDestroyWindow((Display *)CurrentContext.OpenGLLinux.X11Display, GlxWin);
}

#if defined(GLX_ARB_create_context)
static int IrrIgnoreError(Display *display, XErrorEvent *event)
{
	char msg[256];
	XGetErrorText(display, event->error_code, msg, 256);
	os::Printer::log("Ignoring an X error", msg, ELL_DEBUG);
	return 0;
}
#endif

bool CGLXManager::generateContext()
{
	GLXContext context = 0;

	if (glxFBConfig) {
		if (GlxWin) {
#if defined(GLX_ARB_create_context)

			PFNGLXCREATECONTEXTATTRIBSARBPROC glxCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress(reinterpret_cast<const GLubyte *>("glXCreateContextAttribsARB"));

			if (glxCreateContextAttribsARB) {
				os::Printer::log("GLX with GLX_ARB_create_context", ELL_DEBUG);
				int contextAttrBuffer[] = {
						GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
						GLX_CONTEXT_MINOR_VERSION_ARB, 0,
						// GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
						None};
				XErrorHandler old = XSetErrorHandler(IrrIgnoreError);
				context = glxCreateContextAttribsARB((Display *)CurrentContext.OpenGLLinux.X11Display, (GLXFBConfig)glxFBConfig, NULL, True, contextAttrBuffer);
				XSetErrorHandler(old);
				// transparently fall back to legacy call
			}
			if (!context)
#endif
			{
				// create glx context
				context = glXCreateNewContext((Display *)CurrentContext.OpenGLLinux.X11Display, (GLXFBConfig)glxFBConfig, GLX_RGBA_TYPE, NULL, True);
				if (!context) {
					os::Printer::log("Could not create GLX rendering context.", ELL_WARNING);
					return false;
				}
			}
		} else {
			os::Printer::log("GLX window was not properly created.", ELL_WARNING);
			return false;
		}
	} else {
		context = glXCreateContext((Display *)CurrentContext.OpenGLLinux.X11Display, VisualInfo, NULL, True);
		if (!context) {
			os::Printer::log("Could not create GLX rendering context.", ELL_WARNING);
			return false;
		}
	}
	CurrentContext.OpenGLLinux.X11Context = context;
	return true;
}

const SExposedVideoData &CGLXManager::getContext() const
{
	return CurrentContext;
}

bool CGLXManager::activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero)
{
	// TODO: handle restorePrimaryOnZero

	if (videoData.OpenGLLinux.X11Window) {
		if (videoData.OpenGLLinux.X11Display && videoData.OpenGLLinux.X11Context) {
			if (!glXMakeCurrent((Display *)videoData.OpenGLLinux.X11Display, videoData.OpenGLLinux.GLXWindow, (GLXContext)videoData.OpenGLLinux.X11Context)) {
				os::Printer::log("Context activation failed.");
				return false;
			} else {
				CurrentContext.OpenGLLinux.GLXWindow = videoData.OpenGLLinux.GLXWindow;
				CurrentContext.OpenGLLinux.X11Window = videoData.OpenGLLinux.X11Window;
				CurrentContext.OpenGLLinux.X11Display = videoData.OpenGLLinux.X11Display;
			}
		} else {
			// in case we only got a window ID, try with the existing values for display and context
			if (!glXMakeCurrent((Display *)PrimaryContext.OpenGLLinux.X11Display, videoData.OpenGLLinux.GLXWindow, (GLXContext)PrimaryContext.OpenGLLinux.X11Context)) {
				os::Printer::log("Context activation failed.");
				return false;
			} else {
				CurrentContext.OpenGLLinux.GLXWindow = videoData.OpenGLLinux.GLXWindow;
				CurrentContext.OpenGLLinux.X11Window = videoData.OpenGLLinux.X11Window;
				CurrentContext.OpenGLLinux.X11Display = PrimaryContext.OpenGLLinux.X11Display;
			}
		}
	} else if (!restorePrimaryOnZero && !videoData.OpenGLLinux.X11Window && !videoData.OpenGLLinux.X11Display) {
		if (!glXMakeCurrent((Display *)PrimaryContext.OpenGLLinux.X11Display, None, NULL)) {
			os::Printer::log("Render Context reset failed.");
			return false;
		}
		CurrentContext.OpenGLLinux.X11Window = 0;
		CurrentContext.OpenGLLinux.X11Display = 0;
	}
	// set back to main context
	else if (CurrentContext.OpenGLLinux.X11Display != PrimaryContext.OpenGLLinux.X11Display) {
		if (!glXMakeCurrent((Display *)PrimaryContext.OpenGLLinux.X11Display, PrimaryContext.OpenGLLinux.X11Window, (GLXContext)PrimaryContext.OpenGLLinux.X11Context)) {
			os::Printer::log("Context activation failed.");
			return false;
		} else {
			CurrentContext = PrimaryContext;
		}
	}
	return true;
}

void CGLXManager::destroyContext()
{
	if (CurrentContext.OpenGLLinux.X11Context) {
		if (GlxWin) {
			if (!glXMakeContextCurrent((Display *)CurrentContext.OpenGLLinux.X11Display, None, None, NULL))
				os::Printer::log("Could not release glx context.", ELL_WARNING);
		} else {
			if (!glXMakeCurrent((Display *)CurrentContext.OpenGLLinux.X11Display, None, NULL))
				os::Printer::log("Could not release glx context.", ELL_WARNING);
		}
		glXDestroyContext((Display *)CurrentContext.OpenGLLinux.X11Display, (GLXContext)CurrentContext.OpenGLLinux.X11Context);
	}
}

void *CGLXManager::getProcAddress(const std::string &procName)
{
	return (void *)glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(procName.c_str()));
}

bool CGLXManager::swapBuffers()
{
	glXSwapBuffers((Display *)CurrentContext.OpenGLLinux.X11Display, CurrentContext.OpenGLLinux.GLXWindow);
	return true;
}

}

#endif
