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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bencode.h"

static int parse_string(struct be_parser *, char **);
static struct bencode *be_parse_string(struct be_parser *);
static struct bencode *be_parse_integer(struct be_parser *);
static struct bencode *be_parse_list(struct be_parser *);
static struct bencode *be_parse_bp(struct be_parser *);

void
be_free(struct bencode *be)
{
	struct bencode *ben;

	switch (be->be_type) {
	case BET_STRING:
		free(be->be_str);
		break;

	case BET_DICT:
		free(be->be_dictkey);
		/* FALLTHROUGH */
	case BET_LIST:
		while (!TAILQ_EMPTY(&be->be_list)) {
			ben = TAILQ_FIRST(&be->be_list);
			TAILQ_REMOVE(&be->be_list, ben, be_entry);
			be_free(be);
		}
		break;

	default:
		break;
	}

	free(be);
}

static int
parse_string(struct be_parser *bp, char **str)
{
	char *sptr;
	char *dst;
	long long len;

	*str = NULL;

	/* Find out how many bytes of string we have. */
	errno = 0;
	len = strtoll(bp->bp_cur, &sptr, 10);
	if (len < 0 ||
	    (len == 0 && errno))
		return (-1);

	/*
	 * Update the current parser pointer.
	 *
	 * e.g.
	 * 1234:string
	 *      ^
	 */
	bp->bp_cur = sptr + 1;

	/* Test if this is an empty string */
	if (len > 0) {
		dst = malloc(len + 1);
		if (dst == NULL)
			return (-1);

		/* Copy the string and update pointer again. */
		memcpy(dst, bp->bp_cur, len);
		dst[len] = 0;

		bp->bp_cur += len;
	} else
		dst = NULL;

	*str = dst;
	return (0);
}

static struct bencode *
be_parse_string(struct be_parser *bp)
{
	struct bencode *be;
	char *dst;

	if (parse_string(bp, &dst))
		return (NULL);

	/* Prepare the result and return */
	be = calloc(1, sizeof(*be));
	if (be == NULL) {
		free(dst);
		return (NULL);
	}

	be->be_type = BET_STRING;
	be->be_str = dst;
	return (be);
}

static struct bencode *
be_parse_integer(struct be_parser *bp)
{
	struct bencode *be;
	char *sptr;
	long long num;

	/*
	 * Move the pointer to the number start.
	 *
	 * e.g.
	 * i1234e
	 *  ^
	 */
	bp->bp_cur++;

	/* Parse the integer */
	errno = 0;
	num = strtoll(bp->bp_cur, &sptr, 10);
	if (num == 0 && errno)
		return (NULL);

	/* Check for ending with 'e'. */
	bp->bp_cur = sptr;
	if (*bp->bp_cur != 'e')
		return (NULL);

	bp->bp_cur++;

	be = calloc(1, sizeof(*be));
	if (be == NULL)
		return (NULL);

	be->be_type = BET_INTEGER;
	be->be_int = num;
	return (be);
}

static struct bencode *
be_parse_list(struct be_parser *bp)
{
	struct bencode *be, *ben;
	size_t len;

	/*
	 * Move the pointer to the list start.
	 *
	 * e.g.
	 * l1234:abcdi50ee
	 *  ^
	 */
	bp->bp_cur++;

	be = calloc(1, sizeof(*be));
	if (be == NULL)
		return (NULL);

	be->be_type = BET_LIST;

	TAILQ_INIT(&be->be_list);

	for (len = bp->bp_end - bp->bp_cur;
	     *bp->bp_cur && len > 0;
	     len = bp->bp_end - bp->bp_cur) {
		ben = be_parse_bp(bp);
		if (ben == NULL)
			break;

		TAILQ_INSERT_HEAD(&be->be_list, ben, be_entry);
	}

	if (*bp->bp_cur != 'e') {
		be_free(be);
		return (NULL);
	}

	/* Update the cur pointer to after 'e'. */
	bp->bp_cur++;

	return (be);
}

static struct bencode *
be_parse_dict(struct be_parser *bp)
{
	char *dst;
	struct bencode *be, *ben;
	size_t len;

	/*
	 * Move the pointer to the dictionary start.
	 *
	 * e.g.
	 * d1234:abcdi50ee
	 *  ^
	 */
	bp->bp_cur++;

	be = calloc(1, sizeof(*be));
	if (be == NULL)
		return (NULL);

	be->be_type = BET_DICT;
	TAILQ_INIT(&be->be_list);

	for (len = bp->bp_end - bp->bp_cur;
	     *bp->bp_cur && len > 0;
	     len = bp->bp_end - bp->bp_cur) {
		if (parse_string(bp, &dst))
			break;

		ben = be_parse_bp(bp);
		if (ben == NULL) {
			free(dst);
			break;
		}

		ben->be_dictkey = dst;
		TAILQ_INSERT_HEAD(&be->be_list, ben, be_entry);
	}

	if (*bp->bp_cur != 'e') {
		be_free(be);
		return (NULL);
	}

	/* Update the cur pointer to after 'e'. */
	bp->bp_cur++;

	return (be);
}

static struct bencode *
be_parse_bp(struct be_parser *bp)
{
	if (*bp->bp_cur == 0 || bp->bp_cur >= bp->bp_end)
		return (NULL);

	if (isdigit(*bp->bp_cur))
		return (be_parse_string(bp));

	switch (*bp->bp_cur) {
	case 'i':
		return (be_parse_integer(bp));
	case 'l':
		return (be_parse_list(bp));
	case 'd':
		return (be_parse_dict(bp));
	}

	return (NULL);
}

struct bencode *
be_nparse(const char *str, size_t slen)
{
	struct be_parser bp;

	if (slen == 0)
		slen = strlen(str);

	bp.bp_cur = str;
	bp.bp_end = str + slen;
	return (be_parse_bp(&bp));
}

struct bencode *
be_parse(const char *str)
{
	return (be_nparse(str, 0));
}

void
_log_bencode(struct bencode *be, size_t space_count)
{
	struct bencode *ben;
	size_t n;

	switch (be->be_type) {
	case BET_INTEGER:
		printf("%lld", be->be_int);
		break;

	case BET_STRING:
		printf("\"%s\"", be->be_str);
		break;

	case BET_LIST:
		printf("[");
		TAILQ_FOREACH(ben, &be->be_list, be_entry) {
			_log_bencode(ben, space_count + 4);
			if (TAILQ_NEXT(ben, be_entry))
				printf(", ");
		}
		printf("]");
		break;

	case BET_DICT:
		printf("{\n");
		TAILQ_FOREACH(ben, &be->be_list, be_entry) {
			for (n = 0; n < space_count; n++)
				printf(" ");

			printf("\"%s\" = ", ben->be_dictkey);
			_log_bencode(ben, space_count + 4);
			if (TAILQ_NEXT(ben, be_entry))
				printf(",\n");
		}
		printf("\n");
		for (n = 0; n < (space_count - 4); n++)
			printf(" ");

		printf("}");
		break;

	default:
		printf("Unknown[%d]", be->be_type);
		break;
	}
}

void
log_bencode(struct bencode *be)
{
	_log_bencode(be, 4);
}
