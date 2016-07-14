/*
 * Copyright (c) 2016 Rafael Zalamena <rzalamena@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BENCODE_
#define _BENCODE_

#include <sys/queue.h>

#include <stdint.h>

enum bencode_type {
	BET_UNKNOWN = 0,
	BET_DICT,
	BET_LIST,
	BET_INTEGER,
	BET_STRING,
};

struct bencode {
	enum bencode_type be_type;

	/* Dict keys can only be bencoded strings. */
	char *be_dictkey;
	union {
		TAILQ_HEAD(, bencode) bev_list;
		int64_t bev_int;
		char *bev_str;
	} be_value;
#define be_int be_value.bev_int
#define be_list be_value.bev_list
#define be_dict be_value.bev_list
#define be_str be_value.bev_str

	TAILQ_ENTRY(bencode) be_entry;
};

struct be_parser {
	const char *bp_cur;
	const char *bp_end;
};

struct bencode *be_parse(const char *);
struct bencode *be_nparse(const char *, size_t);
void be_free(struct bencode *);
void log_bencode(struct bencode *);

#endif /* _BENCODE_ */
