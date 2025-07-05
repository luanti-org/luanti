/*
   CGUITTFont FreeType class for Irrlicht
   Copyright (c) 2009-2010 John Norman
   with changes from Luanti contributors:
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

#include "CGUITTFont.h"

#include "irr_ptr.h"
#include "log.h"
#include "filesys.h"
#include "debug.h"
#include "IFileSystem.h"
#include "IGUIEnvironment.h"

#include <cstdlib>
#include <iostream>


namespace gui
{

std::map<io::path, SGUITTFace*> SGUITTFace::faces;
FT_Library SGUITTFace::freetype_library = nullptr;
std::size_t SGUITTFace::n_faces = 0;

FT_Library SGUITTFace::getFreeTypeLibrary()
{
	if (freetype_library)
		return freetype_library;
	FT_Library ft;
	if (FT_Init_FreeType(&ft))
		FATAL_ERROR("initializing freetype failed");
	freetype_library = ft;
	return freetype_library;
}

SGUITTFace::SGUITTFace(std::string &&buffer) : face_buffer(std::move(buffer))
{
	memset((void*)&face, 0, sizeof(FT_Face));
	n_faces++;
}

SGUITTFace::~SGUITTFace()
{
	FT_Done_Face(face);
	n_faces--;
	// If there are no more faces referenced by FreeType, clean up.
	if (n_faces == 0) {
		assert(freetype_library);
		FT_Done_FreeType(freetype_library);
		freetype_library = nullptr;
	}
}

SGUITTFace* SGUITTFace::createFace(std::string &&buffer)
{
	irr_ptr<SGUITTFace> face(new SGUITTFace(std::move(buffer)));
	auto ft = getFreeTypeLibrary();
	if (!ft)
		return nullptr;
	return (FT_New_Memory_Face(ft,
			reinterpret_cast<const FT_Byte*>(face->face_buffer.data()),
			face->face_buffer.size(), 0, &face->face))
			? nullptr : face.release();
}

SGUITTFace* SGUITTFace::loadFace(const io::path &filename)
{
	auto it = faces.find(filename);
	if (it != faces.end()) {
		it->second->grab();
		return it->second;
	}

	std::string buffer;
	if (!fs::ReadFile(filename.c_str(), buffer, true)) {
		errorstream << "CGUITTFont: Reading file " << filename.c_str() << " failed." << std::endl;
		return nullptr;
	}

	auto *face = SGUITTFace::createFace(std::move(buffer));
	if (!face) {
		errorstream << "CGUITTFont: FT_New_Memory_Face failed." << std::endl;
		return nullptr;
	}
	faces.emplace(filename, face);
	return face;
}

void SGUITTFace::dropFilename()
{
	if (!filename.has_value())
		return;

	auto it = faces.find(*filename);
	if (it == faces.end())
		return;

	SGUITTFace* f = it->second;
	// Drop our face.  If this was the last face, the destructor will clean up.
	if (f->drop())
		faces.erase(*filename);
}

video::IImage* SGUITTGlyph::createGlyphImage(const FT_Bitmap& bits, video::IVideoDriver* driver) const
{
	// Make sure our casts to s32 in the loops below will not cause problems
	if ((s32)bits.rows < 0 || (s32)bits.width < 0)
		FATAL_ERROR("Insane font glyph size");

	// Determine what our texture size should be.
	// Add 1 because textures are inclusive-exclusive.
	core::dimension2du d(bits.width + 1, bits.rows + 1);
	core::dimension2du texture_size;

	// Create and load our image now.
	video::IImage* image = 0;
	switch (bits.pixel_mode)
	{
		case FT_PIXEL_MODE_MONO:
		{
			// Create a blank image and fill it with transparent pixels.
			texture_size = d.getOptimalSize(true, true);
			image = driver->createImage(video::ECF_A1R5G5B5, texture_size);
			image->fill(video::SColor(0, 255, 255, 255));

			// Load the monochrome data in.
			const u32 image_pitch = image->getPitch() / sizeof(u16);
			u16* image_data = (u16*)image->getData();
			u8* glyph_data = bits.buffer;

			for (s32 y = 0; y < (s32)bits.rows; ++y)
			{
				u16* row = image_data;
				for (s32 x = 0; x < (s32)bits.width; ++x)
				{
					// Monochrome bitmaps store 8 pixels per byte.  The left-most pixel is the bit 0x80.
					// So, we go through the data each bit at a time.
					if ((glyph_data[y * bits.pitch + (x / 8)] & (0x80 >> (x % 8))) != 0)
						*row = 0xFFFF;
					++row;
				}
				image_data += image_pitch;
			}
			break;
		}

		case FT_PIXEL_MODE_GRAY:
		{
			// Create our blank image.
			texture_size = d.getOptimalSize(!driver->queryFeature(video::EVDF_TEXTURE_NPOT), !driver->queryFeature(video::EVDF_TEXTURE_NSQUARE), true, 0);
			image = driver->createImage(video::ECF_A8R8G8B8, texture_size);
			image->fill(video::SColor(0, 255, 255, 255));

			// Load the grayscale data in.
			const float gray_count = static_cast<float>(bits.num_grays);
			const u32 image_pitch = image->getPitch() / sizeof(u32);
			u32* image_data = (u32*)image->getData();
			u8* glyph_data = bits.buffer;
			for (s32 y = 0; y < (s32)bits.rows; ++y)
			{
				u8* row = glyph_data;
				for (s32 x = 0; x < (s32)bits.width; ++x)
				{
					image_data[y * image_pitch + x] |= static_cast<u32>(255.0f * (static_cast<float>(*row++) / gray_count)) << 24;
				}
				glyph_data += bits.pitch;
			}
			break;
		}
		default:
			errorstream << "CGUITTFont: unknown pixel mode " << (int)bits.pixel_mode << std::endl;
			return 0;
	}
	return image;
}

void SGUITTGlyph::preload(u32 char_index, FT_Face face, CGUITTFont *parent, u32 font_size, const FT_Int32 loadFlags)
{
	// Set the size of the glyph.
	FT_Set_Pixel_Sizes(face, 0, font_size);

	// Attempt to load the glyph.
	auto err = FT_Load_Glyph(face, char_index, loadFlags);
	if (err != FT_Err_Ok) {
		warningstream << "SGUITTGlyph: failed to load glyph " << char_index
			<< " with error: " << (int)err << std::endl;
		return;
	}

	FT_GlyphSlot glyph = face->glyph;
	const FT_Bitmap &bits = glyph->bitmap;

	// Setup the glyph information here:
	advance = core::vector2di(glyph->advance.x, glyph->advance.y);
	offset = core::vector2di(glyph->bitmap_left, glyph->bitmap_top);

	// Try to get the last page with available slots.
	CGUITTGlyphPage* page = parent->getLastGlyphPage();

	// If we need to make a new page, do that now.
	if (!page)
	{
		page = parent->createGlyphPage(bits.pixel_mode);
		if (!page)
			return;
	}

	glyph_page = parent->getLastGlyphPageIndex();
	u32 texture_side_length = page->texture->getOriginalSize().Width;
	core::vector2di page_position(
		(page->used_slots % (texture_side_length / font_size)) * font_size,
		(page->used_slots / (texture_side_length / font_size)) * font_size
		);
	source_rect.UpperLeftCorner = page_position;
	source_rect.LowerRightCorner = core::vector2di(page_position.X + bits.width, page_position.Y + bits.rows);

	page->dirty = true;
	++page->used_slots;
	--page->available_slots;

	// We grab the glyph bitmap here so the data won't be removed when the next glyph is loaded.
	surface = createGlyphImage(bits, parent->getDriver());
}

void SGUITTGlyph::unload()
{
	if (surface)
	{
		surface->drop();
		surface = 0;
	}
	// reset isLoaded to false
	source_rect = core::recti();
}

//////////////////////

CGUITTFont* CGUITTFont::createTTFont(IGUIEnvironment *env,
		SGUITTFace *face, u32 size, bool antialias,
		bool transparency, u32 shadow, u32 shadow_alpha)
{
	CGUITTFont* font = new CGUITTFont(env);
	bool ret = font->load(face, size, antialias, transparency);
	if (!ret)
	{
		font->drop();
		return 0;
	}

	font->shadow_offset = shadow;
	font->shadow_alpha = shadow_alpha;

	return font;
}

//////////////////////

//! Constructor.
CGUITTFont::CGUITTFont(IGUIEnvironment *env)
: use_monochrome(false), use_transparency(true), use_hinting(true), use_auto_hinting(true),
batch_load_size(1), Driver(0), GlobalKerningWidth(0), GlobalKerningHeight(0),
shadow_offset(0), shadow_alpha(0), fallback(0)
{

	if (env) {
		// don't grab environment, to avoid circular references
		Driver = env->getVideoDriver();
	}

	if (Driver)
		Driver->grab();

	setInvisibleCharacters(L" ");
}

bool CGUITTFont::load(SGUITTFace *face, const u32 size, const bool antialias, const bool transparency)
{
	if (!Driver || size == 0 || !face)
		return false;

	this->size = size;

	// Update the font loading flags when the font is first loaded.
	this->use_monochrome = !antialias;
	this->use_transparency = transparency;
	update_load_flags();

	// Store our face.
	face->grab();
	tt_face = face->face;

	// Store font metrics.
	FT_Set_Pixel_Sizes(tt_face, size, 0);
	font_metrics = tt_face->size->metrics;

	verbosestream << tt_face->num_glyphs << " glyphs, ascender=" << font_metrics.ascender
		<< " height=" << font_metrics.height << std::endl;

	// Allocate our glyphs.
	Glyphs.clear();
	Glyphs.set_used(tt_face->num_glyphs);

	// Cache the first 127 ascii characters.
	u32 old_size = batch_load_size;
	batch_load_size = 127;
	getGlyphIndexByChar((char32_t)0);
	batch_load_size = old_size;

	return true;
}

CGUITTFont::~CGUITTFont()
{
	// Delete the glyphs and glyph pages.
	reset_images();
	Glyphs.clear();

	// Drop our driver now.
	if (Driver)
		Driver->drop();
}

void CGUITTFont::reset_images()
{
	// Delete the glyphs.
	for (u32 i = 0; i != Glyphs.size(); ++i)
		Glyphs[i].unload();

	// Unload the glyph pages from video memory.
	for (u32 i = 0; i != Glyph_Pages.size(); ++i)
		delete Glyph_Pages[i];
	Glyph_Pages.clear();

	// Always update the internal FreeType loading flags after resetting.
	update_load_flags();
}

void CGUITTFont::update_glyph_pages() const
{
	for (u32 i = 0; i != Glyph_Pages.size(); ++i)
	{
		if (Glyph_Pages[i]->dirty)
			Glyph_Pages[i]->updateTexture();
	}
}

CGUITTGlyphPage* CGUITTFont::getLastGlyphPage() const
{
	CGUITTGlyphPage* page = 0;
	if (Glyph_Pages.empty())
		return 0;
	else
	{
		page = Glyph_Pages[getLastGlyphPageIndex()];
		if (page->available_slots == 0)
			page = 0;
	}
	return page;
}

CGUITTGlyphPage* CGUITTFont::createGlyphPage(const u8 pixel_mode)
{
	CGUITTGlyphPage* page = 0;

	// Name of our page.
	io::path name("TTFontGlyphPage_");
	name += tt_face->family_name;
	name += ".";
	name += tt_face->style_name;
	name += ".";
	name += size;
	name += "_";
	name += Glyph_Pages.size(); // The newly created page will be at the end of the collection.

	// Create the new page.
	page = new CGUITTGlyphPage(Driver, name);

	// Determine our maximum texture size.
	// If we keep getting 0, set it to 1024x1024, as that number is pretty safe.
	core::dimension2du max_texture_size = max_page_texture_size;
	if (max_texture_size.Width == 0 || max_texture_size.Height == 0)
		max_texture_size = Driver->getMaxTextureSize();
	if (max_texture_size.Width == 0 || max_texture_size.Height == 0)
		max_texture_size = core::dimension2du(1024, 1024);

	// We want to try to put at least 144 glyphs on a single texture.
	core::dimension2du page_texture_size;
	if (size <= 21) page_texture_size = core::dimension2du(256, 256);
	else if (size <= 42) page_texture_size = core::dimension2du(512, 512);
	else if (size <= 84) page_texture_size = core::dimension2du(1024, 1024);
	else if (size <= 168) page_texture_size = core::dimension2du(2048, 2048);
	else page_texture_size = core::dimension2du(4096, 4096);

	if (page_texture_size.Width > max_texture_size.Width || page_texture_size.Height > max_texture_size.Height)
		page_texture_size = max_texture_size;

	if (!page->createPageTexture(pixel_mode, page_texture_size)) {
		errorstream << "CGUITTGlyphPage: failed to create texture ("
			<< page_texture_size.Width << "x" << page_texture_size.Height << ")" << std::endl;
		delete page;
		return 0;
	}

	if (page)
	{
		// Determine the number of glyph slots on the page and add it to the list of pages.
		page->available_slots = (page_texture_size.Width / size) * (page_texture_size.Height / size);
		Glyph_Pages.push_back(page);
	}
	return page;
}

void CGUITTFont::setTransparency(const bool flag)
{
	use_transparency = flag;
	reset_images();
}

void CGUITTFont::setMonochrome(const bool flag)
{
	use_monochrome = flag;
	reset_images();
}

void CGUITTFont::setFontHinting(const bool enable, const bool enable_auto_hinting)
{
	use_hinting = enable;
	use_auto_hinting = enable_auto_hinting;
	reset_images();
}

void CGUITTFont::draw(const core::stringw& text, const core::rect<s32>& position, video::SColor color, bool hcenter, bool vcenter, const core::rect<s32>* clip)
{
	// Allow colors to work for strings that have passed through irrlicht by catching
	// them here and converting them to enriched just before drawing.
	EnrichedString s(text.c_str(), color);
	draw(s, position, hcenter, vcenter, clip);
}

void CGUITTFont::draw(const EnrichedString &text, const core::rect<s32>& position, bool hcenter, bool vcenter, const core::rect<s32>* clip)
{
	const auto &colors = text.getColors();

	if (!Driver)
		return;

	// Clear the glyph pages of their render information.
	for (u32 i = 0; i < Glyph_Pages.size(); ++i)
	{
		Glyph_Pages[i]->render_positions.clear();
		Glyph_Pages[i]->render_source_rects.clear();
		Glyph_Pages[i]->render_colors.clear();
	}

	// Set up some variables.
	core::dimension2d<s32> textDimension;
	core::position2d<s32> offset = position.UpperLeftCorner;

	// Determine offset positions.
	if (hcenter || vcenter)
	{
		textDimension = getDimension(text.c_str());

		if (hcenter)
			offset.X = ((position.getWidth() - textDimension.Width) >> 1) + offset.X;

		if (vcenter)
			offset.Y = ((position.getHeight() - textDimension.Height) >> 1) + offset.Y;
	}

	// Convert to a unicode string.
	std::u32string utext = convertWCharToU32String(text.c_str());

	// Set up our render map.
	std::map<u32, CGUITTGlyphPage*> Render_Map;

	// Start parsing characters.
	u32 n;
	char32_t previousChar = 0;
	std::u32string::const_iterator iter = utext.begin();
	while (iter != utext.end())
	{
		char32_t currentChar = *iter;
		n = getGlyphIndexByChar(currentChar);
		bool visible = (Invisible.find_first_of(currentChar) == std::u32string::npos);
		bool lineBreak=false;
		if (currentChar == L'\r') // Mac or Windows breaks
		{
			lineBreak = true;
			if (*(iter + 1) == (char32_t)'\n') 	// Windows line breaks.
				currentChar = *(++iter);
		}
		else if (currentChar == (char32_t)'\n') // Unix breaks
		{
			lineBreak = true;
		}

		if (lineBreak)
		{
			previousChar = 0;
			offset.Y += font_metrics.height / 64;
			offset.X = position.UpperLeftCorner.X;

			if (hcenter)
				offset.X += (position.getWidth() - textDimension.Width) >> 1;
			++iter;
			continue;
		}

		if (n > 0 && visible)
		{
			// Calculate the glyph offset.
			s32 offx = Glyphs[n-1].offset.X;
			s32 offy = (font_metrics.ascender / 64) - Glyphs[n-1].offset.Y;

			// Apply kerning.
			core::vector2di k = getKerning(currentChar, previousChar);
			offset.X += k.X;
			offset.Y += k.Y;

			// Determine rendering information.
			SGUITTGlyph& glyph = Glyphs[n-1];
			CGUITTGlyphPage* const page = Glyph_Pages[glyph.glyph_page];
			page->render_positions.push_back(core::position2di(offset.X + offx, offset.Y + offy));
			page->render_source_rects.push_back(glyph.source_rect);
			const size_t iterPos = iter - utext.begin();
			if (iterPos < colors.size())
				page->render_colors.push_back(colors[iterPos]);
			else
				page->render_colors.push_back(video::SColor(255,255,255,255));
			Render_Map[glyph.glyph_page] = page;
		}
		if (n > 0)
		{
			offset.X += getWidthFromCharacter(currentChar);
		}
		else if (fallback != 0)
		{
			// Let the fallback font draw it, this isn't super efficient but hopefully that doesn't matter
			wchar_t l1[] = { (wchar_t) currentChar, 0 };

			if (visible)
			{
				// Apply kerning.
				offset += fallback->getKerning(*l1, (wchar_t) previousChar);

				const u32 current_color = iter - utext.begin();
				fallback->draw(core::stringw(l1),
					core::rect<s32>({offset.X-1, offset.Y-1}, position.LowerRightCorner), // ???
					current_color < colors.size() ? colors[current_color] : video::SColor(255, 255, 255, 255),
					false, false, clip);
			}

			offset.X += fallback->getDimension(l1).Width;
		}

		previousChar = currentChar;
		++iter;
	}

	// Draw now.
	update_glyph_pages();
	auto it = Render_Map.begin();
	auto ie = Render_Map.end();
	core::array<core::vector2di> tmp_positions;
	core::array<core::recti> tmp_source_rects;
	while (it != ie)
	{
		CGUITTGlyphPage* page = it->second;
		++it;

		// render runs of matching color in batch
		size_t ibegin;
		video::SColor colprev;
		for (size_t i = 0; i < page->render_positions.size(); ++i) {
			ibegin = i;
			colprev = page->render_colors[i];
			do
				++i;
			while (i < page->render_positions.size() && page->render_colors[i] == colprev);
			tmp_positions.set_data(&page->render_positions[ibegin], i - ibegin);
			tmp_source_rects.set_data(&page->render_source_rects[ibegin], i - ibegin);
			--i;

			if (!use_transparency)
				colprev.color |= 0xff000000;

			if (shadow_offset) {
				for (size_t i = 0; i < tmp_positions.size(); ++i)
					tmp_positions[i] += core::vector2di(shadow_offset, shadow_offset);

				u32 new_shadow_alpha = core::clamp(core::round32(shadow_alpha * colprev.getAlpha() / 255.0f), 0, 255);
				video::SColor shadow_color = video::SColor(new_shadow_alpha, 0, 0, 0);
				Driver->draw2DImageBatch(page->texture, tmp_positions, tmp_source_rects, clip, shadow_color, true);

				for (size_t i = 0; i < tmp_positions.size(); ++i)
					tmp_positions[i] -= core::vector2di(shadow_offset, shadow_offset);
			}

			Driver->draw2DImageBatch(page->texture, tmp_positions, tmp_source_rects, clip, colprev, true);
		}
	}
}

core::dimension2d<u32> CGUITTFont::getDimension(const wchar_t* text) const
{
	return getDimension(convertWCharToU32String(text));
}

core::dimension2d<u32> CGUITTFont::getDimension(const std::u32string& text) const
{
	// Get the maximum font height.  Unfortunately, we have to do this hack as
	// Irrlicht will draw things wrong.  In FreeType, the font size is the
	// maximum size for a single glyph, but that glyph may hang "under" the
	// draw line, increasing the total font height to beyond the set size.
	// Irrlicht does not understand this concept when drawing fonts.  Also, I
	// add +1 to give it a 1 pixel blank border.  This makes things like
	// tooltips look nicer.
	s32 test1 = getHeightFromCharacter((char32_t)'g') + 1;
	s32 test2 = getHeightFromCharacter((char32_t)'j') + 1;
	s32 test3 = getHeightFromCharacter((char32_t)'_') + 1;
	s32 max_font_height = core::max_(test1, core::max_(test2, test3));

	core::dimension2d<u32> text_dimension(0, max_font_height);
	core::dimension2d<u32> line(0, max_font_height);

	char32_t previousChar = 0;
	std::u32string::const_iterator iter = text.begin();
	for (; iter != text.end(); ++iter)
	{
		char32_t p = *iter;
		bool lineBreak = false;
		if (p == '\r')	// Mac or Windows line breaks.
		{
			lineBreak = true;
			if (iter + 1 != text.end() && *(iter + 1) == '\n')
			{
				++iter;
				p = *iter;
			}
		}
		else if (p == '\n')	// Unix line breaks.
		{
			lineBreak = true;
		}

		// Kerning.
		core::vector2di k = getKerning(p, previousChar);
		line.Width += k.X;
		previousChar = p;

		// Check for linebreak.
		if (lineBreak)
		{
			previousChar = 0;
			text_dimension.Height += line.Height;
			if (text_dimension.Width < line.Width)
				text_dimension.Width = line.Width;
			line.Width = 0;
			line.Height = max_font_height;
			continue;
		}
		line.Width += getWidthFromCharacter(p);
	}
	if (text_dimension.Width < line.Width)
		text_dimension.Width = line.Width;

	return text_dimension;
}

inline u32 CGUITTFont::getWidthFromCharacter(char32_t c) const
{
	u32 n = getGlyphIndexByChar(c);
	if (n > 0)
	{
		int w = Glyphs[n-1].advance.X / 64;
		return w;
	}
	if (fallback != 0)
	{
		wchar_t s[] = { (wchar_t) c, 0 };
		return fallback->getDimension(s).Width;
	}

	if (c >= 0x2000)
		return (font_metrics.ascender / 64);
	else return (font_metrics.ascender / 64) / 2;
}

inline u32 CGUITTFont::getHeightFromCharacter(char32_t c) const
{
	u32 n = getGlyphIndexByChar(c);
	if (n > 0)
	{
		// Grab the true height of the character, taking into account underhanging glyphs.
		s32 height = (font_metrics.ascender / 64) - Glyphs[n-1].offset.Y + Glyphs[n-1].source_rect.getHeight();
		return height;
	}
	if (fallback != 0)
	{
		wchar_t s[] = { (wchar_t) c, 0 };
		return fallback->getDimension(s).Height;
	}

	if (c >= 0x2000)
		return (font_metrics.ascender / 64);
	else return (font_metrics.ascender / 64) / 2;
}

u32 CGUITTFont::getGlyphIndexByChar(char32_t c) const
{
	// Get the glyph.
	u32 glyph = FT_Get_Char_Index(tt_face, c);

	// Check for a valid glyph.
	if (glyph == 0)
		return 0;

	// If our glyph is already loaded, don't bother doing any batch loading code.
	if (glyph != 0 && Glyphs[glyph - 1].isLoaded())
		return glyph;

	// Determine our batch loading positions.
	u32 half_size = (batch_load_size / 2);
	u32 start_pos = 0;
	if (c > half_size) start_pos = c - half_size;
	u32 end_pos = start_pos + batch_load_size;

	// Load all our characters.
	do
	{
		// Get the character we are going to load.
		u32 char_index = FT_Get_Char_Index(tt_face, start_pos);

		// If the glyph hasn't been loaded yet, do it now.
		if (char_index)
		{
			SGUITTGlyph& glyph = Glyphs[char_index - 1];
			if (!glyph.isLoaded())
			{
				auto *this2 = const_cast<CGUITTFont*>(this); // oh well
				glyph.preload(char_index, tt_face, this2, size, load_flags);
				Glyph_Pages[glyph.glyph_page]->pushGlyphToBePaged(&glyph);
			}
		}
	}
	while (++start_pos < end_pos);

	// Return our original character.
	return glyph;
}

s32 CGUITTFont::getCharacterFromPos(const wchar_t* text, s32 pixel_x) const
{
	return getCharacterFromPos(convertWCharToU32String(text), pixel_x);
}

s32 CGUITTFont::getCharacterFromPos(const std::u32string& text, s32 pixel_x) const
{
	s32 x = 0;

	u32 character = 0;
	char32_t previousChar = 0;
	auto iter = text.begin();
	while (iter != text.end())
	{
		char32_t c = *iter;
		x += getWidthFromCharacter(c);

		// Kerning.
		core::vector2di k = getKerning(c, previousChar);
		x += k.X;

		if (x >= pixel_x)
			return character;

		previousChar = c;
		++iter;
		++character;
	}

	return -1;
}

void CGUITTFont::setKerningWidth(s32 kerning)
{
	GlobalKerningWidth = kerning;
}

void CGUITTFont::setKerningHeight(s32 kerning)
{
	GlobalKerningHeight = kerning;
}

core::vector2di CGUITTFont::getKerning(const wchar_t thisLetter, const wchar_t previousLetter) const
{
	return getKerning((char32_t)thisLetter, (char32_t)previousLetter);
}

core::vector2di CGUITTFont::getKerning(const char32_t thisLetter, const char32_t previousLetter) const
{
	if (tt_face == 0 || thisLetter == 0 || previousLetter == 0)
		return core::vector2di();

	// Set the size of the face.
	// This is because we cache faces and the face may have been set to a different size.
	FT_Set_Pixel_Sizes(tt_face, 0, size);

	core::vector2di ret(GlobalKerningWidth, GlobalKerningHeight);

	u32 n = getGlyphIndexByChar(thisLetter);

	// If we don't have this glyph, ask fallback font
	if (n == 0)
	{
		if (fallback)
			ret = fallback->getKerning((wchar_t) thisLetter, (wchar_t) previousLetter);
		return ret;
	}

	// If we don't have kerning, no point in continuing.
	if (!FT_HAS_KERNING(tt_face))
		return ret;

	// Get the kerning information.
	FT_Vector v;
	FT_Get_Kerning(tt_face, getGlyphIndexByChar(previousLetter), n, FT_KERNING_DEFAULT, &v);

	// If we have a scalable font, the return value will be in font points.
	if (FT_IS_SCALABLE(tt_face))
	{
		// Font points, so divide by 64.
		ret.X += (v.x / 64);
		ret.Y += (v.y / 64);
	}
	else
	{
		// Pixel units.
		ret.X += v.x;
		ret.Y += v.y;
	}
	return ret;
}

void CGUITTFont::setInvisibleCharacters(const wchar_t *s)
{
	Invisible = convertWCharToU32String(s);
}

video::IImage* CGUITTFont::createTextureFromChar(const char32_t& ch)
{
	// This character allows us to print something to the screen for unknown, unrecognizable, or
	// unrepresentable characters. See Unicode spec.
	const char32_t UTF_REPLACEMENT_CHARACTER = 0xFFFD;

	u32 n = getGlyphIndexByChar(ch);
	if (n == 0)
		n = getGlyphIndexByChar(UTF_REPLACEMENT_CHARACTER);

	const SGUITTGlyph& glyph = Glyphs[n-1];
	CGUITTGlyphPage* page = Glyph_Pages[glyph.glyph_page];

	if (page->dirty)
		page->updateTexture();

	video::ITexture* tex = page->texture;

	// Acquire a read-only lock of the corresponding page texture.
	void* ptr = tex->lock(video::ETLM_READ_ONLY);
	if (!ptr)
		return nullptr;

	video::ECOLOR_FORMAT format = tex->getColorFormat();
	core::dimension2du tex_size = tex->getOriginalSize();
	video::IImage* pageholder = Driver->createImageFromData(format, tex_size, ptr, true, false);

	// Copy the image data out of the page texture.
	core::dimension2du glyph_size(glyph.source_rect.getSize());
	video::IImage* image = Driver->createImage(format, glyph_size);
	pageholder->copyTo(image, core::position2di(0, 0), glyph.source_rect);

	tex->unlock();
	pageholder->drop();
	return image;
}

video::ITexture* CGUITTFont::getPageTextureByIndex(const u32& page_index) const
{
	if (page_index < Glyph_Pages.size())
		return Glyph_Pages[page_index]->texture;
	else
		return 0;
}

std::u32string CGUITTFont::convertWCharToU32String(const wchar_t* const charArray) const
{
	static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "unexpected wchar size");

	if (sizeof(wchar_t) == 4) // wchar_t is UTF-32
		return std::u32string(reinterpret_cast<const char32_t*>(charArray));

	// wchar_t is UTF-16 and we need to convert.
	// std::codecvt could do this for us but aside from being deprecated,
	// it turns out that it's laughably slow on MSVC. Thanks Microsoft.

	std::u32string ret;
	ret.reserve(wcslen(charArray));
	const wchar_t *p = charArray;
	while (*p) {
		char32_t c = *p;
		if (c >= 0xD800 && c < 0xDC00) {
			p++;
			char32_t c2 = *p;
			if (!c2)
				break;
			else if (c2 < 0xDC00 || c2 > 0xDFFF)
				continue; // can't find low surrogate, skip
			c = 0x10000 + ( ((c & 0x3ff) << 10) | (c2 & 0x3ff) );
		}
		ret.push_back(c);
		p++;
	}
	return ret;
}


} // end namespace gui
