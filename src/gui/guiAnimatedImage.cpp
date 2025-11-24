#include "guiAnimatedImage.h"

#include "client/guiscalingfilter.h"
#include "porting.h"
#include <ITexture.h>

GUIAnimatedImage::GUIAnimatedImage(gui::IGUIEnvironment *env, gui::IGUIElement *parent,
	s32 id, const core::rect<s32> &rectangle) :
	gui::IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, rectangle)
{
}

void GUIAnimatedImage::draw()
{
	if (m_texture == nullptr)
		return;

	video::IVideoDriver *driver = Environment->getVideoDriver();

	core::dimension2d<u32> size = m_texture->getOriginalSize();

	if ((u32)m_frame_count > size.Height)
		m_frame_count = size.Height;
	if (m_frame_idx >= m_frame_count)
		m_frame_idx = m_frame_count - 1;

	size.Height /= m_frame_count;

	core::rect<s32> rect(core::position2d<s32>(0, size.Height * m_frame_idx), size);
	core::rect<s32> *cliprect = NoClip ? nullptr : &AbsoluteClippingRect;

	if (m_middle.getArea() == 0) {
		const video::SColor color(255, 255, 255, 255);
		const video::SColor colors[] = {color, color, color, color};
		draw2DImageFilterScaled(driver, m_texture, AbsoluteRect, rect, cliprect,
			colors, true);
	} else {
		draw2DImage9Slice(driver, m_texture, AbsoluteRect, rect, m_middle, cliprect);
	}

	// Step the animation
	if (m_frame_count > 1 && m_frame_duration > 0) {
		// Determine the delta time to step
		u64 new_global_time = porting::getTimeMs();
		if (m_global_time > 0)
			m_frame_time += new_global_time - m_global_time;

		m_global_time = new_global_time;

		// Advance by the number of elapsed frames, looping if necessary
		m_frame_idx += (u32)(m_frame_time / m_frame_duration);
		m_frame_idx %= m_frame_count;

		// If 1 or more frames have elapsed, reset the frame time counter with
		// the remainder
		m_frame_time %= m_frame_duration;
	}
}
