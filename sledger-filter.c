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
struct tm begin, end;
bool check_account, check_begin, check_end;

void filter_processor(struct posting *posting, void *data) {
	struct posting temp_posting;

	if (check_account) {
		bool found = false;
		for (int i = 0; i < arrlen(posting->lines); i++) {
			if (strncmp(account, posting->lines[i].account, strlen(account)) == 0) {
				found = true;
				break;
			}
		}

		if (!found)
			return;
	}

	temp_posting.time = begin;
	if (check_begin && tmcmp(&temp_posting, posting) > 0)
		return;

	temp_posting.time = end;
	if (check_end && tmcmp(&temp_posting, posting) < 0)
		return;

	print_posting(posting);
}

int main(int argc, char *argv[]) {
	int opt;
	char *dateend;
	while ((opt = getopt(argc, argv, "a:b:e:")) != -1) {
		switch (opt) {
		case 'a':
			check_account = true;
			account = optarg;
			break;
		case 'b':
			check_begin = true;
			dateend = strptime(optarg, "%Y-%m-%d", &begin);
			if (dateend == NULL) {
				fprintf(stderr, "-%c: invalid date\n", opt);
				return 1;
			}
			break;
		case 'e':
			check_end = true;
			dateend = strptime(optarg, "%Y-%m-%d", &end);
			if (dateend == NULL) {
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
