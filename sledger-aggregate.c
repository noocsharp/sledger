#define _XOPEN_SOURCE
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sledger.h"

bool show_zero_accounts;

struct currencymap {
	char *key;
	struct decimal value;
};

struct accountmap {
	char *key;
	struct currencymap *value;
};

struct period {
	time_t key;
	struct accountmap *value;
} *periods = NULL;

// the keys of periods, but can be sorted later
time_t *period_list = NULL;

enum {
	AGG_NONE = 0,
	AGG_WEEKLY,
	AGG_MONTHLY,
	AGG_YEARLY
} period = AGG_NONE;

int timesort(const void *a, const void *b) {
	time_t first = *((time_t *) a);
	time_t second = *((time_t *) b);
	return first - second;
}

void aggregate_monthly(struct posting *posting, void *data) {
}

void aggregate_processor(struct posting *posting, void *data) {
	struct tm date = posting->time;
	date.tm_sec = date.tm_min = date.tm_hour = 0;
	date.tm_mday = 1;

	time_t time;
	if ((time = mktime(&date)) == -1) {
		perror("mktime:");
		return;
	}

	struct accountmap *accountmap = NULL;
	if (hmgeti(periods, time) == -1) {
		char datebuf[100];
		strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", &date);
		hmput(periods, time, NULL);
		arrput(period_list, time);
	}

	accountmap = hmget(periods, time);

	for (int i = 0; i < arrlen(posting->lines); i++) {
		struct currencymap *currencymap = NULL;
		char *account = NULL;
		if (shgeti(accountmap, posting->lines[i].account) == -1) {
			account = strdup(posting->lines[i].account);
			if (account == NULL) {
				perror("strdup:");
				return;
			}
			shput(accountmap, account, NULL);
		} else {
			account = shgets(accountmap, posting->lines[i].account).key;
		}

		currencymap = shget(accountmap, account);
		struct decimal val = {};
		char *currency = NULL;
		if (shgeti(currencymap, posting->lines[i].currency) == -1) {
			currency = strdup(posting->lines[i].currency);
			if (currency == NULL) {
				perror("strdup:");
				return;
			}

		} else {
			currency = shgets(currencymap, posting->lines[i].currency).key;
			val = shget(currencymap, posting->lines[i].currency);
		}

		decimal_add(&val, &posting->lines[i].val, &val);
		shput(currencymap, currency, val);
		shput(accountmap, account, currencymap);
	}
	
	hmput(periods, time, accountmap);
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "wmy")) != -1) {
		switch (opt) {
		case 'w':
			if (period != AGG_NONE) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			period = AGG_WEEKLY;
			break;
		case 'm':
			if (period != AGG_NONE) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			period = AGG_MONTHLY;
			break;
		case 'y':
			if (period != AGG_NONE) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			period = AGG_YEARLY;
			break;
		default:
			fprintf(stderr, "-%c: invalid opt", opt);
			return 1;

		}
	}

	if (process_postings(aggregate_processor, NULL) == -1) {
		return 1;
	}

	qsort(period_list, arrlen(period_list), sizeof(time_t), timesort);
	for (int i = 0; i < arrlen(period_list); i++) { 
		char datebuf[16];
		char descbuf[64];
		struct tm time;
		gmtime_r(&period_list[i], &time);
		strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", &time);
		strftime(descbuf, sizeof(descbuf), "%B %Y", &time);
		printf("%s %s\n", datebuf, descbuf);
		struct accountmap *accounts = hmget(periods, period_list[i]);

		for (int j = 0; j < shlen(accounts); j++) {

			struct currencymap *currencies = accounts[j].value;
			if (currencies == NULL)
				continue;

			for (int k = 0; k < shlen(currencies); k++) {
				printf("\t%s\t", accounts[j].key);
				decimal_print(&currencies[k].value, 2);
				printf(" %s\n", currencies[k].key);
			}
		}

		putchar('\n');
	}

	return 0;
}
