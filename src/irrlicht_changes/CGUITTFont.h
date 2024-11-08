/*
   CGUITTFont FreeType class for Irrlicht
   Copyright (c) 2009-2010 John Norman
   Copyright (c) 2016 Nathanaëlle Courant
   Copyright (c) 2023 Caleb Butler

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this class can be located at:
   http://irrlicht.suckerfreegames.com/

   John Norman
   john@suckerfreegames.com
*/

#pragma once

#include <ft2build.h>
#include <map>
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "ISceneManager.h"
#include "IVideoDriver.h"
#include "IrrlichtDevice.h"
#include "SMesh.h"
#include "util/enriched_string.h"
#include "util/basic_macros.h"
#include FT_FREETYPE_H

namespace irr
{
namespace gui
{
	struct SGUITTFace;
	class CGUITTFont;

	//! Structure representing a single TrueType glyph.
	struct SGUITTGlyph
	{
		//! Constructor.
		SGUITTGlyph() :
			isLoaded(false),
			glyph_page(0),
			source_rect(),
			offset(),
			advance(),
			surface(0),
			parent(0)
		{}

		DISABLE_CLASS_COPY(SGUITTGlyph);

		//! This class would be trivially copyable except for the reference count on `surface`.
		SGUITTGlyph(SGUITTGlyph &&other) noexcept :
			isLoaded(other.isLoaded),
			glyph_page(other.glyph_page),
			source_rect(other.source_rect),
			offset(other.offset),
			advance(other.advance),
			surface(other.surface),
			parent(other.parent)
		{
			other.surface = 0;
		}

		//! Destructor.
		~SGUITTGlyph() { unload(); }

		//! Preload the glyph.
		//!	The preload process occurs when the program tries to cache the glyph from FT_Library.
		//! However, it simply defines the SGUITTGlyph's properties and will only create the page
		//! textures if necessary.  The actual creation of the textures should only occur right
		//! before the batch draw call.
		void preload(u32 char_index, FT_Face face, video::IVideoDriver* driver, u32 font_size, const FT_Int32 loadFlags);

		//! Unloads the glyph.
		void unload();

		//! Creates the IImage object from the FT_Bitmap.
		video::IImage* createGlyphImage(const FT_Bitmap& bits, video::IVideoDriver* driver) const;

		//! If true, the glyph has been loaded.
		bool isLoaded;

		//! The page the glyph is on.
		u32 glyph_page;

		//! The source rectangle for the glyph.
		core::recti source_rect;

		//! The offset of glyph when drawn.
		core::vector2di offset;

		//! Glyph advance information.
		FT_Vector advance;

		//! This is just the temporary image holder.  After this glyph is paged,
		//! it will be dropped.
		mutable video::IImage* surface;

		//! The pointer pointing to the parent (CGUITTFont)
		CGUITTFont* parent;
	};

	//! Holds a sheet of glyphs.
	class CGUITTGlyphPage
	{
		public:
			CGUITTGlyphPage(video::IVideoDriver* Driver, const io::path& texture_name) :texture(0), available_slots(0), used_slots(0), dirty(false), driver(Driver), name(texture_name) {}
			~CGUITTGlyphPage()
			{
				if (texture)
				{
					if (driver)
						driver->removeTexture(texture);
					else texture->drop();
				}
			}

			//! Create the actual page texture,
			bool createPageTexture(const u8& pixel_mode, const core::dimension2du& texture_size)
			{
				if( texture )
					return false;

				bool flgmip = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
				driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
				bool flgcpy = driver->getTextureCreationFlag(video::ETCF_ALLOW_MEMORY_COPY);
				driver->setTextureCreationFlag(video::ETCF_ALLOW_MEMORY_COPY, true);

				// Set the texture color format.
				switch (pixel_mode)
				{
					case FT_PIXEL_MODE_MONO:
						texture = driver->addTexture(texture_size, name, video::ECF_A1R5G5B5);
						break;
					case FT_PIXEL_MODE_GRAY:
					default:
						texture = driver->addTexture(texture_size, name, video::ECF_A8R8G8B8);
						break;
				}

				// Restore our texture creation flags.
				driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, flgmip);
				driver->setTextureCreationFlag(video::ETCF_ALLOW_MEMORY_COPY, flgcpy);

				return texture ? true : false;
			}

			//! Add the glyph to a list of glyphs to be paged.
			//! This collection will be cleared after updateTexture is called.
			void pushGlyphToBePaged(const SGUITTGlyph* glyph)
			{
				glyph_to_be_paged.push_back(glyph);
			}

			//! Updates the texture atlas with new glyphs.
			void updateTexture()
			{
				if (!dirty) return;

				void* ptr = texture->lock();
				video::ECOLOR_FORMAT format = texture->getColorFormat();
				core::dimension2du size = texture->getOriginalSize();
				video::IImage* pageholder = driver->createImageFromData(format, size, ptr, true, false);

				for (u32 i = 0; i < glyph_to_be_paged.size(); ++i)
				{
					const SGUITTGlyph* glyph = glyph_to_be_paged[i];
					if (glyph && glyph->isLoaded)
					{
						if (glyph->surface)
						{
							glyph->surface->copyTo(pageholder, glyph->source_rect.UpperLeftCorner);
							glyph->surface->drop();
							glyph->surface = 0;
						}
						else
						{
							; // TODO: add error message?
							//currently, if we failed to create the image, just ignore this operation.
						}
					}
				}

				pageholder->drop();
				texture->unlock();
				glyph_to_be_paged.clear();
				dirty = false;
			}

			video::ITexture* texture;
			u32 available_slots;
			u32 used_slots;
			bool dirty;

			core::array<core::vector2di> render_positions;
			core::array<core::recti> render_source_rects;
			core::array<video::SColor> render_colors;

		private:
			core::array<const SGUITTGlyph*> glyph_to_be_paged;
			video::IVideoDriver* driver;
			io::path name;
	};

	//! Class representing a TrueType font.
	class CGUITTFont : public IGUIFont
	{
		public:
			//! Creates a new TrueType font and returns a pointer to it.  The pointer must be drop()'ed when finished.
			//! \param env The IGUIEnvironment the font loads out of.
			//! \param filename The filename of the font.
			//! \param size The size of the font glyphs in pixels.  Since this is the size of the individual glyphs, the true height of the font may change depending on the characters used.
			//! \param antialias set the use_monochrome (opposite to antialias) flag
			//! \param transparency set the use_transparency flag
			//! \return Returns a pointer to a CGUITTFont.  Will return 0 if the font failed to load.
			static CGUITTFont* createTTFont(IGUIEnvironment *env, const io::path& filename, const u32 size, const bool antialias = true, const bool transparency = true, const u32 shadow = 0, const u32 shadow_alpha = 255);

			//! Destructor
			virtual ~CGUITTFont();

			//! Sets the amount of glyphs to batch load.
			virtual void setBatchLoadSize(u32 batch_size) { batch_load_size = batch_size; }

			//! Sets the maximum texture size for a page of glyphs.
			virtual void setMaxPageTextureSize(const core::dimension2du& texture_size) { max_page_texture_size = texture_size; }

			//! Get the font size.
			virtual u32 getFontSize() const { return size; }

			//! Check the font's transparency.
			virtual bool isTransparent() const { return use_transparency; }

			//! Check if the font auto-hinting is enabled.
			//! Auto-hinting is FreeType's built-in font hinting engine.
			virtual bool useAutoHinting() const { return use_auto_hinting; }

			//! Check if the font hinting is enabled.
			virtual bool useHinting()	 const { return use_hinting; }

			//! Check if the font is being loaded as a monochrome font.
			//! The font can either be a 256 color grayscale font, or a 2 color monochrome font.
			virtual bool useMonochrome()  const { return use_monochrome; }

			//! Tells the font to allow transparency when rendering.
			//! Default: true.
			//! \param flag If true, the font draws using transparency.
			virtual void setTransparency(const bool flag);

			//! Tells the font to use monochrome rendering.
			//! Default: false.
			//! \param flag If true, the font draws using a monochrome image.  If false, the font uses a grayscale image.
			virtual void setMonochrome(const bool flag);

			//! Enables or disables font hinting.
			//! Default: Hinting and auto-hinting true.
			//! \param enable If false, font hinting is turned off. If true, font hinting is turned on.
			//! \param enable_auto_hinting If true, FreeType uses its own auto-hinting algorithm.  If false, it tries to use the algorithm specified by the font.
			virtual void setFontHinting(const bool enable, const bool enable_auto_hinting = true);

			//! Draws some text and clips it to the specified rectangle if wanted.
			virtual void draw(const core::stringw& text, const core::rect<s32>& position,
				video::SColor color, bool hcenter=false, bool vcenter=false,
				const core::rect<s32>* clip=0);

			void draw(const EnrichedString& text, const core::rect<s32>& position,
				bool hcenter=false, bool vcenter=false,
				const core::rect<s32>* clip=0);

			//! Returns the dimension of a character produced by this font.
			virtual core::dimension2d<u32> getCharDimension(const wchar_t ch) const;

			//! Returns the dimension of a text string.
			virtual core::dimension2d<u32> getDimension(const wchar_t* text) const;

			//! Calculates the index of the character in the text which is on a specific position.
			virtual s32 getCharacterFromPos(const wchar_t* text, s32 pixel_x) const;

			//! Sets global kerning width for the font.
			virtual void setKerningWidth(s32 kerning);

			//! Sets global kerning height for the font.
			virtual void setKerningHeight(s32 kerning);

			//! Gets kerning values (distance between letters) for the font. If no parameters are provided,
			virtual s32 getKerningWidth(const wchar_t* thisLetter=0, const wchar_t* previousLetter=0) const;
			virtual s32 getKerningWidth(const char32_t thisLetter=0, const char32_t previousLetter=0) const;

			//! Returns the distance between letters
			virtual s32 getKerningHeight() const;

			//! Define which characters should not be drawn by the font.
			virtual void setInvisibleCharacters(const wchar_t *s);

			//! Get the last glyph page if there's still available slots.
			//! If not, it will return zero.
			CGUITTGlyphPage* getLastGlyphPage() const;

			//! Create a new glyph page texture.
			//! \param pixel_mode the pixel mode defined by FT_Pixel_Mode
			//should be better typed. fix later.
			CGUITTGlyphPage* createGlyphPage(const u8& pixel_mode);

			//! Get the last glyph page's index.
			u32 getLastGlyphPageIndex() const { return Glyph_Pages.size() - 1; }

			//! Set font that should be used for glyphs not present in ours
			void setFallback(gui::IGUIFont* font) { fallback = font; }

			//! Create corresponding character's software image copy from the font,
			//! so you can use this data just like any ordinary video::IImage.
			//! \param ch The character you need
			virtual video::IImage* createTextureFromChar(const char32_t& ch);

			//! This function is for debugging mostly. If the page doesn't exist it returns zero.
			//! \param page_index Simply return the texture handle of a given page index.
			virtual video::ITexture* getPageTextureByIndex(const u32& page_index) const;

			//! Add a list of scene nodes generated by putting font textures on the 3D planes.
			virtual core::array<scene::ISceneNode*> addTextSceneNode
				(const wchar_t* text, scene::ISceneManager* smgr, scene::ISceneNode* parent = 0,
				 const video::SColor& color = video::SColor(255, 0, 0, 0), bool center = false );

			inline s32 getAscender() const { return font_metrics.ascender; }

		protected:
			bool use_monochrome;
			bool use_transparency;
			bool use_hinting;
			bool use_auto_hinting;
			u32 size;
			u32 batch_load_size;
			core::dimension2du max_page_texture_size;

		private:
			// Manages the FreeType library.
			static FT_Library c_library;
			static std::map<io::path, SGUITTFace*> c_faces;
			static bool c_libraryLoaded;
			static scene::IMesh* shared_plane_ptr_;
			static scene::SMesh  shared_plane_;

			// Helper functions for the same-named public member functions above
			// (Since std::u32string is nicer to work with than wchar_t *)
			core::dimension2d<u32> getDimension(const std::u32string& text) const;
			s32 getCharacterFromPos(const std::u32string& text, s32 pixel_x) const;

			// Helper function for the above helper functions :P
			std::u32string convertWCharToU32String(const wchar_t* const) const;

			CGUITTFont(IGUIEnvironment *env);
			bool load(const io::path& filename, const u32 size, const bool antialias, const bool transparency);
			void reset_images();
			void update_glyph_pages() const;
			void update_load_flags()
			{
				// Set up our loading flags.
				load_flags = FT_LOAD_DEFAULT | FT_LOAD_RENDER;
				if (!useHinting()) load_flags |= FT_LOAD_NO_HINTING;
				if (!useAutoHinting()) load_flags |= FT_LOAD_NO_AUTOHINT;
				if (useMonochrome()) load_flags |= FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO;
				else load_flags |= FT_LOAD_TARGET_NORMAL;
			}
			u32 getWidthFromCharacter(wchar_t c) const;
			u32 getWidthFromCharacter(char32_t c) const;
			u32 getHeightFromCharacter(wchar_t c) const;
			u32 getHeightFromCharacter(char32_t c) const;
			u32 getGlyphIndexByChar(wchar_t c) const;
			u32 getGlyphIndexByChar(char32_t c) const;
			core::vector2di getKerning(const wchar_t thisLetter, const wchar_t previousLetter) const;
			core::vector2di getKerning(const char32_t thisLetter, const char32_t previousLetter) const;
			core::dimension2d<u32> getDimensionUntilEndOfLine(const wchar_t* p) const;

			void createSharedPlane();

			irr::IrrlichtDevice* Device;
			gui::IGUIEnvironment* Environment;
			video::IVideoDriver* Driver;
			io::path filename;
			FT_Face tt_face;
			FT_Size_Metrics font_metrics;
			FT_Int32 load_flags;

			mutable core::array<CGUITTGlyphPage*> Glyph_Pages;
			mutable core::array<SGUITTGlyph> Glyphs;

			s32 GlobalKerningWidth;
			s32 GlobalKerningHeight;
			std::u32string Invisible;
			u32 shadow_offset;
			u32 shadow_alpha;

			gui::IGUIFont* fallback;
	};

} // end namespace gui
} // end namespace irr
