#define _POSIX_C_SOURCE 200809L // strndup
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

static unsigned long line = 1, col = 1;

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

	long sum = a.sig + b.sig;
	if (a.sig < 0 && b.sig < 0 && sum > 0 ||
	    a.sig > 0 && b.sig > 0 && sum < 0)
		return -1;

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

static void
eat_empty_lines(char **buf, size_t *len)
{
	while (*len) {
		char *ptr = *buf;
		char *next_newline = memchr(*buf, '\n', *len);
		if (next_newline == NULL) {
			next_newline = *buf + *len;
		}

		bool has_content = false;
		while (ptr < next_newline) {
			if (!isspace(*ptr)) {
				has_content = true;
				break;
			}

			ptr++;
		}

		if (has_content)
			break;

		if (next_newline - *buf < *len) {
			*len -= next_newline - *buf + 1;
			*buf = next_newline + 1;
			line++;
		}
	}
}

static int
eat_whitespace(char *buf, size_t len, size_t min, bool sameline)
{
	size_t startlen = len;
	while (len && isspace(*buf)) {
		if (sameline && *buf == '\n') {
			break;
		}

		if (*buf == '\n') {
			line++;
			col = 1;
		} else {
			col++;
		}

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
	col++;

	char *account = buf;
	size_t account_len = 0;
	while (len && (isalpha(*buf) || *buf == ':')) {
		account_len++;
		buf++;
		len--;
		col++;
	}

	if (account_len == 0) {
		fprintf(stderr, "%ld:%ld: expected account name on posting line\n", line, col);
		return -1;
	}

	pl->account = strndup(account, account_len);
	if (pl->account == NULL)
		return -1;

	if (!isspace(*buf)) {
		fprintf(stderr, "%ld:%ld: expected whitespace after account name '%s'\n", line, col, pl->account);
		return -1;
	}

	int ret = eat_whitespace(buf, len, 1, true);
	if (ret != -1) {
		buf += ret;
		len -= ret;
	}

	// the case where the line's value should be inferred
	if (*buf == '\n') {
		buf++;
		len--;
		line++;
		col = 1;
		return olen - len + 1;
	}

	pl->has_value = true;

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
	bool have_point = false, present = false;
	while (len && (isdigit(*buf) || *buf == '.')) {
		present = true;
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

	if (!present) {
		fprintf(stderr, "%ld:%ld: expected value after account name '%s'\n", line, col, pl->account);
		return -1;
	}

	if (neg) val.sig = -val.sig;

	pl->val = val;

	eat_whitespace(buf, len, 0, true);

	// TODO: handle non-ascii values

	char *currency = buf;
	size_t currency_len = 0;
	while (len && isalpha(*buf)) {
		currency_len++;
		buf++;
		len--;
	}

	pl->currency = strndup(currency, currency_len);
	if (pl->currency == NULL)
		return -1;

	eat_whitespace(buf, len, 0, true);

	// account for newline
	buf++;
	len--;
	line++;
	col = 1;

	return olen - len;
}

bool isemptyline(char *lineptr, size_t n) {
	for (int i = 0; i < n; i++) {
		if (!isspace(lineptr[i]))
			return false;
	}

	return true;
}

static ssize_t
parse_posting(char **buf, size_t *bufsize, size_t *len, struct posting *p)
{
	size_t startlen = *len;

	if (*len < strlen("0000-00-00")) {
		fprintf(stderr, "%ld:%ld: invalid date\n", line, col);
		return -1;
	}

	char *dateend = strptime(*buf, "%Y-%m-%d", &p->time);
	if (dateend == NULL) {
		fprintf(stderr, "%ld:%ld: invalid date\n", line, col);
		return -1;
	}

	*len -= dateend - *buf;
	col += dateend - *buf;
	*buf = dateend;

	int ret = eat_whitespace(*buf, *len, 1, true);
	if (ret == -1) {
		fprintf(stderr, "%ld:%ld: expected description for posting\n", line, col);
		return -1;
	}

	*buf += ret;
	*len -= ret;
	col += ret;

	char *nl = memchr(*buf, '\n', *len);
	// TODO: what if this is the last line in the file?
	if (nl == NULL) {
		col += *len;
		fprintf(stderr, "%ld:%ld: expected newline after posting description\n", line, col);
		return -1;
	}

	p->desc = strndup(*buf, nl - *buf);

	*len -= nl - *buf + 1;
	*buf = nl + 1;
	line++;
	col = 1;

	int line_without_value_index = -1;

	struct decimal total = {0};

	struct posting_line pl = {0};
	ssize_t read;

	while ((read = getline(buf, bufsize, stdin)) != -1) {
		if (isemptyline(*buf, read))
			break;

		pl = (struct posting_line){0};
		ssize_t used = parse_posting_line(*buf, read, &pl);
		if (used == -1)
			return -1;

		if (!pl.has_value) {
			if (line_without_value_index == -1) {
				line_without_value_index = arrlen(p->lines);
			} else {
				fprintf(stderr, "%ld:%ld: cannot have more than two lines in a posting without values\n", line, col);
				return -1;
			}
		}

		decimal_add(&total, &pl.val, &total);

		arrput(p->lines, pl);
	}

	if (arrlen(p->lines) < 2) {
		fprintf(stderr, "%ld:%ld: expected at least 2 posting lines after posting description\n", line, col);
		return -1;
	}

	total.sig = -total.sig;
	if (line_without_value_index != -1)
		p->lines[line_without_value_index].val = total;

	return startlen - *len;
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
	struct posting posting = {0};
	char *lineptr = NULL;
	size_t n = 0;

	ssize_t read;
	while ((read = getline(&lineptr, &n, stdin)) != -1) {
		if (isemptyline(lineptr, read))
			continue;

		posting = (struct posting){0};

		int ret = parse_posting(&lineptr, &n, &read, &posting);
		if (ret == -1) {
			fprintf(stderr, "parsing failed\n");
			return -1;
		}

		processor(&posting, data);
	}

	if (feof(stdin))
		return 0;

	perror("process_postings");

	return -1;
}
