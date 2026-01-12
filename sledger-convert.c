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

struct conversion {
	char *key; /* from */
	char *value; /* to */
	struct decimal amount;
} *conversions;

void convert_processor(struct posting *posting, void *data) {
	struct posting dup = {};
	posting_dup(&dup, posting);

	for (int i = 0; i < arrlen(posting->lines); i++) {
		struct conversion *conversion = shgetp_null(conversions, posting->lines[i].currency);
		if (conversion) {
			dup.lines[i].currency = conversion->value;
			decimal_mul(&posting->lines[i].val, &conversion->amount, &dup.lines[i].val);
		}
	}

	print_posting(&dup);
}

char *from, to;
struct decimal amount;
int main(int argc, char *argv[]) {
	int opt;
	char *dateend;
	struct conversion conversion = {0};
	while ((opt = getopt(argc, argv, "a:f:t:")) != -1) {
		switch (opt) {
		case 'f':
			conversion.key = optarg;
			break;
		case 't':
			conversion.value = optarg;
			break;
		case 'a':
			if (conversion.key == NULL || conversion.value == NULL) {
				fprintf(stderr, "expected from and to currencies before amount\n");
				return 1;
			}

			decimal_parse(&conversion.amount, optarg, strlen(optarg));

			shputs(conversions, conversion);
			conversion.key = NULL;
			conversion.value = NULL;
			conversion.amount = (struct decimal){};
			break;
		default:
		}
	}

	if (process_postings(convert_processor, NULL) == -1) {
		return 1;
	}

	return 0;
}
