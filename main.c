#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

struct posting {
	struct tm time;
	char *desc;
	struct posting_line *lines;
	size_t nlines;
};

struct decimal {
	long sig;
	unsigned int places;
};

// WARNING: accounts is destroyed and is no longer a valid string hash table
struct account {
	char *key;
	bool value;
} *accounts = NULL;

struct posting_line {
	ptrdiff_t account;
	bool has_value;
	struct decimal val;
	char *currency;
};

ssize_t eat_whitespace(char *buf, size_t len, size_t min, bool sameline) {
	size_t startlen = len;
	while (len && isspace(*buf)) {
		if (sameline && *buf == '\n')
			return startlen - len;

		buf++;
		len--;
	}

	if (startlen - len < min)
		return -1;

	return startlen - len;
}

ssize_t
parse_posting_line(char *buf, size_t len, struct posting_line *pl)
{
	size_t olen = len;
	if (len < 1)
		return -1;

	if (*buf != '\t')
		return -1;

	buf++;
	len--;

	char *account = buf;
	size_t account_len = 0;
	while (len && (isalpha(*buf) || *buf == ':')) {
		account_len++;
		buf++;
		len--;
	}

	if (*buf == '\n') {
		char *account_name = strndup(account, account_len);
		if (account_name == NULL)
			return -1;

		shput(accounts, account_name, true);

		pl->account = shgeti(accounts, account_name);
		assert(pl->account);
		pl->has_value = true;
		return olen - len + 1;
	}

	ssize_t ret = eat_whitespace(buf, len, 1, true);
	buf += ret;
	len -= ret;

	if (len == 0)
		return -1;

	struct decimal val = {0};
	bool neg = false;
	if (*buf == '-') {
		neg = true;
		buf++;
		len--;
	}

	// parse monetary value
	bool have_point = false;
	while (len && (isdigit(*buf) || *buf == '.')) {
		if (*buf == '.') {
			if (have_point)
				break;

			have_point = true;
			buf++;
			len--;
			continue;
		}

		if (have_point)
			val.places++;

		// TODO: check for overflow
		val.sig = 10 * val.sig + *buf - '0';
		buf++;
		len--;
	}

	if (neg) val.sig = -val.sig;

	ret = eat_whitespace(buf, len, 0, true);
	buf += ret;
	len -= ret;

	// TODO: handle non-ascii values

	char *currency = buf;
	size_t currency_len = 0;
	while (len && isalpha(*buf)) {
		currency_len++;
		buf++;
		len--;
	}

	assert(currency_len);

	ret = eat_whitespace(buf, len, 0, true);

	// account for newline
	buf++;
	len--;

	char *account_name = strndup(account, account_len);
	if (account_name == NULL)
		return -1;

	shput(accounts, account_name, true);
	pl->account = shgeti(accounts, account_name);
	pl->val = val;
	pl->currency = strndup(currency, currency_len);

	if (pl->currency == NULL)
		return -1;

	return olen - len;
}

ssize_t parse_num(char *buf, size_t len, size_t max, int *out) {
	size_t startlen = len;
	while (max && len && isdigit(*buf)) {
		*out = *out * 10 + *buf - '0';
		buf++;
		len--;
		max--;
	}

	return startlen - len;
}

ssize_t parse_posting(char *buf, size_t len, struct posting *p) {
	size_t startlen = len;

	if (len < strlen("0000-00-00"))
		return -1;

	char *dateend = strptime(buf, "%Y-%m-%d", &p->time);
	if (dateend == NULL)
		return -1;

	len -= dateend - buf;
	buf = dateend;

	int ret = eat_whitespace(buf, len, 0, true);
	if (ret == -1)
		return -1;

	buf += ret;
	len -= ret;

	char *nl = memchr(buf, '\n', len);
	if (nl == NULL)
		return -1;

	p->desc = strndup(buf, nl - buf);

	len -= nl - buf + 1;
	buf = nl + 1;

	struct posting_line pl = {0};
	while (len && *buf == '\t') {
		pl = (struct posting_line){0};
		ssize_t used = parse_posting_line(buf, len, &pl);
		if (used == -1)
			return 1;

		buf += used;
		len -= used;

		struct posting_line *newlines = reallocarray(p->lines, p->nlines + 1, sizeof(struct posting_line));
		if (newlines == NULL)
			return -1;

		memcpy(&newlines[p->nlines], &pl, sizeof(pl));
		p->lines = newlines;
		p->nlines++;
	}

	return startlen - len;
}

ssize_t find_posting_end(char *buf, size_t count) {
	char *nl = memchr(buf, '\n', count);
	size_t ocount = count;
	while (nl - buf < count) {
		count = count - (nl - buf + 1);
		buf = nl + 1;
		if (count && nl[1] == '\n')
			return ocount - count;

		nl = memchr(buf, '\n', count);
	}

	return -1;
}

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

int main() {
	char buf[1024];
	struct posting posting = {0};
	size_t count = fread(buf, sizeof(char), sizeof(buf), stdin);

	char *ptr = buf;

	while (count) {
		ssize_t read = eat_whitespace(ptr, count, 0, false);
		ptr += read;
		count -= read;

		posting = (struct posting){0};
		ssize_t out = find_posting_end(ptr, count);

		read = parse_posting(ptr, out == -1 ? count : out, &posting);
		if (read == -1 && out != -1)
			break;

		if (out == -1) {
			if (feof(stdin))
				break;

			memmove(buf, ptr, count);
			ptr = buf + count;
			read = fread(ptr, sizeof(char), sizeof(buf) - count, stdin);
			if (ferror(stdin))
				return 1;

			count += read;
			ptr = buf;
			continue;
		}

		ptr += out;
		count -= out;
	}

	// WARNING: accounts is destroyed and is no longer a valid string hash table
	qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
	for (size_t i = 0; i < shlenu(accounts); i++) {
		printf("%s\n", accounts[i].key);
	}
}
