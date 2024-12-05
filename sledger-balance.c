#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sledger.h"

struct account {
	char *key;
	struct decimal value;
} *accounts = NULL;

struct account_currency {
	char *key;
	char *value;
} *account_currencies = NULL;

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

void account_processor(struct posting *posting, void *data) {
	struct decimal val;
	for (int i = 0; i < arrlen(posting->lines); i++) {
		ptrdiff_t idx = shgeti(accounts, posting->lines[i].account);
		if (idx == -1) {
			char *account = strdup(posting->lines[i].account);
			char *account_currency = strdup(posting->lines[i].currency);
			assert(account);
			assert(account_currency);
			shput(accounts, account, posting->lines[i].val);
			shput(account_currencies, account, account_currency);
		} else {
			decimal_add(&accounts[idx].value, &posting->lines[i].val, &val);
			ptrdiff_t currency_idx = shgeti(account_currencies, posting->lines[i].account);
			assert(currency_idx != -1);
			if (strcmp(account_currencies[currency_idx].value, posting->lines[i].currency) != 0) {
				fprintf(stderr, "currency mismatch for account %s", posting->lines[i].account);
			}
			shput(accounts, accounts[idx].key, val);
		}
	}
}

int main() {
	if (process_postings(account_processor, NULL) == -1) {
		return 1;
	}
	// WARNING: accounts is destroyed and is no longer a valid string hash table
	qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
	for (size_t i = 0; i < shlenu(accounts); i++) {
		printf("%s\t", accounts[i].key);
		decimal_print(&accounts[i].value);
		ptrdiff_t currency_idx = shgeti(account_currencies, accounts[i].key);
		assert(currency_idx != -1);
		printf(" %s", account_currencies[currency_idx].value);
		putchar('\n');
	}

	return 0;
}
