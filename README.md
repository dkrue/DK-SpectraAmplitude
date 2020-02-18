# DK-SpectraAmplitude
_SpectraAmplitude_ is a real-time audio spectrum visualizer with music-reactive effects. The front spectrum display analyzes 16 bands of audio, from sub-bass to 20,000Hz. The OLED module on top displays a continuous audio amplitude waveform. 

![Spectra Amplitude Visualizer Front](/images/spectra_amplitude_front.jpg)

## About
This is one of my _[Spectra](https://github.com/search?q=user%3Adkrue+spectra)_ series audio visualizer projects. This project is available prebuilt in my 
[Etsy Store: Circuits & Sawdust](https://www.etsy.com/listing/557709484/amplitude-spectrum-peak-waveform).

## Goal
My main goal with this project was to build an audio analyzer project with cheaper, more commonly available parts than my original [Spectra Bloom](https://github.com/dkrue/DK-SpectraBloom) project. The _Amplitude_ project uses two red 8x8 LED matrices instead of 3-color matrices, and an inexpensive OLED display instead of Neopixel Jewels on top.

The biggest challenge I had with this project is fitting all the code into program memory.  The audio analysis libraries, graphics libraries, and OLED LCD libraries are too big to fit on a typical Arduino together. I had to write my own OLED LCD drawing methods from scratch since I didn't need text support on the display anyway. Figuring out the correct byte arrays to write to the display based on audio amplitude was key. I modified the [Didel OLED library](https://www.didel.com/OledLib.pdf) to draw the amplitude lines with minimal overhead.

![Spectra Amplitude Visualizer In-Progress Comparison](/images/spectra_amplitude_in_progress.jpg)

## How it works
This is an [Adafruit Pro Trinket](https://www.adafruit.com/product/2000) (5V) based project. The Pro Trinket is great because it's cheap but still offers an analog reference pin to tie line-level audio into. It's 16MHz and fast enough to analyze 16 bands of FFT audio, but it has no serial to USB chip, so I recommend breadboarding this project out on an Arduino Uno. You can use the serial output for debugging, and then transfer everything to the Pro Trinket. Or you could use something like the [Adafruit Metro Mini](https://www.adafruit.com/product/2590) for a similar cost.

The original inspiration for this is the [Adafruit Piccolo](https://learn.adafruit.com/piccolo/overview) project. Rather than rely on a variable-level microphone input for the source audio, I addded a 3.5mm audio input jack with voltage divider circuit to analyze line-level audio. A second 3.5mm output jack is tied to the input jack to pass through audio to other modules.  I found this [spectrum analyzer Instructables](https://www.instructables.com/id/Arduino-Spectrum-Analyzer-on-a-10x10-RGB-LED-Matri/) to be helpful for information on how to wire in audio jacks.

## Ingredients
This project uses generic Chinese parts available on eBay, aside from the Adafruit Trinket microcontroller.

- Two red square pixel LED matrix with I2C backpacks
- 128x32 white OLED LCD module (SSD1306 type with HT16K33 I2C backpack)
- [Adafruit Pro Trinket](https://www.adafruit.com/product/2000)
- A shiny [upcycled project enclosure](https://www.ebay.com/itm/292067232173)

![Spectra Amplitude Visualizer Side](/images/spectra_amplitude_side.jpg)
