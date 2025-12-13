# sledger

sledger is a suite of [plain text accounting](https://plaintextaccounting.org/) tools.
I initially created it for my personal use out of a sense of dissatisfaction with
existing plain text accounting tools.

The goals of sledger ordered by priority are the following:
1. Be correct
2. Minimize external dependencies
3. Minimize the number of programs, but maximize functionality of the whole suite through composability
4. Programs should only do things that cannot easily be done with existing command line tools
5. Be fast

## Examples

Display a tree of all expenses this year, with the sums of each account within expenses:
`sledger-filter -b 2025-01-01 < journal | sledger-balance | grep "^expenses" | sledger-display`

Show all transactions involving an account with the value of the account after each transaction:
`sledger-sort -d < journal | sledger-register account_name`

