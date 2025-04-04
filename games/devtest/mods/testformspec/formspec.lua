local color = core.colorize

-- \208\176 is a cyrillic small a
local unsafe_url = core.formspec_escape("https://u:p@wikipedi\208\176.org:1233/heIIoll?a=b#c")

local clip_fs = [[
	style_type[label,button,image_button,item_image_button,
			tabheader,scrollbar,table,animated_image
			,field,textarea,checkbox,dropdown;noclip=%c]

	label[0,0;A clipping test]
	button_url[0,1;3,0.8;clip_button;A clipping test;]] .. unsafe_url .. [[]
	image_button[0,2;3,0.8;testformspec_button_image.png;clip_image_button;A clipping test]
	item_image_button[0,3;3,0.8;testformspec:item;clip_item_image_button;A clipping test]
	tabheader[0,4.7;3,0.63;clip_tabheader;Clip,Test,Text,Tabs;1;false;false]
	field[0,5;3,0.8;clip_field;Title;]
	textarea[0,6;3,1;clip_textarea;Title;]
	checkbox[0,7.5;clip_checkbox;This is a test;true]
	dropdown[0,8;3,0.8;clip_dropdown;Select An Item,One,Two,Three,Four,Five;1]
	scrollbar[0,9;3,0.8;horizontal;clip_scrollbar;3]
	tablecolumns[text;text]
	table[0,10;3,1;clip_table;one,two,three,four;1]
	animated_image[-0.5,11;4.5,1;clip_animated_image;testformspec_animation.png;4;100]
]]

local tabheaders_fs = [[
	tabheader[0,0;10,0.63;tabs_opaque;Opaque,Without,Border;1;false;false]
	tabheader[0,1;10,0.63;tabs_opaque_border;Opaque,With,Border;1;false;true]
	tabheader[0,2;10,0.63;tabs_transparent;Transparent,Without,Border;1;true;false]
	tabheader[0,3;10,0.63;tabs_transparent_border;Transparent,With,Border;1;true;true]
	tabheader[0,4;tabs_default;Default,Tabs;1]
	tabheader[0,6;10,0.5;tabs_size1;Height=0.5;1;false;false]
	tabheader[2,6;10,0.75;tabs_size1;Height=0.75;1;false;false]
	tabheader[4,6;10,1;tabs_size2;Height=1;1;false;false]
	tabheader[6,6;10,1.25;tabs_size2;Height=1.25;1;false;false]
	tabheader[8,6;10,1.5;tabs_size2;Height=1.5;1;false;false]
]]

local inv_style_fs = [[
	style_type[list;noclip=true]
	list[current_player;main;-0.75,0.75;2,2]

	real_coordinates[false]
	list[current_player;main;1.5,0;3,2]
	real_coordinates[true]

	real_coordinates[false]
	style_type[list;size=1.1;spacing=0.1]
	list[current_player;main;5,0;3,2]
	real_coordinates[true]

	style_type[list;size=.001;spacing=0]
	list[current_player;main;7,3.5;8,4]

	box[3,3.5;1,1;#000000]
	box[5,3.5;1,1;#000000]
	box[4,4.5;1,1;#000000]
	box[3,5.5;1,1;#000000]
	box[5,5.5;1,1;#000000]
	style_type[list;spacing=.25,.125;size=.75,.875]
	list[current_player;main;3,3.5;3,3]

	style_type[list;spacing=0;size=1.1]
	list[current_player;main;.5,7;8,4]
]]

-- Some textures from textures/base/pack and Devtest, with many different sizes
-- and aspect ratios.
local image_column = "image,0=logo.png,1=crack_anylength.png^[invert:rgb,2=checkbox_16.png," ..
		"3=checkbox_32.png,4=checkbox_64.png,5=default_lava.png," ..
		"6=progress_bar.png,7=progress_bar_bg.png"
local words = {
	"esciunt", "repudiandae", "repellat", "voluptatem", "autem", "vitae", "et",
	"minima", "quasi", "facere", "nihil", "ea", "nemo", "rem", "non", "eos",
	"laudantium", "eveniet", "veritatis",
}

local reseed = math.random(2^31-1)
math.randomseed(1337)

local table_content = {}
for i = 1, 100 do
	table.insert(table_content, words[math.random(#words)])
	table.insert(table_content, words[math.random(#words)])
	table.insert(table_content, words[math.random(#words)])
	table.insert(table_content, math.random(0, 7))
	table.insert(table_content, math.random(0, 7))
	table.insert(table_content, math.random(0, 7))
	table.insert(table_content, words[math.random(#words)])
end

math.randomseed(reseed)

local table_fs = table.concat({
	"tablecolumns[text,align=left;text,align=right;text,align=center;",
			image_column, ",align=left;",
			image_column, ",align=right;",
			image_column, ",align=center;text,align=right]",
	"table[0,0;17,12;the_table;", table.concat(table_content, ","), ";1]"
})

local hypertext_basic = [[A hypertext element
<bigger>Normal test</bigger>
This is a normal text.

<bigger><mono>style</mono> test</bigger>
<style color="#FFFF00">Yellow text.</style> <style color='#FF0000'>Red text.</style>
<style size="24">Size 24.</style> <style size=16>Size 16</style>. <style size=12>Size 12.</style>
<style font="normal">Normal font.</style> <style font=mono>Mono font.</style>

<bigger>Tag test</bigger>
<normal>normal</normal>
<mono>mono</mono>
<b>bold</b>
<i>italic</i>
<u>underlined</u>
<big>big</big>
<bigger>bigger</bigger>
<left>left</left>
<center>center</center>
<right>right</right>
<justify>justify. Here comes a blind text: Lorem testum dolor sit amet consecutor celeron fiftifahivus e shadoninia e smalus jokus anrus relsocutoti rubenwardus. Erasputinus hara holisti dominus wusi. Grumarinsti erltusmuate ol fortitusti fla flo, blani burki e sfani fahif. Ultae ratii, e megus gigae don anonimus. Grinus dimondus krockus e nore. Endus finalus nowus comus endus o blindus tekstus.</justify>

<bigger>Custom tag test</bigger>
<tag name=t_green color=green>
<tag name="t_hover" hovercolor=yellow>
<tag name="t_size" size=24>
<tag name="t_mono" font=mono>
<tag name="t_multi" color=green font=mono size=24>
<t_green>color=green</t_green>
Action: <action name="color"><t_green>color=green</t_green></action>
Action: <action name="hovercolor"><t_hover>hovercolor=yellow</t_hover></action>
Action URL: <action name="open" url="https://example.com/?a=b#c">open URL</action>
<t_size>size=24</t_size>
<t_mono>font=mono</t_mono>
<t_multi>color=green font=mono size=24</t_multi>

<bigger><mono>action</mono> test</bigger>
<action name="action_test">action</action>

<bigger><mono>img</mono> test</bigger>
Normal:
<img name=testformspec_item.png>
<mono>width=48 height=48</mono>:
<img name=testformspec_item.png width=48 height=48>
<mono>float=left</mono>:
<img name=testformspec_item.png float=left>
<mono>float=right</mono>:
<img name=testformspec_item.png float=right>

<bigger><mono>item</mono> test</bigger>
Normal:
<item name=testformspec:node>
<mono>width=48 height=48</mono>
<item name=testformspec:node width=48 height=48>
<mono>angle=30,0,0</mono>:
<item name=testformspec:node angle=30,0,0>
<mono>angle=0,30,0</mono>:
<item name=testformspec:node angle=0,30,0>
<mono>angle=0,0,30</mono>:
<item name=testformspec:node angle=0,0,30>
<mono>rotate=yes</mono>:
<item name=testformspec:node rotate=yes>
<mono>rotate=100,0,0</mono>:
<item name=testformspec:node rotate=100,0,0>
<mono>rotate=0,100,0</mono>:
<item name=testformspec:node rotate=0,100,0>
<mono>rotate=0,0,100</mono>:
<item name=testformspec:node rotate=0,0,100>
<mono>rotate=50,75,100</mono>:
<item name=testformspec:node rotate=50,75,100>
<mono>angle=-30,-45,90 rotate=100,150,-50</mono>:
<item name=testformspec:node angle=-30,-45,90 rotate=100,150,-50>]]

local hypertext_global = [[
<global background=gray margin=20 valign=bottom halign=right color=pink hovercolor=purple size=12 font=mono>
This is a test of the global tag. The parameters are:
background=gray margin=20 valign=bottom halign=right color=pink hovercolor=purple size=12 font=mono
<action name=global>action</action>]]

local hypertext_fs = "hypertext[0,0;11,9;hypertext;"..core.formspec_escape(hypertext_basic).."]"..
	"hypertext[0,9.5;11,2.5;hypertext;"..core.formspec_escape(hypertext_global).."]"

local style_fs = [[
	style[one_btn1;bgcolor=red;textcolor=yellow;bgcolor_hovered=orange;
		bgcolor_pressed=purple]
	button_url_exit[0,0;2.5,0.8;one_btn1;Button;]] .. unsafe_url .. [[]

	style[one_btn2;border=false;textcolor=cyan] ]]..
	"button[0,1.05;2.5,0.8;one_btn2;Text " .. color("#FF0", "Yellow") .. [[]

	style[one_btn3;bgimg=testformspec_button_image.png;bgimg_hovered=testformspec_hovered.png;
		bgimg_pressed=testformspec_pressed.png]
	button[0,2.1;1,1;one_btn3;Border]

	style[one_btn4;bgimg=testformspec_button_image.png;bgimg_hovered=testformspec_hovered.png;
		bgimg_pressed=testformspec_pressed.png;border=false]
	button[1.25,2.1;1,1;one_btn4;NoBor]

	style[one_btn5;bgimg=testformspec_button_image.png;bgimg_hovered=testformspec_hovered.png;
		bgimg_pressed=testformspec_pressed.png;border=false;alpha=false]
	button[0,3.35;1,1;one_btn5;Alph]

	style[one_btn6;border=true]
	image_button[0,4.6;1,1;testformspec_button_image.png;one_btn6;Border]

	style[one_btn7;border=false]
	image_button[1.25,4.6;1,1;testformspec_button_image.png;one_btn7;NoBor]

	style[one_btn8;border=false]
	image_button[0,5.85;1,1;testformspec_button_image.png;one_btn8;Border;false;true;testformspec_pressed.png]

	style[one_btn9;border=true]
	image_button[1.25,5.85;1,1;testformspec_button_image.png;one_btn9;NoBor;false;false;testformspec_pressed.png]

	style[one_btn10;alpha=false]
	image_button[0,7.1;1,1;testformspec_button_image.png;one_btn10;NoAlpha]

	style[one_btn11;alpha=true]
	image_button[1.25,7.1;1,1;testformspec_button_image.png;one_btn11;Alpha]

	style[one_btn12;border=true]
	item_image_button[0,8.35;1,1;testformspec:item;one_btn12;Border]

	style[one_btn13;border=false]
	item_image_button[1.25,8.35;1,1;testformspec:item;one_btn13;NoBor]

	style[one_btn14;border=false;bgimg=testformspec_bg.png;fgimg=testformspec_button_image.png]
	style[one_btn14:hovered;bgimg=testformspec_bg_hovered.png;fgimg=testformspec_hovered.png;textcolor=yellow]
	style[one_btn14:pressed;bgimg=testformspec_bg_pressed.png;fgimg=testformspec_pressed.png;textcolor=blue]
	style[one_btn14:hovered+pressed;textcolor=purple]
	style[one_btn14:focused;textcolor=red]
	image_button[0,9.6;1,1;testformspec_button_image.png;one_btn14;Bg]

	style[one_btn15;border=false;bgcolor=#1cc;bgimg=testformspec_bg.png;bgimg_hovered=testformspec_bg_hovered.png;bgimg_pressed=testformspec_bg_pressed.png]
	item_image_button[1.25,9.6;1,1;testformspec:item;one_btn15;Bg]

	style[one_btn16;border=false;bgimg=testformspec_bg_9slice.png;bgimg_middle=4,6;padding=5,7;fgimg=testformspec_bg.png;fgimg_middle=1]
	style[one_btn16:hovered;bgimg=testformspec_bg_9slice_hovered.png;fgimg=testformspec_bg_hovered.png]
	style[one_btn16:pressed;bgimg=testformspec_bg_9slice_pressed.png;fgimg=testformspec_bg_pressed.png]
	style[one_btn16:focused;bgimg=testformspec_bg_9slice_focused.png;fgimg=testformspec_bg_focused.png]
	image_button[2.5,9.6;2,1;;one_btn16;9-Slice Bg]


	container[2.75,0]

	style[one_tb1;textcolor=Yellow]
	tabheader[0,3;2.5,0.63;one_tb1;Yellow,Text,Tabs;1;false;false]

	style[one_f1;textcolor=yellow]
	field[0,4.25;2.5,0.8;one_f1;Field One;Yellow Text]

	style[one_f2;border=false;textcolor=cyan]
	field[0,5.75;2.5,0.8;one_f2;Field Two;Borderless Cyan Text]

	style[one_f3;textcolor=yellow]
	textarea[0,7.025;2.5,0.8;one_f3;Label;]] ..
		core.formspec_escape("Yellow Text\nLine two") .. [[ ]

	style[one_f4;border=false;textcolor=cyan]
	textarea[0,8.324999999999999;2.5,0.8;one_f4;Label;]] ..
		core.formspec_escape("Borderless Cyan Text\nLine two") .. [[ ]

	container_end[]
]]

local scroll_fs =
	"button[8.5,1;4,1;outside;Outside of container]"..
	"box[1,1;8,6;#00aa]"..
	"scroll_container[1,1;8,6;scrbar;vertical]"..
		"button[0,1;1,1;lorem;Lorem]"..
		"animated_image[0,1;4.5,1;clip_animated_image;testformspec_animation.png;4;100]" ..
		"button[0,10;1,1;ipsum;Ipsum]"..
		"pwdfield[2,2;1,1;lorem2;Lorem]"..
		"list[current_player;main;4,4;1,5;]"..
		"box[2,5;3,2;#ffff00]"..
		"image[1,10;3,2;testformspec_item.png]"..
		"image[3,1;testformspec_item.png]"..
		"item_image[2,6;3,2;testformspec:node]"..
		"label[2,15;bla Bli\nfoo bar]"..
		"item_image_button[2,3;1,1;testformspec:node;itemimagebutton;ItemImageButton]"..
		"tooltip[0,11;3,2;Buz;#f00;#000]"..
		"box[0,11;3,2;#00ff00]"..
		"hypertext[3,13;3,3;;" .. hypertext_basic .. "]" ..
		"hypertext[3,17;3,3;;Hypertext with no scrollbar\\; the scroll container should scroll.]" ..
		"textarea[3,21;3,1;textarea;;More scroll within scroll]" ..
		"container[0,18]"..
			"box[1,2;3,2;#0a0a]"..
			"scroll_container[1,2;3,2;scrbar2;horizontal;0.06]"..
				"button[0,0;6,1;butnest;Nest]"..
				"label[10,0.5;nest]"..
			"scroll_container_end[]"..
			"scrollbar[1,0;3.5,0.3;horizontal;scrbar2;0]"..
		"container_end[]"..
		"dropdown[0,6;2;hmdrpdwn;apple,bulb;1]"..
		"image_button[0,4;2,2;testformspec_button_image.png;imagebutton;bbbbtt;false;true;testformspec_pressed.png]"..
		"box[1,22.5;4,1;#a00a]"..
	"scroll_container_end[]"..
	"scrollbaroptions[max=170]".. -- lowest seen pos is: 0.1*170+6=23 (factor*max+height)
	"scrollbar[7.5,0;0.3,4;vertical;scrbar;0]"..
	"scrollbar[8,0;0.3,4;vertical;scrbarhmmm;0]"..
	"dropdown[0,6;2;hmdrpdwnnn;Outside,of,container;1]"..
	"scroll_container[0,8;10,4;scrbar420;vertical;0.1;2]"..
		"button[0.5,0.5;10,1;;Container with padding=2]"..
		"list[current_player;main;0,5;8,4;]"..
	"scroll_container_end[]"..
	"scrollbar[10.1,8;0.5,4;vertical;scrbar420;0]"..
	-- Buttons for scale comparison
	"button[11,8;1,1;;0]"..
	"button[11,9;1,1;;1]"..
	"button[11,10;1,1;;2]"..
	"button[11,11;1,1;;3]"..
	"button[11,12;1,1;;4]"

--style_type[label;textcolor=green]
--label[0,0;Green]
--style_type[label;textcolor=blue]
--label[0,1;Blue]
--style_type[label;textcolor=;border=true]
--label[1.2,0;Border]
--style_type[label;border=true;bgcolor=red]
--label[1.2,1;Background]
--style_type[label;border=;bgcolor=]
--label[0.75,2;Reset]

local window = {
	sizex = 12,
	sizey = 13,
	positionx = 0.5,
	positiony = 0.5,
	anchorx = 0.5,
	anchory = 0.5,
	paddingx = 0.05,
	paddingy = 0.05
}

local pages = {
	-- Real Coordinates
	[[
		formspec_version[3]
		size[12,13]
		allow_close[false]
		image_button[0,0;1,1;logo.png;rc_image_button_1x1;1x1]
		image_button_exit[1,0;2,2;logo.png;rc_image_button_2x2;2x2 exit]
		button[0,2;1,1;rc_button_1x1;1x1]
		button_exit[1,2;2,2;rc_button_2x2;2x2 exit]
		item_image[0,4;1,1;air]
		item_image[1,4;2,2;air]
		item_image_button[0,6;1,1;testformspec:node;rc_item_image_button_1x1;1x1]
		item_image_button[1,6;2,2;testformspec:node;rc_item_image_button_2x2;2x2]
		field[3,.5;3,.5;rc_field;Field;text]
		pwdfield[6,.5;3,1;rc_pwdfield;Password Field]
		field[3,1;3,1;;Read-Only Field;text]
		textarea[3,2;3,.5;rc_textarea_small;Textarea;text]
		textarea[6,2;3,2;rc_textarea_big;Textarea;text\nmore text]
		textarea[3,3;3,1;;Read-Only Textarea;text\nmore text]
		textlist[3,4;3,2;rc_textlist;Textlist,Perfect Coordinates;1;false]
		tableoptions[highlight=#ABCDEF75;background=#00000055;border=false]
		table[6,4;3,2;rc_table;Table,Cool Stuff,Foo,Bar;2]
		dropdown[3,6;3,1;rc_dropdown_small;This,is,a,dropdown;1]
		dropdown[6,6;3,2;rc_dropdown_big;I,am,a,bigger,dropdown;5]
		image[0,8;3,2;ignore.png]
		box[3,7;3,1;#00A3FF]
		checkbox[3,8;rc_checkbox_1;Check me!;false]
		checkbox[3,9;rc_checkbox_2;Uncheck me now!;true]
		scrollbar[0,11.5;11.5,.5;horizontal;rc_scrollbar_horizontal;500]
		scrollbar[11.5,0;.5,11.5;vertical;rc_scrollbar_vertical;0]
		list[current_player;main;6,8;3,2;1]
		button[9,0;2.5,1;rc_empty_button_1;]
		button[9,1;2.5,1;rc_empty_button_2;]
		button[9,2;2.5,1;rc_empty_button_3;] ]]..
		"label[9,0.5;This is a label.\nLine\nLine\nLine\nEnd]"..
		[[button[9,3;1,1;rc_empty_button_4;]
		vertlabel[9,4;VERT]
		label[10,3;HORIZ]
		tabheader[8,0;6,0.65;rc_tabheader;Tab 1,Tab 2,Tab 3,Secrets;1;false;false]
	]],
	-- Style

		"formspec_version[3]size[12,13]" ..
		("label[0.375,0.375;Styled - %s %s]"):format(
			color("#F00", "red text"),
			color("#77FF00CC", "green text")) ..
		"label[6.375,0.375;Unstyled]" ..
		"box[0,0.75;12,0.1;#999]" ..
		"box[6,0.85;0.1,11.15;#999]" ..
		"container[0.375,1.225]" ..
		style_fs ..
		"container_end[]container[6.375,1.225]" ..
		style_fs:gsub("one_", "two_"):gsub("style%[[^%]]+%]", ""):gsub("style_type%[[^%]]+%]", "") ..
		"container_end[]",

	-- Noclip
		"formspec_version[3]size[12,13]" ..
		"label[0.1,0.5;Clip]" ..
		"container[-2.5,1]" .. clip_fs:gsub("%%c", "false") .. "container_end[]" ..
		"label[11,0.5;Noclip]" ..
		"container[11.5,1]" .. clip_fs:gsub("%%c", "true") .. "container_end[]",

	-- Table
		"size[18,13]real_coordinates[true]" ..
		"container[0.5,0.5]" .. table_fs.. "container_end[]",

	-- Hypertext
		"size[12,13]real_coordinates[true]" ..
		"container[0.5,0.5]" .. hypertext_fs .. "container_end[]",

	-- Tabheaders
		"size[12,13]real_coordinates[true]" ..
		"container[0.5,1.5]" .. tabheaders_fs .. "container_end[]",

	-- Inv
		"size[12,13]real_coordinates[true]" .. inv_style_fs,

	-- Window
		function()
			return "formspec_version[3]" ..
				string.format("size[%s,%s]position[%s,%s]anchor[%s,%s]padding[%s,%s]",
					window.sizex, window.sizey, window.positionx, window.positiony,
					window.anchorx, window.anchory, window.paddingx, window.paddingy) ..
				string.format("field[0.5,0.5;2.5,0.5;sizex;X Size;%s]field[3.5,0.5;2.5,0.5;sizey;Y Size;%s]" ..
					"field[0.5,1.5;2.5,0.5;positionx;X Position;%s]field[3.5,1.5;2.5,0.5;positiony;Y Position;%s]" ..
					"field[0.5,2.5;2.5,0.5;anchorx;X Anchor;%s]field[3.5,2.5;2.5,0.5;anchory;Y Anchor;%s]" ..
					"field[0.5,3.5;2.5,0.5;paddingx;X Padding;%s]field[3.5,3.5;2.5,0.5;paddingy;Y Padding;%s]" ..
					"button[2,4.5;2.5,0.5;submit_window;Submit]",
					window.sizex, window.sizey, window.positionx, window.positiony,
					window.anchorx, window.anchory, window.paddingx, window.paddingy) ..
				"field_close_on_enter[sizex;false]field_close_on_enter[sizey;false]" ..
				"field_close_on_enter[positionx;false]field_close_on_enter[positiony;false]" ..
				"field_close_on_enter[anchorx;false]field_close_on_enter[anchory;false]" ..
				"field_close_on_enter[paddingx;false]field_close_on_enter[paddingy;false]"
		end,

	-- Animation
		[[
			formspec_version[6]
			size[12,13]
			animated_image[0.5,0.5;1,1;;testformspec_animation.png;4;100]
			animated_image[0.5,1.75;1,1;;testformspec_animation.jpg;4;100]
			animated_image[1.75,0.5;1,1;;testformspec_animation.png;100;100]
			animated_image[3,0.5;1,1;ani_img_1;testformspec_animation.png;4;1000]
			image[0.5,3;1,1;testformspec_bg.png;1]
			animated_image[0.5,4.25;1,1;;[combine:16x48:0,0=testformspec_bg.png:0,16=testformspec_bg_hovered.png:0,32=testformspec_bg_pressed.png;3;250;1;1]
			image[0.5,5.5;2,1;testformspec_9slice.png;16,0,-16,-16]
			animated_image[2.75,5.5;1.5,0.5;;[combine:300x140:0,0=testformspec_9slice.png:0,70=(testformspec_9slice.png^[transformFX);2;500;1;16,0,-16,-16]
			button[4.25,0.5;1,1;ani_btn_1;Current
Number]
			animated_image[3,1.75;1,1;ani_img_2;testformspec_animation.png;4;1000;2]
			button[4.25,1.75;1,1;ani_btn_2;Current
Number]
			animated_image[3,3;1,1;;testformspec_animation.png;4;0]
			animated_image[3,4.25;1,1;;testformspec_animation.png;4;0;3]
			animated_image[5.5,0.5;5,2;;testformspec_animation.png;4;100]
			animated_image[5.5,2.75;5,2;;testformspec_animation.jpg;4;100]

		]],

	-- Model
		[[
			formspec_version[3]
			size[12,13]
			style[m1;bgcolor=black]
			style[m2;bgcolor=black]
			label[5,1;all defaults]
			label[5,5.1;angle = 0, 180
continuous = false
mouse control = false
frame loop range = 0,30]
			label[5,9.2;continuous = true
mouse control = true]
			model[0.5,0.1;4,4;m1;testformspec_character.b3d;testformspec_character.png]
			model[0.5,4.2;4,4;m2;testformspec_character.b3d;testformspec_character.png;0,180;false;false;0,30]
			model[0.5,8.3;4,4;m3;testformspec_chest.obj;default_chest_top.png,default_chest_top.png,default_chest_side.png,default_chest_side.png,default_chest_front.png,default_chest_inside.png;30,1;true;true]
		]],

	-- Scroll containers
		"formspec_version[7]size[12,13]" ..
		scroll_fs,

	-- Sound
		[[
			formspec_version[3]
			size[12,13]
			style[snd_btn;sound=soundstuff_mono]
			style[snd_ibtn;sound=soundstuff_mono]
			style[snd_drp;sound=soundstuff_mono]
			style[snd_chk;sound=soundstuff_mono]
			style[snd_tab;sound=soundstuff_mono]
			button[0.5,0.5;2,1;snd_btn;Sound]
			image_button[0.5,2;2,1;testformspec_item.png;snd_ibtn;Sound]
			dropdown[0.5,4;4;snd_drp;Sound,Two,Three;]
			checkbox[0.5,5.5.5;snd_chk;Sound;]
			tabheader[0.5,7;8,0.65;snd_tab;Soundtab1,Soundtab2,Soundtab3;1;false;false]
		]],

	-- Background
		[[
			formspec_version[3]
			size[12,13]
			box[0,0;12,13;#f0f1]
			background[0,0;0,0;testformspec_bg.png;true]
			box[3.9,2.9;6.2,4.2;#d00f]
			scroll_container[4,3;6,4;scrbar;vertical]
				background9[1,0.5;0,0;testformspec_bg_9slice.png;true;4,6]
				label[0,0.2;Backgrounds are not be applied to scroll containers,]
				label[0,0.5;but to the whole form.]
			scroll_container_end[]
			scrollbar[3.5,3;0.3,4;vertical;scrbar;0]
			container[2,11]
				box[-0.1,0.5;3.2,1;#fff5]
				background[0,0;2,3;testformspec_bg.png;false]
				background9[1,0;2,3;testformspec_bg_9slice.png;false;4,6]
			container_end[]
		]],

	-- Unsized
		[[
			formspec_version[3]
			background9[0,0;0,0;testformspec_bg_9slice.png;true;4,6]
			background[1,1;0,0;testformspec_bg.png;true]
		]],
}

local page_id = 2
local function show_test_formspec(pname)
	local page = pages[page_id]
	if type(page) == "function" then
		page = page()
	end

	local fs = page .. "tabheader[0,0;11,0.65;maintabs;Real Coord,Styles,Noclip,Table,Hypertext,Tabs,Invs,Window,Anim,Model,ScrollC,Sound,Background,Unsized;" .. page_id .. ";false;false]"

	core.show_formspec(pname, "testformspec:formspec", fs)
end

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname ~= "testformspec:formspec" then
		return false
	end

	if fields.maintabs then
		page_id = tonumber(fields.maintabs)
		show_test_formspec(player:get_player_name())
		return true
	end

	if fields.ani_img_1 and fields.ani_btn_1 then
		core.chat_send_player(player:get_player_name(), "ani_img_1 = " .. tostring(fields.ani_img_1))
		return true
	elseif fields.ani_img_2 and fields.ani_btn_2 then
		core.chat_send_player(player:get_player_name(), "ani_img_2 = " .. tostring(fields.ani_img_2))
		return true
	end

	if fields.hypertext then
		core.chat_send_player(player:get_player_name(), "Hypertext action received: " .. tostring(fields.hypertext))
		return true
	end

	for name, value in pairs(fields) do
		if window[name] then
			print(name, window[name])
			local num_val = tonumber(value) or 0

			if name == "sizex" and num_val < 4 then
				num_val = 6.5
			elseif name == "sizey" and num_val < 5 then
				num_val = 5.5
			end

			window[name] = num_val
			print(name, window[name])
		end
	end

	if fields.submit_window then
		show_test_formspec(player:get_player_name())
	end

	if fields.try_quit then
		core.chat_send_player(player:get_player_name(), "Quit attempt received")
	end
end)

core.register_chatcommand("test_formspec", {
	params = "",
	description = "Open the test formspec",
	func = function(name)
		if not core.get_player_by_name(name) then
			return false, "You need to be online!"
		end

		show_test_formspec(name)
		return true
	end,
})
