/*
   CGUITTFont FreeType class for Irrlicht
   Copyright (c) 2009-2010 John Norman
   Copyright (c) 2016 Nathanaëlle Courant
   Copyright (c) 2023 Caleb Butler
   Copyright (c) 2024 Luanti contributors

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
#include <freetype/freetype.h>

#include "irr_ptr.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "IVideoDriver.h"
#include "util/enriched_string.h"
#include "util/basic_macros.h"

#include <map>

namespace gui
{
	class CGUITTFont;

	// Manages the FT_Face cache.
	struct SGUITTFace : public IReferenceCounted
	{
	private:

		static FT_Library freetype_library;
		static size_t n_faces;

		static FT_Library getFreeTypeLibrary();

		// This holds the font file data for this face.
		// Must not be deallocated until we are done with the face!
		std::string face_buffer;

	public:
		SGUITTFace(std::string &&buffer);
		~SGUITTFace();

		FT_Face face;

		static SGUITTFace* createFace(std::string &&buffer);

		static SGUITTFace* loadFace(const io::path &filename);
	};

	//! Structure representing a single TrueType glyph.
	struct SGUITTGlyph
	{
		SGUITTGlyph() :
			glyph_page(0),
			source_rect(),
			offset(),
			advance()
		{}
		~SGUITTGlyph() { unload(); }

		//! If true, the glyph has been loaded.
		inline bool isLoaded() const {
			return source_rect != core::recti();
		}

		//! Preload the glyph.
		void preload(u32 char_index, FT_Face face, CGUITTFont *parent, u32 font_size, const FT_Int32 loadFlags);

		//! Unloads the glyph.
		void unload();

		//! Creates the IImage object from the FT_Bitmap.
		video::IImage* createGlyphImage(const FT_Bitmap &bits, video::IVideoDriver *driver) const;

		//! The page the glyph is on.
		u32 glyph_page;

		//! The source rectangle for the glyph.
		core::recti source_rect;

		//! The offset of glyph when drawn.
		core::vector2di offset;

		//! Glyph advance information.
		core::vector2di advance;
	};

	//! Wrapper struct for a preloaded glyph
	struct SGUITTGlyphPending {
		SGUITTGlyphPending(const SGUITTGlyph *glyph, video::IImage *surface) noexcept :
			glyph(glyph), surface(surface)
		{}
		~SGUITTGlyphPending() {
			if (surface)
				surface->drop();
		}

		DISABLE_CLASS_COPY(SGUITTGlyphPending)

		SGUITTGlyphPending(SGUITTGlyphPending &&other) noexcept :
			glyph(other.glyph), surface(other.surface)
		{
			other.surface = nullptr;
		}

		const SGUITTGlyph *glyph;
		video::IImage *surface;
	};

	//! Holds a sheet of glyphs.
	class CGUITTGlyphPage
	{
		public:
			CGUITTGlyphPage(video::IVideoDriver *Driver, const io::path &texture_name) :
				texture(0), available_slots(0), used_slots(0),
				driver(Driver), name(texture_name)
			{}
			~CGUITTGlyphPage()
			{
				if (texture)
					driver->removeTexture(texture);
			}

			//! Create the actual page texture,
			bool createPageTexture(u8 pixel_mode, core::dimension2du texture_size);

			//! Add the glyph to a list of glyphs to be paged.
			//! This collection will be cleared after updateTexture is called.
			void pushGlyphToBePaged(const SGUITTGlyph *glyph, video::IImage *surface)
			{
				if (!glyph || !surface)
					return;
				glyph_to_be_paged.emplace_back(glyph, surface);
			}

			inline bool isDirty() const
			{
				return !glyph_to_be_paged.empty();
			}

			//! Updates the texture atlas with new glyphs.
			void updateTexture();

			video::ITexture* texture;
			u32 available_slots;
			u32 used_slots;

			std::vector<core::vector2di> render_positions;
			std::vector<core::recti> render_source_rects;
			std::vector<video::SColor> render_colors;

		private:
			std::vector<SGUITTGlyphPending> glyph_to_be_paged;
			video::IVideoDriver* driver;
			io::path name;
	};

	//! Class representing a TrueType font.
	class CGUITTFont final : public IGUIFont
	{
		public:
			//! Creates a new TrueType font and returns a pointer to it.  The pointer must be drop()'ed when finished.
			//! \param env The IGUIEnvironment the font loads out of.
			//! \param size The size of the font glyphs in pixels.  Since this is the size of the individual glyphs, the true height of the font may change depending on the characters used.
			//! \param antialias set the use_monochrome (opposite to antialias) flag
			//! \param preload create texture with important glyphs directly
			//! \return Returns a pointer to a CGUITTFont.  Will return 0 if the font failed to load.
			static CGUITTFont* createTTFont(IGUIEnvironment *env,
				SGUITTFace *face, u32 size, bool antialias = true,
				bool preload = true, u32 shadow = 0, u32 shadow_alpha = 255);

			//! Destructor
			virtual ~CGUITTFont();

			//! Sets the amount of glyphs to batch load.
			void setBatchLoadSize(u32 batch_size) { batch_load_size = batch_size; }

			//! Get the font size.
			u32 getFontSize() const { return size; }

			//! Check if the font auto-hinting is enabled.
			//! Auto-hinting is FreeType's built-in font hinting engine.
			bool useAutoHinting() const { return use_auto_hinting; }

			//! Check if the font hinting is enabled.
			bool useHinting()	 const { return use_hinting; }

			//! Check if the font is being loaded as a monochrome font.
			//! The font can either be a 256 color grayscale font, or a 2 color monochrome font.
			bool useMonochrome()  const { return use_monochrome; }

			//! Tells the font to use monochrome rendering.
			//! Default: false.
			//! \param flag If true, the font draws using a monochrome image.  If false, the font uses a grayscale image.
			void setMonochrome(const bool flag);

			//! Enables or disables font hinting.
			//! Default: Hinting and auto-hinting true.
			//! \param enable If false, font hinting is turned off. If true, font hinting is turned on.
			//! \param enable_auto_hinting If true, FreeType uses its own auto-hinting algorithm.  If false, it tries to use the algorithm specified by the font.
			void setFontHinting(const bool enable, const bool enable_auto_hinting = true);

			//! Draws some text and clips it to the specified rectangle if wanted.
			virtual void draw(const core::stringw& utext, const core::rect<s32>& position,
				video::SColor color, bool hcenter=false, bool vcenter=false,
				const core::rect<s32>* clip=0) override;

			void draw(const EnrichedString& text, const core::rect<s32>& position,
				bool hcenter=false, bool vcenter=false,
				const core::rect<s32>* clip=0);

			//! Returns the dimension of a text string.
			virtual core::dimension2du getDimension(const wchar_t* text) const override;

			//! Calculates the index of the character in the text which is on a specific position.
			virtual s32 getCharacterFromPos(const wchar_t* text, s32 pixel_x) const override;

			//! Sets global kerning width for the font.
			virtual void setKerningWidth(s32 kerning) override;

			//! Sets global kerning height for the font.
			virtual void setKerningHeight(s32 kerning) override;

			//! Returns the distance between letters
			virtual core::vector2di getKerning(const wchar_t thisLetter, const wchar_t previousLetter) const override;

			//! Define which characters should not be drawn by the font.
			virtual void setInvisibleCharacters(const wchar_t *s) override;

			//! Get the last glyph page if there's still available slots.
			//! If not, it will return zero.
			CGUITTGlyphPage* getLastGlyphPage() const;

			//! Create a new glyph page texture.
			//! \param pixel_mode the pixel mode defined by FT_Pixel_Mode
			//should be better typed. fix later.
			CGUITTGlyphPage* createGlyphPage(const u8 pixel_mode);

			//! Get the last glyph page's index.
			u32 getLastGlyphPageIndex() const { return Glyph_Pages.size() - 1; }

			//! Set font that should be used for glyphs not present in ours
			void setFallback(gui::IGUIFont *font);

			inline s32 getAscender() const { return font_metrics.ascender; }

		protected:
			bool use_monochrome;
			bool use_hinting;
			bool use_auto_hinting;
			u32 size;
			u32 batch_load_size;

		private:
			// Helper functions for the same-named public member functions above
			// (Since std::u32string is nicer to work with than wchar_t *)
			core::dimension2d<u32> getDimension(const std::u32string& text) const;
			s32 getCharacterFromPos(const std::u32string& text, s32 pixel_x) const;

			// Helper function for the above helper functions :P
			std::u32string convertWCharToU32String(const wchar_t* const) const;

			CGUITTFont(IGUIEnvironment *env);
			bool load(SGUITTFace *face, const u32 size, const bool antialias,
				const bool transparency, const bool preload);

			void reset_images();
			void update_glyph_pages() const;
			void update_load_flags()
			{
				// Set up our loading flags.
				load_flags = FT_LOAD_DEFAULT | FT_LOAD_RENDER;
				if (!useHinting())
					load_flags |= FT_LOAD_NO_HINTING;
				if (!useAutoHinting())
					load_flags |= FT_LOAD_NO_AUTOHINT;
				if (useMonochrome())
					load_flags |= FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO;
				else
					load_flags |= FT_LOAD_TARGET_NORMAL;
			}

			/// Gets the overall font height, including a line gap of 1 px
			inline u32 getLineHeight() const { return font_metrics.height / 64 + 1; }
			u32 getWidthFromCharacter(char32_t c) const;
			u32 getHeightFromCharacter(char32_t c) const;
			/// Returns (index + 1) of `this->Glyphs`
			/// Returns 0 if no such glyph is provided by the font.
			u32 getGlyphIndexByChar(char32_t c) const;
			core::vector2di getKerning(const char32_t thisLetter, const char32_t previousLetter) const;

			video::IVideoDriver* Driver = nullptr;
			FT_Face tt_face;
			FT_Size_Metrics font_metrics;
			FT_Int32 load_flags;

			mutable core::array<CGUITTGlyphPage*> Glyph_Pages;
			mutable core::array<SGUITTGlyph> Glyphs;

			s32 GlobalKerningWidth = 0;
			s32 GlobalKerningHeight = 0;
			std::u32string InvisibleChars;
			u32 shadow_offset = 0;
			u32 shadow_alpha = 0;

			irr_ptr<gui::IGUIFont> fallback;
	};

} // end namespace gui
