/*
 * Copyright (c) 2016, Daniel Matusek <daniel.matusek@tu-dresden.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef DDCCI_H
#define DDCCI_H

int ddcci_write_slave (uint8_t *stream, uint8_t len);
int ddcci_read_slave (uint8_t *result);
int ddcc_write_master (BBI2C_t *dev, uint8_t *stream, uint8_t len);
uint8_t * ddcc_read_master (BBI2C_t *dev);
int read_edid (uint8_t *buf);
uint8_t checksum (uint8_t send, uint8_t stream[], uint8_t len);
uint8_t checkNullMessage (uint8_t val);

#endif //DDCCI_H
