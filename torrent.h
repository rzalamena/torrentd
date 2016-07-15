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

#ifndef _TORRENT_
#define _TORRENT_

#include "bencode.h"

struct tracker {
	TAILQ_ENTRY(tracker) tr_entry;

	char *tr_url;
	struct sockaddr_storage *tr_addr;
};

struct tfile {
	TAILQ_ENTRY(tfile) tf_entry;

	char *tf_path;
	size_t tf_length;
};

struct torrent {
	TAILQ_HEAD(, tracker) to_trackerlist;
	TAILQ_HEAD(, tfile) to_filelist;

	char *to_piecesdigest; /* equivalent 'pieces' */
	char *to_comment;
	char *to_creator;
        int64_t to_createdat;
	size_t to_piecelen; /* bytes in each piece */
};

/* torrent.c */
struct torrent *parse_torrent(const char *);
void free_torrent(struct torrent *);

/* log.c */
void log_init(int);
void log_verbose(int);
void log_warn(const char *, ...);
void log_warnx(const char *, ...);
void log_info(const char *, ...);
void log_debug(const char *, ...);
void fatal(const char *, ...);
void fatalx(const char *, ...);

#endif /* _TORRENT_ */
