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

char **accounts;
struct tm begin = {.tm_year = -10000, .tm_mday = 1}, end = {.tm_year = 10000, .tm_mday = 1};

void filter_processor(struct posting *posting, void *data) {
	struct posting temp_posting;

	if (accounts) {
		bool found = false;
		for (int i = 0; i < arrlen(posting->lines); i++) {
			for (int j = 0; j < arrlen(accounts); j++) {
				if (strncmp(accounts[j], posting->lines[i].account, strlen(accounts[j])) == 0) {
					found = true;
					break;
				}
			}
		}

		if (!found)
			return;
	}

	temp_posting.time = begin;
	if (tmcmp(&temp_posting, posting) > 0)
		return;

	temp_posting.time = end;
	if (tmcmp(&temp_posting, posting) < 0)
		return;

	print_posting(posting);
}

int main(int argc, char *argv[]) {
	int opt;
	char *dateend;
	while ((opt = getopt(argc, argv, "a:b:d:e:")) != -1) {
		switch (opt) {
		case 'a':
			arrput(accounts, optarg);
			break;
		case 'b':
			dateend = strptime(optarg, "%Y-%m-%d", &begin);
			if (dateend == NULL || *dateend != 0) {
				fprintf(stderr, "-%c: invalid date\n", opt);
				return 1;
			}
			break;
		case 'e':
			dateend = strptime(optarg, "%Y-%m-%d", &end);
			if (dateend == NULL || *dateend != 0) {
				fprintf(stderr, "-%c: invalid date\n", opt);
				return 1;
			}
			break;
		default:
		}
	}

	if (process_postings(filter_processor, NULL) == -1) {
		return 1;
	}

	return 0;
}
