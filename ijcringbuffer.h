/*
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

*/

#ifndef INCLUDE_IJCRINGBUFFER_H
#define INCLUDE_IJCRINGBUFFER_H

#ifdef IJCRB_STATIC
#define IJCRB_DEF static
#else
#define IJCRB_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ijcringbuffer {
	unsigned char *data;
	unsigned size, mask; /* mask could be calculated each time (size-1) */
	unsigned read_cursor, write_cursor, wrap_cursor;
} ijcringbuffer;

/* data_size must be a power of 2 */
IJCRB_DEF void ijcringbuffer_init(ijcringbuffer *self, void *data, unsigned data_size);
IJCRB_DEF void ijcringbuffer_reset(ijcringbuffer *self);

/* returns the number of continuous bytes that can be read/consumed */
IJCRB_DEF unsigned ijcringbuffer_consumeable_size_continuous(ijcringbuffer *self);

/*	returns the number of bytes that can be read/consumed.
	(as the buffer can split/wrap, this can be more than ijcringbuffer_consumeable_size_continuous)
*/
IJCRB_DEF unsigned ijcringbuffer_consumeable_size(ijcringbuffer *self);

/* advance the consume/read cursor (essentially freeing memory to be used for produce/write) */
IJCRB_DEF void ijcringbuffer_consume(ijcringbuffer *self, unsigned size);

/* returns the current read/consume pointer */
IJCRB_DEF void *ijcringbuffer_peek(ijcringbuffer *self);

IJCRB_DEF int ijcringbuffer_is_empty(ijcringbuffer *self);
IJCRB_DEF int ijcringbuffer_is_full(ijcringbuffer *self);

/* returns 1 on success (insize bytes is written), returns 0 on fail (0 bytes written) */
IJCRB_DEF int ijcringbuffer_produce(ijcringbuffer *self, void *indata, unsigned insize);

#ifdef __cplusplus
}
#endif
#endif

#ifdef IJCRINGBUFFER_IMPLEMENTATION

#ifndef IJCRB_assert
#include <assert.h>
#define IJCRB_assert assert
#endif

#ifndef IJCRB_memcpy
#include <string.h>
#define IJCRB_memcpy memcpy
#endif

static unsigned ijcringbuffer__difference(unsigned a, unsigned b)
{
	unsigned ab = a - b;
	unsigned ba = b - a;
	return ab > ba ? ba : ab;
}

IJCRB_DEF void ijcringbuffer_init(ijcringbuffer *self, void *data, unsigned data_size)
{
	IJCRB_assert(data_size > 0 && !(data_size & (data_size - 1)));
	self->data = (unsigned char*)data;

	self->size = data_size;
	self->mask = data_size - 1;
	self->read_cursor = self->write_cursor = self->wrap_cursor = 0;
}

IJCRB_DEF void ijcringbuffer_reset(ijcringbuffer *self)
{
	self->read_cursor = self->write_cursor = self->wrap_cursor = 0;
}

static int ijcringbuffer__is_split(ijcringbuffer *self)
{
	return ijcringbuffer__difference(self->read_cursor, self->write_cursor) > self->size;
}

IJCRB_DEF void *ijcringbuffer_peek(ijcringbuffer *self)
{
	unsigned peek_rc;
	if (self->read_cursor == self->wrap_cursor && ijcringbuffer__is_split(self))
		peek_rc = 0;
	else
		peek_rc = self->read_cursor&self->mask;

	return self->data+peek_rc;
}

static unsigned ijcringbuffer__consumeable_size(ijcringbuffer *self, int continuous)
{
	unsigned cs;
	if (ijcringbuffer__is_split(self)) {
		if (self->read_cursor == self->wrap_cursor) {
			/* check if the buffer is a wrap + full fill */
			unsigned masked_write = self->write_cursor&self->mask;
			cs = masked_write ? masked_write : self->size;
		} else {
			IJCRB_assert(self->size > ijcringbuffer__difference(self->wrap_cursor, self->read_cursor));
			cs = ((self->wrap_cursor-self->read_cursor)&self->mask) + (continuous ? 0 : (self->write_cursor&self->mask));
		}
	} else {
		cs = self->write_cursor - self->read_cursor;
	}
	IJCRB_assert(self->size >= cs);
	return cs;
}

IJCRB_DEF unsigned ijcringbuffer_consumeable_size_continuous(ijcringbuffer *self)
{
	return ijcringbuffer__consumeable_size(self, 1);
}

IJCRB_DEF unsigned ijcringbuffer_consumeable_size(ijcringbuffer *self)
{
	return ijcringbuffer__consumeable_size(self, 0);
}

IJCRB_DEF void ijcringbuffer_consume(ijcringbuffer *self, unsigned size)
{
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(self) >= size);
	if (self->read_cursor == self->wrap_cursor && ijcringbuffer__is_split(self))
		self->read_cursor += self->size + (self->size - (self->read_cursor&self->mask)) + size;
	else
		self->read_cursor += size;
}

IJCRB_DEF int ijcringbuffer_is_empty(ijcringbuffer *self) { return self->write_cursor == self->read_cursor; }
IJCRB_DEF int ijcringbuffer_is_full(ijcringbuffer *self) { return ijcringbuffer_consumeable_size(self) == self->size; }

IJCRB_DEF int ijcringbuffer_produce(ijcringbuffer *self, void *indata, unsigned insize)
{
	unsigned masked_write = self->write_cursor&self->mask;
	int is_empty;

	if (ijcringbuffer__is_split(self)) {
		unsigned avail_write_size;
		if (self->wrap_cursor == self->read_cursor) {
			avail_write_size = masked_write ? (self->size - masked_write) : 0;
		}
		else {
			IJCRB_assert((self->read_cursor&self->mask) >= (self->write_cursor&self->mask));
			avail_write_size = (self->read_cursor - self->write_cursor) & self->mask;
		}

		if (avail_write_size >= insize) {
			IJCRB_memcpy(self->data + masked_write, indata, insize);
			self->write_cursor += insize;
			return 1;
		}
		return 0;
	}

	is_empty = ijcringbuffer_is_empty(self);
	/*
	check if we are empty and write cursor is not at the start, then we
	start writing to the front and 'split' the buffer (giving us an 'auto-reset')
	*/
	if (masked_write && is_empty && self->size >= insize)
		goto write_to_front;

	/*
		if !masked_write then
			has_filled_the_whole_buffer
			_or_
			has_not_filled_anything -> read == write -> empty
	*/
	if (!masked_write && !is_empty) {
		/* we can not write to _back_ but we still have to check if we can write to front */
		goto check_front;
	}

	/* check write to back */
	if ((self->size - masked_write) >= insize) {
		IJCRB_memcpy(self->data + masked_write, indata, insize);
		self->write_cursor += insize;
		return 1;
	}

check_front:
	/* check write to front */
	if ((self->read_cursor&self->mask) >= insize) {
write_to_front:
		self->wrap_cursor = self->write_cursor;
		IJCRB_memcpy(self->data, indata, insize);
		/* 'split' the buffer */
		self->write_cursor += self->size + (self->size - masked_write) + insize;
		IJCRB_assert((self->write_cursor & self->mask) == insize);
		return 1;
	}

	return 0;
}

#if defined(IJCRINGBUFFER_TEST) || defined(IJCRINGBUFFER_TEST_MAIN)

static void ijcringbuffer_test(void)
{
	unsigned t;
	ijcringbuffer ring, *r = &ring;
	char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

	char temp[9]; memset(temp, 0, sizeof temp);
	ijcringbuffer_init(r, temp, sizeof temp - 1);

	/* */
	IJCRB_assert(ijcringbuffer_produce(r, hex, 8));
	IJCRB_assert(!ijcringbuffer_produce(r, hex, 1));
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 8);
	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex, 8) == 0);
	ijcringbuffer_consume(r, 8);
	IJCRB_assert(ijcringbuffer_produce(r, hex + 4, 7));
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 7);
	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex + 4, 7) == 0);
	ijcringbuffer_consume(r, 6);
	IJCRB_assert(ijcringbuffer_produce(r, hex, 6));
	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex + 4 + 6, 1) == 0);
	ijcringbuffer_consume(r, 1);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 6);
	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex, 6) == 0);
	ijcringbuffer_consume(r, 6);
	IJCRB_assert(ijcringbuffer_is_empty(r));

	/* */
	ijcringbuffer_reset(r); memset(temp, 0, sizeof temp);

	IJCRB_assert(ijcringbuffer_produce(r, hex, 6));
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 6);
	ijcringbuffer_consume(r, 5);
	IJCRB_assert(ijcringbuffer_produce(r, hex, 4));
	IJCRB_assert(ijcringbuffer_produce(r, hex, 1));
	IJCRB_assert(!ijcringbuffer_produce(r, hex, 1));

	/* */
	ijcringbuffer_reset(r); memset(temp, 0, sizeof temp);

	t = 0xffffffffu - 3u;
	r->read_cursor = r->write_cursor = t;
	IJCRB_assert(ijcringbuffer_produce(r, hex, 6));
	IJCRB_assert(ijcringbuffer_produce(r, hex, 2));
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 8);
	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex, 6) == 0);
	ijcringbuffer_consume(r, 6);

	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 2);
	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex, 2) == 0);
	ijcringbuffer_consume(r, 2);

	/* */
	ijcringbuffer_reset(r); memset(temp, 0, sizeof temp);

	t = 0xffffffffu - 3u;
	r->write_cursor = t;
	r->read_cursor = t-1;
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 1);

	IJCRB_assert(ijcringbuffer_produce(r, hex, 4));
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 5);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 5);
	IJCRB_assert(!ijcringbuffer_produce(r, hex, 4));
	IJCRB_assert(ijcringbuffer_produce(r, hex, 3));
	IJCRB_assert(!ijcringbuffer_produce(r, hex, 1));
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 5);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 8);

	/* */
	ijcringbuffer_reset(r); memset(temp, 0, sizeof temp);
	IJCRB_assert(ijcringbuffer_produce(r, hex, 6));
	ijcringbuffer_consume(r, 5);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 1);
	IJCRB_assert(ijcringbuffer_produce(r, hex, 2));
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 3);
	IJCRB_assert(ijcringbuffer_produce(r, hex, 5));
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 3);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 8);

	/* */
	ijcringbuffer_reset(r); memset(temp, 0, sizeof temp);
	IJCRB_assert(ijcringbuffer_produce(r, hex, 8));
	ijcringbuffer_consume(r, 1);
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 7);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 7);
	IJCRB_assert(ijcringbuffer_produce(r, hex + 1, 1));
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 7);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 8);
	ijcringbuffer_consume(r, 7);
	IJCRB_assert(ijcringbuffer_consumeable_size_continuous(r) == 1);
	IJCRB_assert(ijcringbuffer_consumeable_size(r) == 1);

	IJCRB_assert(memcmp(ijcringbuffer_peek(r), hex + 1, 1) == 0);
}

#ifdef IJCRINGBUFFER_TEST_MAIN
int main(int argc, char **argv)
{
	(void)(argc, argv);
	ijcringbuffer_test();
	return 0;
}
#endif /* IJCRINGBUFFER_TEST_MAIN */

#endif /* IJCRINGBUFFER_TEST */

#endif /* IJCRINGBUFFER_IMPLEMENTATION */
