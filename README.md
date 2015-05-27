ss☆ - a collection of screen savers

Compiling (X11):

Download xscreensaver source from jwz.org/xscreensaver, and paste contents of x11/ folder inside xscreensaver root directory. ./configure, make, and make install.

Compiling (Win32):

Open ssstar solution and compile all. Rename win32/Release/NAME.exe to NAME.scr, right click, and click install.

Please note that some things may be very broken in Windows, as it was a hack to port from X11, and also because msvc doesn't like C very much. Sorry :(

Write an email to me at sudo@pt-get.com if you find anything broken (or just submit a PR).

You can find win32 binaries [here](http://pt-get.com/d/ssstar).
