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

bool tree = false;

struct account {
	char *key;
	struct decimal value;
} *accounts = NULL;

struct account_currency {
	char *key;
	char *value;
} *account_currencies = NULL;

struct account_tree {
	char *key;
	struct account_tree *value;
	char *path;
} *account_tree = NULL;

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

int account_tree_add(char *account) {
	char *tokaccount = strdup(account);
	assert(tokaccount);
	char *tok = strtok(tokaccount, ":");
	struct account_tree *parenttree = NULL, **curtree = &account_tree;
	do {
		if (*curtree == NULL || shgeti(*curtree, tok) == -1) {
			shput(*curtree, tok, NULL);
		}
		parenttree = &shgets(*curtree, tok);
		parenttree->path = NULL;
		curtree = &shget(*curtree, tok);
	} while ((tok = strtok(NULL, ":")) != NULL);

	if (parenttree)
		parenttree->path = account;
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
			if (tree)
				account_tree_add(account);
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

int calculate_account_tree(struct account_tree *tree, int padding) {
	int max_width = padding;
	for (int i = 0; i < shlenu(tree); i++) {
		if (tree[i].key != NULL) {
			if (max_width < padding + strlen(tree[i].key)) {
				max_width = padding + strlen(tree[i].key);
			}
			int subwidth = calculate_account_tree(tree[i].value, padding + 4);
			if (subwidth > max_width)
				max_width = subwidth;
		}
	}

	return max_width;
}

void print_account_tree(struct account_tree *tree, int padding, int maxwidth) {
	for (int i = 0; i < shlenu(tree); i++) {
		for (int j = 0; j < padding; j++) {
			putchar(' ');
		}

		if (tree[i].key != NULL) {
			if (tree[i].path) {
				struct decimal value = shget(accounts, tree[i].path);
				printf("%s", tree[i].key);
				for (int j = strlen(tree[i].key) + padding; j < maxwidth + 2; j++) {
					putchar(' ');
				}
				decimal_print(&value, 2);
				ptrdiff_t currency_idx = shgeti(account_currencies, tree[i].path);
				assert(currency_idx != -1);
				printf(" %s", account_currencies[currency_idx].value);
				putchar('\n');
			} else {
				printf("%s\n", tree[i].key);
			}
			print_account_tree(tree[i].value, padding + 4, maxwidth);
		}
	}
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "t")) != -1) {
		switch (opt) {
		case 't':
			tree = true;
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
	if (tree) {
		print_account_tree(account_tree, 0, calculate_account_tree(account_tree, 0));
	} else {
		qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
		for (size_t i = 0; i < shlenu(accounts); i++) {
			printf("%s\t", accounts[i].key);
			decimal_print(&accounts[i].value, 2);
			ptrdiff_t currency_idx = shgeti(account_currencies, accounts[i].key);
			assert(currency_idx != -1);
			printf(" %s", account_currencies[currency_idx].value);
			putchar('\n');
		}
	}

	return 0;
}
