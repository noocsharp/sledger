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

#define EMPTY_CURRENCY_NAME "Empty_Curr"
#define EMPTY_CURRENCY_LEN sizeof(EMPTY_CURRENCY_NAME) - 1

static unsigned long line, col;

bool sl_balance_transactions = true;

static void
decimal_reduce(struct decimal *a)
{
	while (a->sig % 10 == 0 && a->places > 0) {
		a->places--;
		a->sig /= 10;
	}
}

int
decimal_mul(struct decimal *_a, struct decimal *_b, struct decimal *out)
{
	decimal_reduce(_a);
	decimal_reduce(_b);

	long product = _a->sig * _b->sig;
	if (_a->sig < 0 && _b->sig < 0 && product < 0 ||
		_a->sig > 0 && _b->sig > 0 && product < 0 ||
		_a->sig < 0 && _b->sig > 0 && product > 0 ||
		_a->sig > 0 && _b->sig < 0 && product > 0)
		return -1;

	out->sig = product;
	out->places = _a->places + _b->places; /* TODO: overflow? */

	decimal_reduce(out);

	return 0;
}

int
decimal_add(struct decimal *_a, struct decimal *_b, struct decimal *out)
{
	decimal_reduce(_a);
	decimal_reduce(_b);

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

	out->sig = sum;
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

int decimal_cmp(struct decimal *_a, struct decimal *_b)
{
	decimal_reduce(_a);
	decimal_reduce(_b);

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
			return -2;
		}
		a.sig = new_sig;
		a.places++;
	}

	long diff = a.sig - b.sig;
	if (diff == 0) {
		return 0;
	} else if (diff > 0) {
		return 1;
	} else {
		return -1;
	}
}

void
decimal_print(struct decimal *val, int minplaces)
{
	assert(val->places <= 20);

	char buf[44];
	int len = 0;

	long sig = val->sig;
	unsigned int places = val->places;

	if (sig < 0) {
		sig = -sig;
		putchar('-');
	}

	while (places--) {
		buf[len++] = '0' + (sig % 10);
		sig /= 10;
		if (minplaces > 0)
			minplaces--;
	}

	buf[len++] = '.';

	while (sig) {
		buf[len++] = '0' + (sig % 10);
		sig /= 10;
	}

	for (len -= 1; len >= 0; len--) {
		putchar(buf[len]);
	}

	while (minplaces-- > 0) {
		putchar('0');
	}
}

ssize_t decimal_parse(struct decimal *out, const char *buf, size_t len) {
	bool have_point = false;
	size_t startlen = len;

	bool neg = false;

	if (len == 0)
		return -1;

	if (*buf == '-') {
		neg = true;
		buf++;
		len--;
	} else if (*buf == '+') {
		buf++;
		len--;
	}

	if (len == 0 || (!isdigit(*buf) && *buf != '.')) {
		return -1;
	}

	while (len && (isdigit(*buf) || *buf == '.' || *buf == ',')) {
		if (*buf == ',') {
			buf++;
			len--;
			continue;
		}

		if (*buf == '.') {
			if (have_point)
				break;

			have_point = true;
			buf++;
			len--;
			continue;
		}

		if (have_point)
			out->places++;

		long new_sig = 10 * out->sig;
		if (new_sig / 10 != out->sig) {
			return -2;
		}

		new_sig += *buf - '0';
		if (new_sig < 0)
			return -3;

		out->sig = new_sig;
		buf++;
		len--;
	}

	if (neg) out->sig = -out->sig;
	return startlen - len;
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
	printf("%04d-%02d-%02d %s\n", 1900 + posting->time.tm_year, posting->time.tm_mon + 1, posting->time.tm_mday, posting->desc);

	for (int j = 0; j < arrlen(posting->lines); j++) {
		printf("\t%s  ", posting->lines[j].account);
		if (posting->lines[j].has_value) {
			decimal_print(&posting->lines[j].val, 2);
			printf("%s", posting->lines[j].currency);
		}

		putchar('\n');
	}

	putchar('\n');
}

static size_t currency_parse(char** currency, char* buf, size_t len)
{
	size_t currency_len = 0;
	
	*currency = buf;
	while (len && isgraph(*buf) && !isdigit(*buf) && *buf != '+' && *buf != '-') {
		currency_len++;
		buf++;
		len--;
		col++;
	}
	
	if (0 == currency_len) {
		*currency = NULL;
	}
	return currency_len;
}

static int
parse_posting_line(char *buf, size_t len, struct posting_line *pl)
{
	if (len < 1)
		return -1;

	if (*buf != '\t' && *buf != ' ') {
		fprintf(stderr, "%ld:%ld: expected posting line to start with a tab or a space\n", line, col);
		return -1;
	}

	buf++;
	len--;
	col++;
	int ret = eat_whitespace(buf, len, 0);
	if (ret != -1) {
		buf += ret;
		len -= ret;
	}
	char *account = buf;
	size_t account_len = 0;

	// first character of account music be alphabetic, rest can be alphanumeric
	if (len && isalpha(*buf)) {
		account_len++;
		buf++;
		len--;
		col++;
	}


	while (len && (isalnum(*buf) || *buf == ':' || *buf == '_' || (*buf == ' ' && buf[1] != ' '))) {
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

	ret = eat_whitespace(buf, len, 0);
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

	// Case of pre decimal currency like $-5
	char *currency = NULL;
	size_t currency_len = 0;
	if (!isdigit(*buf) && *buf != '-' && *buf != '+') {
		currency_len = currency_parse(&currency, buf, len);
		if (0 == currency_len || NULL == currency) {
			fprintf(stderr, "%ld:%ld: Wrong pre currency format\n", line, col);
			goto err1;
		}
		buf += currency_len;
		len -= currency_len;
		ret = eat_whitespace(buf, len, 0);
		if(-1 != ret)
		{
			buf += ret;
			len -= ret;
		}
	}
	// parse monetary value
	struct decimal val = {0};

	ret = decimal_parse(&val, buf, len);
	if (ret < 0) {
		switch (ret) {
		case -2:
			fprintf(stderr, "%ld:%ld: invalid decimal: overflow\n", line, col);
			break;
		default:
			fprintf(stderr, "%ld:%ld: invalid decimal %d\n", line, col, *buf);
		}
		goto err1;
	}

	buf += ret;
	len -= ret;
	col += ret;

	if (ret == 0) {
		fprintf(stderr, "%ld:%ld: expected value after account name '%s'\n", line, col, pl->account);
		goto err1;
	}

	pl->val = val;

	ret = eat_whitespace(buf, len, 0);
	buf += ret;
	len -= ret;

	if (len == 0 && currency != NULL) {
		fprintf(stderr, "%ld:%ld: expected currency\n", line, col);
		goto err1;
	} else if (currency == NULL) {
		// TODO: handle non-ascii values
		currency_len = currency_parse(&currency, buf, len);
		if (0 == currency_len || NULL == currency) {
			// Ledger supports empty currency we can go around this with naming the currency empty
#ifdef STRICT_CURRENCY_NAMING
			fprintf(stderr, "%ld:%ld: Wrong post currency format\n", line, col);
			goto err1;
#else
			currency_len = EMPTY_CURRENCY_LEN;
			currency = EMPTY_CURRENCY_NAME;
#endif
		}
		else
		{
			buf += currency_len;
			len -= currency_len;
		}
	}


	pl->currency = strndup(currency, currency_len);
	if (pl->currency == NULL)
		goto err1;

	ret = eat_whitespace(buf, len, 0);
	buf += ret;
	len -= ret;

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

bool isfirstcharinline(char *lineptr, size_t n, char searched_char) {
	int i = 0;
	for (; i < n; i++) {
		if (!isspace(lineptr[i]))
			break;
	}

	if(lineptr[i] == searched_char)
		return true;
	
	return false;
}

void free_posting(struct posting *posting) {
	if (posting->desc)
		free(posting->desc);

	posting->desc = NULL;

	for (size_t i = 0; i < arrlen(posting->lines); i++) {
		if (posting->lines[i].account) {
			free(posting->lines[i].account);
			posting->lines[i].account = NULL;
		}

		if (posting->lines[i].currency) {
			free(posting->lines[i].currency);
			posting->lines[i].currency = NULL;
		}
	}

	arrfree(posting->lines);
	posting->lines = NULL;
}

static int
parse_posting(char *buf, size_t len, struct posting *p)
{
	char *linestart = buf;
	size_t datelen = strlen("0000-00-00");
	if (len < datelen) {
		fprintf(stderr, "%ld:%ld: invalid date\n", line, col);
		fprintf(stderr, "length of line %d is less than the supposed length\n", len);
		goto err;
	}

	char *dateend = strptime(buf, "%Y-%m-%d", &p->time);
	if (dateend == NULL) {
		dateend = strptime(buf, "%Y/%m/%d", &p->time);
		if (dateend == NULL || (dateend - buf) != datelen ) {
			fprintf(stderr, "%ld:%ld: invalid date\n", line, col);
			fprintf(stderr, "not correct date\n");
			goto err;
		}
	}

	len -= dateend - buf;
	col += dateend - buf;
	buf = dateend;

	// Skip aux/secondary date if exists, Ledger feature, deprecated in hledger but kept for compatibility
	if (len >= datelen+1 && *buf == '=')
	{
		len -= datelen + 1;
		buf += datelen + 1;
		col += datelen + 1;
	}
	
	int ret = eat_whitespace(buf, len, 1);
	if (ret == -1) {
		fprintf(stderr, "%ld:%ld: expected description for posting\n", line, col);
		goto err;
	}

	buf += ret;
	len -= ret;

	char *nl = memchr(buf, '\n', len);
	if (nl == NULL) {
		col += len;
		fprintf(stderr, "%ld:%ld: expected newline after posting description\n", line, col);
		goto err;
	}

	p->desc = strndup(buf, nl - buf);
	if (p->desc == NULL) {
		fprintf(stderr, "%ld:%ld: strndup failed\n", line, col);
		goto err;
	}

	len -= nl - buf + 1;
	buf = nl + 1;

	int line_without_value_index = -1;
	struct decimal total = {0};
	struct posting_line pl = {0};
	ssize_t read;
	char *currency = NULL;
	bool multicurrency = false;

	char *lineptr = NULL;
	size_t n = 0;
	while (1) {
		char c;
		if ((c = getc(stdin)) != EOF) {
			ungetc(c, stdin);
			if (!isspace(c)) {
				break;
			}
		}

		if ((read = getline(&lineptr, &n, stdin)) == -1) {
			break;
		}

		line++; col = 1;
		if (isemptyline(lineptr, read))
			break;

		// Comment line
		if(isfirstcharinline(lineptr, read, ';'))
			continue;

		pl = (struct posting_line){0};
		ssize_t used = parse_posting_line(lineptr, read, &pl);
		if (used == -1)
			goto err;

		if (!pl.has_value) {
			if (line_without_value_index == -1) {
				line_without_value_index = arrlen(p->lines);
			} else {
				fprintf(stderr, "%ld:%ld: cannot have more than two lines in a posting without values\n", line, col);
				goto err;
			}
		}

		decimal_add(&total, &pl.val, &total);

		if (pl.currency) {
			if (currency && strcmp(currency, pl.currency) != 0) {
				multicurrency = true;
				currency = NULL;
			}

			if (!multicurrency)
				currency = pl.currency;
		}

		arrput(p->lines, pl);
	}

	if (read == -1 && !feof(stdin)) {
		fprintf(stderr, "%ld:%ld: getline read failed\n", line, col);
		goto err;
	}

	if (arrlen(p->lines) < 2) {
		fprintf(stderr, "%ld:%ld: expected at least 2 posting lines after posting description\n", line, col);
		goto err;
	}

	if (line_without_value_index != -1) {
		if (multicurrency) {
			fprintf(stderr, "%ld:%ld: cannot infer value in transaction with different currencies\n", line, col);
			goto err;
		}

		total.sig = -total.sig;
		p->lines[line_without_value_index].val = total;

		assert(currency);
		char *duped_currency = strdup(currency);
		if (duped_currency == NULL) {
			fprintf(stderr, "%ld:%ld: strdup failed\n", line, col);
			goto err;
		}
		p->lines[line_without_value_index].currency = duped_currency;
	} else if (!multicurrency) {
		if (sl_balance_transactions && total.sig != 0) {
			fprintf(stderr, "%ld:%ld: transaction does not balance\n", line, col);
			goto err;
		}
	}

	if (lineptr)
		free(lineptr);

	return 0;

err:
	free_posting(p);

	if (lineptr) free(lineptr);
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

		// Comment line
		// if(*lineptr == ';')
		// 	continue;
		if(isfirstcharinline(lineptr, read, ';'))
			continue;

		// Todo: Add directive skipping or support

		posting = (struct posting){0};

		int ret = parse_posting(lineptr, read, &posting);
		if (ret == -1) {
			goto error;
		}

		processor(&posting, data);

		free_posting(&posting);
	}

	if (lineptr) {
		free(lineptr);
		lineptr = NULL;
	}

	if (feof(stdin))
		return 0;

error:
	if (lineptr)
		free(lineptr);
	return -1;
}

int posting_dup(struct posting *dst, const struct posting *src) {
	dst->time = src->time;
	dst->desc = strdup(src->desc);
	if (dst->desc == NULL)
		goto err;

	for (int i = 0; i < arrlen(src->lines); i++) {
		struct posting_line new_line = src->lines[i];
		if (src->lines[i].account) {
			new_line.account = strdup(src->lines[i].account);
			if (new_line.account == NULL)
				goto err;
		} else {
			new_line.account = NULL;
		}

		if (src->lines[i].currency) {
			new_line.currency = strdup(src->lines[i].currency);
			if (new_line.currency == NULL) {
				free(new_line.account);
				goto err;
			}
		} else {
			new_line.currency = NULL;
		}

		new_line.has_value = src->lines[i].has_value;
		new_line.val = src->lines[i].val;
		arrput(dst->lines, new_line);
	}

	return 0;

err:
	free_posting(dst);
	return -1;
}
