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

## Limitations

sledger currently uses a custom fixed-width floating point decimal
implementation. It can represent any value where the significand can be stored
in a C long, which is usually 64 bits (i.e. ±9,223,372,036,854,775,808). This is
sufficient for my purposes, but if you require more, feel free to send a patch
to either use long long, or implement arbitrary precision decimal arithmetic.

## Examples

Display a tree of all expenses this year, with the sums of each account within expenses:
`sledger-filter -b 2025-01-01 < journal | sledger-aggregate -a | sledger-tree -a expenses`

Show all transactions involving an account with the value of the account after each transaction:
`sledger-sort -d < journal | sledger-register account_name`

