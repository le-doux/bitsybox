var __bitsybox_is_boot_finished__ = false;
var __bitsybox_selected_game__ = "";
var __bitsybox_selected_game_name__ = "";

var frameCount = 0;
var selectedGameIndex = 0;
var isButtonUpCounter = 0;
var isButtonDownCounter = 0;
var buttonHoldMax = 30;
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
var bootAnimFrameCounter = 0;
var bootAnimFrameMax = 10;
var bootAnimRoomIndex = -1;
var selectMenuRoom = "d";

// // for testing
// var selectMenuRooms = ["c", "d", "e", "f", "g", "h"];
// var selectMenuIndex = 0;
// var isButtonLeftDown = false;

bitsyOnLoad(function(gameData, fontData) {
	parseWorld(gameData);
	fontManager.AddResource(defaultFontName + fontManager.GetExtension(), fontData);
	initRoom("0");

	// make sure files are .bitsy files (should I do this in main.c?)
	var validGameFiles = []
	for (var i = 0; i < __bitsybox_game_files__.length; i++) {
		var file = __bitsybox_game_files__[i];
		var fileSplit = file.split(".");
		if (fileSplit.length >= 2 && fileSplit[1] === "bitsy") {
			bitsyLog(file);
			validGameFiles.push(file);
		}
	}
	__bitsybox_game_files__ = validGameFiles;
});

bitsyOnUpdate(function() {
	var curTime = Date.now();
	deltaTime = curTime - prevTime;

	if (bootAnimRoomIndex < bootAnimRooms.length) {
		// boot animation
		bitsySetGraphicsMode(1);

		if (bootAnimRoomIndex < 0) {
			bitsyDrawBegin(0);
			bitsyClear(tileColorStartIndex);
			bitsyDrawEnd();
		}
		else {
			drawRoom(room[bootAnimRooms[bootAnimRoomIndex]]);
		}

		bootAnimFrameCounter++;
		if (bootAnimFrameCounter >= bootAnimFrameMax) {
			bootAnimRoomIndex++;
			bootAnimFrameCounter = 0;
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
		transition.UpdateTransition(deltaTime);
	}
	else if (__bitsybox_game_files__.length <= 0) {
		// no games
		bitsySetGraphicsMode(1);
		drawRoom(bootAnimRooms[bootAnimRooms.length - 1]);
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

		// bitsyLog(gameListStartIndex + ", " + gameListLength);

		bitsySetGraphicsMode(1);
		drawRoom(room[selectMenuRoom]);

		arrowFrameCounter++;
		if (arrowFrameCounter >= 24) {
			arrowFrame = (arrowFrame === 0) ? 1 : 0;
			arrowFrameCounter = 0;
		}

		if (isSelectBlinkAnim) {
			selectBlinkAnimFrameCounter++;
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
				scrollFrameCounter++;

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
		else if (bitsyGetButton(0) && (isButtonUpCounter <= 0 || isButtonUpCounter >= buttonHoldMax)) {
			// prev game
			selectedGameIndex = (selectedGameIndex - 1) >= 0 ? (selectedGameIndex - 1) : (__bitsybox_game_files__.length - 1);
			scrollX = 0;
			scrollFrameCounter = 0;
			buttonHoldMax = isButtonUpCounter <= 0 ? 30 : 10;
			isButtonUpCounter = 0;
		}
		else if (bitsyGetButton(1) && (isButtonDownCounter <= 0 || isButtonDownCounter >= buttonHoldMax)) {
			// next game
			selectedGameIndex = (selectedGameIndex + 1) % __bitsybox_game_files__.length;
			scrollX = 0;
			scrollFrameCounter = 0;
			buttonHoldMax = isButtonDownCounter <= 0 ? 30 : 10;
			isButtonDownCounter = 0;
		}
		else if (bitsyGetButton(2)) {
			// left does nothing
		}
		else if (bitsyGetButton(3) || bitsyGetButton(4)) {
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
		}

		isButtonUpCounter = bitsyGetButton(0) ? isButtonUpCounter + 1 : 0;
		isButtonDownCounter = bitsyGetButton(1) ? isButtonDownCounter + 1 : 0;

		// // testing diff menu styles
		// if (bitsyGetButton(2) && !isButtonLeftDown) {
		// 	selectMenuIndex = ((selectMenuIndex + 1) % selectMenuRooms.length);
		// 	selectMenuRoom = selectMenuRooms[selectMenuIndex];
		// }
		// isButtonLeftDown = bitsyGetButton(2);
	}
});

bitsyOnQuit(function() {
	bitsyResetColors();
	bitsyResetTiles();
});