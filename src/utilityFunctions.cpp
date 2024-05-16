#include "./utilityFunctions.hpp"

// Makes the haptic motor buzz
void vibrate() {
	// set the effect to play
    watch.setWaveform(0, 14);  // play effect
    // play the effect
    watch.run();
}