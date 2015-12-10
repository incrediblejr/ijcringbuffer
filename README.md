ijcringbuffer.h - v0.01 - public domain - @incrediblejr, Dec 2015

no warranty implied; use at your own risk

A continuous ringbuffer ala [@rygorous][2] ['magic ring buffer'][1] (without the magic)

ABOUT

ijcringbuffer is a ringbuffer that handles variable sized commands/data, inspired
by [@rygorous][2] blog post about the ['magic ring buffer'][1] (but implemented sans the magic).

The ring buffer guarantees that data will written as-is or not at all (no splitting).
This can be used when the consumer expects the data be linear in memory.

Usage examples (and tests) is at the bottom of the file in the IJCRINGBUFFER_TEST section.

USAGE

The ringbuffer is implemented as a [stb-style header-file library][3] which means that
in *ONE* source file, put:

    #define IJCRINGBUFFER_IMPLEMENTATION
	// if custom assert and memcpy is wanted/needed
	// (and no dependencies on assert.h + string.h)
	// you can define/override the default by :
    // #define IJCRB_assert   custom_assert
    // #define IJCRB_memcpy   custom_memcpy
    #include "ijcringbuffer.h"

Other source files should just include ijcringbuffer.h


NOTES

There are a myriad of ways to implement said ring buffer, especially if you
would allow the producer to modify the read-cursor (but you have to
have _some_ standards).

I also wanted to solve the (not-in-real-life-?) problem when the read-cursor catches
up with the write-cursor, giving the producer only part of the buffer (worst case
half of the buffer) to write into, when we should be able to start writing to the
start of the buffer (essentially doing a buffer reset).

[1]: https://fgiesen.wordpress.com/2012/07/21/the-magic-ring-buffer/
[2]: https://twitter.com/rygorous
[3]: https://github.com/nothings/stb

LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.
