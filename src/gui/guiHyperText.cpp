// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2019 EvicenceBKidscode / Pierre-Yves Rollo <dev@pyrollo.com>

#include "guiHyperText.h"
#include "guiScrollBar.h"
#include "client/fontengine.h"
#include "drawItemStack.h"
#include "IVideoDriver.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "hud.h"
#include "inventory.h"
#include "util/string.h"
#include "irrlicht_changes/CGUITTFont.h"
#include "mainmenumanager.h"
#include "porting.h"

using namespace gui;

static bool check_color(const std::string &str)
{
	video::SColor color;
	return parseColorString(str, color, false);
}

static bool check_integer(const std::string &str)
{
	if (str.empty())
		return false;

	char *endptr = nullptr;
	strtol(str.c_str(), &endptr, 10);

	return *endptr == '\0';
}

// -----------------------------------------------------------------------------
// ParsedText - A text parser

void ParsedText::Element::setStyle(StyleList &style)
{
	this->underline = is_yes(style["underline"]);

	video::SColor color;

	if (parseColorString(style["color"], color, false))
		this->color = color;
	if (parseColorString(style["hovercolor"], color, false))
		this->hovercolor = color;

	unsigned int font_size = std::atoi(style["fontsize"].c_str());

	FontMode font_mode = FM_Standard;
	if (style["fontstyle"] == "mono")
		font_mode = FM_Mono;

	// hypertext[] only accepts absolute font size values and has a hardcoded
	// default font size of 16. This is the only way to make hypertext[]
	// respect font size settings that I can think of.
	font_size = myround(font_size / 16.0f * g_fontengine->getFontSize(font_mode));

	FontSpec spec(font_size, font_mode,
		is_yes(style["bold"]), is_yes(style["italic"]));

	// TODO: find a way to check font validity
	// Build a new fontengine ?
	this->font = g_fontengine->getFont(spec);

	if (!this->font)
		printf("No font found ! Size=%d, mode=%d, bold=%s, italic=%s\n",
				font_size, font_mode, style["bold"].c_str(),
				style["italic"].c_str());
}

void ParsedText::Paragraph::setStyle(StyleList &style)
{
	if (style["halign"] == "center")
		this->halign = HALIGN_CENTER;
	else if (style["halign"] == "right")
		this->halign = HALIGN_RIGHT;
	else if (style["halign"] == "justify")
		this->halign = HALIGN_JUSTIFY;
	else
		this->halign = HALIGN_LEFT;
}

ParsedText::ParsedText(const wchar_t *text)
{
	// Default style
	m_root_tag.name = "root";
	m_root_tag.style["fontsize"] = "16";
	m_root_tag.style["fontstyle"] = "normal";
	m_root_tag.style["bold"] = "false";
	m_root_tag.style["italic"] = "false";
	m_root_tag.style["underline"] = "false";
	m_root_tag.style["halign"] = "left";
	m_root_tag.style["color"] = "#EEEEEE";
	m_root_tag.style["hovercolor"] = "#FF0000";

	m_active_tags.push_front(&m_root_tag);
	m_style = m_root_tag.style;

	// Default simple tags definitions
	StyleList style;

	style["color"] = "#0000FF";
	style["underline"] = "true";
	m_elementtags["action"] = style;
	style.clear();

	style["bold"] = "true";
	m_elementtags["b"] = style;
	style.clear();

	style["italic"] = "true";
	m_elementtags["i"] = style;
	style.clear();

	style["underline"] = "true";
	m_elementtags["u"] = style;
	style.clear();

	style["fontstyle"] = "mono";
	m_elementtags["mono"] = style;
	style.clear();

	style["fontsize"] = m_root_tag.style["fontsize"];
	m_elementtags["normal"] = style;
	style.clear();

	style["fontsize"] = "24";
	m_elementtags["big"] = style;
	style.clear();

	style["fontsize"] = "36";
	m_elementtags["bigger"] = style;
	style.clear();

	style["halign"] = "center";
	m_paragraphtags["center"] = style;
	style.clear();

	style["halign"] = "justify";
	m_paragraphtags["justify"] = style;
	style.clear();

	style["halign"] = "left";
	m_paragraphtags["left"] = style;
	style.clear();

	style["halign"] = "right";
	m_paragraphtags["right"] = style;
	style.clear();

	m_element = NULL;
	m_paragraph = NULL;
	m_end_paragraph_reason = ER_NONE;

	parse(text);
}

ParsedText::~ParsedText()
{
	for (auto &tag : m_not_root_tags)
		delete tag;
}

void ParsedText::parse(const wchar_t *text)
{
	wchar_t c;
	u32 cursor = 0;
	bool escape = false;

	while ((c = text[cursor]) != L'\0') {
		cursor++;

		if (c == L'\r') { // Mac or Windows breaks
			if (text[cursor] == L'\n')
				cursor++;
			// If text has begun, don't skip empty line
			if (m_paragraph) {
				endParagraph(ER_NEWLINE);
				enterElement(ELEMENT_SEPARATOR);
			}
			escape = false;
			continue;
		}

		if (c == L'\n') { // Unix breaks
			// If text has begun, don't skip empty line
			if (m_paragraph) {
				endParagraph(ER_NEWLINE);
				enterElement(ELEMENT_SEPARATOR);
			}
			escape = false;
			continue;
		}

		if (escape) {
			escape = false;
			pushChar(c);
			continue;
		}

		if (c == L'\\') {
			escape = true;
			continue;
		}

		// Tag check
		if (c == L'<') {
			u32 newcursor = parseTag(text, cursor);
			if (newcursor > 0) {
				cursor = newcursor;
				continue;
			}
		}

		// Default behavior
		pushChar(c);
	}

	endParagraph(ER_NONE);
}

void ParsedText::endElement()
{
	m_element = NULL;
}

void ParsedText::endParagraph(EndReason reason)
{
	if (!m_paragraph)
		return;

	EndReason previous = m_end_paragraph_reason;
	m_end_paragraph_reason = reason;
	if (m_empty_paragraph && (reason == ER_TAG ||
			(reason == ER_NEWLINE && previous == ER_TAG))) {
		// Ignore last empty paragraph
		m_paragraph = nullptr;
		m_paragraphs.pop_back();
		return;
	}
	endElement();
	m_paragraph = NULL;
}

void ParsedText::enterParagraph()
{
	if (!m_paragraph) {
		m_paragraphs.emplace_back();
		m_paragraph = &m_paragraphs.back();
		m_paragraph->setStyle(m_style);
		m_empty_paragraph = true;
	}
}

void ParsedText::enterElement(ElementType type)
{
	enterParagraph();

	if (!m_element || m_element->type != type) {
		m_paragraph->elements.emplace_back();
		m_element = &m_paragraph->elements.back();
		m_element->type = type;
		m_element->tags = m_active_tags;
		m_element->setStyle(m_style);
	}
}

void ParsedText::pushChar(wchar_t c)
{
	// New word if needed
	if (c == L' ' || c == L'\t') {
		if (!m_empty_paragraph)
			enterElement(ELEMENT_SEPARATOR);
		else
			return;
	} else {
		enterElement(ELEMENT_TEXT);
		m_empty_paragraph = false;
	}
	m_element->text += c;
}

ParsedText::Tag *ParsedText::newTag(const std::string &name, const AttrsList &attrs)
{
	endElement();
	Tag *newtag = new Tag();
	newtag->name = name;
	newtag->attrs = attrs;
	m_not_root_tags.push_back(newtag);
	return newtag;
}

ParsedText::Tag *ParsedText::openTag(const std::string &name, const AttrsList &attrs)
{
	Tag *newtag = newTag(name, attrs);
	m_active_tags.push_front(newtag);
	return newtag;
}

bool ParsedText::closeTag(const std::string &name)
{
	bool found = false;
	for (auto id = m_active_tags.begin(); id != m_active_tags.end(); ++id)
		if ((*id)->name == name) {
			m_active_tags.erase(id);
			found = true;
			break;
		}
	return found;
}

void ParsedText::parseGenericStyleAttr(
		const std::string &name, const std::string &value, StyleList &style)
{
	// Color styles
	if (name == "color" || name == "hovercolor") {
		if (check_color(value))
			style[name] = value;

		// Boolean styles
	} else if (name == "bold" || name == "italic" || name == "underline") {
		style[name] = is_yes(value);

	} else if (name == "size") {
		if (check_integer(value))
			style["fontsize"] = value;

	} else if (name == "font") {
		if (value == "mono" || value == "normal")
			style["fontstyle"] = value;
	}
}

void ParsedText::parseStyles(const AttrsList &attrs, StyleList &style)
{
	for (auto const &attr : attrs)
		parseGenericStyleAttr(attr.first, attr.second, style);
}

void ParsedText::globalTag(const AttrsList &attrs)
{
	for (const auto &attr : attrs) {
		// Only page level style
		if (attr.first == "margin") {
			if (check_integer(attr.second))
				margin = stoi(attr.second.c_str());

		} else if (attr.first == "valign") {
			if (attr.second == "top")
				valign = ParsedText::VALIGN_TOP;
			else if (attr.second == "bottom")
				valign = ParsedText::VALIGN_BOTTOM;
			else if (attr.second == "middle")
				valign = ParsedText::VALIGN_MIDDLE;
		} else if (attr.first == "background") {
			video::SColor color;
			if (attr.second == "none") {
				background_type = BACKGROUND_NONE;
			} else if (parseColorString(attr.second, color, false)) {
				background_type = BACKGROUND_COLOR;
				background_color = color;
			}

			// Inheriting styles

		} else if (attr.first == "halign") {
			if (attr.second == "left" || attr.second == "center" ||
					attr.second == "right" ||
					attr.second == "justify")
				m_root_tag.style["halign"] = attr.second;

			// Generic default styles

		} else {
			parseGenericStyleAttr(attr.first, attr.second, m_root_tag.style);
		}
	}
}

u32 ParsedText::parseTag(const wchar_t *text, u32 cursor)
{
	// Tag name
	bool end = false;
	std::string name = "";
	wchar_t c = text[cursor];

	if (c == L'/') {
		end = true;
		c = text[++cursor];
		if (c == L'\0')
			return 0;
	}

	while (c != ' ' && c != '>') {
		name += c;
		c = text[++cursor];
		if (c == L'\0')
			return 0;
	}

	// Tag attributes
	AttrsList attrs;
	while (c != L'>') {
		std::string attr_name = "";
		std::wstring attr_val = L"";

		// Consume whitespace
		while (c == ' ') {
			c = text[++cursor];
			if (c == L'\0' || c == L'=')
				return 0;
		}

		// Read attribute name
		while (c != L' ' && c != L'=') {
			attr_name += (char)c;
			c = text[++cursor];
			if (c == L'\0' || c == L'>')
				return 0;
		}

		// Consume whitespace
		while (c == L' ') {
			c = text[++cursor];
			if (c == L'\0' || c == L'>')
				return 0;
		}

		// Skip equals
		if (c != L'=')
			return 0;
		c = text[++cursor];
		if (c == L'\0')
			return 0;

		// Read optional quote
		wchar_t quote_used = 0;
		if (c == L'"' || c == L'\'') {
			quote_used = c;
			c = text[++cursor];
			if (c == L'\0')
				return 0;
		}

		// Read attribute value
		bool escape = false;
		while (escape || (quote_used && c != quote_used) || (!quote_used && c != L'>' && c != L' ')) {
			if (quote_used && !escape && c == L'\\') {
				escape = true;
			} else {
				escape = false;
				attr_val += c;
			}
			c = text[++cursor];
			if (c == L'\0')
				return 0;
		}

		// Remove quote
		if (quote_used) {
			if (c != quote_used)
				return 0;
			c = text[++cursor];
		}

		attrs[attr_name] = wide_to_utf8(attr_val);
	}

	++cursor; // Last ">"

	// Tag specific processing
	StyleList style;

	if (name == "global") {
		if (end)
			return 0;
		globalTag(attrs);

	} else if (name == "style") {
		if (end) {
			closeTag(name);
		} else {
			parseStyles(attrs, style);
			openTag(name, attrs)->style = style;
		}
		endElement();
	} else if (name == "img" || name == "item") {
		if (end)
			return 0;

		// Name is a required attribute
		if (!attrs.count("name"))
			return 0;

		// Rotate attribute is only for <item>
		if (attrs.count("rotate") && name != "item")
			return 0;

		// Angle attribute is only for <item>
		if (attrs.count("angle") && name != "item")
			return 0;

		// Ok, element can be created
		newTag(name, attrs);

		if (name == "img")
			enterElement(ELEMENT_IMAGE);
		else
			enterElement(ELEMENT_ITEM);

		m_element->text = utf8_to_stringw(attrs["name"]);

		if (attrs.count("float")) {
			if (attrs["float"] == "left")
				m_element->floating = FLOAT_LEFT;
			if (attrs["float"] == "right")
				m_element->floating = FLOAT_RIGHT;
		}

		if (attrs.count("width")) {
			int width = stoi(attrs["width"]);
			if (width > 0)
				m_element->dim.Width = width;
		}

		if (attrs.count("height")) {
			int height = stoi(attrs["height"]);
			if (height > 0)
				m_element->dim.Height = height;
		}

		if (attrs.count("angle")) {
			std::string str = attrs["angle"];
			std::vector<std::string> parts = split(str, ',');
			if (parts.size() == 3) {
				m_element->angle = v3s16(
						rangelim(stoi(parts[0]), -180, 180),
						rangelim(stoi(parts[1]), -180, 180),
						rangelim(stoi(parts[2]), -180, 180));
				m_element->rotation = v3s16(0, 0, 0);
			}
		}

		if (attrs.count("rotate")) {
			if (attrs["rotate"] == "yes") {
				m_element->rotation = v3s16(0, 100, 0);
			} else {
				std::string str = attrs["rotate"];
				std::vector<std::string> parts = split(str, ',');
				if (parts.size() == 3) {
					m_element->rotation = v3s16 (
							rangelim(stoi(parts[0]), -1000, 1000),
							rangelim(stoi(parts[1]), -1000, 1000),
							rangelim(stoi(parts[2]), -1000, 1000));
				}
			}
		}

		endElement();

	} else if (name == "tag") {
		// Required attributes
		if (!attrs.count("name"))
			return 0;

		StyleList tagstyle;
		parseStyles(attrs, tagstyle);

		if (is_yes(attrs["paragraph"]))
			m_paragraphtags[attrs["name"]] = tagstyle;
		else
			m_elementtags[attrs["name"]] = tagstyle;

	} else if (name == "action") {
		if (end) {
			closeTag(name);
		} else {
			if (!attrs.count("name"))
				return 0;
			openTag(name, attrs)->style = m_elementtags["action"];
		}
		endElement();

	} else if (m_elementtags.count(name)) {
		if (end) {
			closeTag(name);
		} else {
			openTag(name, attrs)->style = m_elementtags[name];
		}
		endElement();

	} else if (m_paragraphtags.count(name)) {
		if (end) {
			closeTag(name);
		} else {
			openTag(name, attrs)->style = m_paragraphtags[name];
		}
		endParagraph(ER_TAG);

	} else
		return 0; // Unknown tag

	// Update styles accordingly
	m_style.clear();
	for (auto tag = m_active_tags.crbegin(); tag != m_active_tags.crend(); ++tag)
		for (const auto &prop : (*tag)->style)
			m_style[prop.first] = prop.second;

	return cursor;
}

// -----------------------------------------------------------------------------
// Text Drawer

TextDrawer::TextDrawer(const wchar_t *text, Client *client,
		gui::IGUIEnvironment *environment, ISimpleTextureSource *tsrc) :
		m_text(text), m_client(client), m_tsrc(tsrc), m_guienv(environment)
{
	// Size all elements
	for (auto &p : m_text.m_paragraphs) {
		for (auto &e : p.elements) {
			switch (e.type) {
			case ParsedText::ELEMENT_SEPARATOR:
			case ParsedText::ELEMENT_TEXT:
				if (e.font) {
					e.dim.Width = e.font->getDimension(e.text.c_str()).Width;
					e.dim.Height = e.font->getDimension(L"Yy").Height;
					if (e.font->getType() == gui::EGFT_CUSTOM) {
						CGUITTFont *tmp = static_cast<CGUITTFont*>(e.font);
						e.baseline = e.dim.Height - 1 - tmp->getAscender() / 64;
					}
				} else {
					e.dim = {0, 0};
				}
				break;

			case ParsedText::ELEMENT_IMAGE:
			case ParsedText::ELEMENT_ITEM:
				// Resize only non sized items
				if (e.dim.Height != 0 && e.dim.Width != 0)
					break;

				// Default image and item size
				core::dimension2d<u32> dim(80, 80);

				if (e.type == ParsedText::ELEMENT_IMAGE) {
					video::ITexture *texture =
						m_tsrc->
							getTexture(stringw_to_utf8(e.text));
					if (texture)
						dim = texture->getOriginalSize();
				}

				if (e.dim.Height == 0)
					if (e.dim.Width == 0)
						e.dim = dim;
					else
						e.dim.Height = dim.Height * e.dim.Width /
								dim.Width;
				else
					e.dim.Width = dim.Width * e.dim.Height /
							dim.Height;
				break;
			}
		}
	}
}

// Get element at given coordinates. Coordinates are inner coordinates (starting
// at 0,0).
ParsedText::Element *TextDrawer::getElementAt(core::position2d<s32> pos)
{
	pos.Y -= m_voffset;
	for (auto &p : m_text.m_paragraphs) {
		for (auto &el : p.elements) {
			core::rect<s32> rect(el.pos, el.dim);
			if (rect.isPointInside(pos))
				return &el;
		}
	}
	return 0;
}

/*
   This function places all elements according to given width. Elements have
   been previously sized by constructor and will be later drawed by draw.
   It may be called each time width changes and resulting height can be
   retrieved using getHeight. See GUIHyperText constructor, it uses it once to
   test if text fits in window and eventually another time if width is reduced
   m_floating because of scrollbar added.
*/
void TextDrawer::place(const core::rect<s32> &dest_rect)
{
	m_floating.clear();
	s32 y = 0;
	s32 ymargin = m_text.margin;

	// Iterator used :
	// p - Current paragraph, walked only once
	// el - Current element, walked only once
	// e and f - local element and floating operators

	for (auto &p : m_text.m_paragraphs) {
		// Find and place floating stuff in paragraph
		for (auto e = p.elements.begin(); e != p.elements.end(); ++e) {
			if (e->floating != ParsedText::FLOAT_NONE) {
				if (y)
					e->pos.Y = y + std::max(ymargin, e->margin);
				else
					e->pos.Y = ymargin;

				if (e->floating == ParsedText::FLOAT_LEFT)
					e->pos.X = m_text.margin;
				if (e->floating == ParsedText::FLOAT_RIGHT)
					e->pos.X = dest_rect.getWidth() - e->dim.Width -
							m_text.margin;

				RectWithMargin floating;
				floating.rect = core::rect<s32>(e->pos, e->dim);
				floating.margin = e->margin;

				m_floating.push_back(floating);
			}
		}

		if (y)
			y = y + std::max(ymargin, p.margin);

		ymargin = p.margin;

		// Place non floating stuff
		auto el = p.elements.begin();

		while (el != p.elements.end()) {
			// Determine line width and y pos
			s32 left, right;
			s32 nexty = y;
			do {
				y = nexty;
				nexty = 0;

				// Inner left & right
				left = m_text.margin;
				right = dest_rect.getWidth() - m_text.margin;

				for (const auto &f : m_floating) {
					// Does floating rect intersect paragraph y line?
					if (f.rect.UpperLeftCorner.Y - f.margin <= y &&
							f.rect.LowerRightCorner.Y + f.margin >= y) {

						// Next Y to try if no room left
						if (!nexty || f.rect.LowerRightCorner.Y +
								std::max(f.margin, p.margin) < nexty) {
							nexty = f.rect.LowerRightCorner.Y +
									std::max(f.margin, p.margin) + 1;
						}

						if (f.rect.UpperLeftCorner.X - f.margin <= left &&
								f.rect.LowerRightCorner.X + f.margin < right) {
							// float on left
							if (f.rect.LowerRightCorner.X +
									std::max(f.margin, p.margin) > left) {
								left = f.rect.LowerRightCorner.X +
										std::max(f.margin, p.margin);
							}
						} else if (f.rect.LowerRightCorner.X + f.margin >= right &&
								f.rect.UpperLeftCorner.X - f.margin > left) {
							// float on right
							if (f.rect.UpperLeftCorner.X -
									std::max(f.margin, p.margin) < right)
								right = f.rect.UpperLeftCorner.X -
										std::max(f.margin, p.margin);

						} else if (f.rect.UpperLeftCorner.X - f.margin <= left &&
								f.rect.LowerRightCorner.X + f.margin >= right) {
							// float taking all space
							left = right;
						}
						else
						{ // float in the middle -- should not occure yet, see that later
						}
					}
				}
			} while (nexty && right <= left);

			u32 linewidth = right - left;
			float x = left;

			u32 charsheight = 0;
			u32 charswidth = 0;
			u32 wordcount = 0;

			// Skip beginning of line separators but include them in height
			// computation.
			while (el != p.elements.end() &&
					el->type == ParsedText::ELEMENT_SEPARATOR) {
				if (el->floating == ParsedText::FLOAT_NONE) {
					el->drawwidth = 0;
					if (charsheight < el->dim.Height)
						charsheight = el->dim.Height;
				}
				el++;
			}

			auto linestart = el;
			auto lineend = p.elements.end();

			// First pass, find elements fitting into line
			// (or at least one element)
			while (el != p.elements.end() && (charswidth == 0 ||
					charswidth + el->dim.Width <= linewidth)) {
				if (el->floating == ParsedText::FLOAT_NONE) {
					if (el->type != ParsedText::ELEMENT_SEPARATOR) {
						lineend = el;
						wordcount++;
					}
					charswidth += el->dim.Width;
					if (charsheight < el->dim.Height)
						charsheight = el->dim.Height;
				}
				el++;
			}

			// Empty line, nothing to place only go down line height
			if (lineend == p.elements.end()) {
				y += charsheight;
				continue;
			}

			// Point to the first position outside line (may be end())
			lineend++;

			// Second pass, compute printable line width and adjustments
			charswidth = 0;
			s32 top = 0;
			s32 bottom = 0;
			for (auto e = linestart; e != lineend; ++e) {
				if (e->floating == ParsedText::FLOAT_NONE) {
					charswidth += e->dim.Width;
					if (top < (s32)e->dim.Height - e->baseline)
						top = e->dim.Height - e->baseline;
					if (bottom < e->baseline)
						bottom = e->baseline;
				}
			}

			float extraspace = 0.f;

			switch (p.halign) {
			case ParsedText::HALIGN_CENTER:
				x += (linewidth - charswidth) / 2.f;
				break;
			case ParsedText::HALIGN_JUSTIFY:
				if (wordcount > 1 && // Justification only if at least two words
					!(lineend == p.elements.end())) // Don't justify last line
					extraspace = ((float)(linewidth - charswidth)) / (wordcount - 1);
				break;
			case ParsedText::HALIGN_RIGHT:
				x += linewidth - charswidth;
				break;
			case ParsedText::HALIGN_LEFT:
				break;
			}

			// Third pass, actually place everything
			for (auto e = linestart; e != lineend; ++e) {
				if (e->floating != ParsedText::FLOAT_NONE)
					continue;

				e->pos.X = x;
				e->pos.Y = y;

				switch (e->type) {
				case ParsedText::ELEMENT_TEXT:
				case ParsedText::ELEMENT_SEPARATOR:
					e->pos.X = x;

					// Align char baselines
					e->pos.Y = y + top + e->baseline - e->dim.Height;

					x += e->dim.Width;
					if (e->type == ParsedText::ELEMENT_SEPARATOR)
						x += extraspace;
					break;

				case ParsedText::ELEMENT_IMAGE:
				case ParsedText::ELEMENT_ITEM:
					x += e->dim.Width;
					break;
				}

				// Draw width for separator can be different than element
				// width. This will be important for char effects like
				// underline.
				e->drawwidth = x - e->pos.X;
			}
			y += charsheight;
		} // Elements (actually lines)
	} // Paragraph

	// Check if float goes under paragraph
	for (const auto &f : m_floating) {
		if (f.rect.LowerRightCorner.Y >= y)
			y = f.rect.LowerRightCorner.Y;
	}

	m_height = y + m_text.margin;
	// Compute vertical offset according to vertical alignment
	if (m_height < dest_rect.getHeight())
		switch (m_text.valign) {
		case ParsedText::VALIGN_BOTTOM:
			m_voffset = dest_rect.getHeight() - m_height;
			break;
		case ParsedText::VALIGN_MIDDLE:
			m_voffset = (dest_rect.getHeight() - m_height) / 2;
			break;
		case ParsedText::VALIGN_TOP:
		default:
			m_voffset = 0;
		}
	else
		m_voffset = 0;
}

// Draw text in a rectangle with a given offset. Items are actually placed in
// relative (to upper left corner) coordinates.
void TextDrawer::draw(const core::rect<s32> &clip_rect,
		const core::position2d<s32> &dest_offset)
{
	video::IVideoDriver *driver = m_guienv->getVideoDriver();
	core::position2d<s32> offset = dest_offset;
	offset.Y += m_voffset;

	if (m_text.background_type == ParsedText::BACKGROUND_COLOR)
		driver->draw2DRectangle(m_text.background_color, clip_rect);

	for (auto &p : m_text.m_paragraphs) {
		for (auto &el : p.elements) {
			core::rect<s32> rect(el.pos + offset, el.dim);
			if (!rect.isRectCollided(clip_rect))
				continue;

			switch (el.type) {
			case ParsedText::ELEMENT_SEPARATOR:
			case ParsedText::ELEMENT_TEXT: {
				video::SColor color = el.color;

				for (auto tag : el.tags)
					if (&(*tag) == m_hovertag)
						color = el.hovercolor;

				if (!el.font)
					break;

				if (el.type == ParsedText::ELEMENT_TEXT)
					el.font->draw(el.text, rect, color, false, true,
							&clip_rect);

				if (el.underline &&  el.drawwidth) {
					s32 linepos = el.pos.Y + offset.Y +
							el.dim.Height - (el.baseline >> 1);

					core::rect<s32> linerect(el.pos.X + offset.X,
							linepos - (el.baseline >> 3) - 1,
							el.pos.X + offset.X + el.drawwidth,
							linepos + (el.baseline >> 3));

					driver->draw2DRectangle(color, linerect, &clip_rect);
				}
			} break;

			case ParsedText::ELEMENT_IMAGE: {
				video::ITexture *texture =
						m_tsrc->getTexture(
								stringw_to_utf8(el.text));
				if (texture != 0)
					m_guienv->getVideoDriver()->draw2DImage(
							texture, rect,
							core::rect<s32>(
									core::position2d<s32>(0, 0),
									texture->getOriginalSize()),
							&clip_rect, 0, true);
			} break;

			case ParsedText::ELEMENT_ITEM: {
				if (m_client) {
					IItemDefManager *idef = m_client->idef();
					ItemStack item;
					item.deSerialize(stringw_to_utf8(el.text), idef);

					drawItemStack(m_guienv->getVideoDriver(),
							g_fontengine->getFont(), item, rect, &clip_rect, m_client,
							IT_ROT_OTHER, el.angle, el.rotation);
				}
			} break;
			}
		}
	}
}

// -----------------------------------------------------------------------------
// GUIHyperText - The formated text area formspec item

//! constructor
GUIHyperText::GUIHyperText(const wchar_t *text, IGUIEnvironment *environment,
		IGUIElement *parent, s32 id, const core::rect<s32> &rectangle,
		Client *client, ISimpleTextureSource *tsrc) :
		IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle),
		m_tsrc(tsrc), m_vscrollbar(nullptr),
		m_drawer(text, client, environment, tsrc), m_text_scrollpos(0, 0)
{

	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();

	m_scrollbar_width = skin ? skin->getSize(gui::EGDS_SCROLLBAR_SIZE) : 16;

	core::rect<s32> rect = core::rect<s32>(
			RelativeRect.getWidth() - m_scrollbar_width, 0,
			RelativeRect.getWidth(), RelativeRect.getHeight());

	m_vscrollbar = new GUIScrollBar(Environment, this, -1, rect, false, true, tsrc);
	m_vscrollbar->setVisible(false);
}

//! destructor
GUIHyperText::~GUIHyperText()
{
	m_vscrollbar->remove();
	m_vscrollbar->drop();
}

ParsedText::Element *GUIHyperText::getElementAt(s32 X, s32 Y)
{
	core::position2d<s32> pos{X, Y};
	pos -= m_display_text_rect.UpperLeftCorner;
	pos -= m_text_scrollpos;
	return m_drawer.getElementAt(pos);
}

void GUIHyperText::checkHover(s32 X, s32 Y)
{
	m_drawer.m_hovertag = nullptr;

	if (AbsoluteRect.isPointInside(core::position2d<s32>(X, Y))) {
		ParsedText::Element *element = getElementAt(X, Y);

		if (element) {
			for (auto &tag : element->tags) {
				if (tag->name == "action") {
					m_drawer.m_hovertag = tag;
					break;
				}
			}
		}
	}

	ICursorControl *cursor_control = RenderingEngine::get_raw_device()->getCursorControl();

	if (cursor_control)
		cursor_control->setActiveIcon(m_drawer.m_hovertag ? gui::ECI_HAND : gui::ECI_NORMAL);
}

bool GUIHyperText::OnEvent(const SEvent &event)
{
	// Scroll bar
	if (event.EventType == EET_GUI_EVENT &&
			event.GUIEvent.EventType == EGET_SCROLL_BAR_CHANGED &&
			event.GUIEvent.Caller == m_vscrollbar) {
		m_text_scrollpos.Y = -m_vscrollbar->getPos();
	}

	// Reset hover if element left
	if (event.EventType == EET_GUI_EVENT &&
			event.GUIEvent.EventType == EGET_ELEMENT_LEFT) {
		m_drawer.m_hovertag = nullptr;

		ICursorControl *cursor_control = RenderingEngine::get_raw_device()->getCursorControl();

		if (cursor_control && cursor_control->isVisible())
			cursor_control->setActiveIcon(gui::ECI_NORMAL);
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		if (event.MouseInput.Event == EMIE_MOUSE_MOVED)
			checkHover(event.MouseInput.X, event.MouseInput.Y);

		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL && m_vscrollbar->isVisible()) {
			m_vscrollbar->setPosInterpolated(m_vscrollbar->getTargetPos() -
					event.MouseInput.Wheel * m_vscrollbar->getSmallStep());
			m_text_scrollpos.Y = -m_vscrollbar->getPos();
			m_drawer.draw(m_display_text_rect, m_text_scrollpos);
			checkHover(event.MouseInput.X, event.MouseInput.Y);
			return true;

		} else if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
			ParsedText::Element *element = getElementAt(
					event.MouseInput.X, event.MouseInput.Y);

			if (element) {
				for (auto &tag : element->tags) {
					if (tag->name == "action") {
						Text = core::stringw(L"action:") +
						       utf8_to_stringw(tag->attrs["name"]);
						if (Parent) {
							SEvent newEvent;
							newEvent.EventType = EET_GUI_EVENT;
							newEvent.GUIEvent.Caller = this;
							newEvent.GUIEvent.Element = 0;
							newEvent.GUIEvent.EventType = EGET_BUTTON_CLICKED;
							Parent->OnEvent(newEvent);
						}

						auto url_it = tag->attrs.find("url");
						if (url_it != tag->attrs.end()) {
							if (g_gamecallback) {
								// in game
								g_gamecallback->showOpenURLDialog(url_it->second);
							} else {
								// main menu
								porting::open_url(url_it->second);
							}
						}

						return true;
					}
				}
			}
		}
	}

	return IGUIElement::OnEvent(event);
}

//! draws the element and its children
void GUIHyperText::draw()
{
	if (!IsVisible)
		return;

	// Text
	m_display_text_rect = AbsoluteRect;
	m_drawer.place(m_display_text_rect);

	// Show a scrollbar if the text overflows vertically
	if (m_drawer.getHeight() > m_display_text_rect.getHeight()) {
		// Showing a scrollbar will reduce the width of the viewport, causing
		// more text to be wrapped and thus increasing the height of the text.
		// Therefore, we have to re-layout the text *before* setting the height
		// of the scrollbar.
		core::rect<s32> smaller_rect = m_display_text_rect;
		smaller_rect.LowerRightCorner.X -= m_scrollbar_width;
		m_drawer.place(smaller_rect);

		m_vscrollbar->setSmallStep(m_display_text_rect.getHeight() * 0.1f);
		m_vscrollbar->setLargeStep(m_display_text_rect.getHeight() * 0.5f);
		m_vscrollbar->setMax(m_drawer.getHeight() - m_display_text_rect.getHeight());

		m_vscrollbar->setVisible(true);

		m_vscrollbar->setPageSize(s32(m_drawer.getHeight()));
	} else {
		m_vscrollbar->setMax(0);
		m_vscrollbar->setPos(0);
		m_vscrollbar->setVisible(false);
	}
	m_drawer.draw(AbsoluteClippingRect,
			m_display_text_rect.UpperLeftCorner + m_text_scrollpos);

	// draw children
	IGUIElement::draw();
}
