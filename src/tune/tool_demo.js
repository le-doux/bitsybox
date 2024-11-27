bitsy.log("~ initializing tool demo ~");

// bitsybox interop
var __bitsybox_should_quit_tool_demo__ = false;

// global settings
var isPlayMode = false;
var tilesize = 8;
function refreshGameData() {};

// mouse input
var enableTooltipLog = false;
var enableMouseInput = true;
var mouse = {
	hover: function() { return enableMouseInput && __bitsybox_mouse_hover__; },
	down: function() { return enableMouseInput && __bitsybox_mouse_down__; },
	pos: function() { return { x: __bitsybox_mouse_x__, y: __bitsybox_mouse_y__, }; },
	alt: function() { return enableMouseInput && __bitsybox_mouse_alt__; },
	tooltip: function(text) {
		if (enableTooltipLog) {
			bitsy.log("tooltip: " + text);
		}
	},
}

// localization system stub
var localization = {
	GetStringOrFallback(id, fallback) { return fallback; },
};

// menu system
var dialogModule = new Dialog();
var dialogRenderer = dialogModule.CreateRenderer();
var dialogBuffer = dialogModule.CreateBuffer();
var fontManager = new FontManager();

var isMenuActive = false;
var menuControls = [];
var menuItemIndex = 0;
var menuString = "";
var cursorClickAnimCounter = 0;

var menu = {
	push: function(control) {
		// skip groups because they're non-interactive
		if (control.control != "group") {
			menuControls.push(control)
		}
	},
	pop: function() {
		// TODO
	},
};

// input state
var isUpButtonHeld = false
var isDownButtonHeld = false;
var isLeftButtonHeld = false;
var isRightButtonHeld = false;
var isOkButtonHeld = false;
var isMenuButtonHeld = false;

// intialize tool object
var tool = {
	world: null,
	mouse: mouse,
	menu: menu,
	renderer: new TileRenderer("TOOL_DEMO"),
};

function stringifyMenu(menuControls) {
	var visited = [];

	return JSON.stringify(menuControls, function(key, value) {
		// skip already visited objects to avoid cycles
		if (value != null && typeof(value) === "object") {
			if (visited.indexOf(value) >= 0) {
				return;
			}

			visited.push(value);
		}

		return value;
	});
}

// update loop
bitsy.loop(function(dt) {
	if (!tool.world) {
		// parse tool data
		tool.world = parseWorld(bitsy.getGameData());

		// give drawing data to the renderer
		tool.renderer.SetDrawings(tool.world.drawings);

		// hack: copy tunes from the tool data to global data so we have something to play in the demo
		tune = tool.world.tune;

		// select tune 0
		tool.onSelect("0");

		// load default font
		var fontData = bitsy.getFontData();
		fontManager.AddResource(tool.world.fontName + fontManager.GetExtension(), fontData);
		var font = fontManager.Get(tool.world.fontName);

		// initialize dialog system
		dialogBuffer.Reset();
		dialogBuffer.SetFont(font);
		dialogRenderer.SetFont(font);
		dialogBuffer.SetPixelsPerRow(dialogRenderer.GetPixelsPerRow());
	}

	enableMouseInput = true;

	// if it's active, update the tool menu
	if (isMenuActive && tool.menuUpdate) {
		// disable the mouse while the menu is open
		enableMouseInput = false;

		// update menu state
		var prevMenuString = menuString;
		menuControls = [];
		tool.menuUpdate(dt);
		menuString = stringifyMenu(menuControls);

		// update menu input
		var prevMenuItemIndex = menuItemIndex;
		if (bitsy.button(bitsy.BTN_UP) && !isUpButtonHeld && menuItemIndex > 0) {
			menuItemIndex--;
		}
		else if (bitsy.button(bitsy.BTN_DOWN) && !isDownButtonHeld && menuItemIndex < (menuControls.length - 1)) {
			menuItemIndex++;
		}

		var didCursorClick = false;
		if ((bitsy.button(bitsy.BTN_RIGHT) && !isRightButtonHeld) || (bitsy.button(bitsy.BTN_OK) && !isOkButtonHeld)) {
			var selectedControl = menuControls[menuItemIndex];
			if (selectedControl.control === "button") {
				if (selectedControl.onclick) {
					selectedControl.onclick();
				}

				didCursorClick = true;
			}
			else if (selectedControl.control === "toggle") {
				var nextChecked = !selectedControl.checked;
				if (selectedControl.onclick) {
					var event = { target: { checked: nextChecked, }, };
					selectedControl.onclick(event);						
				}

				didCursorClick = true;
			}
			else if (selectedControl.control === "select") {
				var nextValue = (selectedControl.value + 1) % selectedControl.options.length;
				if (selectedControl.onchange) {
					var event = { target: { value: nextValue, }, };
					selectedControl.onchange(event);
				}

				didCursorClick = true;
			}
		}
		else if (bitsy.button(bitsy.BTN_LEFT) && !isLeftButtonHeld) {
			var selectedControl = menuControls[menuItemIndex];
			if (selectedControl.control === "select") {
				var nextValue = (selectedControl.value - 1);
				if (nextValue < 0) {
					nextValue = selectedControl.options.length - 1;
				}
				if (selectedControl.onchange) {
					var event = { target: { value: nextValue, }, };
					selectedControl.onchange(event);
				}

				didCursorClick = true;
			}
		}

		// keep the menu index in range
		if (menuItemIndex < 0) {
			menuItemIndex = 0;
		}
		else if (menuItemIndex >= menuControls.length) {
			menuItemIndex = menuControls.length - 1;
		}

		// update click animation counter
		var didCursorChange = false;
		if (didCursorClick) {
			cursorClickAnimCounter = 100;
			didCursorChange = true;
		}
		else if (cursorClickAnimCounter > 0) {			
			cursorClickAnimCounter -= dt;
			if (cursorClickAnimCounter <= 0) {
				cursorClickAnimCounter = 0;
				didCursorChange = true;
			}
		}

		// update menu UI
		if (menuString != prevMenuString || prevMenuItemIndex != menuItemIndex || didCursorChange)
		{
			dialogRenderer.Reset();
			dialogRenderer.SetCentered(true);
			dialogBuffer.Reset();

			for (var i = 0; i < menuControls.length; i++) {
				var control = menuControls[i];

				if (i == menuItemIndex) {
					dialogBuffer.AddDrawing(cursorClickAnimCounter > 0 ? "SPR_c" : "SPR_b", tool.renderer);
					dialogBuffer.Skip();
				}

				dialogBuffer.AddText(" ");
				dialogBuffer.Skip();

				if (control.control === "toggle" && control.checked != undefined && control.checked) {
					dialogBuffer.AddText("[ ");
					dialogBuffer.Skip();
				}

				if (control.icon != undefined) {
					var iconTileName = "icon_" + control.icon;
					var iconTileId = tool.world.names.tile[iconTileName];
					dialogBuffer.AddDrawing("TIL_" + iconTileId, tool.renderer);
					dialogBuffer.Skip();

					dialogBuffer.AddText(" ");
					dialogBuffer.Skip();
				}

				if (control.text != undefined) {
					dialogBuffer.AddText(control.text + " ");
					dialogBuffer.Skip();
				}

				if (control.value != undefined) {
					if (control.control === "select") {
						dialogBuffer.AddText("< ");
						dialogBuffer.Skip();

						var selectedOption;
						for (var j = 0; j < control.options.length; j++) {
							var controlOption = control.options[j];
							if (controlOption.value === control.value) {
								selectedOption = controlOption;
							}
						}

						if (selectedOption) {
							if (selectedOption.icon != undefined) {
								var iconTileName = "icon_" + selectedOption.icon;
								var iconTileId = tool.world.names.tile[iconTileName];
								dialogBuffer.AddDrawing("TIL_" + iconTileId, tool.renderer);
								dialogBuffer.Skip();

								dialogBuffer.AddText(" ");
								dialogBuffer.Skip();
							}

							if (selectedOption.text != undefined) {
								dialogBuffer.AddText(selectedOption.text);
								dialogBuffer.Skip();
							}
						}

						dialogBuffer.AddText(" >");
						dialogBuffer.Skip();
					}
					else {
						dialogBuffer.AddText("(" + control.value + ")");
						dialogBuffer.Skip();
					}
				}

				if (control.control === "toggle" && control.checked != undefined && control.checked) {
					dialogBuffer.AddText(" ]");
					dialogBuffer.Skip();
				}

				dialogBuffer.AddLinebreak();
				dialogBuffer.Skip();
			}
		}

		// update text rendering
		if (dialogBuffer.IsActive()) {
			dialogRenderer.Draw(dialogBuffer, dt);
			dialogBuffer.Update(dt);
		}
	}

	// update tool main loop
	if (tool.loop) {
		tool.loop(dt);
	}

	// TODO : need a new way to quit the tool demo now that I'm using the menu button for this
	if (bitsy.button(bitsy.BTN_MENU) && !isMenuButtonHeld) {
		isMenuActive = !isMenuActive;

		menuControls = [];

		// reset dialog buffer (is this necessary?)
		dialogRenderer.Reset();
		dialogRenderer.SetCentered(true);
		dialogBuffer.Reset();

		// kind of a hack to just directly change textbox rendering state...
		bitsy.textbox(isMenuActive);
	}

	isUpButtonHeld = bitsy.button(bitsy.BTN_UP);
	isDownButtonHeld = bitsy.button(bitsy.BTN_DOWN);
	isLeftButtonHeld = bitsy.button(bitsy.BTN_LEFT);
	isRightButtonHeld = bitsy.button(bitsy.BTN_RIGHT);
	isOkButtonHeld = bitsy.button(bitsy.BTN_OK);
	isMenuButtonHeld = bitsy.button(bitsy.BTN_MENU);
});