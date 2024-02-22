#define _DEFAULT_SOURCE // reallocarray
#define _XOPEN_SOURCE // strptime

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "stb_ds.h"
#include "sledger.h"

int
decimal_add(struct decimal *_a, struct decimal *_b, struct decimal *out)
{
	struct decimal a, b;
	if (_a->places > _b->places) {
		b = *_a;
		a = *_b;
	} else {
		a = *_a;
		b = *_b;
	}

	while (a.places < b.places) {
		long new_sig = 10 * a.sig;
		if (new_sig / 10 != a.sig) {
			return -1;
		}
		a.sig = new_sig;
		a.places++;
	}

	// TODO: deal with overflow
	out->sig = a.sig + b.sig;
	out->places = b.places;

	return 0;
}

void
decimal_print(struct decimal *val)
{
	char buf[22];
	int len = 0;

	long sig = val->sig;
	unsigned int places = val->places;

	// TODO: print minus properly
	if (sig < 0) {
		sig = -sig;
		putchar('-');
	}

	while (sig > 0) {
		buf[len++] = '0' + (sig % 10);
		places--;

		if (places == 0)
			buf[len++] = '.';
		sig /= 10;
	}

	for (len -= 1; len >= 0; len--) {
		putchar(buf[len]);
	}
}

static ssize_t
eat_whitespace(char *buf, size_t len, size_t min, bool sameline)
{
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

static ssize_t
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
		pl->account = strndup(account, account_len);
		if (pl->account == NULL)
			return -1;

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

		long new_sig = 10 * val.sig;
		if (new_sig / 10 != val.sig) {
			return -1;
		}

		new_sig += *buf - '0';
		if (new_sig < 0)
			return -1;

		val.sig = new_sig;
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

	pl->account = strndup(account, account_len);
	pl->val = val;
	pl->currency = strndup(currency, currency_len);
	pl->has_value = true;

	if (pl->currency == NULL)
		return -1;

	return olen - len;
}

static ssize_t
parse_posting(char *buf, size_t len, struct posting *p)
{
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

	int lines_without_values = 0;
	int line_without_value_index = -1;

	struct decimal total = {0};

	struct posting_line pl = {0};
	while (len && *buf == '\t') {
		pl = (struct posting_line){0};
		ssize_t used = parse_posting_line(buf, len, &pl);
		if (used == -1)
			return -1;

		lines_without_values += !pl.has_value;
		if (lines_without_values > 1)
			return -1;

		line_without_value_index = p->nlines;

		if (pl.has_value) {
			decimal_add(&total, &pl.val, &total);
			total.sig = -total.sig;
		}

		buf += used;
		len -= used;

		struct posting_line *newlines = reallocarray(p->lines, p->nlines + 1, sizeof(struct posting_line));
		if (newlines == NULL)
			return -1;

		memcpy(&newlines[p->nlines], &pl, sizeof(pl));
		p->lines = newlines;
		p->nlines++;
	}

	p->lines[line_without_value_index].val = total;
	
	return startlen - len;
}

static ssize_t
find_posting_end(char *buf, size_t count)
{
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

int
process_postings(void (*processor)(struct posting *posting, void *data), void *data)
{
	char buf[1024];
	struct posting posting = {0};
	size_t count = fread(buf, sizeof(char), sizeof(buf), stdin);

	char *ptr = buf;

	while (count) {
		ssize_t read = eat_whitespace(ptr, count, 0, false);
		ptr += read;
		count -= read;

		if (count == 0)
			break;

		posting = (struct posting){0};
		ssize_t out = find_posting_end(ptr, count);
		if (out == -1) {
			// could be the last posting which doesn't end in newlines, so try to parse it anyway
			if (feof(stdin)) {
				goto parse_posting;
			}

			memmove(buf, ptr, count);
			ptr = buf + count;
			read = fread(ptr, sizeof(char), sizeof(buf) - count, stdin);
			if (ferror(stdin))
				return -1;

			count += read;
			ptr = buf;
			continue;
		}

parse_posting:
		read = parse_posting(ptr, out == -1 ? count : out, &posting);
		if (read == -1) {
			return -1;
		}

		processor(&posting, data);

		ptr += read;
		count -= read;
	}

	return 0;
}
