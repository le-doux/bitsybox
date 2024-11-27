// test setting graphics mode
bitsy.graphicsMode(bitsy.GFX_VIDEO);

// test palette
bitsy.color(0, 0, 0, 0); // black
bitsy.color(1, 255, 0, 0); // red
bitsy.color(2, 0, 255, 0); // green
bitsy.color(3, 0, 0, 255); // blue
bitsy.color(4, 255, 255, 255); // white

// test fill
bitsy.fill(bitsy.VIDEO, 0);
var pixelIndex = 0;

// test tile allocation
bitsy.graphicsMode(bitsy.GFX_MAP);
var t1 = bitsy.tile();
bitsy.fill(t1, 1);
var t2 = bitsy.tile();
bitsy.fill(t2, 2);
var t3 = bitsy.tile();
bitsy.fill(t3, 3);

// test tile delete
bitsy.log("tiles " + t3);
var t4 = bitsy.tile();
bitsy.log("add tile " + t4);
var t5 = bitsy.tile();
bitsy.log("add tile " + t5);
bitsy.delete(t4);
var t6 = bitsy.tile();
bitsy.log("add tile after delete " + t6);

// test setting tile pixels
bitsy.set(t1, 0, 4);
bitsy.set(t1, (bitsy.TILE_SIZE) - 1, 4);
bitsy.set(t1, (bitsy.TILE_SIZE * bitsy.TILE_SIZE) - 1, 4);

// test tile drawing
bitsy.fill(bitsy.MAP1, 0);
bitsy.set(bitsy.MAP1, 0, t1);
bitsy.set(bitsy.MAP1, (bitsy.MAP_SIZE) - 1, t2);
bitsy.set(bitsy.MAP1, (bitsy.MAP_SIZE * bitsy.MAP_SIZE) - 1, t3);

// test textbox
bitsy.textbox(true, 8, 0, 200, 100);
bitsy.fill(bitsy.TEXTBOX, 4);
bitsy.set(bitsy.TEXTBOX, (200 * 0) + 0, 0);
bitsy.set(bitsy.TEXTBOX, (200 * 1) + 1, 0);
bitsy.set(bitsy.TEXTBOX, (200 * 2) + 2, 0);
bitsy.set(bitsy.TEXTBOX, (200 * 3) + 3, 0);
// bitsy.textMode(bitsy.TXT_LOREZ);

// input variables
var isUp = false
var isDown = false;
var isOk = false;

// audio test variables
var freq = 44000;
var pulse = bitsy.PULSE_1_2;
var channel = bitsy.SOUND1;

var isVideoTest = false;

function demo(dt) {
	if (isVideoTest) {
		bitsy.log('TEST DEMO ' + pixelIndex);

		// immediately fill screen with different colors
		if (bitsy.button(bitsy.BTN_UP)) {
			bitsy.fill(bitsy.VIDEO, 0);
		}
		else if (bitsy.button(bitsy.BTN_DOWN)) {
			bitsy.fill(bitsy.VIDEO, 1);
		}
		else if (bitsy.button(bitsy.BTN_LEFT)) {
			bitsy.fill(bitsy.VIDEO, 2);
		}
		else if (bitsy.button(bitsy.BTN_RIGHT)) {
			bitsy.fill(bitsy.VIDEO, 3);
		}

		// slowly fill screen pixel by pixel with white
		bitsy.set(bitsy.VIDEO, pixelIndex, 4);
		pixelIndex = (pixelIndex + 1) % (bitsy.VIDEO_SIZE * bitsy.VIDEO_SIZE);		
	}
	else {
		// audio test (in tile mode)
		if (bitsy.button(bitsy.BTN_UP) && !isUp) {
			freq = freq * 2;
		}
		else if (bitsy.button(bitsy.BTN_DOWN) && !isDown) {
			freq = freq / 2;
		}
		
		if (bitsy.button(bitsy.BTN_LEFT)) {
			channel = bitsy.SOUND1;
			pulse = bitsy.PULSE_1_2;
		}
		else if (bitsy.button(bitsy.BTN_RIGHT)) {
			channel = bitsy.SOUND2;
			pulse = bitsy.PULSE_1_8;
		}

		if (bitsy.button(bitsy.BTN_OK) && !isOk) {
			bitsy.sound(channel, 2000, freq, 7, pulse);
		}
	}

	isUp = bitsy.button(bitsy.BTN_UP);
	isDown = bitsy.button(bitsy.BTN_DOWN);
	isOk = bitsy.button(bitsy.BTN_OK);
}

bitsy.loop(demo);