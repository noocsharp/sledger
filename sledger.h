struct posting {
	struct tm time;
	char *desc;
	struct posting_line *lines;
	size_t nlines;
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

int process_postings(void (*processor)(struct posting *posting, void *data), void *data);
