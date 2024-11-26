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

struct tm before, after;
bool check_before, check_after;

void sort_processor(struct posting *posting, void *data) {
	struct posting temp_posting;

	temp_posting.time = before;
	if (check_before && tmcmp(&temp_posting, posting) < 0)
		return;

	temp_posting.time = after;
	if (check_after && tmcmp(&temp_posting, posting) > 0)
		return;

	print_posting(posting);
}

int main(int argc, char *argv[]) {
	int opt;
	char *dateend;
	while ((opt = getopt(argc, argv, "a:b:")) != -1) {
		switch (opt) {
		case 'a':
			check_after = true;
			dateend = strptime(optarg, "%Y-%m-%d", &after);
			if (dateend == NULL) {
				fprintf(stderr, "-%c: invalid date\n", opt);
				return 1;
			}
			break;
		case 'b':
			check_before = true;
			dateend = strptime(optarg, "%Y-%m-%d", &before);
			if (dateend == NULL) {
				fprintf(stderr, "-%c: invalid date\n", opt);
				return 1;
			}
			break;
		default:
		}
	}

	if (process_postings(sort_processor, NULL) == -1) {
		return 1;
	}

	return 0;
}
