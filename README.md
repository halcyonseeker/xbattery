Xbattery for Linux
===

A basic X battery charge monitor for ACPI systems, akin to Xclock or
Xload.

This project was inspired by 
[julienxx's XBattery](https://git.sr.ht/~julienxx/XBattery), 
only with support for modern Linux (ACPI instead of APM). Most of the
X code in `display()` and `init_x()`, as well as `color_for_percent()`
was heavily adapted or outright copy-pasted from the original project.

Installation
---
Install Noto font to display emoji icons.

Build with `make`.
Run `make install` as root to copy the binary to `/usr/local/bin`.

Known Bugs
---
* The window is white for a few seconds after startup.
* The text takes a 1-10 seconds to change when plugging/unplugging the 
  AC adaptor

Roadmap
---
* Add support for custom colors using Xresources.
* Adjust size and position using command line flags.
* Add support for multi-battery systems.
* Add an APM backend.
* Add a man page
