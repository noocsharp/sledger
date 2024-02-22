all: sledger-accounts sledger-balance

sledger-accounts: sledger-accounts.o sledger.o
	gcc sledger-accounts.o sledger.o -o sledger-accounts

sledger-balance: sledger-balance.o sledger.o
	gcc sledger-balance.o sledger.o -o sledger-balance

sledger-accounts.o: sledger-accounts.c
	$(CC) -c sledger-accounts.c -o sledger-accounts.o

sledger-balance.o: sledger-balance.c
	$(CC) -c sledger-balance.c -o sledger-balance.o

sledger.o: sledger.c
	$(CC) -c sledger.c -o sledger.o
