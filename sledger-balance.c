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

struct account_data {
	char *key; /* currency */
	struct decimal value; /* quantity */
};

struct account {
	char *key;
	struct account_data *value;
} *accounts = NULL;

struct account_tree {
	char *key;
	struct account_tree *value;
	char *path;
	struct decimal val;
	char *currency;
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
			struct account_tree *temp =  &shgets(*curtree, tok);
			temp->path = NULL;
			temp->val = (struct decimal){0};
		}
		parenttree = &shgets(*curtree, tok);
		// TODO: replace with something when account processing is fixed with new currency model
		//decimal_add(&parenttree->val, &shget(accounts, account), &parenttree->val);
		//parenttree->currency = shget(account_currencies, account);
		curtree = &shget(*curtree, tok);
	} while ((tok = strtok(NULL, ":")) != NULL);

	if (parenttree)
		parenttree->path = account;
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
			printf("%s", tree[i].key);
			for (int j = strlen(tree[i].key) + padding; j < maxwidth + 2; j++) {
				putchar(' ');
			}

			decimal_print(&tree[i].val, 2);
			/*
			ptrdiff_t currency_idx = shgeti(account_currencies, tree[i].path);
			assert(currency_idx != -1);
			printf(" %s", account_currencies[currency_idx].value);
			*/
			putchar('\n');
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

	if (tree) {
		for (int i = 0; i < shlen(accounts); i++) {
			account_tree_add(accounts[i].key);
		}

		print_account_tree(account_tree, 0, calculate_account_tree(account_tree, 0));
	// WARNING: accounts is destroyed and is no longer a valid string hash table
	} else {
		qsort(accounts, shlenu(accounts), sizeof(struct account), &strcmp_keys);
		for (size_t i = 0; i < shlenu(accounts); i++) {
			printf("%s\t", accounts[i].key);
			for (size_t j = 0; j < shlenu(accounts[i].value); j++) {
				struct account_data *data = &accounts[i].value[j];
				decimal_print(&data->value, 2);
				printf(" %s\t", data->key);
			}
			putchar('\n');
		}
	}

	return 0;
}
