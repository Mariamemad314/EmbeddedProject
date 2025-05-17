#include "mbed.h"

//  Shift Register Pin Setup 
// These pins control the 74HC595 shift register that drives the display.
DigitalOut latchPin(D4);  // ST_CP: Latch pin – toggled to update display
DigitalOut clockPin(D7);  // SH_CP: Clock pin – shifts each bit on rising edge
DigitalOut dataPin(D8);   // DS: Data pin – sends individual bits

// Button Inputs
// These buttons are active LOW (pressed = 0), with pull-up enabled.
DigitalIn s1(A1), s2(A2), s3(A3);

// 7-Segment Digit Patterns 
// These values represent each digit (0–9) for a common anode 7-segment display.
// Since common anode uses inverted logic, we bitwise-NOT the standard pattern.
const uint8_t digitPattern[10] = {
    static_cast<uint8_t>(~0x3F), // 0 → Segments A–F ON
    static_cast<uint8_t>(~0x06), // 1
    static_cast<uint8_t>(~0x5B), // 2
    static_cast<uint8_t>(~0x4F), // 3
    static_cast<uint8_t>(~0x66), // 4
    static_cast<uint8_t>(~0x6D), // 5
    static_cast<uint8_t>(~0x7D), // 6
    static_cast<uint8_t>(~0x07), // 7
    static_cast<uint8_t>(~0x7F), // 8
    static_cast<uint8_t>(~0x6F)  // 9
};

//  Digit Position Control
// Only one digit is shown at a time; these values select which digit is active.
// Display is scanned rapidly to show all digits .
const uint8_t digitPos[4] = {
    0x01, // Leftmost digit
    0x02, // Second digit
    0x04, // Third digit
    0x08  // Rightmost digit
};

//Time Variables
// We'll count minutes and seconds up to 99:59.
// volatile ensures the values are safely accessed across threads.
volatile int seconds = 0, minutes = 0;

// A Ticker is used to call a function every second like a timer interrupt.
Ticker timerTicker;

//Timer Callback
// This function runs once every second. It increments the time.
// If seconds hit 60, they roll over and increase the minutes.
void updateTime() {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
        if (minutes >= 100) minutes = 0;  // Cap at 99 minutes
    }
}

// Shift Register Writer 
// Sends 8 bits to the shift register, starting from the Most Significant Bit.
void shiftOutMSBFirst(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        dataPin = (value & (1 << i)) ? 1 : 0;  // Set data bit
        clockPin = 1;  // Pulse clock to shift it in
        clockPin = 0;
    }
}

// Shift Register Display Control.
// First sends the segment pattern, then the digit select to the shift register.
// Latching makes the new data appear on the output.
void writeToShiftRegister(uint8_t segments, uint8_t digit) {
    latchPin = 0;  // Begin transmission
    shiftOutMSBFirst(segments);  // Send the segment pattern (which LEDs to light)
    shiftOutMSBFirst(digit);     // Send digit enable mask (which digit to activate)
    latchPin = 1;  // Finalize and show the data
}

// 4-Digit Display 
// Breaks a number into digits.

void displayNumber(int number) {
    int digits[4] = {
        (number / 1000) % 10,
        (number / 100) % 10,
        (number / 10) % 10,
        number % 10
    };

    // Scan each digit rapidly (2 ms per digit)
    for (int i = 0; i < 4; i++) {
        writeToShiftRegister(digitPattern[digits[i]], digitPos[i]);
        ThisThread::sleep_for(2ms);  // Small delay to make digit visible
    }
}

//  Main Loop 
int main() {
    s1.mode(PullUp);  // Ensure button reads HIGH when not pressed
    timerTicker.attach(&updateTime, 1.0f);  // Call updateTime() every second

    while(1) {
        if (!s1) {  // If reset button is pressed
            seconds = minutes = 0;  // Reset timer
            ThisThread::sleep_for(200ms);  // Debounce delay
        }

        // Continuously update the 4-digit display with current MMSS time
        displayNumber(minutes * 100 + seconds);
    }
}
