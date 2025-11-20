#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sledger.h"

bool show_zero_accounts;

struct account_data {
	char *key; /* currency */
	struct decimal value; /* quantity */
};

struct account {
	char *key;
	struct account_data *value;
} *accounts = NULL;

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

void account_processor(struct posting *posting, void *data) {
	struct decimal val;
	for (int i = 0; i < arrlen(posting->lines); i++) {
		struct account *account = shgetp_null(accounts, posting->lines[i].account);
		if (account == NULL) {
			char *account = strdup(posting->lines[i].account);
			char *currency = strdup(posting->lines[i].currency);
			assert(account);
			assert(currency);
			struct account_data *account_data = NULL;
			shput(account_data, currency, posting->lines[i].val);
			shput(accounts, account, account_data);
		} else {
			struct account_data *currency_entry = shgetp_null(account->value, posting->lines[i].currency);

			if (currency_entry == NULL) {
				char *currency = strdup(posting->lines[i].currency);
				assert(currency);
				shput(account->value, currency, posting->lines[i].val);
			} else {
				decimal_add(&currency_entry->value, &posting->lines[i].val, &currency_entry->value);
			}
		}
	}
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "z")) != -1) {
		switch (opt) {
		case 'z':
			show_zero_accounts = true;
			break;
		default:
			fprintf(stderr, "-%c: invalid opt", opt);
			return 1;

		}
	}

	if (process_postings(account_processor, NULL) == -1) {
		return 1;
	}

	// WARNING: accounts is destroyed and is no longer a valid string hash table
	qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
	for (size_t i = 0; i < shlenu(accounts); i++) {
		if (!show_zero_accounts) {
			bool any_nonzero = false;
			for (size_t j = 0; j < shlenu(accounts[i].value); j++) {
				if (accounts[i].value[j].value.sig != 0) {
					any_nonzero = true;
					break;
				}
			}

			if (!any_nonzero)
				continue;
		}


		printf("%s\t", accounts[i].key);
		for (size_t j = 0; j < shlenu(accounts[i].value); j++) {
			if (!show_zero_accounts && accounts[i].value[j].value.sig == 0) {
				continue;
			}

			struct account_data *data = &accounts[i].value[j];
			decimal_print(&data->value, 2);
			printf(" %s\t", data->key);
		}
		putchar('\n');
	}

	return 0;
}
