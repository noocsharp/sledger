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

struct posting *postings;

int flowcmp(const void *_a, const void *_b) {
	struct posting *a = (struct posting *) _a;
	struct posting *b = (struct posting *) _b;
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

	int ret = decimal_cmp(&da, &db);
	if (ret == -2) {
		fprintf(stderr, "decimal_cmp: failure\n");
		exit(1);
	}

	return ret;
}

int (*cmp)(const void *a, const void *b) = NULL;

void sort_processor(struct posting *posting, void *data) {
	(void)data;
	struct posting copy = {};
	if (posting_dup(&copy, posting) != 0) {
		fprintf(stderr, "posting_dup: failure\n");
		exit(1);
	}
	arrput(postings, copy);
}

int main(int argc, char *argv[]) {
	int opt;
	bool invert = false;
	while ((opt = getopt(argc, argv, "dfi")) != -1) {
		switch (opt) {
		case 'i':
			invert = true;
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

	qsort(postings, arrlen(postings), sizeof(*postings), cmp);

	if (invert) {
		for (int i = arrlen(postings) - 1; i >= 0; i--) {
			struct posting *posting = postings + i;
			print_posting(posting);
		}
	} else {
		for (int i = 0; i < arrlen(postings); i++) {
			struct posting *posting = postings + i;
			print_posting(posting);
		}
	}

	return 0;
}
