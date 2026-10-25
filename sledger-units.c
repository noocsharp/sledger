#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "sledger.h"

bool show_zero_accounts;

struct unit_info {
	char *key; /* currency */
	bool value;
} *units;

void unit_processor(struct posting *posting, void *data) {
	(void)data;
	for (int i = 0; i < arrlen(posting->lines); i++) {
		char *dup = strdup(posting->lines[i].currency);
		assert(dup);
		shput(units, dup, true);
	}
}

int main() {

	if (process_postings(unit_processor, NULL) == -1) {
		return 1;
	}

	for (int i = 0; i < shlen(units); i++) {
		printf("%s\n", units[i].key);
	}

	return 0;
}
