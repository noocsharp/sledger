#define _XOPEN_SOURCE /* strptime */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sledger.h"

char *account;
struct decimal running_total;

struct currency_amounts {
	char *key; // currency
	struct decimal value;
} *currency_amounts;

void register_processor(struct posting *posting, void *data) {
	struct posting temp_posting;

	struct currency_amounts *posting_currency_amounts = NULL;
	for (int i = 0; i < arrlen(posting->lines); i++) {
		if (strncmp(account, posting->lines[i].account, strlen(account)) == 0) {
			// for posting
			struct currency_amounts *entry = shgetp_null(posting_currency_amounts, posting->lines[i].currency);
			if (entry == NULL) {
				shput(posting_currency_amounts, posting->lines[i].currency, (struct decimal){0});
				entry = shgetp(posting_currency_amounts, posting->lines[i].currency);
			}

			decimal_add(&entry->value, &posting->lines[i].val, &entry->value);

			// for global
			entry = shgetp_null(currency_amounts, posting->lines[i].currency);
			if (entry == NULL) {
				char *currency = strdup(posting->lines[i].currency);
				assert(currency);
				shput(currency_amounts, currency, (struct decimal){0});
				entry = shgetp(currency_amounts, posting->lines[i].currency);
			}

			decimal_add(&entry->value, &posting->lines[i].val, &entry->value);
		}
	}

	if (shlen(posting_currency_amounts) != 0) {
		char buf[16];
		printf("%04d-%02d-%02d %s\t", 1900 + posting->time.tm_year, posting->time.tm_mon + 1, posting->time.tm_mday, posting->desc);
		for (int i = 0; i < shlen(posting_currency_amounts); i++) {
			decimal_print(&posting_currency_amounts[i].value, 2);
			printf(" %s\t", posting_currency_amounts[i].key);
		}

		for (int i = 0; i < shlen(currency_amounts); i++) {
			decimal_print(&currency_amounts[i].value, 2);
			printf(" %s\t", currency_amounts[i].key);
		}
		putchar('\n');
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s account\n", argv[0]);
		return 1;
	}

	account = argv[1];

	if (process_postings(register_processor, NULL) == -1) {
		return 1;
	}

	return 0;
}
