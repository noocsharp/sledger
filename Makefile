sledger-accounts: sledger-accounts.o sledger.o
	gcc sledger-accounts.o sledger.o -o sledger-accounts

sledger-accounts.o: sledger-accounts.c
	$(CC) -c sledger-accounts.c -o sledger-accounts.o

sledger.o: sledger.c
	$(CC) -c sledger.c -o sledger.o
