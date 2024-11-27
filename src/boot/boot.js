var __bitsybox_is_boot_finished__ = false;
var __bitsybox_selected_game__ = "";
var __bitsybox_selected_game_name__ = "";

var __bitsybox_should_start_tune_demo__ = false;

var frameCount = 0;
var selectedGameIndex = 0;
var isButtonUpCounter = 0;
var isButtonDownCounter = 0;
var buttonHoldSlow = 15 * 16;
var buttonHoldFast = 5 * 16;
var buttonHoldMax = buttonHoldSlow;
var transitionFxGroups = [["fade_w", "fade_b"], ["wave", "tunnel"], ["slide_u", "slide_d", "slide_l", "slide_r"]];
var alphanum = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
var scrollX = 0;
var scrollFrameCounter = 0;
var arrowFrame = 0;
var arrowFrameCounter = 0;
var gameListLengthMax = 14;
var isSelectBlinkAnim = false;
var selectBlinkAnimFrameCounter = 0;
var bootAnimRooms = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b"];
var bootTuneTimeSteps = [188, 188, 188, 188, 188, 0, (188 * 4)];
var bootAnimFrameCounter = 0;
var bootAnimFrameMax = 10;
var bootAnimRoomIndex = -1;
var selectMenuRoom = "d";

var isMenuButtonCounter = 0;

// // for testing
// var selectMenuRooms = ["c", "d", "e", "f", "g", "h"];
// var selectMenuIndex = 0;
// var isButtonLeftDown = false;

function bootLoad(gameData, fontData) {
	// bitsy.log(gameData);

	load_game(gameData, fontData, false);

	// fontManager.AddResource(defaultFontName + fontManager.GetExtension(), fontData);
	// initRoom("0");

	// make sure files are .bitsy files (should I do this in main.c?)
	var validGameFiles = []
	for (var i = 0; i < __bitsybox_game_files__.length; i++) {
		var file = __bitsybox_game_files__[i];
		var fileSplit = file.split(".");

		if (fileSplit.length >= 2 && fileSplit[1] === "bitsy") {
			bitsy.log(file);
			validGameFiles.push(file);
		}
	}

	__bitsybox_game_files__ = validGameFiles;
}

// all the boot menu animations are keyed to ~30fps frames
// so we need to track when those should update
var animationFrameDeltaTime = 0;

function bootLoop(dt) {
	// update animation frame counter
	animationFrameDeltaTime += dt;
	var shouldTickAnimationFrames = (animationFrameDeltaTime >= 16);
	var playNextNote = false;

	if (bootAnimRoomIndex < bootAnimRooms.length) {
		// boot animation
		bitsy.graphicsMode(bitsy.GFX_MAP);

		if (bootAnimRoomIndex < 0) {
			bitsy.fill(bitsy.MAP1, 0);
			bitsy.fill(bitsy.MAP2, 0);
		}
		else {
			drawRoom(room[bootAnimRooms[bootAnimRoomIndex]], { redrawAll: true });
		}

		if (shouldTickAnimationFrames) {
			bootAnimFrameCounter++;
		}
		if (bootAnimFrameCounter >= bootAnimFrameMax) {
			bootAnimRoomIndex++;
			bootAnimFrameCounter = 0;

			playNextNote = true;
		}

		if (bootAnimRoomIndex >= bootAnimRooms.length && __bitsybox_game_files__.length > 1) {
			var fxGroup = transitionFxGroups[Math.floor(Math.random() * transitionFxGroups.length)];
			var fx = fxGroup[Math.floor(Math.random() * fxGroup.length)];
			transition.BeginTransition(bootAnimRooms[bootAnimRooms.length - 1], 4, 7, selectMenuRoom, -1, -1, fx);
			transition.OnTransitionComplete(function() { initRoom(selectMenuRoom); });
		}
	}
	else if (transition.IsTransitionActive()) {
		// transition to select menu
		transition.UpdateTransition(dt);
	}
	else if (__bitsybox_game_files__.length <= 0) {
		// no games
		bitsy.graphicsMode(bitsy.GFX_MAP);
		drawRoom(bootAnimRooms[bootAnimRooms.length - 1], { redrawAll: true });
	}
	else if (__bitsybox_game_files__.length == 1) {
		// only one game: play immediately
		__bitsybox_selected_game__ = __bitsybox_game_files__[0];

		var filenameUpper = __bitsybox_selected_game__.split(".")[0].toUpperCase();
		__bitsybox_selected_game_name__ = "";
		for (var i = 0; i < filenameUpper.length; i++) {
			if (alphanum.indexOf(filenameUpper[i]) != -1) {
				__bitsybox_selected_game_name__ += filenameUpper[i];
			}
			else {
				__bitsybox_selected_game_name__ += " ";
			}
		}

		__bitsybox_is_boot_finished__ = true;
	}
	else {
		// select menu
		var pageIndex = Math.floor(selectedGameIndex / gameListLengthMax);
		var gameListStartIndex = pageIndex * gameListLengthMax;
		var gameListLength = Math.min(gameListLengthMax, __bitsybox_game_files__.length - gameListStartIndex);

		bitsy.graphicsMode(bitsy.GFX_MAP);
		drawRoom(room[selectMenuRoom], { redrawAll: true });

		if (shouldTickAnimationFrames) {
			arrowFrameCounter++;
		}
		if (arrowFrameCounter >= 24) {
			arrowFrame = (arrowFrame === 0) ? 1 : 0;
			arrowFrameCounter = 0;
		}

		if (isSelectBlinkAnim) {
			if (shouldTickAnimationFrames) {
				selectBlinkAnimFrameCounter++;
			}
		}

		for (var i = 0; i < gameListLength; i++) {
			var gameIndex = i + gameListStartIndex;
			var isSelected = (gameIndex === selectedGameIndex);
			var y = i + 1;

			// draw arrow
			if (isSelected && !isSelectBlinkAnim) {
				drawTile(getTileFrame(tile["g"], arrowFrame), 1, y);
			}

			var gameFile = __bitsybox_game_files__[gameIndex];
			var gameName = gameFile.split(".")[0].toUpperCase();

			// scroll long game names so they're readable
			if (isSelected && !isSelectBlinkAnim && gameName.length > 13) {
				if (shouldTickAnimationFrames) {
					scrollFrameCounter++;
				}

				var scrollFrameMax = (scrollX === 0) ? 48 : 12;

				if (scrollFrameCounter >= scrollFrameMax) {
					scrollFrameCounter = 0;
					scrollX++;

					if (scrollX > gameName.length) {
						scrollX = 0;
					}
				}
			}

			// only draw the selected game during the blink animation
			if (isSelectBlinkAnim && !isSelected) {
				continue;
			}

			// blink
			if (isSelectBlinkAnim &&
				((selectBlinkAnimFrameCounter > 8 && selectBlinkAnimFrameCounter < 16) ||
				selectBlinkAnimFrameCounter > 24 && selectBlinkAnimFrameCounter < 32)) {
				continue;
			}

			for (var j = 0; j < gameName.length; j++) {
				var char = gameName[j];
				var charTileId = names.tile["CHAR2_" + char];
				if (charTileId) {
					var charDrawing = tile[charTileId];
					charDrawing.col = isSelected ? 2 : 1;
					var x = (isSelected ? 2 - (scrollX) : 1) + j;
					if (x >= (isSelected ? 2 : 1) && x < 15) {
						drawTile(getTileFrame(charDrawing), x, y);
					}
				}
			}
		}

		if (isSelectBlinkAnim) {
			if (selectBlinkAnimFrameCounter > 60) {
				// start after blink animation
				__bitsybox_is_boot_finished__ = true;
			}
		}
		else if (bitsy.button(bitsy.BTN_UP) && (isButtonUpCounter <= 0 || isButtonUpCounter >= buttonHoldMax)) {
			// prev game
			selectedGameIndex = (selectedGameIndex - 1) >= 0 ? (selectedGameIndex - 1) : (__bitsybox_game_files__.length - 1);
			scrollX = 0;
			scrollFrameCounter = 0;
			buttonHoldMax = isButtonUpCounter <= 0 ? buttonHoldSlow : buttonHoldFast;
			isButtonUpCounter = 0;

			soundPlayer.playBlip(blip["2"]);
		}
		else if (bitsy.button(bitsy.BTN_DOWN) && (isButtonDownCounter <= 0 || isButtonDownCounter >= buttonHoldMax)) {
			// next game
			selectedGameIndex = (selectedGameIndex + 1) % __bitsybox_game_files__.length;
			scrollX = 0;
			scrollFrameCounter = 0;
			buttonHoldMax = isButtonDownCounter <= 0 ? buttonHoldSlow : buttonHoldFast;
			isButtonDownCounter = 0;

			soundPlayer.playBlip(blip["2"]);
		}
		else if (bitsy.button(bitsy.BTN_LEFT)) {
			// left does nothing
		}
		else if (bitsy.button(bitsy.BTN_RIGHT) || bitsy.button(bitsy.BTN_OK)) {
			// play game
			__bitsybox_selected_game__ = __bitsybox_game_files__[selectedGameIndex];

			var filenameUpper = __bitsybox_selected_game__.split(".")[0].toUpperCase();
			__bitsybox_selected_game_name__ = "";
			for (var i = 0; i < filenameUpper.length; i++) {
				if (alphanum.indexOf(filenameUpper[i]) != -1) {
					__bitsybox_selected_game_name__ += filenameUpper[i];
				}
				else {
					__bitsybox_selected_game_name__ += " ";
				}
			}

			isSelectBlinkAnim = true;
			scrollX = 0;

			soundPlayer.playBlip(blip["3"]);
		}

		// hold the menu button to start the tool demo
		if (bitsy.button(bitsy.BTN_MENU)) {
			isMenuButtonCounter += dt;

			if (isMenuButtonCounter >= (1000)) {
				__bitsybox_should_start_tune_demo__ = true;
				__bitsybox_is_boot_finished__ = true;
			}
		}
		else {
			isMenuButtonCounter = 0;
		}

		isButtonUpCounter = bitsy.button(bitsy.BTN_UP) ? isButtonUpCounter + 1 : 0;
		isButtonDownCounter = bitsy.button(bitsy.BTN_DOWN) ? isButtonDownCounter + 1 : 0;
	}

	// update audio
	if (bootAnimRoomIndex < bootAnimRooms.length) {		
		// during the boot animation, update the sound player with specific
		// time steps so that the tune matches up with the animation
		if (bootTuneTimeSteps[bootAnimRoomIndex] != undefined) {
			if (playNextNote) {
				soundPlayer.update(bootTuneTimeSteps[bootAnimRoomIndex]);
			}
		}
		else {
			// keep playing after the tune ends to finish the last sound effect
			soundPlayer.update(dt);
		}
	}
	else {
		soundPlayer.update(dt);
	}

	// reset animation frame counter
	if (shouldTickAnimationFrames) {
		animationFrameDeltaTime = 0;
	}
}

var isLoaded = false;

bitsy.loop(function(dt) {
	if (!isLoaded) {
		bitsy.log("boot load");

		bootLoad(bitsy.getGameData(), bitsy.getFontData());

		// start playing the boot tune
		soundPlayer.playTune(tune["1"]);

		isLoaded = true;
	}

	bootLoop(dt);

	return !(__bitsybox_is_boot_finished__);
});