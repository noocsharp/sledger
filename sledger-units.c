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

bool show_zero_accounts;

struct unit_info {
	char *key; /* currency */
	bool value;
} *units;

void unit_processor(struct posting *posting, void *data) {
	for (int i = 0; i < arrlen(posting->lines); i++) {
		char *dup = strdup(posting->lines[i].currency);
		assert(dup);
		shput(units, dup, true);
	}
}

int main(int argc, char **argv) {

	if (process_postings(unit_processor, NULL) == -1) {
		return 1;
	}

	for (int i = 0; i < shlenu(units); i++) {
		printf("%s\n", units[i].key);
	}

	return 0;
}
