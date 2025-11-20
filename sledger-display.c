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

struct amount {
	char *key;
	struct decimal value;
};

struct account_tree {
	char *key;
	struct account_tree *value; // string hash map of account_tree
	struct amount *amount;
} *account_tree = NULL;

int add_account(char *account, struct decimal value, char *currency) {
	char *tokaccount = strdup(account);
	assert(tokaccount);
	char *tok = strtok(tokaccount, ":");
	struct account_tree *parenttree = NULL, **curtree = &account_tree;
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
	} while ((tok = strtok(NULL, ":")) != NULL);
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

struct currency_list {
	char *key;
	bool value;
} *currencies = NULL;

int main(int argc, char **argv) {
	int opt;
	bool balance = false;
	while ((opt = getopt(argc, argv, "bc:")) != -1) {
		switch (opt) {
		case 'b':
			balance = true;
			break;
		case 'c':
			shput(currencies, optarg, true);
			break;
		default:
			fprintf(stderr, "-%c: invalid opt", opt);
			return 1;

		}
	}

	char *line = NULL;
	size_t size = 0;
	ssize_t nread;

	while ((nread = getline(&line, &size, stdin)) != -1) {
		char *account = line;
		char *delim = strchr(line, '\t');
		if (delim == NULL) {
			fprintf(stderr, "expected tab on line '%s'\n", line);
		}
		char *valuestr = delim + 1;
		*delim = '\0';
		if (*valuestr == '\n') {
			fprintf(stderr, "expected value on line '%s'\n", line);
		}

		char *tok = strtok(valuestr, "\t");
		struct account_tree *parenttree = NULL, **curtree = &account_tree;
		do {
			struct decimal value = {0};
			ssize_t len = decimal_parse(&value, valuestr, strlen(valuestr));
			if (len < 0) {
				fprintf(stderr, "invalid decimal '%s'\n", valuestr);
				return 1;
			}

			char *aftervalue = valuestr + len;
			while (isspace(*aftervalue)) {
				aftervalue++;
			}

			if (*aftervalue == '\0') {
				fprintf(stderr, "expected currency after value\n");
			}

			char *currency = aftervalue;
			char *newline = strchr(currency, '\n');
			if (newline)
				*newline = '\0';

			if (currencies) {
				if (shgetp_null(currencies, currency) != NULL) {
					add_account(account, value, currency);
				}
			} else {
				add_account(account, value, currency);
			}
		} while ((tok = strtok(NULL, ":")) != NULL);
	}

	print_account_tree(account_tree, 0, 30);

	return 0;
}
