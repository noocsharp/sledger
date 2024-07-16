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

struct posting *sorted_postings;

int factor = 1;

int flowcmp(const struct posting *a, const struct posting *b) {
	struct decimal da = {}, db = {};

	struct decimal temp;
	for (int i = 0; i < arrlen(a->lines); i++) {
		decimal_abs(&a->lines[i].val, &temp);
		decimal_add(&da, &temp, &da);
	}

	for (int i = 0; i < arrlen(b->lines); i++) {
		decimal_abs(&b->lines[i].val, &temp);
		decimal_add(&db, &temp, &db);
	}

	return decimal_leq(&da, &db);
}

int (*cmp)(const struct posting *a, const struct posting *b) = NULL;

void sort_processor(struct posting *posting, void *data) {
	int left = 0;
	int right = arrlen(sorted_postings) - 1;
	int mid;

	while (left <= right) {
		mid = left + (right - left) / 2;

		if (factor * cmp(posting, &sorted_postings[mid]) < 0) {
			right = mid - 1;
		} else {
			left = mid + 1;
		}
	}

	arrins(sorted_postings, left, (struct posting){});
	assert(posting_dup(sorted_postings + left, posting) >= 0);
}

int main(int argc, char *argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "dfi")) != -1) {
		switch (opt) {
		case 'i':
			factor = -1;
			break;
		case 'd':
			cmp = tmcmp;
			break;
		case 'f':
			cmp = flowcmp;
			break;
		default:
		}
	}

	if (cmp == NULL)
		return 1;

	if (process_postings(sort_processor, NULL) == -1) {
		return 1;
	}

	for (int i = 0; i < arrlen(sorted_postings); i++) {
		struct posting *posting = sorted_postings + i;
		print_posting(posting);
	}

	return 0;
}
