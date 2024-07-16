struct posting {
	struct tm time;
	char *desc;
	struct posting_line *lines;
};

struct decimal {
	long sig;
	unsigned int places;
};

struct posting_line {
	char *account;
	bool has_value;
	struct decimal val;
	char *currency;
};

void print_posting(struct posting *posting);
int process_postings(void (*processor)(struct posting *posting, void *data), void *data);
int decimal_add(struct decimal *a, struct decimal *b, struct decimal *out);
int decimal_leq(struct decimal *a, struct decimal *b);
void decimal_abs(struct decimal *in, struct decimal *out);
void decimal_print(struct decimal *val);
int tmcmp(const struct posting *a, const struct posting *b);
int posting_dup(struct posting *dst, const struct posting *src);
