// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier

#include "guiEngine.h"

#include "client/fontengine.h"
#include "client/guiscalingfilter.h"
#include "client/renderingengine.h"
#include "client/shader.h"
#include "client/tile.h"
#include "clientdynamicinfo.h"
#include "config.h"
#include "content/content.h"
#include "content/mods.h"
#include "filesys.h"
#include "guiMainMenu.h"
#include "httpfetch.h"
#include "irrlicht_changes/static_text.h"
#include "log.h"
#include "porting.h"
#include "scripting_mainmenu.h"
#include "settings.h"
#include "sound.h"
#include "version.h"
#include <ICameraSceneNode.h>
#include <IGUIStaticText.h>
#include "client/imagefilters.h"
#include "util/tracy_wrapper.h"
#include "script/common/c_types.h" // LuaError

#if USE_SOUND
	#include "client/sound/sound_openal.h"
#endif

#include <csignal>


/******************************************************************************/
void TextDestGuiEngine::gotText(const StringMap &fields)
{
	m_engine->getScriptIface()->handleMainMenuButtons(fields);
}

/******************************************************************************/
MenuTextureSource::~MenuTextureSource()
{
	u32 before = m_driver->getTextureCount();

	for (const auto &it: m_to_delete) {
		m_driver->removeTexture(it);
	}
	m_to_delete.clear();

	infostream << "~MenuTextureSource() before cleanup: "<< before
			<< " after: " << m_driver->getTextureCount() << std::endl;
}

/******************************************************************************/
video::ITexture *MenuTextureSource::getTexture(const std::string &name, u32 *id)
{
	if (id)
		*id = 1;

	if (name.empty())
		return NULL;

	// return if already loaded
	video::ITexture *retval = m_driver->findTexture(name.c_str());
	if (retval)
		return retval;

	verbosestream << "MenuTextureSource: loading " << name << std::endl;
	video::IImage *image = m_driver->createImageFromFile(name.c_str());
	if (!image)
		return NULL;

	retval = m_driver->addTexture(name.c_str(), image);
	image->drop();

	if (retval)
		m_to_delete.push_back(retval);
	return retval;
}

/******************************************************************************/
/** MenuMusicFetcher                                                          */
/******************************************************************************/
void MenuMusicFetcher::addThePaths(const std::string &name,
		std::vector<std::string> &paths)
{
	// Allow full paths
	if (name.find(DIR_DELIM_CHAR) != std::string::npos) {
		addAllAlternatives(name, paths);
	} else {
		addAllAlternatives(porting::path_share + DIR_DELIM + "sounds" + DIR_DELIM + name, paths);
		addAllAlternatives(porting::path_user + DIR_DELIM + "sounds" + DIR_DELIM + name, paths);
	}
}

/******************************************************************************/
/** GUIEngine                                                                 */
/******************************************************************************/
GUIEngine::GUIEngine(JoystickController *joystick,
		gui::IGUIElement *parent,
		RenderingEngine *rendering_engine,
		IMenuManager *menumgr,
		MainMenuData *data,
		volatile std::sig_atomic_t &kill) :
	m_rendering_engine(rendering_engine),
	m_parent(parent),
	m_menumanager(menumgr),
	m_smgr(rendering_engine->get_scene_manager()),
	m_data(data),
	m_kill(kill)
{
	// Go back to our mainmenu fonts
	// Delayed until mainmenu initialization because of #15883
	g_fontengine->clearMediaFonts();

	// initialize texture pointers
	for (image_definition &texture : m_textures) {
		texture.texture = NULL;
	}
	// is deleted by guiformspec!
	auto buttonhandler = std::make_unique<TextDestGuiEngine>(this);
	m_buttonhandler = buttonhandler.get();

	// create texture source
	m_texture_source = std::make_unique<MenuTextureSource>(rendering_engine->get_video_driver());

	// create soundmanager
#if USE_SOUND
	if (g_sound_manager_singleton.get()) {
		m_sound_manager = createOpenALSoundManager(g_sound_manager_singleton.get(),
				std::make_unique<MenuMusicFetcher>());
	}
#endif
	if (!m_sound_manager)
		m_sound_manager = std::make_unique<DummySoundManager>();

	// create topleft header
	m_toplefttext = L"";

	core::rect<s32> rect(0, 0, g_fontengine->getTextWidth(m_toplefttext.c_str()),
		g_fontengine->getTextHeight());
	rect += v2s32(4, 0);

	m_irr_toplefttext = gui::StaticText::add(rendering_engine->get_gui_env(),
			m_toplefttext, rect, false, true, 0, -1);

	// create formspecsource
	auto formspecgui = std::make_unique<FormspecFormSource>("");
	m_formspecgui = formspecgui.get();

	/* Create menu */
	m_menu = make_irr<GUIFormSpecMenu>(
			joystick,
			m_parent,
			-1,
			m_menumanager,
			nullptr /* &client */,
			m_rendering_engine->get_gui_env(),
			m_texture_source.get(),
			m_sound_manager.get(),
			formspecgui.release(),
			buttonhandler.release(),
			"",
			false);

	m_menu->defaultAllowClose(false);
	m_menu->lockSize(true,v2u32(800,600));

	// Initialize scripting

	infostream << "GUIEngine: Initializing Lua" << std::endl;

	m_script = std::make_unique<MainMenuScripting>(this);

	g_settings->registerChangedCallback("fullscreen", fullscreenChangedCallback, this);

	try {
		m_script->setMainMenuData(&m_data->script_data);
		m_data->script_data.errormessage.clear();

		if (!loadMainMenuScript()) {
			errorstream << "No future without main menu!" << std::endl;
			abort();
		}

		run();
	} catch (LuaError &e) {
		errorstream << "Main menu error: " << e.what() << std::endl;
		m_data->script_data.errormessage = e.what();
	}

	m_menu->quitMenu();
	m_menu.reset();
}


/******************************************************************************/
std::string findLocaleFileWithExtension(const std::string &path)
{
	if (fs::PathExists(path + ".mo"))
		return path + ".mo";
	if (fs::PathExists(path + ".po"))
		return path + ".po";
	if (fs::PathExists(path + ".tr"))
		return path + ".tr";
	return "";
}


/******************************************************************************/
std::string findLocaleFileInMods(const std::string &path, const std::string &filename_no_ext)
{
	std::vector<ModSpec> mods = flattenMods(getModsInPath(path, "root", true));

	for (const auto &mod : mods) {
		std::string ret = findLocaleFileWithExtension(
				mod.path + DIR_DELIM "locale" DIR_DELIM + filename_no_ext);
		if (!ret.empty())
			return ret;
	}

	return "";
}

/******************************************************************************/
Translations *GUIEngine::getContentTranslations(const std::string &path,
		const std::string &domain, const std::string &lang_code)
{
	if (domain.empty() || lang_code.empty())
		return nullptr;

	std::string filename_no_ext = domain + "." + lang_code;
	std::string key = path + DIR_DELIM "locale" DIR_DELIM + filename_no_ext;

	if (key == m_last_translations_key)
		return &m_last_translations;

	std::string trans_path = key;

	switch (getContentType(path)) {
	case ContentType::GAME:
		trans_path = findLocaleFileInMods(path + DIR_DELIM "mods" DIR_DELIM,
				filename_no_ext);
		break;
	case ContentType::MODPACK:
		trans_path = findLocaleFileInMods(path, filename_no_ext);
		break;
	default:
		trans_path = findLocaleFileWithExtension(trans_path);
		break;
	}

	if (trans_path.empty())
		return nullptr;

	m_last_translations_key = key;
	m_last_translations = {};

	std::string data;
	if (fs::ReadFile(trans_path, data)) {
		m_last_translations.loadTranslation(fs::GetFilenameFromPath(trans_path.c_str()), data);
	}

	return &m_last_translations;
}

/******************************************************************************/
bool GUIEngine::loadMainMenuScript()
{
	// Set main menu path (for core.get_mainmenu_path())
	m_scriptdir = g_settings->get("main_menu_path");
	if (m_scriptdir.empty()) {
		m_scriptdir = porting::path_share + DIR_DELIM + "builtin" + DIR_DELIM + "mainmenu";
	}

	// Load builtin (which will load the main menu script)
	std::string script = porting::path_share + DIR_DELIM "builtin" + DIR_DELIM "init.lua";
	try {
		m_script->loadScript(script);
		m_script->checkSetByBuiltin();
		// Menu script loaded
		return true;
	} catch (const ModError &e) {
		errorstream << "GUIEngine: execution of menu script failed: "
			<< e.what() << std::endl;
	}

	return false;
}

/******************************************************************************/
void GUIEngine::run()
{
	IrrlichtDevice *device = m_rendering_engine->get_raw_device();
	video::IVideoDriver *driver = device->getVideoDriver();

	unsigned int text_height = g_fontengine->getTextHeight();

	// Reset fog color
	{
		video::SColor fog_color;
		video::E_FOG_TYPE fog_type = video::EFT_FOG_LINEAR;
		f32 fog_start = 0;
		f32 fog_end = 0;
		f32 fog_density = 0;
		bool fog_pixelfog = false;
		bool fog_rangefog = false;
		driver->getFog(fog_color, fog_type, fog_start, fog_end, fog_density,
				fog_pixelfog, fog_rangefog);

		driver->setFog(RenderingEngine::MENU_SKY_COLOR, fog_type, fog_start,
				fog_end, fog_density, fog_pixelfog, fog_rangefog);
	}

	const core::dimension2d<u32> initial_screen_size(
			g_settings->getU16("screen_w"),
			g_settings->getU16("screen_h")
		);
	const bool initial_window_maximized = !g_settings->getBool("fullscreen") &&
			g_settings->getBool("window_maximized");
	auto last_window_info = ClientDynamicInfo::getCurrent();

	FpsControl fps_control;
	f32 dtime = 0.0f;

	fps_control.reset();

	auto framemarker = FrameMarker("GUIEngine::run()-frame").started();

	while (m_rendering_engine->run() && !m_startgame && !m_kill) {
		framemarker.end();
		fps_control.limit(device, &dtime);
		framemarker.start();

		g_fontengine->handleReload();

		if (device->isWindowVisible()) {
			// check if we need to update the "upper left corner"-text
			if (text_height != g_fontengine->getTextHeight()) {
				updateTopLeftTextSize();
				text_height = g_fontengine->getTextHeight();
			}
			auto window_info = ClientDynamicInfo::getCurrent();
			if (!window_info.equal(last_window_info)) {
				m_script->handleMainMenuEvent("WindowInfoChange");
				last_window_info = window_info;
			}

			driver->beginScene(true, true, RenderingEngine::MENU_SKY_COLOR);

			if (m_clouds_enabled) {
				drawClouds(dtime);
				drawOverlay(driver);
			} else {
				drawBackground(driver);
			}

			drawFooter(driver);

			m_rendering_engine->get_gui_env()->drawAll();

			// The header *must* be drawn after the menu because it uses
			// GUIFormspecMenu::getAbsoluteRect().
			// The header *can* be drawn after the menu because it never intersects
			// the menu.
			drawHeader(driver);

			driver->endScene();
		}

		m_script->step();

		sound_volume_control(m_sound_manager.get(), device->isWindowActive());
		m_sound_manager->step(dtime);

#ifdef __ANDROID__
		m_menu->getAndroidUIInput();
#endif
	}
	framemarker.end();

	m_script->beforeClose();

	RenderingEngine::autosaveScreensizeAndCo(initial_screen_size, initial_window_maximized);
}

/******************************************************************************/
GUIEngine::~GUIEngine()
{
	g_settings->deregisterAllChangedCallbacks(this);

	// deinitialize script first. gc destructors might depend on other stuff
	infostream << "GUIEngine: Deinitializing scripting" << std::endl;
	m_script.reset();

	m_sound_manager.reset();

	m_irr_toplefttext->remove();
}

/******************************************************************************/
void GUIEngine::drawClouds(float dtime)
{
	g_menuclouds->step(dtime * 3);
	g_menucloudsmgr->drawAll();
}

/******************************************************************************/
void GUIEngine::setFormspecPrepend(const std::string &fs)
{
	if (m_menu) {
		m_menu->setFormspecPrepend(fs);
	}
}


/******************************************************************************/
void GUIEngine::drawBackground(video::IVideoDriver *driver)
{
	v2u32 screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_BACKGROUND].texture;

	/* If no texture, draw background of solid color */
	if(!texture){
		video::SColor color(255,80,58,37);
		core::rect<s32> rect(0, 0, screensize.X, screensize.Y);
		driver->draw2DRectangle(color, rect, NULL);
		return;
	}

	v2u32 sourcesize = texture->getOriginalSize();

	if (m_textures[TEX_LAYER_BACKGROUND].tile)
	{
		v2u32 tilesize(
				MYMAX(sourcesize.X,m_textures[TEX_LAYER_BACKGROUND].minsize),
				MYMAX(sourcesize.Y,m_textures[TEX_LAYER_BACKGROUND].minsize));
		for (unsigned int x = 0; x < screensize.X; x += tilesize.X )
		{
			for (unsigned int y = 0; y < screensize.Y; y += tilesize.Y )
			{
				draw2DImageFilterScaled(driver, texture,
					core::rect<s32>(x, y, x+tilesize.X, y+tilesize.Y),
					core::rect<s32>(0, 0, sourcesize.X, sourcesize.Y),
					NULL, NULL, true);
			}
		}
		return;
	}

	// Chop background image to the smaller screen dimension
	v2u32 bg_size = screensize;
	v2f32 scale(
			(f32) bg_size.X / sourcesize.X,
			(f32) bg_size.Y / sourcesize.Y);
	if (scale.X < scale.Y)
		bg_size.X = (int) (scale.Y * sourcesize.X);
	else
		bg_size.Y = (int) (scale.X * sourcesize.Y);
	v2s32 offset = v2s32(
		(s32) screensize.X - (s32) bg_size.X,
		(s32) screensize.Y - (s32) bg_size.Y
	) / 2;
	/* Draw background texture */
	draw2DImageFilterScaled(driver, texture,
		core::rect<s32>(offset.X, offset.Y, bg_size.X + offset.X, bg_size.Y + offset.Y),
		core::rect<s32>(0, 0, sourcesize.X, sourcesize.Y),
		NULL, NULL, true);
}

/******************************************************************************/
void GUIEngine::drawOverlay(video::IVideoDriver *driver)
{
	v2u32 screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_OVERLAY].texture;

	/* If no texture, draw nothing */
	if(!texture)
		return;

	/* Draw background texture */
	v2u32 sourcesize = texture->getOriginalSize();
	draw2DImageFilterScaled(driver, texture,
		core::rect<s32>(0, 0, screensize.X, screensize.Y),
		core::rect<s32>(0, 0, sourcesize.X, sourcesize.Y),
		NULL, NULL, true);
}

/******************************************************************************/
void GUIEngine::drawHeader(video::IVideoDriver *driver)
{
	core::dimension2d<u32> screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_HEADER].texture;

	// If no texture, draw nothing
	if (!texture)
		return;

	/*
	 * Calculate the maximum rectangle
	 */
	core::rect<s32> formspec_rect = m_menu->getAbsoluteRect();
	// 4 px of padding on each side
	core::rect<s32> max_rect(4, 4, screensize.Width - 8, formspec_rect.UpperLeftCorner.Y - 8);

	// If no space (less than 16x16 px), draw nothing
	if (max_rect.getWidth() < 16 || max_rect.getHeight() < 16)
		return;

	/*
	 * Calculate the preferred rectangle
	 */
	f32 mult = (((f32)screensize.Width / 2.0)) /
			((f32)texture->getOriginalSize().Width);

	v2s32 splashsize(((f32)texture->getOriginalSize().Width) * mult,
			((f32)texture->getOriginalSize().Height) * mult);

	s32 free_space = (((s32)screensize.Height)-320)/2;

	core::rect<s32> desired_rect(0, 0, splashsize.X, splashsize.Y);
	desired_rect += v2s32((screensize.Width/2)-(splashsize.X/2),
			((free_space/2)-splashsize.Y/2)+10);

	/*
	 * Make the preferred rectangle fit into the maximum rectangle
	 */
	// 1. Scale
	f32 scale = std::min((f32)max_rect.getWidth() / (f32)desired_rect.getWidth(),
			(f32)max_rect.getHeight() / (f32)desired_rect.getHeight());
	if (scale < 1.0f) {
		v2s32 old_center = desired_rect.getCenter();
		desired_rect.LowerRightCorner.X = desired_rect.UpperLeftCorner.X + desired_rect.getWidth() * scale;
		desired_rect.LowerRightCorner.Y = desired_rect.UpperLeftCorner.Y + desired_rect.getHeight() * scale;
		desired_rect += old_center - desired_rect.getCenter();
	}

	// 2. Move
	desired_rect.constrainTo(max_rect);

	draw2DImageFilterScaled(driver, texture, desired_rect,
		core::rect<s32>(core::position2d<s32>(0,0),
		core::dimension2di(texture->getOriginalSize())),
		NULL, NULL, true);
}

/******************************************************************************/
void GUIEngine::drawFooter(video::IVideoDriver *driver)
{
	core::dimension2d<u32> screensize = driver->getScreenSize();

	video::ITexture* texture = m_textures[TEX_LAYER_FOOTER].texture;

	/* If no texture, draw nothing */
	if(!texture)
		return;

	f32 mult = (((f32)screensize.Width)) /
			((f32)texture->getOriginalSize().Width);

	v2s32 footersize(((f32)texture->getOriginalSize().Width) * mult,
			((f32)texture->getOriginalSize().Height) * mult);

	// Don't draw the footer if there isn't enough room
	s32 free_space = (((s32)screensize.Height)-320)/2;

	if (free_space > footersize.Y) {
		core::rect<s32> rect(0,0,footersize.X,footersize.Y);
		rect += v2s32(screensize.Width/2,screensize.Height-footersize.Y);
		rect -= v2s32(footersize.X/2, 0);

		draw2DImageFilterScaled(driver, texture, rect,
			core::rect<s32>(core::position2d<s32>(0,0),
			core::dimension2di(texture->getOriginalSize())),
			NULL, NULL, true);
	}
}

/******************************************************************************/
bool GUIEngine::setTexture(texture_layer layer, const std::string &texturepath,
		bool tile_image, unsigned int minsize)
{
	m_textures[layer].texture = nullptr;

	if (texturepath.empty() || !fs::PathExists(texturepath))
		return false;

	m_textures[layer].texture = m_texture_source->getTexture(texturepath);
	m_textures[layer].tile    = tile_image;
	m_textures[layer].minsize = minsize;

	return m_textures[layer].texture != nullptr;
}

/******************************************************************************/
bool GUIEngine::downloadFile(const std::string &url, const std::string &target)
{
#if USE_CURL
	auto target_file = open_ofstream(target.c_str(), true);
	if (!target_file.good())
		return false;

	HTTPFetchRequest fetch_request;
	HTTPFetchResult fetch_result;
	fetch_request.url = url;
	fetch_request.caller = HTTPFETCH_SYNC;
	fetch_request.timeout = std::max(MIN_HTTPFETCH_TIMEOUT,
		(long)g_settings->getS32("curl_file_download_timeout"));
	bool completed = httpfetch_sync_interruptible(fetch_request, fetch_result);

	if (!completed || !fetch_result.succeeded) {
		target_file.close();
		fs::DeleteSingleFileOrEmptyDirectory(target);
		return false;
	}
	// TODO: directly stream the response data into the file instead of first
	// storing the complete response in memory
	target_file << fetch_result.data;

	return true;
#else
	return false;
#endif
}

/******************************************************************************/
void GUIEngine::setTopleftText(const std::string &text)
{
	m_toplefttext = translate_string(utf8_to_wide(text));

	updateTopLeftTextSize();
}

/******************************************************************************/
void GUIEngine::updateTopLeftTextSize()
{
	core::rect<s32> rect(0, 0, g_fontengine->getTextWidth(m_toplefttext.c_str()),
		g_fontengine->getTextHeight());
	rect += v2s32(4, 0);

	m_irr_toplefttext->remove();
	m_irr_toplefttext = gui::StaticText::add(m_rendering_engine->get_gui_env(),
			m_toplefttext, rect, false, true, 0, -1);
}

/******************************************************************************/
void GUIEngine::fullscreenChangedCallback(const std::string &name, void *data)
{
	static_cast<GUIEngine*>(data)->getScriptIface()->handleMainMenuEvent("FullscreenChange");
}
