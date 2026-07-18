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

char *account;

struct amount {
	char *key;
	struct decimal value;
};

struct account_tree {
	char *key;
	struct account_tree *value; // string hash map of account_tree
	struct amount *amount;
};

int add_account(struct account_tree **account_tree, char *account, struct decimal value, char *currency) {
	char *tokaccount = strdup(account);
	assert(tokaccount);
	char *saveptr;
	char *tok = strtok_r(tokaccount, ":", &saveptr);
	struct account_tree *parenttree = NULL, **curtree = account_tree;
	do {
		if (*curtree == NULL || shgeti(*curtree, tok) == -1) {
			shput(*curtree, tok, NULL);
			struct account_tree *temp = &shgets(*curtree, tok);
			temp->amount = NULL;
		}

		parenttree = &shgets(*curtree, tok);
		struct amount *amount_for_currency = shgetp_null(parenttree->amount, currency);
		if (amount_for_currency == NULL) {
			char *currency_duped = strdup(currency);
			assert(currency_duped);
			shput(parenttree->amount, currency_duped, (struct decimal){0});
			amount_for_currency = shgetp(parenttree->amount, currency_duped);
		}

		decimal_add(&amount_for_currency->value, &value, &amount_for_currency->value);
		curtree = &shget(*curtree, tok);
	} while ((tok = strtok_r(NULL, ":", &saveptr)) != NULL);
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

			for (int j = 0; j < shlenu(tree[i].amount); j++) {
				decimal_print(&tree[i].amount[j].value, 2);
				printf(" %s\t", tree[i].amount[j].key);
			}
			putchar('\n');
			print_account_tree(tree[i].value, padding + 4, maxwidth);
		}
	}
}

void tree_processor(struct posting *posting, void *data) {
	struct account_tree *account_tree = NULL;
	for (int i = 0; i < arrlen(posting->lines); i++) {
		//if (strncmp(posting->lines[i].account, account, strlen(account) < strlen(posting->lines[i].account) ? strlen(account) : strlen(posting->lines[i].account)))
			add_account(&account_tree, posting->lines[i].account, posting->lines[i].val, posting->lines[i].currency);
	}

	printf("%04d-%02d-%02d %s\n", 1900 + posting->time.tm_year, posting->time.tm_mon + 1, posting->time.tm_mday, posting->desc);
	print_account_tree(account_tree, 0, 30);
}

int main(int argc, char **argv) {
	int opt;
	while ((opt = getopt(argc, argv, "a:d")) != -1) {
		switch (opt) {
		case 'a':
			account = optarg;
			break;
		case 'd':
			sl_balance_transactions = false;
			break;
		default:
			fprintf(stderr, "-%c: invalid opt", opt);
			return 1;

		}
	}

	int ret = process_postings(tree_processor, NULL);

	return 0;
}
