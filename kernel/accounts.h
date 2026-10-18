#ifndef TURBOS_ACCOUNTS_H
#define TURBOS_ACCOUNTS_H

#define ACCOUNTS_MAX 8
#define ACCOUNT_NAME_MAX 8

typedef struct {
    char name[ACCOUNT_NAME_MAX + 1];
} account_t;

int accounts_init(void);
int accounts_boot_login(void);
int accounts_count(void);
const account_t *accounts_get(int index);
int accounts_create(const char *name);
int accounts_login(int index);
void accounts_system_console(void);

#endif
