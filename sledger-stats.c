#define _XOPEN_SOURCE
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sledger.h"

// WARNING: accounts is destroyed and is no longer a valid string hash table
struct account {
	char *key;
	bool value;
} *accounts = NULL;

int strcmp_keys(const void *a, const void *b) {
	return strcmp(((struct account *)a)->key, ((struct account *)b)->key);
}

bool begin, end;

time_t target;
int count;

void account_processor(struct posting *posting, void *data) {
	time_t posting_time = mktime(&posting->time);
	if (posting_time == (time_t) -1) {
		fprintf(stderr, "invalid time\n");
		exit(1);
	}

	if (begin) {
		if (posting_time < target)
			target = posting_time;
	}

	if (end) {
		if (posting_time > target)
			target = posting_time;
	}

	if (count) {
		count++;
	}
}

int main(int argc, char *argv[]) {
	int opt;
	while ((opt = getopt(argc, argv, "bce")) != -1) {
		switch (opt) {
		case 'b':
			begin = true;
			target = LONG_MAX;
			break;
		case 'e':
			end = true;
			target = LONG_MIN;
			break;
		case 'c':
			count = true;
			break;
		default:
			fprintf(stderr, "-%c: invalid opt", opt);
			return 1;
		}
	}

	if (!begin && !end && !count) {
		fprintf(stderr, "%s: expected a flag\n", argv[0]);
		return 1;
	}

	if (process_postings(account_processor, NULL) == -1) {
		return 1;
	}

	if (begin || end) {
		struct tm *time = gmtime(&target);
		char timestr[256];
		strftime(timestr, sizeof(timestr), "%Y-%m-%d", time);
		printf("%s\n", timestr);
	}

	if (count) {
		printf("%d\n", count);
	}

	return 0;
}
