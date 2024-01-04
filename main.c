#include <stdio.h>
#include <string.h>
#include <ctype.h>

struct posting {
	int year;
	int month;
	int day;
	char *desc;
	char *account;
};

struct posting_line {
	char *account;
	int val;
	char *currency;
};

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
		pl->account = strndup(account, account_len);
		// TODO signal that value is not valid
		pl->val = 0;
		pl->currency = NULL;
		return olen - len + 1;
	}

	if (!isspace(*buf) || *buf == '\n')
		return -1;

	do {
		buf++;
		len--;
	} while (len && (isspace(*buf) && *buf != '\n'));

	if (len == 0)
		return -1;

	int val = 0;

	while (len && isdigit(*buf)) {
		val = 10 * val + *buf - '0';
		buf++;
		len--;
	}

	while (len && (isspace(*buf) && *buf != '\n')) {
		buf++;
		len--;
	}

	char *currency = buf;
	size_t currency_len = 0;
	while (len && isalpha(*buf)) {
		currency_len++;
		buf++;
		len--;
	}

	while (len && isspace(*buf)) {
		if (*buf == '\n')
			break;

		buf++;
		len--;
	}

	buf++;
	len--;

	pl->account = strndup(account, account_len);
	pl->val = val;
	pl->currency = strndup(currency, currency_len);

	if (pl->account == NULL || pl->currency == NULL)
		return -1;

	return olen - len;
}

int main() {
	char buf[1024];
	struct posting posting = {0};
	size_t count = fread(buf, sizeof(char), sizeof(buf), stdin);

	char *ptr = buf;

	int year = 0;
	while (ptr - buf < count && isdigit(*ptr)) {
		year = year * 10 + *ptr - '0';
		ptr++;
	}

	if (ptr - buf >= count)
		return 1;

	if (*ptr != '-') {
		return 1;
	}

	ptr++;

	posting.year = year;

	int month = 0;
	int mlen = 0;
	while (ptr - buf < count && isdigit(*ptr) && mlen <= 2) {
		month = month * 10 + *ptr - '0';
		mlen++;
		ptr++;
	}

	if (month <= 0 || month > 12)
		return 1;

	if (ptr - buf >= count)
		return 1;

	if (*ptr != '-') {
		return 1;
	}

	ptr++;

	posting.month = month;

	int day = 0;
	int dlen = 0;
	while (ptr - buf < count && isdigit(*ptr) && dlen <= 2) {
		day = day * 10 + *ptr - '0';
		dlen++;
		ptr++;
	}

	// TODO: base off of days in month
	if (day <= 0 || day > 31)
		return 1;

	if (ptr - buf < count && *ptr != ' ') {
		return 1;
	}

	ptr++;

	posting.day = day;

	if (ptr - buf >= count)
		return 1;

	char *nl = memchr(ptr, '\n', count - (ptr - buf));
	posting.desc = strndup(ptr, nl - ptr);

	struct posting_line pl = {0};
	ptr = nl + 1;
	size_t len = count - (nl + 1 - buf);

	fprintf(stderr, "%d-%d-%d %s\n", year, month, day, posting.desc);
	while (len && *ptr == '\t') {
		ssize_t used = parse_posting_line(ptr, len, &pl);
		if (used == -1) {
			fprintf(stderr, "%d\n", used);
			return 1;
		}

		ptr += used;
		len -= used;
		fprintf(stderr, "%s %d %s\n", pl.account, pl.val, pl.currency);
	}
}
