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
	PERIOD_WEEKLY,
	PERIOD_MONTHLY,
	PERIOD_YEARLY,
	PERIOD_ALL
};

time_t bucket_weekly(struct posting *posting) {
	struct tm date = posting->time;
	date.tm_sec = date.tm_min = date.tm_hour = 0;
	int week = (date.tm_yday - date.tm_wday + 10) / 7;
	date.tm_mday = 1;

	struct tm jan4 = {.tm_mday = 4, .tm_mon = 0, .tm_year = date.tm_year};
	mktime(&jan4);
	int doy_for_week = week * 7 - (jan4.tm_wday + 3);

	struct tm tm_for_week = {.tm_mday = 1 + doy_for_week, .tm_mon = 0, .tm_year = date.tm_year};

	return mktime(&tm_for_week);
}

void desc_weekly(char *buf, size_t size, struct tm *time) {
	int week = (time->tm_yday - time->tm_wday + 10) / 7;
	char fmt[64];
	snprintf(fmt, sizeof(fmt), "Week %d %%Y", week);
	strftime(buf, size, fmt, time);
}

time_t bucket_monthly(struct posting *posting) {
	struct tm date = posting->time;
	date.tm_sec = date.tm_min = date.tm_hour = 0;
	date.tm_mday = 1;

	return mktime(&date);
}

void desc_monthly(char *buf, size_t size, struct tm *time) {
	strftime(buf, size, "%B %Y", time);
}

time_t bucket_yearly(struct posting *posting) {
	struct tm date = posting->time;
	date.tm_sec = date.tm_min = date.tm_hour = 0;
	date.tm_mday = 1;
	date.tm_mon = 0;

	return mktime(&date);
}

void desc_yearly(char *buf, size_t size, struct tm *time) {
	strftime(buf, size, "%Y", time);
}

time_t bucket_all(struct posting *posting) {
	struct tm date = {};
	date.tm_sec = date.tm_min = date.tm_hour = date.tm_mon = 0;
	date.tm_mday = 1;
	date.tm_year = -1900;

	return mktime(&date);
}

void desc_all(char *buf, size_t size, struct tm *time) {
	strncpy(buf, "All Time", size);
}

struct perioddata {
	void (*format)(char *buf, size_t size, struct tm *time);
	time_t (*func)(struct posting *posting);
} perioddata[] = {
	[PERIOD_WEEKLY] = (struct perioddata){ desc_weekly, bucket_weekly },
	[PERIOD_MONTHLY] = (struct perioddata){ desc_monthly, bucket_monthly },
	[PERIOD_YEARLY] = (struct perioddata){ desc_yearly, bucket_yearly },
	[PERIOD_ALL] = (struct perioddata){ desc_all, bucket_all },
};

struct perioddata *periodptr;

void aggregate_processor(struct posting *posting, void *data) {
	time_t time;
	if ((time = periodptr->func(posting)) == -1) {
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
	while ((opt = getopt(argc, argv, "amwy")) != -1) {
		switch (opt) {
		case 'a':
			if (periodptr != NULL) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			periodptr = &perioddata[PERIOD_ALL];
			break;
		case 'w':
			if (periodptr != NULL) {
				fprintf(stderr, "-%c: period already provided", opt);
				return 1;
			}

			periodptr = &perioddata[PERIOD_WEEKLY];
			break;
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
		periodptr->format(descbuf, sizeof(descbuf), &time);
		printf("%s %s\n", datebuf, descbuf);
		struct accountmap *accounts = hmget(periods, period_list[i]);

		for (int j = 0; j < shlen(accounts); j++) {
			struct currencymap *currencies = accounts[j].value;
			if (currencies == NULL)
				continue;

			for (int k = 0; k < shlen(currencies); k++) {
				if (currencies[k].value.sig == 0)
					continue;

				printf("\t%s\t", accounts[j].key);
				decimal_print(&currencies[k].value, 2);
				printf(" %s\n", currencies[k].key);
			}
		}

		putchar('\n');
	}

	return 0;
}
