/** @file
    A two-dimensional bit buffer consisting of bytes.

    Copyright (C) 2015 Tommy Vestermark

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "bitbuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// If we make this an inline function instead of a macro, it means we don't
// have to worry about using bit numbers with side-effects (bit++).
static inline uint8_t bit_at(const uint8_t *bytes, uint32_t bit)
{
    return (uint8_t)(bytes[bit >> 3] >> (7 - (bit & 7)) & 1);
}

void bitbuffer_clear(bitbuffer_t *bits)
{
    memset(bits, 0, sizeof(*bits));
}

/// Set the width of the current (last) row by expanding or truncating as needed.
static void bitbuffer_set_width(bitbuffer_t *bits, uint16_t width)
{
    if (bits->num_rows == 0)
        bits->free_row = bits->num_rows = 1; // Add first row automatically

    uint32_t remaining_rows = BITBUF_ROWS - bits->num_rows + 1;
    uint32_t remaining_bits = remaining_rows * BITBUF_COLS * 8;
    if (width > remaining_bits) {
        // fprintf(stderr, "%s: Could not add more bits\n", __func__);
        width = remaining_bits;
    }

    // clear bits when truncating
    if (bits->bits_per_row[bits->num_rows - 1] > width) {
        uint8_t *b        = bits->bb[bits->num_rows - 1];
        uint32_t clr_from = (width + 7) / 8;
        uint32_t clr_end  = (bits->bits_per_row[bits->num_rows - 1] + 7) / 8;
        memset(&b[clr_from], 0, clr_end - clr_from);

        // note that width became strictly smaller, that way we don't overflow
        b[width / 8] &= 0xff00 >> (width % 8);
    }

    bits->bits_per_row[bits->num_rows - 1] = width;

    uint32_t extra_rows = width == 0 ? 0 : (width - 1) / (BITBUF_COLS * 8);
    bits->free_row      = bits->num_rows + extra_rows;
}
void bitbuffer_add_bit(bitbuffer_t *bits, int32_t bit)
{
    uint16_t len_row = 0;
    bitbuffer_add_bit_graph(bits,bit, len_row); 
}

void bitbuffer_add_bit_graph(bitbuffer_t *bits, int32_t bit, uint32_t len_row) 
{
    if (bits->num_rows == 0) {
        bits->free_row = bits->num_rows = 1; // Add first row automatically
    }
    if (bits->bits_per_row[bits->num_rows - 1] == UINT16_MAX) {
        // fprintf(stderr, "%s: Could not add more bits\n", __func__);
        return;
    }
    //if (bits->bits_per_row[bits->num_rows - 1] == UINT16_MAX - 1) {
    //    //print_logf(LOG_WARNING, __func__, "Warning: row length limit (%u bits) reached", UINT16_MAX);
    //    fprintf(stderr, "%s: Warning: row length limit (%u bits) reached\n", __func__, UINT16_MAX);
    //}

    uint16_t col_index = bits->bits_per_row[bits->num_rows - 1] / 8;
    uint16_t bit_index = bits->bits_per_row[bits->num_rows - 1] % 8;
    if (bits->bits_per_row[bits->num_rows - 1] > 0 && bits->bits_per_row[bits->num_rows - 1] % (BITBUF_COLS * 8) == 0) {
        // spill into next row
        //if (bits->free_row == BITBUF_ROWS - 1) {
        //    //print_logf(LOG_WARNING, __func__, "Warning: row count limit (%d rows) reached", BITBUF_ROWS);
        //    fprintf(stderr, "%s: Warning: row count limit (%d rows) reached\n", __func__, BITBUF_ROWS);
        //}
        if (bits->free_row < BITBUF_ROWS) {
            bits->free_row++;
        }
        else {
            // fprintf(stderr, "%s: Could not add more rows\n", __func__);
            return;
        }
    }
    if (bits->bits_per_row[bits->num_rows - 1]==0)
		len_row   = 0;
    uint8_t *b = bits->bb[bits->num_rows - 1];
    b[col_index] |= (bit << (7 - bit_index));
    bits->bits_per_row[bits->num_rows - 1]++;


    /*
    // preamble compression
    if (bits->bits_per_row[bits->num_rows - 1] == 60 * 8) {
        uint8_t *b = bits->bb[bits->num_rows - 1];
        for (int32_t i = 21; i < 60; ++i) {
            if (b[20] != b[i]) {
                return;
            }
        }
        // fprintf(stderr, "%s: preamble compression\n", __func__);
        memset(&b[30], 0, 30);
        bits->bits_per_row[bits->num_rows - 1] = 30 * 8;
    }
*/
}

void bitbuffer_add_last_len(bitbuffer_t *bits, uint32_t len_row)
{
    if (bits->num_rows > 0)
        if (!bits->len_rows[bits->num_rows - 1] )
			{
            bits->len_rows[bits->num_rows - 1] = len_row;
        }
}

void bitbuffer_add_sync(bitbuffer_t *bits, uint32_t *len_row)
{
    if (bits->num_rows == 0)
        bits->free_row = bits->num_rows = 1; // Add first row automatically
    if (bits->bits_per_row[bits->num_rows - 1]) {
        bitbuffer_add_row(bits, len_row);
    }
    bits->syncs_before_row[bits->num_rows - 1]++;
}

void bitbuffer_add_row(bitbuffer_t *bits, uint32_t *len_row)
{
    if (bits->num_rows == 0)
        bits->free_row = bits->num_rows = 1; // Add first row automatically
    bits->len_rows[bits->num_rows - 1] = *len_row;
    *len_row                            = 0;
    if (bits->free_row < BITBUF_ROWS) {
        bits->free_row++;
        bits->num_rows = bits->free_row;
    }
    else {
        bits->bits_per_row[bits->num_rows - 1] = 0; // Clear last row to handle overflow somewhat gracefully
    }
}
uint32_t bitbuffer_manchester_decode(bitbuffer_t *inbuf, int32_t row, uint32_t start,
        bitbuffer_t *outbuf, uint32_t max)
{
    uint8_t *bits     = inbuf->bb[row];
    uint32_t len  = inbuf->bits_per_row[row];
    uint32_t ipos = start;

    if (max && len > start + (max * 2))
        len = start + (max * 2);

    while (ipos < len) {
        uint8_t bit1, bit2;

        bit1 = bit_at(bits, ipos++);
        bit2 = bit_at(bits, ipos++);

        if (bit1 == bit2)
            break;

        bitbuffer_add_bit(outbuf, bit2);
    }

    return ipos;
}

uint32_t bitbuffer_differential_manchester_decode(bitbuffer_t *inbuf, int32_t row, uint32_t start,
        bitbuffer_t *outbuf, uint32_t max)
{
    uint8_t *bits     = inbuf->bb[row];
    uint32_t len  = inbuf->bits_per_row[row];
    uint32_t ipos = start;
    uint8_t bit1, bit2 = 0;

    if (max && len > start + (max * 2))
        len = start + (max * 2);

    // the first long pulse will determine the clock
    // if needed skip one short pulse to get in synch
    while (ipos < len) {
        bit1         = bit_at(bits, ipos++);
        bit2         = bit_at(bits, ipos++);
        uint8_t bit3 = bit_at(bits, ipos);

        if (bit1 != bit2) {
            if (bit2 != bit3) {
                bitbuffer_add_bit(outbuf, 0);
            }
            else {
                bit2 = bit1;
                ipos --;
                break;
            }
        }
        else {
            bit2 = 1 - bit1;
            ipos -= 2;
            break;
        }
    }

    while (ipos < len) {
        bit1 = bit_at(bits, ipos++);
        if (bit1 == bit2)
            break; // clock missing, abort
        bit2 = bit_at(bits, ipos++);

        if (bit1 == bit2)
            bitbuffer_add_bit(outbuf, 1);
        else
            bitbuffer_add_bit(outbuf, 0);
    }

    return ipos;
}

void bitbuffer_invert(bitbuffer_t *bits)
{
    for (int32_t row = 0; row < bits->num_rows; ++row) {
        if (bits->bits_per_row[row] > 0) {
            uint8_t *b = bits->bb[row];

            const uint32_t last_col  = (bits->bits_per_row[row] - 1) / 8;
            const uint32_t last_bits = ((bits->bits_per_row[row] - 1) % 8) + 1;
            for (uint32_t col = 0; col <= last_col; ++col) {
                b[col] = ~b[col]; // Invert
            }
            b[last_col] ^= 0xFF >> last_bits; // Re-invert unused bits in last byte
        }
    }
}

void bitbuffer_nrzs_decode(bitbuffer_t *bits)
{
    for (int32_t row = 0; row < bits->num_rows; ++row) {
        if (bits->bits_per_row[row] > 0) {
            uint8_t *b = bits->bb[row];

            const uint32_t last_col  = (bits->bits_per_row[row] - 1) / 8;
            const uint32_t last_bits = ((bits->bits_per_row[row] - 1) % 8) + 1;

            int32_t prev = 0;
            for (uint32_t col = 0; col <= last_col; ++col) {
                int32_t mask = (prev << 7) | b[col] >> 1;
                prev     = b[col];
                b[col]   = b[col] ^ ~mask;
            }
            b[last_col] &= 0xFF << (8 - last_bits); // Clear unused bits in last byte
        }
    }
}

void bitbuffer_nrzm_decode(bitbuffer_t *bits)
{
    for (int32_t row = 0; row < bits->num_rows; ++row) {
        if (bits->bits_per_row[row] > 0) {
            uint8_t *b = bits->bb[row];

            const uint32_t last_col  = (bits->bits_per_row[row] - 1) / 8;
            const uint32_t last_bits = ((bits->bits_per_row[row] - 1) % 8) + 1;

            int32_t prev = 0;
            for (uint32_t col = 0; col <= last_col; ++col) {
                int32_t mask = (prev << 7) | b[col] >> 1;
                prev     = b[col];
                b[col]   = b[col] ^ mask;
            }
            b[last_col] &= 0xFF << (8 - last_bits); // Clear unused bits in last byte
        }
    }
}

void bitbuffer_extract_bytes(bitbuffer_t *bitbuffer, int32_t row,
        uint32_t pos, uint8_t *out, uint32_t len)
{
    uint8_t *bits = bitbuffer->bb[row];
    if (len == 0)
        return;
    if ((pos & 7) == 0) {
        memcpy(out, bits + (pos / 8), (len + 7) / 8);
    }
    else {
        uint32_t shift = 8 - (pos & 7);
        uint32_t bytes = (len + 7) >> 3;
        uint8_t *p     = out;
        uint16_t word;
        pos = pos >> 3; // Convert to bytes

        word = bits[pos];

        while (bytes--) {
            word <<= 8;
            word |= bits[++pos];
            *(p++) = word >> shift;
        }
    }
    if (len & 7)
        out[(len - 1) / 8] &= 0xff00 >> (len & 7); // mask off bottom bits
}

uint32_t bitbuffer_search(bitbuffer_t *bitbuffer, int32_t row, uint32_t start,
        const uint8_t *pattern, uint32_t pattern_bits_len)
{
    uint8_t *bits = bitbuffer->bb[row];
    uint32_t len  = bitbuffer->bits_per_row[row];
    uint32_t ipos = start;
    uint32_t ppos = 0; // cursor on init pattern

    while (ipos < len && ppos < pattern_bits_len) {
        if (bit_at(bits, ipos) == bit_at(pattern, ppos)) {
            ppos++;
            ipos++;
            if (ppos == pattern_bits_len)
                return ipos - pattern_bits_len;
        }
        else {
            ipos -= ppos;
            ipos++;
            ppos = 0;
        }
    }

    // Not found
    return len;
}

static void print_bitrow(uint8_t const *bitrow, uint32_t bit_len, uint32_t highest_indent, int32_t always_binary)
{
    uint32_t row_len = 0;

    fprintf(stderr, "{%2u} ", bit_len);
    for (uint32_t col = 0; col < (bit_len + 7) / 8; ++col) {
        row_len += fprintf(stderr, "%02x ", bitrow[col]);
    }
    // Print binary values also?
    if (always_binary || bit_len <= BITBUF_MAX_PRINT_BITS) {
        fprintf(stderr, "%-*s: ", highest_indent > row_len ? highest_indent - row_len : 0, "");
        for (uint32_t bit = 0; bit < bit_len; ++bit) {
            if (bitrow[bit / 8] & (0x80 >> (bit % 8))) {
                fprintf(stderr, "1");
            }
            else {
                fprintf(stderr, "0");
            }
            if ((bit % 8) == 7) // Add byte separators
                fprintf(stderr, " ");
        }
    }
    fprintf(stderr, "\n");
}

static void print_bitbuffer(const bitbuffer_t *bits, int32_t always_binary)
{
	// Figure out the longest row of bits to get the highest_indent
	uint32_t highest_indent = sizeof("[dd] {dd} ") - 1;
	for (uint32_t row = 0; row < bits->num_rows; ++row) {
		uint32_t hex_bytes = (bits->bits_per_row[row] + 7) / 8;
		uint32_t indent_this_row = (2 + 1) * hex_bytes;
		if (indent_this_row > highest_indent) {
			highest_indent = indent_this_row;
		}
	}
    fprintf(stderr, "bitbuffer:: Number of rows: %u \n", bits->num_rows);
    for (uint32_t row = 0; row < bits->num_rows; ++row) {
        fprintf(stderr, "[%02u] ", row);
        print_bitrow(bits->bb[row], bits->bits_per_row[row], highest_indent, always_binary);
    }
    if (bits->num_rows >= BITBUF_ROWS) {
        fprintf(stderr, "... Maximum number of rows reached. Message is likely truncated.\n");
    }
}

void bitbuffer_print(const bitbuffer_t *bits)
{
    print_bitbuffer(bits, 0);
}

void bitbuffer_debug(const bitbuffer_t *bits)
{
    print_bitbuffer(bits, 1);
}

void bitrow_print(uint8_t const *bitrow, uint32_t bit_len)
{
    print_bitrow(bitrow, bit_len, 0, 0);
}

void bitrow_debug(uint8_t const *bitrow, uint32_t bit_len)
{
    print_bitrow(bitrow, bit_len, 0, 1);
}

int32_t bitrow_snprint(uint8_t const *bitrow, uint32_t bit_len, uint8_t *str, uint32_t size)
{
    if (bit_len == 0 && size > 0) {
        str[0] = '\0';
    }
    int32_t len = 0;
    for (uint32_t i = 0; size > (uint32_t)len && i < (bit_len + 7) / 8; ++i) {
        len += snprintf(str + len, size - len, "%02x", bitrow[i]);
    }
    return len;
}

void bitbuffer_parse(bitbuffer_t *bits, const uint8_t *code)
{
    const uint8_t *c;
    int32_t data  = 0;
    int32_t width = -1;

    bitbuffer_clear(bits);

    for (c = code; *c; ++c) {

        if (*c == ' ') {
            continue;
        }
        else if (*c == '0' && (*(c + 1) == 'x' || *(c + 1) == 'X')) {
            ++c;
            continue;
        }
        else if (*c == '{') {
            if (width >= 0) {
                bitbuffer_set_width(bits, width);
            }
            if (bits->num_rows > 0) {
                uint32_t l = 0;
                bitbuffer_add_row(bits, &l);
            }

            uint8_t const *p = c;
            width         = strtol(c + 1, (uint8_t **)&c, 0);
            while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n')
                c++;
            if (*c != '}')
                fprintf(stderr, "Bad length indication: %.10s\n", p);
            if (width > BITBUF_MAX_ROW_BITS)
                width = BITBUF_MAX_ROW_BITS;
            if (!*c)
                break; // no closing brace and end of string
            continue;
        }
        else if (*c == '/') {
            if (width >= 0) {
                bitbuffer_set_width(bits, width);
                width = -1;
            }
            uint32_t l = 0;
            bitbuffer_add_row(bits, &l);
            continue;
        }
        else if (*c >= '0' && *c <= '9') {
            data = *c - '0';
        }
        else if (*c >= 'A' && *c <= 'F') {
            data = *c - 'A' + 10;
        }
        else if (*c >= 'a' && *c <= 'f') {
            data = *c - 'a' + 10;
        }
        bitbuffer_add_bit(bits, data >> 3 & 0x01);
        bitbuffer_add_bit(bits, data >> 2 & 0x01);
        bitbuffer_add_bit(bits, data >> 1 & 0x01);
        bitbuffer_add_bit(bits, data >> 0 & 0x01);
    }
    if (width >= 0) {
        bitbuffer_set_width(bits, width);
    }
}

int32_t bitbuffer_compare_rows(bitbuffer_t *bits, uint32_t row_a, uint32_t row_b, uint32_t max_bits)
{
    if (max_bits == 0 || bits->bits_per_row[row_a] < max_bits || bits->bits_per_row[row_b] < max_bits) {
        // full compare, no max_bits or rows too short
        return (bits->bits_per_row[row_a] == bits->bits_per_row[row_b] && !memcmp(bits->bb[row_a], bits->bb[row_b],
                                                                                  (bits->bits_per_row[row_a] + 7) / 8));
    }
    else {
        // prefix-only compare, both rows are at least max_bits long
        uint8_t *a    = bits->bb[row_a];
        uint8_t *b    = bits->bb[row_b];
        uint32_t last = (max_bits - 1) / 8;       // max_bits is at least 1
        uint32_t mask = 0xff00 >> (max_bits & 7); // mask off bottom bits
        return (!memcmp(bits->bb[row_a], bits->bb[row_b], max_bits / 8) && (a[last] & mask) == (b[last] & mask));
    }
}

uint32_t bitbuffer_count_repeats(bitbuffer_t *bits, int32_t row, uint32_t max_bits)
{
    uint32_t cnt = 0;
    for (int32_t i = 0; i < bits->num_rows; ++i) {
        if (bitbuffer_compare_rows(bits, row, i, max_bits)) {
            ++cnt;
        }
    }
    return cnt;
}

int32_t bitbuffer_find_repeated_row(bitbuffer_t *bits, uint32_t min_repeats, uint32_t min_bits)
{
    for (int32_t i = 0; i < bits->num_rows; ++i) {
        if (bits->bits_per_row[i] >= min_bits &&
                bitbuffer_count_repeats(bits, i, 0) >= min_repeats) {
            return i;
        }
    }
    return -1;
}

int32_t bitbuffer_find_repeated_prefix(bitbuffer_t *bits, uint32_t min_repeats, uint32_t min_bits)
{
    for (int32_t i = 0; i < bits->num_rows; ++i) {
        if (bits->bits_per_row[i] >= min_bits &&
                bitbuffer_count_repeats(bits, i, min_bits) >= min_repeats) {
            return i;
        }
    }
    return -1;
}

// Unit testing
#ifdef _TEST

#define ASSERT(expr) \
    do { \
        if (expr) { \
            ++passed; \
        } \
        else { \
            ++failed; \
            fprintf(stderr, "FAIL: line %d: %s\n", __LINE__, #expr); \
        } \
    } while (0)

int32_t main(void)
{
    uint32_t passed = 0;
    uint32_t failed = 0;

    fprintf(stderr, "bitbuffer:: test\n");

    bitbuffer_t bits = {0};

    fprintf(stderr, "TEST: bitbuffer:: The empty buffer\n");
    bitbuffer_print(&bits);
    ASSERT(bits.num_rows == 0);

    fprintf(stderr, "TEST: bitbuffer:: Add 1 bit\n");
    bitbuffer_add_bit(&bits, 1);
    bitbuffer_print(&bits);
    ASSERT(bits.num_rows == 1);

    fprintf(stderr, "TEST: bitbuffer:: Add 1 new row\n");
    bitbuffer_add_row(&bits);
    bitbuffer_print(&bits);
    ASSERT(bits.num_rows == 2);

    fprintf(stderr, "TEST: bitbuffer:: Fill row\n");
    for (int32_t i = 0; i < BITBUF_COLS * 8; ++i) {
        bitbuffer_add_bit(&bits, i % 2);
    }
    bitbuffer_print(&bits);
    ASSERT(bits.num_rows == 2);

    fprintf(stderr, "TEST: bitbuffer:: Add row and fill 1 column too many\n");
    bitbuffer_add_row(&bits);
    for (int32_t i = 0; i <= BITBUF_COLS * 8; ++i) {
        bitbuffer_add_bit(&bits, i % 2);
    }
    bitbuffer_print(&bits);
    ASSERT(bits.num_rows == 3);

    fprintf(stderr, "TEST: bitbuffer:: invert\n");
    bitbuffer_invert(&bits);
    bitbuffer_print(&bits);

    fprintf(stderr, "TEST: bitbuffer:: nrzs_decode\n");
    bits.num_rows        = 1;
    bits.bb[0][0]        = 0x74;
    bits.bb[0][1]        = 0x60;
    bits.bits_per_row[0] = 12;
    bitbuffer_nrzs_decode(&bits);
    bitbuffer_print(&bits);
    ASSERT(bits.bb[0][0] == 0xB1);
    ASSERT(bits.bb[0][1] == 0xA0);

    fprintf(stderr, "TEST: bitbuffer:: Clear\n");
    bitbuffer_clear(&bits);
    ASSERT(bits.num_rows == 0);
    bitbuffer_print(&bits);

    fprintf(stderr, "TEST: bitbuffer:: Add 1 row too many\n");
    for (int32_t i = 0; i <= BITBUF_ROWS; ++i) {
        bitbuffer_add_row(&bits);
    }
    bitbuffer_add_bit(&bits, 1);
    bitbuffer_print(&bits);

    fprintf(stderr, "bitbuffer:: test (%u/%u) passed, (%u) failed.\n", passed, passed + failed, failed);

    return failed > 0 ? 1 : 0;
}
#endif /* _TEST */
