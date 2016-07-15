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

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "torrent.h"

static int be_strdup(struct bencode *, char **);
static int load_bencode(const char *, char **, size_t *);
static int load_tracker(struct torrent *, struct bencode *);
static int load_file(struct torrent *, struct bencode *);
static int load_files(struct torrent *, struct bencode *);
static int load_info(struct torrent *, struct bencode *);
static int load_torrent(struct torrent *, struct bencode *);

static int
be_strdup(struct bencode *be, char **str)
{
	if (be->be_type != BET_STRING)
		return (-1);

	*str = malloc(be->be_strlen + 1);
	if (*str == NULL) {
		log_warn("%s failed", __FUNCTION__);
		return (-1);
	}

	memcpy(*str, be->be_str, be->be_strlen);
	(*str)[be->be_strlen] = 0;
	return (0);
}

static int
load_bencode(const char *path, char **bestr, size_t *belen)
{
	FILE *fs;
	struct stat st;

	/* Test if we can access the file. */
	if (access(path, R_OK)) {
		log_warn("not enough permissions to read torrent file");
		return (-1);
	}

	/* Get the bencode string size and allocate space. */
	if (stat(path, &st)) {
		log_warn("failed to stat torrent file");
		return (-1);
	}

	*belen = st.st_size;
	*bestr = malloc(st.st_size);
	if (*bestr == NULL) {
		log_warn("failed to allocate torrent file data");
		return (-1);
	}

	/* Open the file and load the string. */
	fs = fopen(path, "r");
	if (fs == NULL) {
		free(*bestr);
		log_warn("failed to open torrent file");
		return (-1);
	}

	if (fread(*bestr, st.st_size, 1, fs) != 1) {
		free(*bestr);
		log_warn("failed to read torrent file");
		return (-1);
	}

	fclose(fs);

	return (0);
}

static int
load_tracker(struct torrent *to, struct bencode *be)
{
	struct tracker *tr;
	struct bencode *ben;

	switch (be->be_type) {
	case BET_STRING:
		tr = calloc(1, sizeof(*tr));
		if (tr == NULL) {
			log_warn("failed to allocate tracker data");
			return (-1);
		}

		tr->tr_url = strdup(be->be_str);
		if (tr->tr_url == NULL) {
			free(tr);
			return (-1);
		}
		if (be->be_dictkey)
			TAILQ_INSERT_HEAD(&to->to_trackerlist, tr, tr_entry);
		else
			TAILQ_INSERT_TAIL(&to->to_trackerlist, tr, tr_entry);
		return (0);

	case BET_LIST:
		break;

	default:
		return (-1);
	}

	TAILQ_FOREACH(ben, &be->be_list, be_entry) {
		if (ben->be_type == BET_STRING) {
			load_tracker(to, ben);
			continue;
		}
		if (ben->be_type != BET_LIST)
			continue;

		load_tracker(to, TAILQ_FIRST(&ben->be_list));
	}

	return (0);
}

static int
load_file(struct torrent *to, struct bencode *be)
{
	struct tfile *tf;
	struct bencode *ben, *bep;
	char *tname = NULL;
	size_t fsize = 0;

	TAILQ_FOREACH(ben, &be->be_dict, be_entry) {
		if (ben->be_type == BET_LIST &&
		    strcmp(ben->be_dictkey, "path") == 0) {
			bep = TAILQ_FIRST(&ben->be_list);
			if (bep == NULL) {
				log_debug("empty path list");
				continue;
			}

			if (be_strdup(bep, &tname))
				return (-1);

			continue;
		}

		if (ben->be_type == BET_INTEGER &&
		    strcmp(ben->be_dictkey, "length") == 0) {
			if (ben->be_int <= 0) {
				log_debug("negative file size in torrent");
				return (-1);
			}

			fsize = ben->be_int;
			continue;
		}
	}

	if (tname == NULL || fsize == 0)
		return (-1);

	tf = calloc(1, sizeof(*tf));
	if (tf == NULL) {
		free(tname);
		return (-1);
	}

	tf->tf_path = tname;
	tf->tf_length = fsize;
	TAILQ_INSERT_TAIL(&to->to_filelist, tf, tf_entry);

	return (0);
}

static int
load_files(struct torrent *to, struct bencode *be)
{
	struct bencode *ben;

	TAILQ_FOREACH(ben, &be->be_dict, be_entry) {
		if (ben->be_type != BET_DICT)
			continue;
		if (load_file(to, ben))
			return (-1);
	}

	return (0);
}

static int
load_info(struct torrent *to, struct bencode *be)
{
	struct bencode *ben;
	struct tfile *tf;
	char *tname = NULL;
	size_t fsize = 0;
	int is_single_file = 1;

	TAILQ_FOREACH(ben, &be->be_dict, be_entry) {
		if (ben->be_type == BET_STRING &&
		    to->to_piecesdigest == NULL &&
		    strcmp(ben->be_dictkey, "pieces") == 0) {
			if (be_strdup(ben, &to->to_piecesdigest))
				return (-1);

			continue;
		}

		if (ben->be_type == BET_INTEGER &&
		    strcmp(ben->be_dictkey, "piece length") == 0) {
			if (ben->be_int <= 0) {
				log_debug("negative piece length in torrent");
				return (-1);
			}

			to->to_piecelen = ben->be_int;
			continue;
		}

		if (ben->be_type == BET_STRING &&
		    strcmp(ben->be_dictkey, "name") == 0) {
			if (be_strdup(ben, &tname))
				return (-1);

			continue;
		}

		if (ben->be_type == BET_INTEGER &&
		    strcmp(ben->be_dictkey, "length") == 0) {
			if (ben->be_int <= 0) {
				log_debug("negative file size in torrent");
				return (-1);
			}

			fsize = ben->be_int;
			continue;
		}

		if (ben->be_type == BET_LIST &&
		    strcmp(ben->be_dictkey, "files") == 0) {
			if (load_files(to, ben)) {
				log_debug("failed to decode torrent files");
				return (-1);
			}

			is_single_file = 0;
			continue;
		}
	}

	if (is_single_file) {
		if (fsize == 0 || tname == NULL) {
			log_debug("failed to find torrent file description");
			return (-1);
		}

		tf = calloc(1, sizeof(*tf));
		if (tf == NULL) {
			log_warn("failed to allocate file description");
			return (-1);
		}

		tf->tf_path = tname;
		tf->tf_length = fsize;
		TAILQ_INSERT_HEAD(&to->to_filelist, tf, tf_entry);
	}

	return (0);
}

static int
load_torrent(struct torrent *to, struct bencode *be)
{
	struct bencode *ben;

	TAILQ_FOREACH(ben, &be->be_dict, be_entry) {
		if (ben->be_type == BET_STRING) {
			if (to->to_comment == NULL &&
			    strcmp(ben->be_dictkey, "comment") == 0) {
				be_strdup(ben, &to->to_comment);
				continue;
			}
			if (to->to_creator == NULL &&
			    strcmp(ben->be_dictkey, "created by") == 0) {
				be_strdup(ben, &to->to_creator);
				continue;
			}
		}
		if (ben->be_type == BET_INTEGER &&
		    strcmp(ben->be_dictkey, "creation date") == 0) {
			to->to_createdat = ben->be_int;
			continue;
		}
		if (strcmp(ben->be_dictkey, "announce") == 0) {
			if (load_tracker(to, ben))
				return (-1);

			continue;
		}
		if (strcmp(ben->be_dictkey, "announce-list") == 0) {
			load_tracker(to, ben);
			continue;
		}
		if (ben->be_type == BET_DICT &&
		    strcmp(ben->be_dictkey, "info") == 0) {
			if (load_info(to, ben))
				return (-1);

			continue;
		}
	}

	return (0);
}

struct torrent *
parse_torrent(const char *path)
{
	struct torrent *to;
	struct bencode *be;
	char *bestr;
	size_t belen;

	to = calloc(1, sizeof(*to));
	if (to == NULL) {
		log_warn("failed to allocate torrent structure");
		return (NULL);
	}

	TAILQ_INIT(&to->to_trackerlist);
	TAILQ_INIT(&to->to_filelist);

	if (load_bencode(path, &bestr, &belen)) {
		free(to);
		log_warnx("failed to load bencoded data");
		return (NULL);
	}

	be = be_nparse(bestr, belen);
	if (be == NULL) {
		free(to);
		free(bestr);
		log_warnx("failed to parse bencoded data");
		return (NULL);
	}

	if (load_torrent(to, be)) {
		free_torrent(to);
		free(bestr);
		log_warnx("failed to parse torrent data");
		return (NULL);
	}

	return (to);
}

void
free_torrent(struct torrent *to)
{
	struct tracker *tr;
	struct tfile *tf;

	if (to) {
		free(to->to_comment);
		free(to->to_creator);
		free(to->to_piecesdigest);
		while (!TAILQ_EMPTY(&to->to_trackerlist)) {
			tr = TAILQ_FIRST(&to->to_trackerlist);
			TAILQ_REMOVE(&to->to_trackerlist, tr, tr_entry);
			free(tr);
		}
		while (!TAILQ_EMPTY(&to->to_filelist)) {
			tf = TAILQ_FIRST(&to->to_filelist);
			TAILQ_REMOVE(&to->to_filelist, tf, tf_entry);
			free(tf);
		}
	}

	free(to);
}
