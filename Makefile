all: sledger-accounts sledger-balance sledger-sort sledger-filter sledger-cashflow sledger-display

sledger-accounts: sledger-accounts.o sledger.o
	$(CC) sledger-accounts.o sledger.o -o sledger-accounts

sledger-balance: sledger-balance.o sledger.o
	$(CC) sledger-balance.o sledger.o -o sledger-balance

sledger-sort: sledger-sort.o sledger.o
	$(CC) sledger-sort.o sledger.o -o sledger-sort

sledger-filter: sledger-filter.o sledger.o
	$(CC) sledger-filter.o sledger.o -o sledger-filter

sledger-cashflow: sledger-cashflow.o sledger.o
	$(CC) sledger-cashflow.o sledger.o -o sledger-cashflow

sledger-display: sledger-display.o sledger.o
	$(CC) sledger-display.o sledger.o -o sledger-display

sledger-register: sledger-register.o sledger.o
	$(CC) sledger-register.o sledger.o -o sledger-register

sledger-accounts.o: sledger-accounts.c
	$(CC) -c sledger-accounts.c -o sledger-accounts.o

sledger-balance.o: sledger-balance.c
	$(CC) -c sledger-balance.c -o sledger-balance.o

sledger-sort.o: sledger-sort.c
	$(CC) -c sledger-sort.c -o sledger-sort.o

sledger-filter.o: sledger-filter.c
	$(CC) -c sledger-filter.c -o sledger-filter.o

sledger-cashflow.o: sledger-cashflow.c
	$(CC) -c sledger-cashflow.c -o sledger-cashflow.o

sledger-display.o: sledger-display.c
	$(CC) -c sledger-display.c -o sledger-display.o

sledger-register.o: sledger-register.c
	$(CC) -c sledger-register.c -o sledger-register.o

sledger.o: sledger.c
	$(CC) -c sledger.c -o sledger.o
