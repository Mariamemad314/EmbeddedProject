#include "mbed.h"

// === Pin Definitions ===
// These are the pins connected to your 74HC595 shift register.
// We're controlling a 4-digit 7-segment common-anode display via this register.
DigitalOut latchPin(D4);  // Tells the register to update its outputs
DigitalOut clockPin(D7);  // Clock pin for shifting data
DigitalOut dataPin(D8);   // Serial data input

// === Button Inputs ===
// S1 will be used to reset the timer, S3 to show voltage on the display.
DigitalIn s1(A1), s3(A3);

// === Analog Input ===
// Potentiometer input for measuring a voltage between 0 and 3.3V.
AnalogIn pot(A0);

// === Segment Patterns ===
// Each byte represents the segments that should light up for digits 0-9.
// Since it's a **common anode**, the bits are inverted (~) to turn on a segment.
const uint8_t digitPattern[10] = {
    static_cast<uint8_t>(~0x3F), // 0
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

// === Digit Selector ===
// These values represent which digit position we want to light up (1 of 4).
// For example, 0x01 lights up the first digit, 0x02 the second, etc.
const uint8_t digitPos[4] = {0x01, 0x02, 0x04, 0x08};

// === Timer Variables ===
volatile int seconds = 0, minutes = 0;       // For keeping time like a stopwatch
volatile float minVoltage = 3.3f, maxVoltage = 0.0f; // Track voltage range
Ticker timerTicker;                         // Will call a function every second

// === Timer Interrupt ===
// This gets called every second via the Ticker.
void updateTime() {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes = (minutes + 1) % 100; // Stop counting at 99 mins
    }
}

// === Shift Register Writer (MSB first) ===
// This function "manually" shifts out bits to the shift register one at a time.
void shiftOutMSBFirst(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        dataPin = (value & (1 << i)) ? 1 : 0;
        clockPin = 1;   // Pulse the clock
        clockPin = 0;
    }
}

// === Send Both Segment Pattern and Digit Position ===
// Sends two bytes: one for the segment pattern (which segments light up),
// and one for digit selection (which digit to light).
void writeToShiftRegister(uint8_t segments, uint8_t digit) {
    latchPin = 0;                       // Start transfer
    shiftOutMSBFirst(segments);        // Send segment data first
    shiftOutMSBFirst(digit);          // Then digit control
    latchPin = 1;                       // Latch it (make it appear on output)
}

// === Main Display Function ===
// Breaks the number into 4 digits, applies segment encoding, and shows it.
// Can optionally show a decimal point (e.g., for showing voltage).
void displayNumber(int number, bool showDecimal = false, int decimalPos = -1) {
    // Split number into thousands, hundreds, tens, ones
    int digits[4] = {
        (number / 1000) % 10,
        (number / 100) % 10,
        (number / 10) % 10,
        number % 10
    };

    // Light up one digit at a time rapidly (multiplexing)
    for (int i = 0; i < 4; i++) {
        uint8_t pattern = digitPattern[digits[i]];
        if (showDecimal && i == decimalPos)
            pattern &= ~0x80; // Decimal point is bit 7, clear to turn it ON
        writeToShiftRegister(pattern, digitPos[i]);
        ThisThread::sleep_for(2ms); // Small delay to help visual persistence
    }
}

int main() {
    // === Setup ===
    s1.mode(PullUp); // Enable pull-up resistors for buttons (active-low)
    s3.mode(PullUp);
    timerTicker.attach(&updateTime, 1.0f); // Call updateTime() every second

    // === Main Loop ===
    while (1) {
        // If S1 is pressed, reset the timer
        if (!s1) { // Button is active-low
            seconds = 0;
            minutes = 0;
            ThisThread::sleep_for(200ms); // Debounce delay
        }

        // Read current voltage from potentiometer
        float voltage = pot.read() * 3.3f;

        // Keep track of the highest and lowest voltage seen
        if (voltage < minVoltage) minVoltage = voltage;
        if (voltage > maxVoltage) maxVoltage = voltage;

        // If S3 is pressed, display voltage (scaled to 3 digits, like 2.45V -> 245)
        if (!s3) {
            int voltageScaled = static_cast<int>(voltage * 100); // 0.00V to 3.30V → 0 to 330
            displayNumber(voltageScaled, true, 1); // Show as X.XX with decimal point at position 1
        } else {
            // Otherwise, show the timer (MMSS format)
            int timeDisplay = minutes * 100 + seconds;
            displayNumber(timeDisplay);
        }
    }
}
