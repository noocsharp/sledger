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

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

void account_processor(struct posting *posting, void *data) {
	struct decimal val;
	for (int i = 0; i < posting->nlines; i++) {
		ptrdiff_t idx = shgeti(accounts, posting->lines[i].account);
		if (idx == -1) {
			shput(accounts, posting->lines[i].account, posting->lines[i].val);
		} else {
			decimal_add(&accounts[idx].value, &posting->lines[i].val, &val);
			shput(accounts, posting->lines[i].account, val);
		}
	}
}

int main() {
	if (process_postings(account_processor, NULL) == -1) {
		fprintf(stderr, "Processing postings failed\n");
		return 1;
	}
	// WARNING: accounts is destroyed and is no longer a valid string hash table
	qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
	for (size_t i = 0; i < shlenu(accounts); i++) {
		printf("%s\t", accounts[i].key);
		decimal_print(&accounts[i].value);
		putchar('\n');
	}

	return 0;
}
