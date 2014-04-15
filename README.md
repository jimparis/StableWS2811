StableWS2811 by Jim Paris
-------------------------

Based on
[OctoWS2811](https://www.pjrc.com/teensy/td_libs_OctoWS2811.html), and
requires a Teensy 3.1.

Benefits:

* No jitter at all in timing output, which solves flickering problems
  with newer WS2812B chips
* Low CPU usage
* Unaffected by other running code and interrupts, including USB

Drawbacks:

* Only one single output, versus OctoWS2811's eight parallel outputs
* To drive N LEDs, requires (27 * N) bytes of RAM
* Uses SPI0

Usage:

* Copy to Arduino `libraries` directory
* Restart Arduino IDE
* File → Examples → StableWS2811 → FlickerTest
* Adjust `stripLen` and connect your strip to Teensy pin 7 (OctoWS2811 adapter output #3)
