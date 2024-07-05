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

// WARNING: accounts is destroyed and is no longer a valid string hash table
struct account {
	char *key;
	bool value;
} *accounts = NULL;

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

void account_processor(struct posting *posting, void *data) {
	for (int i = 0; i < arrlen(posting->lines); i++) {
		char *account_str = strdup(posting->lines[i].account);
		assert(account_str);
		shput(accounts, account_str, true);
	}
}

int main() {
	if (process_postings(account_processor, NULL) == -1) {
		return 1;
	}

	// WARNING: accounts is destroyed and is no longer a valid string hash table
	qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
	for (size_t i = 0; i < shlenu(accounts); i++) {
		printf("%s\n", accounts[i].key);
		free(accounts[i].key);
	}

	shfree(accounts);

	return 0;
}
