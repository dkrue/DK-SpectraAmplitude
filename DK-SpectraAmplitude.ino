/*
Hardware requirements:
 - Most Arduino or Arduino-compatible boards (ATmega 328P or better).
 - Adafruit Bicolor LED Matrix with I2C Backpack (ID: 902)

Software requirements:
 - elm-chan's ffft library for Arduino

Connections:
 - 3.3V to mic amp+ and Arduino AREF pin <-- important!
 - GND to mic amp-
 - Analog pin 0 to mic amp output
 - +5V, GND, SDA (or analog 4) and SCL (analog 5) to I2C Matrix backpack

Written by Adafruit Industries.  Distributed under the BSD license --
see license.txt for more information.  This paragraph must be included
in any redistribution.

ffft library is provided under its own terms -- see ffft.S for specifics.
*/

// IMPORTANT: FFT_N should be #defined as 128 in ffft.h.

#include <avr/pgmspace.h>
#include <ffft.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_NeoPixel.h>
#include "OledI2C.h"
#include "OledPix.h"

// Audio input connects to Analog Pin 0.  Corresponding ADC channel number
// varies among boards...it's ADC0 on Uno and Mega, ADC7 on Leonardo.
// Other boards may require different settings; refer to datasheet.
#ifdef __AVR_ATmega32U4__
 #define ADC_CHANNEL 7
#else
 #define ADC_CHANNEL 0
#endif

// DK array of matrices for more columns
#define MATRICES 2
#define COLUMNS 16
byte waveformX = 0;

int16_t       capture[FFT_N];    // Audio capture buffer
complex_t     bfly_buff[FFT_N];  // FFT "butterfly" buffer
uint16_t      spectrum[FFT_N/2]; // Spectrum output buffer
volatile byte samplePos = 0;     // Buffer position counter

byte
  peak[COLUMNS],      // Peak level of each column; used for falling dots
  dotCount = 0, // Frame counter for delaying dot-falling speed
  colCount = 0; // Frame counter for storing past column data
int
  col[COLUMNS][10],   // Column levels for the prior 10 frames
  minLvlAvg[COLUMNS], // For dynamic adjustment of low & high ends of graph,
  maxLvlAvg[COLUMNS], // pseudo rolling averages for the prior few frames.
  colDiv[COLUMNS];    // Used when filtering FFT output to 16 columns

/*
These tables were arrived at through testing, modeling and trial and error,
exposing the unit to assorted music and sounds.  But there's no One Perfect
EQ Setting to Rule Them All, and the graph may respond better to some
inputs than others.  The software works at making the graph interesting,
but some columns will always be less lively than others, especially
comparing live speech against ambient music of varying genres.
*/
static const uint8_t PROGMEM
  // This is low-level noise that's subtracted from each FFT output column:
  noise[64]={ 8,6,6,5,3,4,4,4,3,4,4,3,2,3,3,4,
              2,1,2,1,3,2,3,2,1,2,3,1,2,3,4,4,
              3,2,2,2,2,2,2,1,3,2,2,2,2,2,2,2,
              2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,4 },
  // These are scaling quotients for each FFT output column, sort of a
  // graphic EQ in reverse.  Most music is pretty heavy at the bass end.
  eq[64]={
    255, 175,218,225,220,198,147, 99, 68, 47, 33, 22, 14,  8,  4,  2,
      0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
  // When filtering down to 16 columns, these tables contain indexes
  // and weightings of the FFT spectrum output values to use.  Not all
  // buckets are used -- the bottom-most and several at the top are
  // either noisy or out of range or generally not good for a graph.
  // DK Change: Include lowest bucket for line-in sub bass and expanded led display
  //    Interpolate column weights
  col0data[] = {  2,  0,  // # of spectrum bins to merge, index of first
    111,  51 },           // Weights for each bin
  col1data[] = {  3,  0,  // DK add
     35, 120, 11 },             
  col2data[] = {  4,  1,  // 4 bins, starting at index 1
     19, 186,  38,   2 }, // Weights for 4 bins.  Got it now?
  col3data[] = {  4,  1,  // DK add
      6,  69,  121,   6 },      
  col4data[] = {  5,  2,
     11, 156, 118,  16,   1 },
  col5data[] = {  6,  2,  // DK add
      7,  32, 156, 118,  16,   1 },     
  col6data[] = {  8,  3,
      5,  55, 165, 164,  71,  18,   4,   1 },
  col7data[] = {  8,  3,
      1,  22, 56, 111,  166,  56,   16,   5 }, // DK add
  col8data[] = { 11,  5,
      3,  24,  89, 169, 178, 114,  51,  20,   6,   2,   1 },
  col9data[] = { 11,  6,
      3,  24,  50,  85, 156, 169, 128,  28,   6,   2,   1 }, // DK add 
  col10data[] = { 17,  7,
      2,   9,  29,  70, 125, 172, 185, 162, 118, 81,
     51,  21,  10,   5,   2,   1,   1 },
  col11data[] = { 21,  9,
      2,   9,  39,  83, 125, 172, 185, 162, 118, 74, // DK add
     55,  29,  21,  14,  10,   5,   2,   1,   1 },     
  col12data[] = { 25, 11,
      1,   4,  11,  25,  49,  83, 121, 156, 180, 185,
    174, 149, 118,  87,  60,  40,  25,  16,  10,   6,
      4,   2,   1,   1,   1 },
  col13data[] = { 31, 13,
      1,   4,  11,  25,  49,  73, 111, 166, 185, 180, // DK add
    169, 135, 110,  77,  55,  37,  19,  10,  6,   2,
      4,   2,   1,   1,   1 },      
  col14data[] = { 37, 16,
      1,   2,   5,  10,  18,  30,  46,  67,  92, 118,
    143, 164, 179, 185, 184, 174, 158, 139, 118,  97,
     77,  60,  45,  34,  25,  18,  13,   12,   12,   10,
      10,   9,   9,   8,   8,   12,   12 },
  col15data[] = { 44, 13,
      1,   2,   5,  10,  18,  30,  46,  67,  92, 118, // DK add
    143, 164, 179, 185, 184, 174, 158, 139, 118,  97,
     77,  60,  45,  34,  25,  18,  17,   16,   15,   14,
     13,   13,   12,   12},      
            
  // And then this points to the start of the data for each of the columns:
  * const colData[]  = {
    col0data, col1data, col2data, col3data,
    col4data, col5data, col6data, col7data,
    col8data, col9data, col10data, col11data,
    col12data, col13data, col14data, col15data};

// DK array of 8x8 matrices
Adafruit_8x8matrix matrix[MATRICES] = {Adafruit_8x8matrix(), Adafruit_8x8matrix()};

void setup() {

  uint8_t i, j, nBins, binNum, *data;

  memset(peak, 0, sizeof(peak));
  memset(col , 0, sizeof(col));

  for(i=0; i<COLUMNS; i++) {
    minLvlAvg[i] = 0;
    maxLvlAvg[i] = 512;
    data         = (uint8_t *)pgm_read_word(&colData[i]);
    nBins        = pgm_read_byte(&data[0]) + 2;
    binNum       = pgm_read_byte(&data[1]);
    for(colDiv[i]=0, j=2; j<nBins; j++)
      colDiv[i] += pgm_read_byte(&data[j]);
  }

  matrix[0].begin(0x70);
  matrix[1].begin(0x71);
  for(i=0;i<MATRICES; i++)
    matrix[i].setRotation(1);  

  // Init ADC free-run mode; f = ( 16MHz/prescaler ) / 13 cycles/conversion 
  ADMUX  = ADC_CHANNEL; // Channel sel, right-adj, use AREF pin
  ADCSRA = _BV(ADEN)  | // ADC enable
           _BV(ADSC)  | // ADC start
           _BV(ADATE) | // Auto trigger
           _BV(ADIE)  | // Interrupt enable
           _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); // 128:1 / 13 = 9615 Hz
  ADCSRB = 0;                // Free run mode, no high MUX bit
  DIDR0  = 1 << ADC_CHANNEL; // Turn off digital input for ADC pin
  //TIMSK0 = 0;                // Timer0 off // DK disable

  sei(); // Enable interrupts

  SetupOledI2C();
  SetupOledPix();
}

void loop() {
  uint8_t  i, x, L, *data, nBins, binNum, weighting, c;
  uint16_t minLvl, maxLvl;
  int      level, y, sum;

  while(ADCSRA & _BV(ADIE)); // Wait for audio sampling to finish

  fft_input(capture, bfly_buff);   // Samples -> complex #s
  samplePos = 0;                   // Reset sample counter
  ADCSRA |= _BV(ADIE);             // Resume sampling interrupt
  fft_execute(bfly_buff);          // Process complex data
  fft_output(bfly_buff, spectrum); // Complex -> spectrum

  // Remove noise and apply EQ levels
  for(x=0; x<FFT_N/2; x++) {
    L = pgm_read_byte(&noise[x]);
    spectrum[x] = (spectrum[x] <= L) ? 0 :
      (((spectrum[x] - L) * (256L - pgm_read_byte(&eq[x]))) >> 8);
  }

  // Fill background w/colors, then idle parts of columns will erase (X, Y, W, H)
  for(i=0;i<MATRICES; i++) {
    matrix[i].fillRect(0, 0, 8, 2, LED_ON);    // Upper section
    matrix[i].fillRect(0, 2, 8, 2, LED_ON); // Mid LED_YELLOW
    matrix[i].fillRect(0, 4, 8, 4, LED_ON);  // Lower section LED_GREEN
  }

  // Downsample spectrum output to 16 columns:
  for(x=0; x<COLUMNS; x++) {
    data   = (uint8_t *)pgm_read_word(&colData[x]);
    nBins  = pgm_read_byte(&data[0]) + 2;
    binNum = pgm_read_byte(&data[1]);
    for(sum=0, i=2; i<nBins; i++)
      sum += spectrum[binNum++] * pgm_read_byte(&data[i]); // Weighted
    col[x][colCount] = sum / colDiv[x];                    // Average
    minLvl = maxLvl = col[x][0];
    for(i=1; i<10; i++) { // Get range of prior 10 frames
      if(col[x][i] < minLvl)      minLvl = col[x][i];
      else if(col[x][i] > maxLvl) maxLvl = col[x][i];
    }
    // minLvl and maxLvl indicate the extents of the FFT output, used
    // for vertically scaling the output graph (so it looks interesting
    // regardless of volume level).  If they're too close together though
    // (e.g. at very low volume levels) the graph becomes super coarse
    // and 'jumpy'...so keep some minimum distance between them (this
    // also lets the graph go to zero when no sound is playing):
    if((maxLvl - minLvl) < 8) maxLvl = minLvl + 8;
    minLvlAvg[x] = (minLvlAvg[x] * 7 + minLvl) >> 3; // Dampen min/max levels
    maxLvlAvg[x] = (maxLvlAvg[x] * 7 + maxLvl) >> 3; // (fake rolling average)

    // Second fixed-point scale based on dynamic min/max levels:
    level = 10L * (col[x][colCount] - minLvlAvg[x]) /
      (long)(maxLvlAvg[x] - minLvlAvg[x]);

    // Clip output and convert to byte:
    if(level < 0L)      c = 0;
    else if(level > 10) c = 10; // Allow dot to go a couple pixels off top
    else                c = (uint8_t)level;

    if(c > peak[x]) peak[x] = c; // Keep dot on top

    if(peak[x] <= 0) { // Empty column?
      for(i=0;i<MATRICES; i++)
        matrix[i].drawLine(x - i*8, 0, x - i*8, 7, LED_OFF);
      continue;
    } else if(c < 8) { // Partial column?
      for(i=0;i<MATRICES; i++)
        matrix[i].drawLine(x - i*8, 0, x - i*8, 7 - c, LED_OFF);
    }

    // The 'peak' dot color varies, but doesn't necessarily match
    // the three screen regions...yellow has a little extra influence.
    y = 8 - peak[x];
    for(i=0;i<MATRICES; i++)
      if(y < 2)      matrix[i].drawPixel(x - i*8, y, LED_ON);
      else if(y < 6) matrix[i].drawPixel(x - i*8, y, LED_ON); //LED_YELLOW
      else           matrix[i].drawPixel(x - i*8, y, LED_ON); //LED_GREEN

  }

  for(i=0;i<MATRICES; i++)
    matrix[i].writeDisplay();

  // Every 2nd frame, make the peak pixels drop by 1: // every 10 for "peak mode"
  if(++dotCount >= 2) {
    dotCount = 0;
    for(x=0; x<COLUMNS; x++) {
      if(peak[x] > 0) peak[x]--;
    }
  }

  if(++colCount >= 10) colCount = 0;

  // Update OLED waveform using column levels, with inverse linear weighting for bass frequencies DK
  for(i=0;i<COLUMNS; i++) {
    level += (20-i)*5 * (col[i][colCount] - minLvlAvg[i]) /
        (long)(maxLvlAvg[i] - minLvlAvg[i]) / COLUMNS;
  }

  vLineErase((waveformX+1)%128);
  vCenterLine(waveformX, level+1);

  if(++waveformX > 127) { waveformX = 0; }
}


ISR(ADC_vect) { // Audio-sampling interrupt
  static const int16_t noiseThreshold = 4;
  int16_t              sample         = ADC; // 0-1023

  capture[samplePos] =
    ((sample > (512-noiseThreshold)) &&
     (sample < (512+noiseThreshold))) ? 0 :
    sample - 512; // Sign-convert for FFT; -512 to +511

  if(++samplePos >= FFT_N) ADCSRA &= ~_BV(ADIE); // Buffer full, interrupt off
}
