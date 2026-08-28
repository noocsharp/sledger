EXE=sledger-accounts sledger-sort sledger-filter sledger-cashflow sledger-register sledger-convert sledger-aggregate sledger-stats sledger-tree
MAN=sledger-accounts.1 sledger-sort.1 sledger-filter.1 sledger-register.1 sledger-aggregate.1 sledger-stats.1 sledger-tree.1

all: $(EXE)

install:
	mkdir -p /usr/local/bin
	cp $(EXE) /usr/local/bin
	mkdir -p /usr/local/share/man/man1
	cp $(MAN) /usr/local/share/man/man1

sledger-accounts: sledger-accounts.o sledger.o
	$(CC) sledger-accounts.o sledger.o -o sledger-accounts

sledger-sort: sledger-sort.o sledger.o
	$(CC) sledger-sort.o sledger.o -o sledger-sort

sledger-filter: sledger-filter.o sledger.o
	$(CC) sledger-filter.o sledger.o -o sledger-filter

sledger-cashflow: sledger-cashflow.o sledger.o
	$(CC) sledger-cashflow.o sledger.o -o sledger-cashflow

sledger-tree: sledger-tree.o sledger.o
	$(CC) sledger-tree.o sledger.o -o sledger-tree

sledger-register: sledger-register.o sledger.o
	$(CC) sledger-register.o sledger.o -o sledger-register

sledger-convert: sledger-convert.o sledger.o
	$(CC) sledger-convert.o sledger.o -o sledger-convert

sledger-units: sledger-units.o sledger.o
	$(CC) sledger-units.o sledger.o -o sledger-units

sledger-aggregate: sledger-aggregate.o sledger.o
	$(CC) sledger-aggregate.o sledger.o -o sledger-aggregate

sledger-stats: sledger-stats.o sledger.o
	$(CC) sledger-stats.o sledger.o -o sledger-stats

sledger-accounts.o: sledger-accounts.c
	$(CC) -c sledger-accounts.c -o sledger-accounts.o

sledger-sort.o: sledger-sort.c
	$(CC) -c sledger-sort.c -o sledger-sort.o

sledger-filter.o: sledger-filter.c
	$(CC) -c sledger-filter.c -o sledger-filter.o

sledger-cashflow.o: sledger-cashflow.c
	$(CC) -c sledger-cashflow.c -o sledger-cashflow.o

sledger-tree.o: sledger-tree.c
	$(CC) -c sledger-tree.c -o sledger-tree.o

sledger-register.o: sledger-register.c
	$(CC) -c sledger-register.c -o sledger-register.o

sledger-convert.o: sledger-convert.c
	$(CC) -c sledger-convert.c -o sledger-convert.o

sledger-units.o: sledger-units.c
	$(CC) -c sledger-units.c -o sledger-units.o

sledger-aggregate.o: sledger-aggregate.c
	$(CC) -c sledger-aggregate.c -o sledger-aggregate.o

sledger-stats.o: sledger-stats.c
	$(CC) -c sledger-stats.c -o sledger-stats.o

sledger.o: sledger.c
	$(CC) -c sledger.c -o sledger.o

clean:
	rm -f $(EXE) *.o
