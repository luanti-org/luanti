// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CNullDriver.h"
#include "IVideoDriver.h"
#include "SMaterial.h"
#include "os.h"
#include "CImage.h"
#include "IReadFile.h"
#include "IWriteFile.h"
#include "IImageLoader.h"
#include "IImageWriter.h"
#include "IMaterialRenderer.h"
#include "IAnimatedMeshSceneNode.h"
#include "CMeshManipulator.h"
#include "CColorConverter.h"
#include "IReferenceCounted.h"
#include "IRenderTarget.h"

#include <cassert>

namespace video
{

//! creates a loader which is able to load jpeg images
IImageLoader *createImageLoaderJPG();

//! creates a loader which is able to load targa images
IImageLoader *createImageLoaderTGA();

//! creates a loader which is able to load png images
IImageLoader *createImageLoaderPNG();

//! creates a writer which is able to save jpg images
IImageWriter *createImageWriterJPG();

//! creates a writer which is able to save png images
IImageWriter *createImageWriterPNG();

namespace
{
//! no-op material renderer
class CDummyMaterialRenderer : public IMaterialRenderer
{
public:
	CDummyMaterialRenderer() {}
};
}

//! constructor
CNullDriver::CNullDriver(io::IFileSystem *io, const core::dimension2d<u32> &screenSize) :
		SharedRenderTarget(0), CurrentRenderTarget(0), CurrentRenderTargetSize(0, 0), FileSystem(io), MeshManipulator(0),
		ViewPort(0, 0, 0, 0), ScreenSize(screenSize), MinVertexCountForVBO(500),
		TextureCreationFlags(0), OverrideMaterial2DEnabled(false), AllowZWriteOnTransparent(false)
{
	setFog();

	setTextureCreationFlag(ETCF_ALWAYS_32_BIT, true);
	setTextureCreationFlag(ETCF_CREATE_MIP_MAPS, true);
	setTextureCreationFlag(ETCF_ALLOW_MEMORY_COPY, false);

	ViewPort = core::rect<s32>(core::position2d<s32>(0, 0), core::dimension2di(screenSize));

	// create manipulator
	MeshManipulator = new scene::CMeshManipulator();

	if (FileSystem)
		FileSystem->grab();

	// create surface loaders and writers
	SurfaceLoader.push_back(video::createImageLoaderTGA());
	SurfaceLoader.push_back(video::createImageLoaderPNG());
	SurfaceLoader.push_back(video::createImageLoaderJPG());

	SurfaceWriter.push_back(video::createImageWriterJPG());
	SurfaceWriter.push_back(video::createImageWriterPNG());

	// set ExposedData to 0
	memset((void *)&ExposedData, 0, sizeof(ExposedData));
	for (u32 i = 0; i < video::EVDF_COUNT; ++i)
		FeatureEnabled[i] = true;

	InitMaterial2D.AntiAliasing = video::EAAM_OFF;
	InitMaterial2D.ZWriteEnable = video::EZW_OFF;
	InitMaterial2D.ZBuffer = video::ECFN_DISABLED;
	InitMaterial2D.UseMipMaps = false;
	InitMaterial2D.forEachTexture([](video::SMaterialLayer &tex) {
		// Best preset for 2D pixel-perfect graphics
		tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;

		// Best preset for downscaled 2D graphics using trilinear interpolation
		//tex.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
		// Lower bias  -> more crisp images, more jitter
		// Higher bias -> burry images, less jitter
		//tex.LODBias = -1;

		tex.MagFilter = video::ETMAGF_NEAREST;
		tex.TextureWrapU = video::ETC_REPEAT;
		tex.TextureWrapV = video::ETC_REPEAT;
		tex.TextureWrapW = video::ETC_REPEAT;
	});
	OverrideMaterial2D = InitMaterial2D;
}

//! destructor
CNullDriver::~CNullDriver()
{
	if (FileSystem)
		FileSystem->drop();

	if (MeshManipulator)
		MeshManipulator->drop();

	removeAllRenderTargets();

	deleteAllTextures();

	u32 i;
	for (i = 0; i < SurfaceLoader.size(); ++i)
		SurfaceLoader[i]->drop();

	for (i = 0; i < SurfaceWriter.size(); ++i)
		SurfaceWriter[i]->drop();

	// delete material renderers
	deleteMaterialRenders();

	// delete hardware mesh buffers
	removeAllHardwareBuffers();
}

//! Adds an external surface loader to the engine.
void CNullDriver::addExternalImageLoader(IImageLoader *loader)
{
	if (!loader)
		return;

	loader->grab();
	SurfaceLoader.push_back(loader);
}

//! Adds an external surface writer to the engine.
void CNullDriver::addExternalImageWriter(IImageWriter *writer)
{
	if (!writer)
		return;

	writer->grab();
	SurfaceWriter.push_back(writer);
}

//! Retrieve the number of image loaders
u32 CNullDriver::getImageLoaderCount() const
{
	return SurfaceLoader.size();
}

//! Retrieve the given image loader
IImageLoader *CNullDriver::getImageLoader(u32 n)
{
	if (n < SurfaceLoader.size())
		return SurfaceLoader[n];
	return 0;
}

//! Retrieve the number of image writers
u32 CNullDriver::getImageWriterCount() const
{
	return SurfaceWriter.size();
}

//! Retrieve the given image writer
IImageWriter *CNullDriver::getImageWriter(u32 n)
{
	if (n < SurfaceWriter.size())
		return SurfaceWriter[n];
	return 0;
}

//! deletes all textures
void CNullDriver::deleteAllTextures()
{
	// we need to remove previously set textures which might otherwise be kept in the
	// last set material member. Could be optimized to reduce state changes.
	setMaterial(SMaterial());

	// reset render targets.

	for (u32 i = 0; i < RenderTargets.size(); ++i)
		RenderTargets[i]->setTexture(0, 0);

	// remove textures.

	for (u32 i = 0; i < Textures.size(); ++i)
		Textures[i].Surface->drop();

	Textures.clear();

	SharedDepthTextures.clear();
}

bool CNullDriver::beginScene(u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil, const SExposedVideoData &videoData, core::rect<s32> *sourceRect)
{
	FrameStats = {};
	return true;
}

bool CNullDriver::endScene()
{
	FPSCounter.registerFrame(os::Timer::getRealTime());
	expireHardwareBuffers();
	updateAllOcclusionQueries();
	return true;
}

//! Disable a feature of the driver.
void CNullDriver::disableFeature(E_VIDEO_DRIVER_FEATURE feature, bool flag)
{
	FeatureEnabled[feature] = !flag;
}

//! queries the features of the driver, returns true if feature is available
bool CNullDriver::queryFeature(E_VIDEO_DRIVER_FEATURE feature) const
{
	return false;
}

//! sets transformation
void CNullDriver::setTransform(E_TRANSFORMATION_STATE state, const core::matrix4 &mat)
{
}

//! Returns the transformation set by setTransform
const core::matrix4 &CNullDriver::getTransform(E_TRANSFORMATION_STATE state) const
{
	return TransformationMatrix;
}

//! sets a material
void CNullDriver::setMaterial(const SMaterial &material)
{
}

//! Removes a texture from the texture cache and deletes it, freeing lot of
//! memory.
void CNullDriver::removeTexture(ITexture *texture)
{
	if (!texture)
		return;
	SSurface s;
	s.Surface = texture;

	s32 last;
	s32 first = Textures.binary_search_multi(s, last);
	if (first == -1)
		return;
	for (u32 i = first; i <= (u32)last; i++) {
		if (Textures[i].Surface == texture) {
			texture->drop();
			Textures.erase(i);
			return;
		}
	}
}

//! Removes all texture from the texture cache and deletes them, freeing lot of
//! memory.
void CNullDriver::removeAllTextures()
{
	setMaterial(SMaterial());
	deleteAllTextures();
}

//! Returns amount of textures currently loaded
u32 CNullDriver::getTextureCount() const
{
	return Textures.size();
}

ITexture *CNullDriver::addTexture(const core::dimension2d<u32> &size, const io::path &name, ECOLOR_FORMAT format)
{
	IImage *image = new CImage(format, size);
	ITexture *t = addTexture(name, image);
	image->drop();
	return t;
}

ITexture *CNullDriver::addTexture(const io::path &name, IImage *image)
{
	if (0 == name.size()) {
		os::Printer::log("Could not create ITexture, texture needs to have a non-empty name.", ELL_WARNING);
		return 0;
	}

	if (!image)
		return 0;

	ITexture *t = 0;

	if (checkImage(image)) {
		std::vector tmp { image };
		t = createDeviceDependentTexture(name, ETT_2D, tmp);
	}

	if (t) {
		addTexture(t);
		t->drop();
	}

	return t;
}

ITexture *CNullDriver::addArrayTexture(const io::path &name, IImage **images, u32 count)
{
	if (0 == name.size()) {
		os::Printer::log("Could not create ITexture, texture needs to have a non-empty name.", ELL_WARNING);
		return 0;
	}

	// this is stupid but who cares
	std::vector<IImage*> tmp(images, images + count);

	ITexture *t = nullptr;
	if (checkImage(tmp)) {
		t = createDeviceDependentTexture(name, ETT_2D_ARRAY, tmp);
	}
	if (t) {
		addTexture(t);
		t->drop();
	}
	return t;
}

ITexture *CNullDriver::addTextureCubemap(const io::path &name, IImage *imagePosX, IImage *imageNegX, IImage *imagePosY,
		IImage *imageNegY, IImage *imagePosZ, IImage *imageNegZ)
{
	if (0 == name.size() || !imagePosX || !imageNegX || !imagePosY || !imageNegY || !imagePosZ || !imageNegZ)
		return 0;

	ITexture *t = 0;

	std::vector<IImage*> imageArray;
	imageArray.push_back(imagePosX);
	imageArray.push_back(imageNegX);
	imageArray.push_back(imagePosY);
	imageArray.push_back(imageNegY);
	imageArray.push_back(imagePosZ);
	imageArray.push_back(imageNegZ);

	if (checkImage(imageArray)) {
		t = createDeviceDependentTexture(name, ETT_CUBEMAP, imageArray);
	}

	if (t) {
		addTexture(t);
		t->drop();
	}

	return t;
}

ITexture *CNullDriver::addTextureCubemap(const u32 sideLen, const io::path &name, ECOLOR_FORMAT format)
{
	if (0 == sideLen)
		return 0;

	if (0 == name.size()) {
		os::Printer::log("Could not create ITexture, texture needs to have a non-empty name.", ELL_WARNING);
		return 0;
	}

	std::vector<IImage*> imageArray;
	for (int i = 0; i < 6; ++i)
		imageArray.push_back(new CImage(format, core::dimension2du(sideLen, sideLen)));

	ITexture *t = 0;
	if (checkImage(imageArray)) {
		t = createDeviceDependentTexture(name, ETT_CUBEMAP, imageArray);

		if (t) {
			addTexture(t);
			t->drop();
		}
	}

	for (int i = 0; i < 6; ++i)
		imageArray[i]->drop();

	return t;
}

//! loads a Texture
ITexture *CNullDriver::getTexture(const io::path &filename)
{
	// Identify textures by their absolute filenames if possible.
	const io::path absolutePath = FileSystem->getAbsolutePath(filename);

	ITexture *texture = findTexture(absolutePath);
	if (texture)
		return texture;

	// Then try the raw filename, which might be in an Archive
	texture = findTexture(filename);
	if (texture)
		return texture;

	// Now try to open the file using the complete path.
	io::IReadFile *file = FileSystem->createAndOpenFile(absolutePath);

	if (!file) {
		// Try to open it using the raw filename.
		file = FileSystem->createAndOpenFile(filename);
	}

	if (file) {
		// Re-check name for actual archive names
		texture = findTexture(file->getFileName());
		if (texture) {
			file->drop();
			return texture;
		}

		texture = loadTextureFromFile(file);
		file->drop();

		if (texture) {
			addTexture(texture);
			texture->drop(); // drop it because we created it, one grab too much
		} else
			os::Printer::log("Could not load texture", filename, ELL_ERROR);
		return texture;
	} else {
		os::Printer::log("Could not open file of texture", filename, ELL_WARNING);
		return 0;
	}
}

//! loads a Texture
ITexture *CNullDriver::getTexture(io::IReadFile *file)
{
	ITexture *texture = 0;

	if (file) {
		texture = findTexture(file->getFileName());

		if (texture)
			return texture;

		texture = loadTextureFromFile(file);

		if (texture) {
			addTexture(texture);
			texture->drop(); // drop it because we created it, one grab too much
		}

		if (!texture)
			os::Printer::log("Could not load texture", file->getFileName(), ELL_WARNING);
	}

	return texture;
}

//! opens the file and loads it into the surface
video::ITexture *CNullDriver::loadTextureFromFile(io::IReadFile *file, const io::path &hashName)
{
	ITexture *texture = nullptr;

	IImage *image = createImageFromFile(file);
	if (!image)
		return nullptr;

	if (checkImage(image)) {
		std::vector tmp { image };
		texture = createDeviceDependentTexture(hashName.size() ? hashName : file->getFileName(), ETT_2D, tmp);
		if (texture)
			os::Printer::log("Loaded texture", file->getFileName(), ELL_DEBUG);
	}

	image->drop();

	return texture;
}

//! adds a surface, not loaded or created by the Irrlicht Engine
void CNullDriver::addTexture(video::ITexture *texture)
{
	if (texture) {
		SSurface s;
		s.Surface = texture;
		texture->grab();

		Textures.push_back(s);

		// the new texture is now at the end of the texture list. when searching for
		// the next new texture, the texture array will be sorted and the index of this texture
		// will be changed.
	}
}

//! looks if the image is already loaded
video::ITexture *CNullDriver::findTexture(const io::path &filename)
{
	SSurface s;
	SDummyTexture dummy(filename, ETT_2D);
	s.Surface = &dummy;

	s32 index = Textures.binary_search(s);
	if (index != -1)
		return Textures[index].Surface;

	return 0;
}

ITexture *CNullDriver::createDeviceDependentTexture(const io::path &name, E_TEXTURE_TYPE type,
		const std::vector<IImage*> &images)
{
	if (type != ETT_2D && type != ETT_CUBEMAP)
		return nullptr;
	SDummyTexture *dummy = new SDummyTexture(name, type);
	dummy->setSize(images[0]->getDimension());
	return dummy;
}

bool CNullDriver::setRenderTargetEx(IRenderTarget *target, u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil)
{
	return false;
}

bool CNullDriver::setRenderTarget(ITexture *texture, u16 clearFlag, SColor clearColor, f32 clearDepth, u8 clearStencil)
{
	if (texture) {
		// create render target if require.
		if (!SharedRenderTarget)
			SharedRenderTarget = addRenderTarget();

		ITexture *depthTexture = 0;

		// try to find available depth texture with require size.
		for (u32 i = 0; i < SharedDepthTextures.size(); ++i) {
			if (SharedDepthTextures[i]->getSize() == texture->getSize()) {
				depthTexture = SharedDepthTextures[i];

				break;
			}
		}

		// create depth texture if require.
		if (!depthTexture) {
			depthTexture = addRenderTargetTexture(texture->getSize(), "IRR_DEPTH_STENCIL", video::ECF_D24S8);
			SharedDepthTextures.push_back(depthTexture);
		}

		SharedRenderTarget->setTexture(texture, depthTexture);

		return setRenderTargetEx(SharedRenderTarget, clearFlag, clearColor, clearDepth, clearStencil);
	} else {
		return setRenderTargetEx(0, clearFlag, clearColor, clearDepth, clearStencil);
	}
}

//! sets a viewport
void CNullDriver::setViewPort(const core::rect<s32> &area)
{
}

//! gets the area of the current viewport
const core::rect<s32> &CNullDriver::getViewPort() const
{
	return ViewPort;
}

//! draws a vertex primitive list
void CNullDriver::drawVertexPrimitiveList(const void *vertices, u32 vertexCount, const void *indexList, u32 primitiveCount, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	if ((iType == EIT_16BIT) && (vertexCount > 65536))
		os::Printer::log("Too many vertices for 16bit index type, render artifacts may occur.");
	FrameStats.Drawcalls++;
	FrameStats.PrimitivesDrawn += primitiveCount;
}

//! draws a vertex primitive list in 2d
void CNullDriver::draw2DVertexPrimitiveList(const void *vertices, u32 vertexCount, const void *indexList, u32 primitiveCount, E_VERTEX_TYPE vType, scene::E_PRIMITIVE_TYPE pType, E_INDEX_TYPE iType)
{
	if ((iType == EIT_16BIT) && (vertexCount > 65536))
		os::Printer::log("Too many vertices for 16bit index type, render artifacts may occur.");
	FrameStats.Drawcalls++;
	FrameStats.PrimitivesDrawn += primitiveCount;
}

//! Draws a 3d line.
void CNullDriver::draw3DLine(const core::vector3df &start,
		const core::vector3df &end, SColor color)
{
}

//! Draws a 3d axis aligned box.
void CNullDriver::draw3DBox(const core::aabbox3d<f32> &box, SColor color)
{
	core::vector3df edges[8];
	box.getEdges(edges);

	video::S3DVertex v[8];
	for (u32 i = 0; i < 8; i++) {
		v[i].Pos = edges[i];
		v[i].Color = color;
	}

	const static u16 box_indices[24] = {
		5, 1, 1, 3, 3, 7, 7, 5, 0, 2, 2, 6, 6, 4, 4, 0, 1, 0, 3, 2, 7, 6, 5, 4
	};

	drawVertexPrimitiveList(v, 8, box_indices, 12, EVT_STANDARD, scene::EPT_LINES);
}

//! draws an 2d image
void CNullDriver::draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos, bool useAlphaChannelOfTexture)
{
	if (!texture)
		return;

	draw2DImage(texture, destPos, core::rect<s32>(core::position2d<s32>(0, 0), core::dimension2di(texture->getOriginalSize())),
			0,
			SColor(255, 255, 255, 255),
			useAlphaChannelOfTexture);
}

//! draws a set of 2d images, using a color and the alpha channel of the
//! texture if desired.
void CNullDriver::draw2DImageBatch(const video::ITexture *texture,
		const core::array<core::position2d<s32>> &positions,
		const core::array<core::rect<s32>> &sourceRects,
		const core::rect<s32> *clipRect,
		SColor color,
		bool useAlphaChannelOfTexture)
{
	const u32 drawCount = core::min_<u32>(positions.size(), sourceRects.size());

	for (u32 i = 0; i < drawCount; ++i) {
		draw2DImage(texture, positions[i], sourceRects[i],
				clipRect, color, useAlphaChannelOfTexture);
	}
}

//! Draws a part of the texture into the rectangle.
void CNullDriver::draw2DImage(const video::ITexture *texture, const core::rect<s32> &destRect,
		const core::rect<s32> &sourceRect, const core::rect<s32> *clipRect,
		const video::SColor *const colors, bool useAlphaChannelOfTexture)
{
	if (destRect.isValid())
		draw2DImage(texture, core::position2d<s32>(destRect.UpperLeftCorner),
				sourceRect, clipRect, colors ? colors[0] : video::SColor(0xffffffff),
				useAlphaChannelOfTexture);
}

//! Draws a 2d image, using a color (if color is other then Color(255,255,255,255)) and the alpha channel of the texture if wanted.
void CNullDriver::draw2DImage(const video::ITexture *texture, const core::position2d<s32> &destPos,
		const core::rect<s32> &sourceRect,
		const core::rect<s32> *clipRect, SColor color,
		bool useAlphaChannelOfTexture)
{
}

//! Draw a 2d rectangle
void CNullDriver::draw2DRectangle(SColor color, const core::rect<s32> &pos, const core::rect<s32> *clip)
{
	draw2DRectangle(pos, color, color, color, color, clip);
}

//! Draws a 2d rectangle with a gradient.
void CNullDriver::draw2DRectangle(const core::rect<s32> &pos,
		SColor colorLeftUp, SColor colorRightUp, SColor colorLeftDown, SColor colorRightDown,
		const core::rect<s32> *clip)
{
}

//! Draws a 2d line.
void CNullDriver::draw2DLine(const core::position2d<s32> &start,
		const core::position2d<s32> &end, SColor color)
{
}

//! returns color format
ECOLOR_FORMAT CNullDriver::getColorFormat() const
{
	return ECF_R5G6B5;
}

//! returns screen size
const core::dimension2d<u32> &CNullDriver::getScreenSize() const
{
	return ScreenSize;
}

//! get current render target
IRenderTarget *CNullDriver::getCurrentRenderTarget() const
{
	return CurrentRenderTarget;
}

const core::dimension2d<u32> &CNullDriver::getCurrentRenderTargetSize() const
{
	if (CurrentRenderTargetSize.Width == 0)
		return ScreenSize;
	else
		return CurrentRenderTargetSize;
}

// returns current frames per second value
s32 CNullDriver::getFPS() const
{
	return FPSCounter.getFPS();
}

SFrameStats CNullDriver::getFrameStats() const
{
	return FrameStats;
}

//! \return Returns the name of the video driver. Example: In case of the DIRECT3D8
//! driver, it would return "Direct3D8".

const char *CNullDriver::getName() const
{
	return "Irrlicht NullDevice";
}

//! Creates a boolean alpha channel of the texture based of an color key.
void CNullDriver::makeColorKeyTexture(video::ITexture *texture,
		video::SColor color) const
{
	if (!texture)
		return;

	if (texture->getColorFormat() != ECF_A1R5G5B5 &&
			texture->getColorFormat() != ECF_A8R8G8B8) {
		os::Printer::log("Error: Unsupported texture color format for making color key channel.", ELL_ERROR);
		return;
	}

	if (texture->getColorFormat() == ECF_A1R5G5B5) {
		u16 *p = (u16 *)texture->lock();

		if (!p) {
			os::Printer::log("Could not lock texture for making color key channel.", ELL_ERROR);
			return;
		}

		const core::dimension2d<u32> dim = texture->getSize();
		const u32 pitch = texture->getPitch() / 2;

		// color with alpha disabled (i.e. fully transparent)
		const u16 refZeroAlpha = (0x7fff & color.toA1R5G5B5());

		const u32 pixels = pitch * dim.Height;

		for (u32 pixel = 0; pixel < pixels; ++pixel) {
			// If the color matches the reference color, ignoring alphas,
			// set the alpha to zero.
			if (((*p) & 0x7fff) == refZeroAlpha)
				(*p) = refZeroAlpha;

			++p;
		}

		texture->unlock();
	} else {
		u32 *p = (u32 *)texture->lock();

		if (!p) {
			os::Printer::log("Could not lock texture for making color key channel.", ELL_ERROR);
			return;
		}

		core::dimension2d<u32> dim = texture->getSize();
		u32 pitch = texture->getPitch() / 4;

		// color with alpha disabled (fully transparent)
		const u32 refZeroAlpha = 0x00ffffff & color.color;

		const u32 pixels = pitch * dim.Height;
		for (u32 pixel = 0; pixel < pixels; ++pixel) {
			// If the color matches the reference color, ignoring alphas,
			// set the alpha to zero.
			if (((*p) & 0x00ffffff) == refZeroAlpha)
				(*p) = refZeroAlpha;

			++p;
		}

		texture->unlock();
	}
	texture->regenerateMipMapLevels();
}

//! Creates an boolean alpha channel of the texture based of an color key position.
void CNullDriver::makeColorKeyTexture(video::ITexture *texture,
		core::position2d<s32> colorKeyPixelPos) const
{
	if (!texture)
		return;

	if (texture->getColorFormat() != ECF_A1R5G5B5 &&
			texture->getColorFormat() != ECF_A8R8G8B8) {
		os::Printer::log("Error: Unsupported texture color format for making color key channel.", ELL_ERROR);
		return;
	}

	SColor colorKey;

	if (texture->getColorFormat() == ECF_A1R5G5B5) {
		u16 *p = (u16 *)texture->lock(ETLM_READ_ONLY);

		if (!p) {
			os::Printer::log("Could not lock texture for making color key channel.", ELL_ERROR);
			return;
		}

		u32 pitch = texture->getPitch() / 2;

		const u16 key16Bit = 0x7fff & p[colorKeyPixelPos.Y * pitch + colorKeyPixelPos.X];

		colorKey = video::A1R5G5B5toA8R8G8B8(key16Bit);
	} else {
		u32 *p = (u32 *)texture->lock(ETLM_READ_ONLY);

		if (!p) {
			os::Printer::log("Could not lock texture for making color key channel.", ELL_ERROR);
			return;
		}

		u32 pitch = texture->getPitch() / 4;
		colorKey = 0x00ffffff & p[colorKeyPixelPos.Y * pitch + colorKeyPixelPos.X];
	}

	texture->unlock();
	makeColorKeyTexture(texture, colorKey);
}

//! Returns the maximum amount of primitives (mostly vertices) which
//! the device is able to render with one drawIndexedTriangleList
//! call.
u32 CNullDriver::getMaximalPrimitiveCount() const
{
	return 0xFFFFFFFF;
}

//! checks triangle count and print warning if wrong
bool CNullDriver::checkPrimitiveCount(u32 prmCount) const
{
	const u32 m = getMaximalPrimitiveCount();

	if (prmCount > m) {
		char tmp[128];
		snprintf_irr(tmp, sizeof(tmp), "Could not draw triangles, too many primitives(%u), maximum is %u.", prmCount, m);
		os::Printer::log(tmp, ELL_ERROR);
		return false;
	}

	return true;
}

bool CNullDriver::checkImage(IImage *image) const
{
	return true;
}

bool CNullDriver::checkImage(const std::vector<IImage*> &image) const
{
	if (image.empty())
		return false;

	ECOLOR_FORMAT lastFormat = image[0]->getColorFormat();
	auto lastSize = image[0]->getDimension();

	for (size_t i = 0; i < image.size(); ++i) {
		if (!image[i])
			return false;

		ECOLOR_FORMAT format = image[i]->getColorFormat();
		auto size = image[i]->getDimension();

		if (!checkImage(image[i]))
			return false;

		if (format != lastFormat || size != lastSize)
			return false;
	}
	return true;
}

//! Enables or disables a texture creation flag.
void CNullDriver::setTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag, bool enabled)
{
	if (enabled && ((flag == ETCF_ALWAYS_16_BIT) || (flag == ETCF_ALWAYS_32_BIT) || (flag == ETCF_OPTIMIZED_FOR_QUALITY) || (flag == ETCF_OPTIMIZED_FOR_SPEED))) {
		// disable other formats
		setTextureCreationFlag(ETCF_ALWAYS_16_BIT, false);
		setTextureCreationFlag(ETCF_ALWAYS_32_BIT, false);
		setTextureCreationFlag(ETCF_OPTIMIZED_FOR_QUALITY, false);
		setTextureCreationFlag(ETCF_OPTIMIZED_FOR_SPEED, false);
	}

	if (enabled)
		TextureCreationFlags |= flag;
	else
		TextureCreationFlags &= ~flag;
}

//! Returns if a texture creation flag is enabled or disabled.
bool CNullDriver::getTextureCreationFlag(E_TEXTURE_CREATION_FLAG flag) const
{
	return (TextureCreationFlags & flag) != 0;
}

IImage *CNullDriver::createImageFromFile(const io::path &filename)
{
	if (!filename.size())
		return nullptr;

	io::IReadFile *file = FileSystem->createAndOpenFile(filename);
	if (!file) {
		os::Printer::log("Could not open file of image", filename, ELL_WARNING);
		return nullptr;
	}

	IImage *image = createImageFromFile(file);
	file->drop();
	return image;
}

IImage *CNullDriver::createImageFromFile(io::IReadFile *file)
{
	if (!file)
		return nullptr;

	// try to load file based on file extension
	for (int i = SurfaceLoader.size() - 1; i >= 0; --i) {
		if (!SurfaceLoader[i]->isALoadableFileExtension(file->getFileName()))
			continue;

		file->seek(0); // reset file position which might have changed due to previous loadImage calls
		// avoid warnings if extension is wrong
		if (!SurfaceLoader[i]->isALoadableFileFormat(file))
			continue;

		file->seek(0);
		if (IImage *image = SurfaceLoader[i]->loadImage(file))
			return image;
	}

	// try to load file based on what is in it
	for (int i = SurfaceLoader.size() - 1; i >= 0; --i) {
		if (SurfaceLoader[i]->isALoadableFileExtension(file->getFileName()))
			continue;  // extension was tried above already
		file->seek(0); // dito
		if (!SurfaceLoader[i]->isALoadableFileFormat(file))
			continue;

		file->seek(0);
		if (IImage *image = SurfaceLoader[i]->loadImage(file))
			return image;
	}

	return nullptr;
}

//! Writes the provided image to disk file
bool CNullDriver::writeImageToFile(IImage *image, const io::path &filename, u32 param)
{
	io::IWriteFile *file = FileSystem->createAndWriteFile(filename);
	if (!file)
		return false;

	bool result = writeImageToFile(image, file, param);
	file->drop();

	return result;
}

//! Writes the provided image to a file.
bool CNullDriver::writeImageToFile(IImage *image, io::IWriteFile *file, u32 param)
{
	if (!file)
		return false;

	for (s32 i = SurfaceWriter.size() - 1; i >= 0; --i) {
		if (SurfaceWriter[i]->isAWriteableFileExtension(file->getFileName())) {
			bool written = SurfaceWriter[i]->writeImage(file, image, param);
			if (written)
				return true;
		}
	}
	return false; // failed to write
}

//! Creates a software image from a byte array.
IImage *CNullDriver::createImageFromData(ECOLOR_FORMAT format,
		const core::dimension2d<u32> &size, void *data, bool ownForeignMemory,
		bool deleteMemory)
{
	return new CImage(format, size, data, ownForeignMemory, deleteMemory);
}

//! Creates an empty software image.
IImage *CNullDriver::createImage(ECOLOR_FORMAT format, const core::dimension2d<u32> &size)
{
	return new CImage(format, size);
}

//! Creates a software image from part of a texture.
IImage *CNullDriver::createImage(ITexture *texture, const core::position2d<s32> &pos, const core::dimension2d<u32> &size)
{
	if ((pos == core::position2di(0, 0)) && (size == texture->getSize())) {
		void *data = texture->lock(ETLM_READ_ONLY);
		if (!data)
			return 0;
		IImage *image = new CImage(texture->getColorFormat(), size, data, false, false);
		texture->unlock();
		return image;
	} else {
		// make sure to avoid buffer overruns
		// make the vector a separate variable for g++ 3.x
		const core::vector2d<u32> leftUpper(core::clamp(static_cast<u32>(pos.X), 0u, texture->getSize().Width),
				core::clamp(static_cast<u32>(pos.Y), 0u, texture->getSize().Height));
		const core::rect<u32> clamped(leftUpper,
				core::dimension2du(core::clamp(static_cast<u32>(size.Width), 0u, texture->getSize().Width),
						core::clamp(static_cast<u32>(size.Height), 0u, texture->getSize().Height)));
		if (!clamped.isValid())
			return 0;
		u8 *src = static_cast<u8 *>(texture->lock(ETLM_READ_ONLY));
		if (!src)
			return 0;
		IImage *image = new CImage(texture->getColorFormat(), clamped.getSize());
		u8 *dst = static_cast<u8 *>(image->getData());
		src += clamped.UpperLeftCorner.Y * texture->getPitch() + image->getBytesPerPixel() * clamped.UpperLeftCorner.X;
		for (u32 i = 0; i < clamped.getHeight(); ++i) {
			video::CColorConverter::convert_viaFormat(src, texture->getColorFormat(), clamped.getWidth(), dst, image->getColorFormat());
			src += texture->getPitch();
			dst += image->getPitch();
		}
		texture->unlock();
		return image;
	}
}

//! Sets the fog mode.
void CNullDriver::setFog(SColor color, E_FOG_TYPE fogType, f32 start, f32 end,
		f32 density, bool pixelFog, bool rangeFog)
{
	FogColor = color;
	FogType = fogType;
	FogStart = start;
	FogEnd = end;
	FogDensity = density;
	PixelFog = pixelFog;
	RangeFog = rangeFog;
}

//! Gets the fog mode.
void CNullDriver::getFog(SColor &color, E_FOG_TYPE &fogType, f32 &start, f32 &end,
		f32 &density, bool &pixelFog, bool &rangeFog)
{
	color = FogColor;
	fogType = FogType;
	start = FogStart;
	end = FogEnd;
	density = FogDensity;
	pixelFog = PixelFog;
	rangeFog = RangeFog;
}

void CNullDriver::drawBuffers(const scene::IVertexBuffer *vb,
		const scene::IIndexBuffer *ib, u32 primCount,
		scene::E_PRIMITIVE_TYPE pType)
{
	if (!vb || !ib)
		return;

	if (vb->getHWBuffer() || ib->getHWBuffer()) {
		// subclass is supposed to override this if it supports hw buffers
		assert(false);
	}

	drawVertexPrimitiveList(vb->getData(), vb->getCount(), ib->getData(),
		primCount, vb->getType(), pType, ib->getType());
}

//! Draws the normals of a mesh buffer
void CNullDriver::drawMeshBufferNormals(const scene::IMeshBuffer *mb, f32 length, SColor color)
{
	const u32 count = mb->getVertexCount();
	for (u32 i = 0; i < count; ++i) {
		core::vector3df normal = mb->getNormal(i);
		const core::vector3df &pos = mb->getPosition(i);
		draw3DLine(pos, pos + (normal * length), color);
	}
}

CNullDriver::SHWBufferLink *CNullDriver::getBufferLink(const scene::IVertexBuffer *vb)
{
	if (!vb || !isHardwareBufferRecommend(vb))
		return 0;

	// search for hardware links
	SHWBufferLink *HWBuffer = reinterpret_cast<SHWBufferLink *>(vb->getHWBuffer());
	if (HWBuffer)
		return HWBuffer;

	return createHardwareBuffer(vb); // no hardware links, and mesh wants one, create it
}

CNullDriver::SHWBufferLink *CNullDriver::getBufferLink(const scene::IIndexBuffer *ib)
{
	if (!ib || !isHardwareBufferRecommend(ib))
		return 0;

	// search for hardware links
	SHWBufferLink *HWBuffer = reinterpret_cast<SHWBufferLink *>(ib->getHWBuffer());
	if (HWBuffer)
		return HWBuffer;

	return createHardwareBuffer(ib); // no hardware links, and mesh wants one, create it
}

void CNullDriver::registerHardwareBuffer(SHWBufferLink *HWBuffer)
{
	assert(HWBuffer);
	HWBuffer->ListPosition = HWBufferList.size();
	HWBufferList.push_back(HWBuffer);
}

void CNullDriver::expireHardwareBuffers()
{
	for (size_t i = 0; i < HWBufferList.size(); ) {
		auto *Link = HWBufferList[i];

		bool del;
		if (Link->IsVertex)
			del = !Link->VertexBuffer || Link->VertexBuffer->getReferenceCount() == 1;
		else
			del = !Link->IndexBuffer || Link->IndexBuffer->getReferenceCount() == 1;
		// deleting can reorder, so don't advance in list
		if (del)
			deleteHardwareBuffer(Link);
		else
			i++;
	}

	FrameStats.HWBuffersActive = HWBufferList.size();
}

void CNullDriver::deleteHardwareBuffer(SHWBufferLink *HWBuffer)
{
	if (!HWBuffer)
		return;
	const size_t pos = HWBuffer->ListPosition;
	assert(HWBufferList.at(pos) == HWBuffer);
	if (HWBufferList.size() < 2 || pos == HWBufferList.size() - 1) {
		HWBufferList.erase(HWBufferList.begin() + pos);
	} else {
		// swap with last
		std::swap(HWBufferList[pos], HWBufferList.back());
		HWBufferList.pop_back();
		HWBufferList[pos]->ListPosition = pos;
	}
	delete HWBuffer;
}

void CNullDriver::updateHardwareBuffer(const scene::IVertexBuffer *vb)
{
	if (!vb)
		return;
	auto *link = getBufferLink(vb);
	if (link)
		updateHardwareBuffer(link);
}

void CNullDriver::updateHardwareBuffer(const scene::IIndexBuffer *ib)
{
	if (!ib)
		return;
	auto *link = getBufferLink(ib);
	if (link)
		updateHardwareBuffer(link);
}

void CNullDriver::removeHardwareBuffer(const scene::IVertexBuffer *vb)
{
	if (!vb)
		return;
	SHWBufferLink *HWBuffer = reinterpret_cast<SHWBufferLink *>(vb->getHWBuffer());
	if (HWBuffer)
		deleteHardwareBuffer(HWBuffer);
}

void CNullDriver::removeHardwareBuffer(const scene::IIndexBuffer *ib)
{
	if (!ib)
		return;
	SHWBufferLink *HWBuffer = reinterpret_cast<SHWBufferLink *>(ib->getHWBuffer());
	if (HWBuffer)
		deleteHardwareBuffer(HWBuffer);
}

//! Remove all hardware buffers
void CNullDriver::removeAllHardwareBuffers()
{
	while (!HWBufferList.empty())
		deleteHardwareBuffer(HWBufferList.front());
}

bool CNullDriver::isHardwareBufferRecommend(const scene::IVertexBuffer *vb)
{
	if (!vb || vb->getHardwareMappingHint() == scene::EHM_NEVER)
		return false;

	if (vb->getCount() < MinVertexCountForVBO)
		return false;

	return true;
}

bool CNullDriver::isHardwareBufferRecommend(const scene::IIndexBuffer *ib)
{
	if (!ib || ib->getHardwareMappingHint() == scene::EHM_NEVER)
		return false;

	// This is a bit stupid
	if (ib->getCount() < MinVertexCountForVBO * 3)
		return false;

	return true;
}

//! Create occlusion query.
/** Use node for identification and mesh for occlusion test. */
void CNullDriver::addOcclusionQuery(scene::ISceneNode *node, const scene::IMesh *mesh)
{
	if (!node)
		return;
	if (!mesh) {
		if ((node->getType() != scene::ESNT_MESH) && (node->getType() != scene::ESNT_ANIMATED_MESH))
			return;
		else if (node->getType() == scene::ESNT_MESH)
			mesh = static_cast<scene::IMeshSceneNode *>(node)->getMesh();
		else
			mesh = static_cast<scene::IAnimatedMeshSceneNode *>(node)->getMesh();
		if (!mesh)
			return;
	}

	// search for query
	s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index != -1) {
		if (OcclusionQueries[index].Mesh != mesh) {
			OcclusionQueries[index].Mesh->drop();
			OcclusionQueries[index].Mesh = mesh;
			mesh->grab();
		}
	} else {
		OcclusionQueries.push_back(SOccQuery(node, mesh));
		node->setAutomaticCulling(node->getAutomaticCulling() | scene::EAC_OCC_QUERY);
	}
}

//! Remove occlusion query.
void CNullDriver::removeOcclusionQuery(scene::ISceneNode *node)
{
	// search for query
	s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index != -1) {
		node->setAutomaticCulling(node->getAutomaticCulling() & ~scene::EAC_OCC_QUERY);
		OcclusionQueries.erase(index);
	}
}

//! Remove all occlusion queries.
void CNullDriver::removeAllOcclusionQueries()
{
	for (s32 i = OcclusionQueries.size() - 1; i >= 0; --i) {
		removeOcclusionQuery(OcclusionQueries[i].Node);
	}
}

//! Run occlusion query. Draws mesh stored in query.
/** If the mesh shall be rendered visible, use
flag to enable the proper material setting. */
void CNullDriver::runOcclusionQuery(scene::ISceneNode *node, bool visible)
{
	if (!node)
		return;
	s32 index = OcclusionQueries.linear_search(SOccQuery(node));
	if (index == -1)
		return;
	OcclusionQueries[index].Run = 0;
	if (!visible) {
		SMaterial mat;
		mat.AntiAliasing = video::EAAM_OFF;
		mat.ColorMask = ECP_NONE;
		mat.ZWriteEnable = EZW_OFF;
		setMaterial(mat);
	}
	setTransform(video::ETS_WORLD, node->getAbsoluteTransformation());
	const scene::IMesh *mesh = OcclusionQueries[index].Mesh;
	for (u32 i = 0; i < mesh->getMeshBufferCount(); ++i) {
		if (visible)
			setMaterial(mesh->getMeshBuffer(i)->getMaterial());
		drawMeshBuffer(mesh->getMeshBuffer(i));
	}
}

//! Run all occlusion queries. Draws all meshes stored in queries.
/** If the meshes shall not be rendered visible, use
overrideMaterial to disable the color and depth buffer. */
void CNullDriver::runAllOcclusionQueries(bool visible)
{
	for (u32 i = 0; i < OcclusionQueries.size(); ++i)
		runOcclusionQuery(OcclusionQueries[i].Node, visible);
}

//! Update occlusion query. Retrieves results from GPU.
/** If the query shall not block, set the flag to false.
Update might not occur in this case, though */
void CNullDriver::updateOcclusionQuery(scene::ISceneNode *node, bool block)
{
}

//! Update all occlusion queries. Retrieves results from GPU.
/** If the query shall not block, set the flag to false.
Update might not occur in this case, though */
void CNullDriver::updateAllOcclusionQueries(bool block)
{
	for (u32 i = 0; i < OcclusionQueries.size(); ++i) {
		if (OcclusionQueries[i].Run == u32(~0))
			continue;
		updateOcclusionQuery(OcclusionQueries[i].Node, block);
		++OcclusionQueries[i].Run;
		if (OcclusionQueries[i].Run > 1000)
			removeOcclusionQuery(OcclusionQueries[i].Node);
	}
}

//! Return query result.
/** Return value is the number of visible pixels/fragments.
The value is a safe approximation, i.e. can be larger then the
actual value of pixels. */
u32 CNullDriver::getOcclusionQueryResult(scene::ISceneNode *node) const
{
	return ~0;
}

//! Create render target.
IRenderTarget *CNullDriver::addRenderTarget()
{
	return 0;
}

//! Remove render target.
void CNullDriver::removeRenderTarget(IRenderTarget *renderTarget)
{
	if (!renderTarget)
		return;

	for (u32 i = 0; i < RenderTargets.size(); ++i) {
		if (RenderTargets[i] == renderTarget) {
			RenderTargets[i]->drop();
			RenderTargets.erase(i);

			return;
		}
	}
}

//! Remove all render targets.
void CNullDriver::removeAllRenderTargets()
{
	for (u32 i = 0; i < RenderTargets.size(); ++i)
		RenderTargets[i]->drop();

	RenderTargets.clear();

	SharedRenderTarget = 0;
}

//! Only used by the internal engine. Used to notify the driver that
//! the window was resized.
void CNullDriver::OnResize(const core::dimension2d<u32> &size)
{
	if (ViewPort.getWidth() == (s32)ScreenSize.Width &&
			ViewPort.getHeight() == (s32)ScreenSize.Height)
		ViewPort = core::rect<s32>(core::position2d<s32>(0, 0),
				core::dimension2di(size));

	ScreenSize = size;
}

// adds a material renderer and drops it afterwards. To be used for internal creation
s32 CNullDriver::addAndDropMaterialRenderer(IMaterialRenderer *m)
{
	s32 i = addMaterialRenderer(m);

	if (m)
		m->drop();

	return i;
}

//! Adds a new material renderer to the video device.
s32 CNullDriver::addMaterialRenderer(IMaterialRenderer *renderer, const char *name)
{
	if (!renderer)
		return -1;

	SMaterialRenderer r;
	r.Renderer = renderer;
	r.Name = name;

	if (name == 0 && MaterialRenderers.size() < numBuiltInMaterials) {
		// set name of built in renderer so that we don't have to implement name
		// setting in all available renderers.
		r.Name = sBuiltInMaterialTypeNames[MaterialRenderers.size()];
	}

	MaterialRenderers.push_back(r);
	renderer->grab();

	return MaterialRenderers.size() - 1;
}

//! Sets the name of a material renderer.
void CNullDriver::setMaterialRendererName(u32 idx, const char *name)
{
	if (idx < numBuiltInMaterials || idx >= MaterialRenderers.size())
		return;

	MaterialRenderers[idx].Name = name;
}

void CNullDriver::swapMaterialRenderers(u32 idx1, u32 idx2, bool swapNames)
{
	if (idx1 < MaterialRenderers.size() && idx2 < MaterialRenderers.size()) {
		std::swap(MaterialRenderers[idx1].Renderer, MaterialRenderers[idx2].Renderer);
		if (swapNames)
			std::swap(MaterialRenderers[idx1].Name, MaterialRenderers[idx2].Name);
	}
}

//! Returns driver and operating system specific data about the IVideoDriver.
const SExposedVideoData &CNullDriver::getExposedVideoData()
{
	return ExposedData;
}

//! Returns type of video driver
E_DRIVER_TYPE CNullDriver::getDriverType() const
{
	return EDT_NULL;
}

//! deletes all material renderers
void CNullDriver::deleteMaterialRenders()
{
	// delete material renderers
	for (u32 i = 0; i < MaterialRenderers.size(); ++i)
		if (MaterialRenderers[i].Renderer)
			MaterialRenderers[i].Renderer->drop();

	MaterialRenderers.clear();
}

//! Returns pointer to material renderer or null
IMaterialRenderer *CNullDriver::getMaterialRenderer(u32 idx) const
{
	if (idx < MaterialRenderers.size())
		return MaterialRenderers[idx].Renderer;
	else
		return 0;
}

//! Returns amount of currently available material renderers.
u32 CNullDriver::getMaterialRendererCount() const
{
	return MaterialRenderers.size();
}

//! Returns name of the material renderer
const char *CNullDriver::getMaterialRendererName(u32 idx) const
{
	if (idx < MaterialRenderers.size())
		return MaterialRenderers[idx].Name.c_str();

	return 0;
}

//! Returns pointer to the IGPUProgrammingServices interface.
IGPUProgrammingServices *CNullDriver::getGPUProgrammingServices()
{
	return this;
}

//! Adds a new material renderer to the VideoDriver, based on a high level shading language.
s32 CNullDriver::addHighLevelShaderMaterial(
		const c8 *vertexShaderProgram,
		const c8 *pixelShaderProgram,
		const c8 *geometryShaderProgram,
		const c8 *shaderName,
		scene::E_PRIMITIVE_TYPE inType, scene::E_PRIMITIVE_TYPE outType,
		u32 verticesOut,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial,
		s32 userData)
{
	os::Printer::log("Shader materials not available in this driver", ELL_ERROR);
	return -1;
}

s32 CNullDriver::addHighLevelShaderMaterialFromFiles(
		const io::path &vertexShaderProgramFileName,
		const io::path &pixelShaderProgramFileName,
		const io::path &geometryShaderProgramFileName,
		const c8 *shaderName,
		scene::E_PRIMITIVE_TYPE inType, scene::E_PRIMITIVE_TYPE outType,
		u32 verticesOut,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial,
		s32 userData)
{
	io::IReadFile *vsfile = 0;
	io::IReadFile *psfile = 0;
	io::IReadFile *gsfile = 0;

	if (vertexShaderProgramFileName.size()) {
		vsfile = FileSystem->createAndOpenFile(vertexShaderProgramFileName);
		if (!vsfile) {
			os::Printer::log("Could not open vertex shader program file",
					vertexShaderProgramFileName, ELL_WARNING);
		}
	}

	if (pixelShaderProgramFileName.size()) {
		psfile = FileSystem->createAndOpenFile(pixelShaderProgramFileName);
		if (!psfile) {
			os::Printer::log("Could not open pixel shader program file",
					pixelShaderProgramFileName, ELL_WARNING);
		}
	}

	if (geometryShaderProgramFileName.size()) {
		gsfile = FileSystem->createAndOpenFile(geometryShaderProgramFileName);
		if (!gsfile) {
			os::Printer::log("Could not open geometry shader program file",
					geometryShaderProgramFileName, ELL_WARNING);
		}
	}

	s32 result = addHighLevelShaderMaterialFromFiles(
			vsfile, psfile, gsfile, shaderName,
			inType, outType, verticesOut,
			callback, baseMaterial, userData);

	if (psfile)
		psfile->drop();

	if (vsfile)
		vsfile->drop();

	if (gsfile)
		gsfile->drop();

	return result;
}

s32 CNullDriver::addHighLevelShaderMaterialFromFiles(
		io::IReadFile *vertexShaderProgram,
		io::IReadFile *pixelShaderProgram,
		io::IReadFile *geometryShaderProgram,
		const c8 *shaderName,
		scene::E_PRIMITIVE_TYPE inType, scene::E_PRIMITIVE_TYPE outType,
		u32 verticesOut,
		IShaderConstantSetCallBack *callback,
		E_MATERIAL_TYPE baseMaterial,
		s32 userData)
{
	c8 *vs = 0;
	c8 *ps = 0;
	c8 *gs = 0;

	if (vertexShaderProgram) {
		const long size = vertexShaderProgram->getSize();
		if (size) {
			vs = new c8[size + 1];
			vertexShaderProgram->read(vs, size);
			vs[size] = 0;
		}
	}

	if (pixelShaderProgram) {
		const long size = pixelShaderProgram->getSize();
		if (size) {
			// if both handles are the same we must reset the file
			if (pixelShaderProgram == vertexShaderProgram)
				pixelShaderProgram->seek(0);
			ps = new c8[size + 1];
			pixelShaderProgram->read(ps, size);
			ps[size] = 0;
		}
	}

	if (geometryShaderProgram) {
		const long size = geometryShaderProgram->getSize();
		if (size) {
			// if both handles are the same we must reset the file
			if ((geometryShaderProgram == vertexShaderProgram) ||
					(geometryShaderProgram == pixelShaderProgram))
				geometryShaderProgram->seek(0);
			gs = new c8[size + 1];
			geometryShaderProgram->read(gs, size);
			gs[size] = 0;
		}
	}

	s32 result = this->addHighLevelShaderMaterial(
			vs, ps, gs, shaderName,
			inType, outType, verticesOut,
			callback, baseMaterial, userData);

	delete[] vs;
	delete[] ps;
	delete[] gs;

	return result;
}

void CNullDriver::deleteShaderMaterial(s32 material)
{
	const u32 idx = (u32)material;
	if (idx < numBuiltInMaterials || idx >= MaterialRenderers.size())
		return;

	// if this is the last material we can drop it without consequence
	if (idx == MaterialRenderers.size() - 1) {
		if (MaterialRenderers[idx].Renderer)
			MaterialRenderers[idx].Renderer->drop();
		MaterialRenderers.erase(idx);
		return;
	}
	// otherwise replace with a dummy renderer, we have to preserve the IDs
	auto &ref = MaterialRenderers[idx];
	if (ref.Renderer)
		ref.Renderer->drop();
	ref.Renderer = new CDummyMaterialRenderer();
	ref.Name.clear();
}

//! Creates a render target texture.
ITexture *CNullDriver::addRenderTargetTexture(const core::dimension2d<u32> &size,
		const io::path &name, const ECOLOR_FORMAT format)
{
	return 0;
}

ITexture *CNullDriver::addRenderTargetTextureMs(const core::dimension2d<u32> &size, u8 msaa,
		const io::path &name, const ECOLOR_FORMAT format)
{
	return 0;
}

ITexture *CNullDriver::addRenderTargetTextureCubemap(const u32 sideLen,
		const io::path &name, const ECOLOR_FORMAT format)
{
	return 0;
}

void CNullDriver::clearBuffers(u16 flag, SColor color, f32 depth, u8 stencil)
{
}

//! Returns a pointer to the mesh manipulator.
scene::IMeshManipulator *CNullDriver::getMeshManipulator()
{
	return MeshManipulator;
}

//! Returns an image created from the last rendered frame.
IImage *CNullDriver::createScreenShot(video::ECOLOR_FORMAT format, video::E_RENDER_TARGET target)
{
	return 0;
}

// prints renderer version
void CNullDriver::printVersion()
{
	core::stringc namePrint = "Using renderer: ";
	namePrint += getName();
	os::Printer::log(namePrint.c_str(), ELL_INFORMATION);
}

//! creates a video driver
IVideoDriver *createNullDriver(io::IFileSystem *io, const core::dimension2d<u32> &screenSize)
{
	CNullDriver *nullDriver = new CNullDriver(io, screenSize);

	// create empty material renderers
	for (u32 i = 0; sBuiltInMaterialTypeNames[i]; ++i) {
		IMaterialRenderer *imr = new IMaterialRenderer();
		nullDriver->addMaterialRenderer(imr);
		imr->drop();
	}

	return nullDriver;
}

void CNullDriver::setMinHardwareBufferVertexCount(u32 count)
{
	MinVertexCountForVBO = count;
}

SOverrideMaterial &CNullDriver::getOverrideMaterial()
{
	return OverrideMaterial;
}

//! Get the 2d override material for altering its values
SMaterial &CNullDriver::getMaterial2D()
{
	return OverrideMaterial2D;
}

//! Enable the 2d override material
void CNullDriver::enableMaterial2D(bool enable)
{
	OverrideMaterial2DEnabled = enable;
}

core::dimension2du CNullDriver::getMaxTextureSize() const
{
	return core::dimension2du(0x10000, 0x10000); // maybe large enough
}

bool CNullDriver::needsTransparentRenderPass(const video::SMaterial &material) const
{
	// TODO: I suspect it would be nice if the material had an enum for further control.
	//		Especially it probably makes sense to allow disabling transparent render pass as soon as material.ZWriteEnable is on.
	//      But then we might want an enum for the renderpass in material instead of just a transparency flag in material - and that's more work.
	//      Or we could at least set return false when material.ZWriteEnable is EZW_ON? Still considering that...
	//		Be careful - this function is deeply connected to getWriteZBuffer as transparent render passes are usually about rendering with
	//      zwrite disabled and getWriteZBuffer calls this function.

	video::IMaterialRenderer *rnd = getMaterialRenderer(material.MaterialType);
	// TODO: I suspect IMaterialRenderer::isTransparent also often could use SMaterial as parameter
	//       We could for example then get rid of IsTransparent function in SMaterial and move that to the software material renderer.
	if (rnd && rnd->isTransparent())
		return true;

	return false;
}

} // end namespace
