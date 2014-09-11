LiveEVM
=======

This is a working implementation of Eulerian video magnification (EVM) that
operates on a real-time video stream with some delay. It is the result of
my senior project in computer science. To fulfill the requirements of that
project, I implemented Gaussian and Laplacian image pyramid algorithms
myself, in spite of the fact that these are already available in OpenCV.

It is somewhat difficult to isolate desired frequencies when playing with
current implementations of EVM that run on video files. This project allows
you to adjust parameters like the filter to use (Gaussian or Laplacian),
the amplification factor, and the high/low frequencies for the bandpass
filter. This allows you to hone in on invisible changes that occur at
various frequencies.

By adjusting the values in the constants.hpp file, you can change the
buffer size and pyramid levels. While this might make the program run
too slow, it may also make it easier to isolate changes because the
buffer contains more frames from which to glean information.

Dependencies
===========

This project depends entirely on OpenCV2 and C++ 11, with some minor areas
that depend on the "unistd.h" header file from Unix-like systems (for the
sleep function). Eventually, this dependency will be removed and it will
be possible to compile this project on Linux, Windows, and Mac OSX without
making any modifications to the code.

To compile on Linux, make sure you have OpenCV installed and that you are
running the latest version of the gcc compiler (for the best C++ 11
support). Run `make`, then execute the `LiveEVM` binary file.

Sources
=======

This project is largely based on the good work of the folks at MIT who
published on this subject. A great informational page is available
[here](http://people.csail.mit.edu/mrub/vidmag/).

Licensing
=========

This code is distributed under the terms of the license available
[here](http://people.csail.mit.edu/mrub/vidmag/code/LICENSE.pdf).
