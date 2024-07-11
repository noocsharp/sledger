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

int tmcmp(const struct posting *a, const struct posting *b) {
	if (a->time.tm_year != b->time.tm_year) return a->time.tm_year - b->time.tm_year;
	if (a->time.tm_mon != b->time.tm_mon) return a->time.tm_mon - b->time.tm_mon;
	return a->time.tm_mday - b->time.tm_mday;
}

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
		printf("%04d-%02d-%02d %s\n", 1900 + posting->time.tm_year, posting->time.tm_mon, posting->time.tm_mday, posting->desc);

		for (int j = 0; j < arrlen(posting->lines); j++) {
			printf("\t%s ", posting->lines[j].account);
			if (posting->lines[j].has_value) {
				decimal_print(&posting->lines[j].val);
				printf("%s", posting->lines[j].currency);
			}

			putchar('\n');
		}

		putchar('\n');
	}

	return 0;
}
