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


int timesort(const void *a, const void *b) {
	time_t first = *((time_t *) a);
	time_t second = *((time_t *) b);
	return first - second;
}

enum {
	PERIOD_NONE,
	PERIOD_MONTHLY,
	PERIOD_YEARLY
};

time_t bucket_monthly(struct posting *posting) {
	struct tm date = posting->time;
	date.tm_sec = date.tm_min = date.tm_hour = 0;
	date.tm_mday = 1;

	return mktime(&date);
}

time_t bucket_yearly(struct posting *posting) {
	struct tm date = posting->time;
	date.tm_sec = date.tm_min = date.tm_hour = 0;
	date.tm_mday = 1;
	date.tm_mon = 0;

	return mktime(&date);
}

struct perioddata {
	char *format;
	time_t (*func)(struct posting *posting);
} perioddata[] = {
	[PERIOD_MONTHLY] = (struct perioddata){ "%B %Y", bucket_monthly },
	[PERIOD_YEARLY] = (struct perioddata){ "%Y", bucket_yearly },
};

struct perioddata *periodptr;

void aggregate_processor(struct posting *posting, void *data) {
	time_t time;
	if ((time = periodptr->func(posting)) < 0) {
		return;
	}

	struct accountmap *accountmap = NULL;
	if (hmgeti(periods, time) == -1) {
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
	while ((opt = getopt(argc, argv, "my")) != -1) {
		switch (opt) {
		case 'm':
			if (periodptr != NULL) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			periodptr = &perioddata[PERIOD_MONTHLY];
			break;
		case 'y':
			if (periodptr != NULL) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			periodptr = &perioddata[PERIOD_YEARLY];
			break;
		default:
			fprintf(stderr, "-%c: invalid opt", opt);
			return 1;

		}
	}

	if (periodptr == NULL) {
		fprintf(stderr, "%s: expected period argument\n", argv[0]);
		return 1;
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
		strftime(descbuf, sizeof(descbuf), periodptr->format, &time);
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
