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

static unsigned long line, col;
static int count = 0;

static void
decimal_reduce(struct decimal *a, struct decimal *b)
{
	while (a->sig % 10 == 0 && b->sig % 10 == 0 && a->places > 0 && b->places > 0) {
		a->places--;
		b->places--;
		a->sig /= 10;
		b->sig /= 10;
	}
}

int
decimal_add(struct decimal *_a, struct decimal *_b, struct decimal *out)
{
	decimal_reduce(_a, _b);

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

void decimal_abs(struct decimal *in, struct decimal *out)
{
	*out = *in;

	if (out->sig < 0) {
		out->sig = -out->sig;
	}
}

int decimal_leq(struct decimal *_a, struct decimal *_b)
{
	decimal_reduce(_a, _b);

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

	return a.sig - b.sig;
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

int tmcmp(const struct posting *a, const struct posting *b) {
	if (a->time.tm_year != b->time.tm_year) return a->time.tm_year - b->time.tm_year;
	if (a->time.tm_mon != b->time.tm_mon) return a->time.tm_mon - b->time.tm_mon;
	return a->time.tm_mday - b->time.tm_mday;
}

static int
eat_whitespace(char *buf, size_t len, size_t min)
{
	size_t startlen = len;
	while (len && isspace(*buf)) {
		if (*buf == '\n') {
			break;
		}

		col++;
		buf++;
		len--;
	}

	if (startlen - len < min)
		return -1;

	return startlen - len;
}

void
print_posting(struct posting *posting)
{
	printf("%04d-%02d-%02d %s\n", 1900 + posting->time.tm_year, posting->time.tm_mon, posting->time.tm_mday, posting->desc);

	for (int j = 0; j < arrlen(posting->lines); j++) {
		printf("\t%s ", posting->lines[j].account);
		if (posting->lines[j].has_value) {
			decimal_print(&posting->lines[j].val);
			printf("%s", posting->lines[j].currency);
		}

		putchar('\n');
	}

	putchar('\n');
}

static int
parse_posting_line(char *buf, size_t len, struct posting_line *pl)
{
	if (len < 1)
		return -1;

	if (*buf != '\t') {
		fprintf(stderr, "%ld:%ld: expected posting line to start with a tab\n", line, col);
		return -1;
	}

	buf++;
	len--;
	col++;

	char *account = buf;
	size_t account_len = 0;

	// first character of account music be alphabetic, rest can be alphanumeric
	if (len && isalpha(*buf)) {
		account_len++;
		buf++;
		len--;
		col++;
	}

	while (len && (isalnum(*buf) || *buf == ':' || *buf == '_')) {
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
		goto err1;
	}

	int ret = eat_whitespace(buf, len, 1);
	if (ret != -1) {
		buf += ret;
		len -= ret;
	}

	// the case where the line's value should be inferred
	if (*buf == '\n') {
		buf++;
		len--;
		return 0;
	}

	pl->has_value = true;

	if (len == 0)
		goto err1;

	struct decimal val = {0};
	bool neg = false;
	if (*buf == '-') {
		neg = true;
		buf++;
		len--;
		col++;
	}

	// parse monetary value
	bool have_point = false, present = false;
	while (len && (isdigit(*buf) || *buf == '.' || *buf == ',')) {
		present = true;
		if (*buf == ',') {
			buf++;
			len--;
			col++;
			continue;
		}

		if (*buf == '.') {
			if (have_point)
				break;

			have_point = true;
			buf++;
			len--;
			col++;
			continue;
		}

		if (have_point)
			val.places++;

		long new_sig = 10 * val.sig;
		if (new_sig / 10 != val.sig) {
			goto err1;
		}

		new_sig += *buf - '0';
		if (new_sig < 0)
			goto err1;

		val.sig = new_sig;
		buf++;
		len--;
		col++;
	}

	if (!present) {
		fprintf(stderr, "%ld:%ld: expected value after account name '%s'\n", line, col, pl->account);
		goto err1;
	}

	if (neg) val.sig = -val.sig;

	pl->val = val;

	ret = eat_whitespace(buf, len, 0);
	buf += ret;
	len -= ret;
	col += ret;

	// TODO: handle non-ascii values

	char *currency = buf;
	size_t currency_len = 0;
	while (len && isalpha(*buf)) {
		currency_len++;
		buf++;
		len--;
		col++;
	}

	pl->currency = strndup(currency, currency_len);
	if (pl->currency == NULL)
		goto err1;

	ret = eat_whitespace(buf, len, 0);
	buf += ret;
	len -= ret;
	col += ret;

	// account for newline
	buf++;
	len--;

	return 0;

err1:
	free(pl->account);
	return -1;
}

bool isemptyline(char *lineptr, size_t n) {
	for (int i = 0; i < n; i++) {
		if (!isspace(lineptr[i]))
			return false;
	}

	return true;
}

static int
parse_posting(char **buf, size_t *bufsize, size_t *len, struct posting *p)
{
	char **lineptr = buf;
	char *linestart = *buf;
	if (*len < strlen("0000-00-00")) {
		fprintf(stderr, "%ld:%ld: invalid date\n", line, col);
		goto err0;
	}

	char *dateend = strptime(*buf, "%Y-%m-%d", &p->time);
	if (dateend == NULL) {
		fprintf(stderr, "%ld:%ld: invalid date\n", line, col);
		goto err0;
	}

	*len -= dateend - *buf;
	col += dateend - *buf;
	*buf = dateend;

	int ret = eat_whitespace(*buf, *len, 1);
	if (ret == -1) {
		fprintf(stderr, "%ld:%ld: expected description for posting\n", line, col);
		goto err0;
	}

	*buf += ret;
	*len -= ret;
	col += ret;

	char *nl = memchr(*buf, '\n', *len);
	if (nl == NULL) {
		col += *len;
		fprintf(stderr, "%ld:%ld: expected newline after posting description\n", line, col);
		goto err0;
	}

	p->desc = strndup(*buf, nl - *buf);

	*len -= nl - *buf + 1;
	*buf = nl + 1;

	int line_without_value_index = -1;
	struct decimal total = {0};
	struct posting_line pl = {0};
	ssize_t read;
	char *currency = NULL;

	*lineptr = linestart;
	while ((read = getline(lineptr, bufsize, stdin)) != -1) {
		line++; col = 1;
		if (isemptyline(*lineptr, read))
			break;

		pl = (struct posting_line){0};
		ssize_t used = parse_posting_line(*lineptr, read, &pl);
		if (used == -1)
			goto err1;

		if (!pl.has_value) {
			if (line_without_value_index == -1) {
				line_without_value_index = arrlen(p->lines);
			} else {
				fprintf(stderr, "%ld:%ld: cannot have more than two lines in a posting without values\n", line, col);
				goto err2;
			}
		}

		decimal_add(&total, &pl.val, &total);

		if (currency && pl.currency && strcmp(currency, pl.currency) != 0) {
			fprintf(stderr, "%ld:%ld: currency mismatch", line, col);
			goto err2;
		}

		if (pl.currency)
			currency = pl.currency;

		arrput(p->lines, pl);
	}

	if (read == -1 && !feof(stdin)) {
		fprintf(stderr, "%ld:%ld: getline read failed\n", line, col);
		goto err1;
	}

	if (arrlen(p->lines) < 2) {
		fprintf(stderr, "%ld:%ld: expected at least 2 posting lines after posting description\n", line, col);
		goto err1;
	}

	total.sig = -total.sig;
	if (line_without_value_index != -1) {
		p->lines[line_without_value_index].val = total;
		p->lines[line_without_value_index].currency = strdup(currency);
	}

	return 0;

err2:
	free(pl.account);
	if (pl.currency) free(pl.currency);
err1:
	free(p->desc);
	p->desc = NULL;
err0:
	*lineptr = linestart;
	return -1;
}

int
process_postings(void (*processor)(struct posting *posting, void *data), void *data)
{
	struct posting posting = {0};
	char *lineptr = NULL;
	size_t n = 0;
	line = 0;
	col = 1;

	ssize_t read;
	while ((read = getline(&lineptr, &n, stdin)) != -1) {
		line++; col = 1;
		if (isemptyline(lineptr, read))
			continue;

		posting = (struct posting){0};

		int ret = parse_posting(&lineptr, &n, &read, &posting);
		if (ret == -1) {
			fprintf(stderr, "parsing failed\n");
			goto error;
		}

		processor(&posting, data);

		if (posting.desc)
			free(posting.desc);
		for (size_t i = 0; i < arrlen(posting.lines); i++) {
			if (posting.lines[i].account) free(posting.lines[i].account);
			if (posting.lines[i].currency) free(posting.lines[i].currency);
		}
		arrfree(posting.lines);
	}

	if (lineptr) {
		free(lineptr);
		lineptr = NULL;
	}

	if (feof(stdin))
		return 0;

error:

	if (posting.desc)
		free(posting.desc);
	for (size_t i = 0; i < arrlen(posting.lines); i++) {
		if (posting.lines[i].account) free(posting.lines[i].account);
		if (posting.lines[i].currency) free(posting.lines[i].currency);
	}
	arrfree(posting.lines);
	if (lineptr)
		free(lineptr);
	return -1;
}

int posting_dup(struct posting *dst, const struct posting *src) {
	dst->time = src->time;
	dst->desc = strdup(src->desc);
	if (dst->desc == NULL)
		return -1;

	for (int i = 0; i < arrlen(src->lines); i++) {
		struct posting_line new_line = src->lines[i];
		if (src->lines[i].account) {
			new_line.account = strdup(src->lines[i].account);
			// TODO: free things here?
			if (new_line.account == NULL)
				return -1;
		} else {
			new_line.account = NULL;
		}

		if (src->lines[i].currency) {
			new_line.currency = strdup(src->lines[i].currency);
			// TODO: free things here?
			if (new_line.currency == NULL)
				return -1;
		} else {
			new_line.currency = NULL;
		}

		new_line.has_value = src->lines[i].has_value;
		new_line.val = src->lines[i].val;
		arrput(dst->lines, new_line);
	}

	return 0;
}
