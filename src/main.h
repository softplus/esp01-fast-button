/*
	Copyright (c) 2022-2022 John Mueller

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

/* main.h */

#ifndef MAIN_H
#define MAIN_H

// various debug modes, uncomment as needed
//#define DEBUG_MODE
//#define DEBUG_AUTODISCOVER
//#define DEBUG_AP_MODE

// pin definitions for hardware
#define LED_PIN 2
#define NOTIFY_PIN 3

// Macro to display a debug text + timing
#ifdef DEBUG_MODE
#define DEBUG_LOG(x) Serial.print(F(x)); Serial.print(F(" @ ")); Serial.println(millis())
#else
#define DEBUG_LOG(x)
#endif

#endif
