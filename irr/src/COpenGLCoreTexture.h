// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "SMaterialLayer.h"
#include "ITexture.h"
#include "EDriverFeatures.h"
#include "os.h"
#include "CImage.h"
#include "CColorConverter.h"

#include "mt_opengl.h"

namespace irr
{
namespace video
{

template <class TOpenGLDriver>
class COpenGLCoreTexture : public ITexture
{
public:
	struct SStatesCache
	{
		SStatesCache() :
				WrapU(ETC_REPEAT), WrapV(ETC_REPEAT), WrapW(ETC_REPEAT),
				LODBias(0), AnisotropicFilter(0), MinFilter(video::ETMINF_NEAREST_MIPMAP_NEAREST),
				MagFilter(video::ETMAGF_NEAREST), MipMapStatus(false), IsCached(false)
		{
		}

		u8 WrapU;
		u8 WrapV;
		u8 WrapW;
		s8 LODBias;
		u8 AnisotropicFilter;
		video::E_TEXTURE_MIN_FILTER MinFilter;
		video::E_TEXTURE_MAG_FILTER MagFilter;
		bool MipMapStatus;
		bool IsCached;
	};

	COpenGLCoreTexture(const io::path &name, const std::vector<IImage *> &srcImages, E_TEXTURE_TYPE type, TOpenGLDriver *driver) :
			ITexture(name, type), Driver(driver), TextureType(GL_TEXTURE_2D),
			TextureName(0), InternalFormat(GL_RGBA), PixelFormat(GL_RGBA), PixelType(GL_UNSIGNED_BYTE), MSAA(0), Converter(0), LockReadOnly(false), LockImage(0), LockLayer(0),
			KeepImage(false), MipLevelStored(0)
	{
		_IRR_DEBUG_BREAK_IF(srcImages.empty())

		DriverType = Driver->getDriverType();
		_IRR_DEBUG_BREAK_IF(Type == ETT_2D_MS) // not supported by this constructor
		TextureType = TextureTypeIrrToGL(Type);
		HasMipMaps = Driver->getTextureCreationFlag(ETCF_CREATE_MIP_MAPS);
		KeepImage = Driver->getTextureCreationFlag(ETCF_ALLOW_MEMORY_COPY);

		getImageValues(srcImages[0]);
		if (!InternalFormat)
			return;

		char lbuf[128];
		snprintf_irr(lbuf, sizeof(lbuf),
			"COpenGLCoreTexture: Type = %d Size = %dx%d (%dx%d) ColorFormat = %d (%d)%s -> %#06x %#06x %#06x%s",
			(int)Type, Size.Width, Size.Height, OriginalSize.Width, OriginalSize.Height,
			(int)ColorFormat, (int)OriginalColorFormat,
			HasMipMaps ? " +Mip" : "",
			InternalFormat, PixelFormat, PixelType, Converter ? " (c)" : ""
		);
		os::Printer::log(lbuf, ELL_DEBUG);

		const auto *tmpImages = &srcImages;

		if (KeepImage || OriginalSize != Size || OriginalColorFormat != ColorFormat) {
			Images.resize(srcImages.size());

			for (size_t i = 0; i < srcImages.size(); ++i) {
				Images[i] = Driver->createImage(ColorFormat, Size);

				if (srcImages[i]->getDimension() == Size)
					srcImages[i]->copyTo(Images[i]);
				else
					srcImages[i]->copyToScaling(Images[i]);
			}

			tmpImages = &Images;
		}

		GL.GenTextures(1, &TextureName);
		TEST_GL_ERROR(Driver);
		if (!TextureName) {
			os::Printer::log("COpenGLCoreTexture: texture not created", ELL_ERROR);
			return;
		}

		const COpenGLCoreTexture *prevTexture = Driver->getCacheHandler()->getTextureCache().get(0);
		Driver->getCacheHandler()->getTextureCache().set(0, this);

		GL.TexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		GL.TexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		if (HasMipMaps) {
			if (Driver->getTextureCreationFlag(ETCF_OPTIMIZED_FOR_SPEED))
				GL.Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
			else if (Driver->getTextureCreationFlag(ETCF_OPTIMIZED_FOR_QUALITY))
				GL.Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
			else
				GL.Hint(GL_GENERATE_MIPMAP_HINT, GL_DONT_CARE);
		}
		TEST_GL_ERROR(Driver);

		for (size_t i = 0; i < tmpImages->size(); ++i)
			uploadTexture(true, i, 0, (*tmpImages)[i]->getData());

		if (HasMipMaps) {
			for (size_t i = 0; i < tmpImages->size(); ++i)
				regenerateMipMapLevels(i);
		}

		if (!KeepImage) {
			for (size_t i = 0; i < Images.size(); ++i)
				Images[i]->drop();

			Images.clear();
		}

		if (!name.empty())
			Driver->irrGlObjectLabel(GL_TEXTURE, TextureName, name.c_str());

		Driver->getCacheHandler()->getTextureCache().set(0, prevTexture);

		TEST_GL_ERROR(Driver);
	}

	COpenGLCoreTexture(const io::path &name, const core::dimension2d<u32> &size, E_TEXTURE_TYPE type, ECOLOR_FORMAT format, TOpenGLDriver *driver, u8 msaa = 0) :
			ITexture(name, type),
			Driver(driver), TextureType(GL_TEXTURE_2D),
			TextureName(0), InternalFormat(GL_RGBA), PixelFormat(GL_RGBA), PixelType(GL_UNSIGNED_BYTE), MSAA(msaa), Converter(0), LockReadOnly(false), LockImage(0), LockLayer(0), KeepImage(false),
			MipLevelStored(0)
	{
		DriverType = Driver->getDriverType();
		TextureType = TextureTypeIrrToGL(Type);
		HasMipMaps = false;
		IsRenderTarget = true;

		OriginalColorFormat = format;

		if (ECF_UNKNOWN == OriginalColorFormat)
			ColorFormat = getBestColorFormat(Driver->getColorFormat());
		else
			ColorFormat = OriginalColorFormat;

		OriginalSize = size;
		Size = OriginalSize;

		Pitch = Size.Width * IImage::getBitsPerPixelFromFormat(ColorFormat) / 8;

		if (!Driver->getColorFormatParameters(ColorFormat, InternalFormat, PixelFormat, PixelType, &Converter)) {
			os::Printer::log("COpenGLCoreTexture: Color format is not supported", ColorFormatNames[ColorFormat < ECF_UNKNOWN ? ColorFormat : ECF_UNKNOWN], ELL_ERROR);
			return;
		}

#ifndef IRR_COMPILE_GL_COMMON
		// On GLES 3.0 we must use sized internal formats for textures in certain
		// cases (e.g. with ETT_2D_MS). However ECF_A8R8G8B8 is mapped to GL_BGRA
		// (an unsized format).
		// Since we don't upload to RTT we can safely pick a different combo that works.
		if (InternalFormat == GL_BGRA && Driver->Version.Major >= 3) {
			InternalFormat = GL_RGBA8;
			PixelFormat = GL_RGBA;
		}
#endif

		char lbuf[100];
		snprintf_irr(lbuf, sizeof(lbuf),
			"COpenGLCoreTexture: RTT Type = %d Size = %dx%d ColorFormat = %d -> %#06x %#06x %#06x%s",
			(int)Type, Size.Width, Size.Height, (int)ColorFormat,
			InternalFormat, PixelFormat, PixelType, Converter ? " (c)" : ""
		);
		os::Printer::log(lbuf, ELL_DEBUG);

		GL.GenTextures(1, &TextureName);
		TEST_GL_ERROR(Driver);
		if (!TextureName) {
			os::Printer::log("COpenGLCoreTexture: texture not created", ELL_ERROR);
			return;
		}

		const COpenGLCoreTexture *prevTexture = Driver->getCacheHandler()->getTextureCache().get(0);
		Driver->getCacheHandler()->getTextureCache().set(0, this);

		// An INVALID_ENUM error is generated by TexParameter* if target is either
		// TEXTURE_2D_MULTISAMPLE or TEXTURE_2D_MULTISAMPLE_ARRAY, and pname is any
		// sampler state from table 23.18.
		// ~ https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
		if (Type != ETT_2D_MS) {
			GL.TexParameteri(TextureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			GL.TexParameteri(TextureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			GL.TexParameteri(TextureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GL.TexParameteri(TextureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			GL.TexParameteri(TextureType, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			StatesCache.WrapU = ETC_CLAMP_TO_EDGE;
			StatesCache.WrapV = ETC_CLAMP_TO_EDGE;
			StatesCache.WrapW = ETC_CLAMP_TO_EDGE;
		}

		switch (Type) {
		case ETT_2D:
			GL.TexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			break;
		case ETT_2D_MS: {
			// glTexImage2DMultisample is supported by OpenGL 3.2+
			// glTexStorage2DMultisample is supported by OpenGL 4.3+ and OpenGL ES 3.1+
#ifdef IRR_COMPILE_GL_COMMON // legacy driver
			constexpr bool use_gl_impl = true;
#else
			const bool use_gl_impl = Driver->Version.Spec != OpenGLSpec::ES;
#endif
			GLint max_samples = 0;
			GL.GetIntegerv(GL_MAX_SAMPLES, &max_samples);
			MSAA = core::min_(MSAA, (u8)max_samples);

			if (use_gl_impl)
				GL.TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA, InternalFormat, Size.Width, Size.Height, GL_TRUE);
			else
				GL.TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA, InternalFormat, Size.Width, Size.Height, GL_TRUE);
			break;
		}
		case ETT_CUBEMAP:
			GL.TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			GL.TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			GL.TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			GL.TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			GL.TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			GL.TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, InternalFormat, Size.Width, Size.Height, 0, PixelFormat, PixelType, 0);
			break;
		default:
			_IRR_DEBUG_BREAK_IF(1)
			break;
		}

		if (!name.empty())
			Driver->irrGlObjectLabel(GL_TEXTURE, TextureName, name.c_str());

		Driver->getCacheHandler()->getTextureCache().set(0, prevTexture);
		if (TEST_GL_ERROR(Driver)) {
			char msg[256];
			snprintf_irr(msg, 256, "COpenGLCoreTexture: InternalFormat:0x%04x PixelFormat:0x%04x", (int)InternalFormat, (int)PixelFormat);
			os::Printer::log(msg, ELL_ERROR);
		}
	}

	virtual ~COpenGLCoreTexture()
	{
		if (TextureName)
			GL.DeleteTextures(1, &TextureName);

		if (LockImage)
			LockImage->drop();

		for (auto *image : Images)
			image->drop();
	}

	void *lock(E_TEXTURE_LOCK_MODE mode = ETLM_READ_WRITE, u32 mipmapLevel = 0, u32 layer = 0, E_TEXTURE_LOCK_FLAGS lockFlags = ETLF_FLIP_Y_UP_RTT) override
	{
		if (LockImage)
			return LockImage->getData();

		if (IImage::isCompressedFormat(ColorFormat))
			return 0;

		LockReadOnly |= (mode == ETLM_READ_ONLY);
		LockLayer = layer;
		MipLevelStored = mipmapLevel;

		if (KeepImage) {
			_IRR_DEBUG_BREAK_IF(LockLayer > Images.size())

			if (mipmapLevel == 0) {
				LockImage = Images[LockLayer];
				LockImage->grab();
			}
		}

		if (!LockImage) {
			core::dimension2d<u32> lockImageSize(IImage::getMipMapsSize(Size, MipLevelStored));
			_IRR_DEBUG_BREAK_IF(lockImageSize.Width == 0 || lockImageSize.Height == 0)

			LockImage = Driver->createImage(ColorFormat, lockImageSize);

			if (LockImage && mode != ETLM_WRITE_ONLY) {
				bool passed = true;

#ifdef IRR_COMPILE_GL_COMMON // legacy driver
				constexpr bool use_gl_impl = true;
#else
				const bool use_gl_impl = Driver->Version.Spec != OpenGLSpec::ES;
#endif

				if (use_gl_impl) {

				IImage *tmpImage = LockImage;

				Driver->getCacheHandler()->getTextureCache().set(0, this);
				TEST_GL_ERROR(Driver);

				GLenum tmpTextureType = getTextureTarget(layer);

				GL.GetTexImage(tmpTextureType, MipLevelStored, PixelFormat, PixelType, tmpImage->getData());
				TEST_GL_ERROR(Driver);

				if (IsRenderTarget && lockFlags == ETLF_FLIP_Y_UP_RTT) {
					const s32 pitch = tmpImage->getPitch();

					u8 *srcA = static_cast<u8 *>(tmpImage->getData());
					u8 *srcB = srcA + (tmpImage->getDimension().Height - 1) * pitch;

					u8 *tmpBuffer = new u8[pitch];

					for (u32 i = 0; i < tmpImage->getDimension().Height; i += 2) {
						memcpy(tmpBuffer, srcA, pitch);
						memcpy(srcA, srcB, pitch);
						memcpy(srcB, tmpBuffer, pitch);
						srcA += pitch;
						srcB -= pitch;
					}

					delete[] tmpBuffer;
				}

				} else {

				GLuint tmpFBO = 0;
				Driver->irrGlGenFramebuffers(1, &tmpFBO);

				GLuint prevFBO = 0;
				Driver->getCacheHandler()->getFBO(prevFBO);
				Driver->getCacheHandler()->setFBO(tmpFBO);

				GLenum tmpTextureType = getTextureTarget(layer);

				// Warning: on GLES 2.0 this call will only work with mipmapLevel == 0
				Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
					tmpTextureType, getOpenGLTextureName(), mipmapLevel);
				TEST_GL_ERROR(Driver);

				IImage *tmpImage = Driver->createImage(ECF_A8R8G8B8, lockImageSize);
				GL.ReadPixels(0, 0, lockImageSize.Width, lockImageSize.Height,
					GL_RGBA, GL_UNSIGNED_BYTE, tmpImage->getData());

				Driver->irrGlFramebufferTexture2D(GL_FRAMEBUFFER,
					GL_COLOR_ATTACHMENT0, tmpTextureType, 0, 0);

				Driver->getCacheHandler()->setFBO(prevFBO);

				Driver->irrGlDeleteFramebuffers(1, &tmpFBO);

				TEST_GL_ERROR(Driver);

				void *src = tmpImage->getData();
				void *dest = LockImage->getData();

				// FIXME: what about ETLF_FLIP_Y_UP_RTT

				switch (ColorFormat) {
				case ECF_A1R5G5B5:
					CColorConverter::convert_A8R8G8B8toA1B5G5R5(src, tmpImage->getDimension().getArea(), dest);
					break;
				case ECF_R5G6B5:
					CColorConverter::convert_A8R8G8B8toR5G6B5(src, tmpImage->getDimension().getArea(), dest);
					break;
				case ECF_R8G8B8:
					CColorConverter::convert_A8R8G8B8toB8G8R8(src, tmpImage->getDimension().getArea(), dest);
					break;
				case ECF_A8R8G8B8:
					CColorConverter::convert_A8R8G8B8toA8B8G8R8(src, tmpImage->getDimension().getArea(), dest);
					break;
				default:
					passed = false;
					break;
				}
				tmpImage->drop();

				}

				if (!passed) {
					LockImage->drop();
					LockImage = 0;
				}
			}
		}

		return (LockImage) ? LockImage->getData() : 0;
	}

	void unlock() override
	{
		if (!LockImage)
			return;

		if (!LockReadOnly) {
			const COpenGLCoreTexture *prevTexture = Driver->getCacheHandler()->getTextureCache().get(0);
			Driver->getCacheHandler()->getTextureCache().set(0, this);

			uploadTexture(false, LockLayer, MipLevelStored, LockImage->getData());

			Driver->getCacheHandler()->getTextureCache().set(0, prevTexture);
		}

		LockImage->drop();

		LockReadOnly = false;
		LockImage = 0;
		LockLayer = 0;
	}

	void regenerateMipMapLevels(u32 layer = 0) override
	{
		if (!HasMipMaps || (Size.Width <= 1 && Size.Height <= 1))
			return;

		const COpenGLCoreTexture *prevTexture = Driver->getCacheHandler()->getTextureCache().get(0);
		Driver->getCacheHandler()->getTextureCache().set(0, this);

		Driver->irrGlGenerateMipmap(TextureType);
		TEST_GL_ERROR(Driver);

		Driver->getCacheHandler()->getTextureCache().set(0, prevTexture);
	}

	GLenum getOpenGLTextureType() const
	{
		return TextureType;
	}

	GLuint getOpenGLTextureName() const
	{
		return TextureName;
	}

	SStatesCache &getStatesCache() const
	{
		return StatesCache;
	}

protected:
	ECOLOR_FORMAT getBestColorFormat(ECOLOR_FORMAT format)
	{
		// We only try for to adapt "simple" formats
		ECOLOR_FORMAT destFormat = (format <= ECF_A8R8G8B8) ? ECF_A8R8G8B8 : format;

		switch (format) {
		case ECF_A1R5G5B5:
			if (!Driver->getTextureCreationFlag(ETCF_ALWAYS_32_BIT))
				destFormat = ECF_A1R5G5B5;
			break;
		case ECF_R5G6B5:
			if (!Driver->getTextureCreationFlag(ETCF_ALWAYS_32_BIT))
				destFormat = ECF_R5G6B5;
			break;
		case ECF_A8R8G8B8:
			if (Driver->getTextureCreationFlag(ETCF_ALWAYS_16_BIT) ||
					Driver->getTextureCreationFlag(ETCF_OPTIMIZED_FOR_SPEED))
				destFormat = ECF_A1R5G5B5;
			break;
		case ECF_R8G8B8:
			// Note: Using ECF_A8R8G8B8 even when ETCF_ALWAYS_32_BIT is not set as 24 bit textures fail with too many cards
			if (Driver->getTextureCreationFlag(ETCF_ALWAYS_16_BIT) || Driver->getTextureCreationFlag(ETCF_OPTIMIZED_FOR_SPEED))
				destFormat = ECF_A1R5G5B5;
		default:
			break;
		}

		if (Driver->getTextureCreationFlag(ETCF_NO_ALPHA_CHANNEL)) {
			switch (destFormat) {
			case ECF_A1R5G5B5:
				destFormat = ECF_R5G6B5;
				break;
			case ECF_A8R8G8B8:
				destFormat = ECF_R8G8B8;
				break;
			default:
				break;
			}
		}

		return destFormat;
	}

	void getImageValues(const IImage *image)
	{
		OriginalColorFormat = image->getColorFormat();
		ColorFormat = getBestColorFormat(OriginalColorFormat);

		if (!Driver->getColorFormatParameters(ColorFormat, InternalFormat, PixelFormat, PixelType, &Converter)) {
			os::Printer::log("getImageValues: Color format is not supported", ColorFormatNames[ColorFormat < ECF_UNKNOWN ? ColorFormat : ECF_UNKNOWN], ELL_ERROR);
			InternalFormat = 0;
			return;
		}

		if (IImage::isCompressedFormat(image->getColorFormat())) {
			KeepImage = false;
		}

		OriginalSize = image->getDimension();
		Size = OriginalSize;

		if (Size.Width == 0 || Size.Height == 0) {
			os::Printer::log("Invalid size of image for texture.", ELL_ERROR);
			return;
		}

		const f32 ratio = (f32)Size.Width / (f32)Size.Height;

		if ((Size.Width > Driver->MaxTextureSize) && (ratio >= 1.f)) {
			Size.Width = Driver->MaxTextureSize;
			Size.Height = (u32)(Driver->MaxTextureSize / ratio);
		} else if (Size.Height > Driver->MaxTextureSize) {
			Size.Height = Driver->MaxTextureSize;
			Size.Width = (u32)(Driver->MaxTextureSize * ratio);
		}

		bool needSquare = (!Driver->queryFeature(EVDF_TEXTURE_NSQUARE) || Type == ETT_CUBEMAP);

		Size = Size.getOptimalSize(!Driver->queryFeature(EVDF_TEXTURE_NPOT), needSquare, true, Driver->MaxTextureSize);

		Pitch = Size.Width * IImage::getBitsPerPixelFromFormat(ColorFormat) / 8;
	}

	void uploadTexture(bool initTexture, u32 layer, u32 level, void *data)
	{
		if (!data)
			return;

		u32 width = Size.Width >> level;
		u32 height = Size.Height >> level;
		if (width < 1)
			width = 1;
		if (height < 1)
			height = 1;

		GLenum tmpTextureType = getTextureTarget(layer);

		if (!IImage::isCompressedFormat(ColorFormat)) {
			CImage *tmpImage = 0;
			void *tmpData = data;

			if (Converter) {
				const core::dimension2d<u32> tmpImageSize(width, height);

				tmpImage = new CImage(ColorFormat, tmpImageSize);
				tmpData = tmpImage->getData();

				Converter(data, tmpImageSize.getArea(), tmpData);
			}

			switch (TextureType) {
			case GL_TEXTURE_2D:
			case GL_TEXTURE_CUBE_MAP:
				if (initTexture)
					GL.TexImage2D(tmpTextureType, level, InternalFormat, width, height, 0, PixelFormat, PixelType, tmpData);
				else
					GL.TexSubImage2D(tmpTextureType, level, 0, 0, width, height, PixelFormat, PixelType, tmpData);
				TEST_GL_ERROR(Driver);
				break;
			default:
				_IRR_DEBUG_BREAK_IF(1)
				break;
			}

			delete tmpImage;
		} else {
			u32 dataSize = IImage::getDataSizeFromFormat(ColorFormat, width, height);

			switch (TextureType) {
			case GL_TEXTURE_2D:
			case GL_TEXTURE_CUBE_MAP:
				if (initTexture)
					Driver->irrGlCompressedTexImage2D(tmpTextureType, level, InternalFormat, width, height, 0, dataSize, data);
				else
					Driver->irrGlCompressedTexSubImage2D(tmpTextureType, level, 0, 0, width, height, PixelFormat, dataSize, data);
				TEST_GL_ERROR(Driver);
				break;
			default:
				_IRR_DEBUG_BREAK_IF(1)
				break;
			}
		}
	}

	GLenum TextureTypeIrrToGL(E_TEXTURE_TYPE type) const
	{
		switch (type) {
		case ETT_2D:
			return GL_TEXTURE_2D;
		case ETT_2D_MS:
			return GL_TEXTURE_2D_MULTISAMPLE;
		case ETT_CUBEMAP:
			return GL_TEXTURE_CUBE_MAP;
		}

		os::Printer::log("COpenGLCoreTexture::TextureTypeIrrToGL unknown texture type", ELL_WARNING);
		return GL_TEXTURE_2D;
	}

	GLenum getTextureTarget(u32 layer) const
	{
		GLenum tmp = TextureType;
		if (tmp == GL_TEXTURE_CUBE_MAP) {
			_IRR_DEBUG_BREAK_IF(layer > 5)
			tmp = GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer;
		}
		return tmp;
	}

	TOpenGLDriver *Driver;

	GLenum TextureType;
	GLuint TextureName;
	GLint InternalFormat;
	GLenum PixelFormat;
	GLenum PixelType;
	u8 MSAA;
	void (*Converter)(const void *, s32, void *);

	bool LockReadOnly;
	IImage *LockImage;
	u32 LockLayer;

	bool KeepImage;
	std::vector<IImage*> Images;

	u8 MipLevelStored;

	mutable SStatesCache StatesCache;
};

}
}
