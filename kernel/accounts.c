#include "accounts.h"
#include "state.h"
#include "../fs/vfs.h"
#include "../drivers/keyboard.h"
#include "../drivers/vga.h"

static account_t accounts[ACCOUNTS_MAX];
static int account_count;
static vfs_node_t *system_dir;
static vfs_node_t *users_dir;
static vfs_node_t *registry;

static int same(const char *a, const char *b)
{
    while(*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static int valid_name(const char *s)
{
    int n = 0;

    if(!s || !*s)
        return 0;

    while(s[n]) {
        char c = s[n];
        if(n >= ACCOUNT_NAME_MAX)
            return 0;
        if(!((c >= 'a' && c <= 'z') ||
             (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') ||
             c == '_' || c == '-'))
            return 0;
        ++n;
    }

    return 1;
}

static int save_registry(void)
{
    int pos = 0;

    if(!registry)
        return 0;

    for(int i = 0; i < account_count; ++i) {
        for(int j = 0; accounts[i].name[j]; ++j) {
            if(pos >= VFS_NODE_DATA - 2)
                return 0;
            registry->data[pos++] = accounts[i].name[j];
        }
        registry->data[pos++] = '\n';
    }

    registry->data[pos] = 0;
    registry->size = (unsigned int)pos;
    return vfs_write(registry);
}

int accounts_init(void)
{
    vfs_node_t *root = vfs_get_root();

    account_count = 0;
    system_dir = vfs_find(root, "system");
    users_dir = vfs_find(root, "users");

    if(!system_dir)
        system_dir = vfs_create_dir(root, "system");
    if(!users_dir)
        users_dir = vfs_create_dir(root, "users");
    if(!system_dir || !users_dir)
        return 0;

    registry = vfs_find(system_dir, "users.dat");
    if(!registry)
        registry = vfs_create_file(system_dir, "users.dat");
    if(!registry)
        return 0;

    if(!vfs_read(registry))
        return 0;

    int pos = 0;
    while(registry->data[pos] && account_count < ACCOUNTS_MAX) {
        char name[ACCOUNT_NAME_MAX + 1];
        int n = 0;

        while(registry->data[pos] &&
              registry->data[pos] != '\n' &&
              n < ACCOUNT_NAME_MAX) {
            name[n++] = registry->data[pos++];
        }
        if(registry->data[pos] == '\n')
            ++pos;
        name[n] = 0;

        if(valid_name(name) && n > 0 &&
           !vfs_find(users_dir, name))
            continue;

        if(valid_name(name) && n > 0) {
            int duplicate = 0;
            for(int i = 0; i < account_count; ++i)
                if(same(accounts[i].name, name))
                    duplicate = 1;
            if(!duplicate) {
                for(int j = 0; j <= n; ++j)
                    accounts[account_count].name[j] = name[j];
                ++account_count;
            }
        }
    }

    for(int i = 0; i < account_count; ++i) {
        vfs_node_t *user_dir = vfs_find(users_dir, accounts[i].name);
        if(!user_dir)
            return 0;

        if(!vfs_find(user_dir, "home") &&
           !vfs_create_dir(user_dir, "home"))
            return 0;
    }

    return 1;
}

int accounts_count(void)
{
    return account_count;
}

const account_t *accounts_get(int index)
{
    if(index < 0 || index >= account_count)
        return 0;
    return &accounts[index];
}

int accounts_create(const char *name)
{
    if(!valid_name(name) || account_count >= ACCOUNTS_MAX)
        return 0;

    for(int i = 0; i < account_count; ++i)
        if(same(accounts[i].name, name))
            return 0;

    if(vfs_find(users_dir, name))
        return 0;

    /* Every account owns /users/<name>/home. */
    vfs_node_t *user_dir = vfs_create_dir(users_dir, name);
    if(!user_dir)
        return 0;

    vfs_node_t *home_dir = vfs_create_dir(user_dir, "home");
    if(!home_dir) {
        vfs_delete(user_dir);
        return 0;
    }

    vfs_node_t *profile = vfs_create_file(user_dir, "profile.txt");
    if(!profile) {
        vfs_delete(home_dir);
        vfs_delete(user_dir);
        return 0;
    }

    int i = 0;
    while(name[i]) {
        accounts[account_count].name[i] = name[i];
        ++i;
    }
    accounts[account_count].name[i] = 0;
    ++account_count;

    if(!save_registry()) {
        --account_count;
        accounts[account_count].name[0] = 0;
        vfs_delete(profile);
        vfs_delete(home_dir);
        vfs_delete(user_dir);
        return 0;
    }

    return 1;
}

int accounts_login(int index)
{
    if(index < 0 || index >= account_count)
        return 0;

    username = accounts[index].name;

    vfs_node_t *user_dir = vfs_find(users_dir, username);
    if(!user_dir)
        return 0;

    vfs_node_t *home_dir = vfs_find(user_dir, "home");
    if(!home_dir)
        return 0;

    fm_dir = home_dir;
    fm_index = 0;
    fm_scroll = 0;
    home_color = 0x1F;

    return 1;
}

static void accounts_draw(int selected, const char *typed, int setup)
{
    vga_clear(0x1F);
    vga_print("turbOS ACCOUNT LOGIN", 2, 27, 0x1F);
    vga_print("====================", 3, 29, 0x1F);

    if(setup) {
        vga_print("FIRST-TIME SETUP", 6, 31, 0x1E);
        vga_print("Create username (1-8 letters, digits, _ or -):",
                  9, 14, 0x1F);
        vga_print(typed, 11, 20, 0x1E);
        vga_print("ENTER: create account", 16, 28, 0x1F);
        vga_print("ESC: cancel", 18, 34, 0x1F);
    } else {
        vga_print("Choose an account with UP/DOWN and ENTER",
                  5, 19, 0x1F);
        for(int i = 0; i < accounts_count(); ++i) {
            const account_t *acc = accounts_get(i);
            vga_print(i == selected ? ">" : " ",
                      8 + i * 2, 24, 0x1F);
            vga_print(acc->name, 8 + i * 2, 27,
                      i == selected ? 0x2F : 0x1F);
        }
        vga_print("N: create account", 21, 25, 0x1F);
        vga_print("ENTER: login", 22, 25, 0x1F);
    }
    vga_cursor_hide();
}

int accounts_boot_login(void)
{
    char typed[ACCOUNT_NAME_MAX + 1] = {0};
    int len = 0;
    int selected = 0;
    int setup = accounts_count() == 0;

    accounts_draw(selected, typed, setup);

    for(;;) {
        int key = keyboard_getkey();
        if(key == KEY_NONE)
            continue;

        setup = accounts_count() == 0;

        if(setup) {
            if(key == KEY_BACKSPACE) {
                if(len > 0)
                    typed[--len] = 0;
                accounts_draw(selected, typed, 1);
            } else if(key == KEY_ENTER) {
                if(accounts_create(typed))
                    return accounts_login(accounts_count() - 1);

                vga_print("Invalid name, duplicate, or storage full.",
                          14, 18, 0x4F);
            } else if(key >= 32 && key <= 126 &&
                      len < ACCOUNT_NAME_MAX) {
                char c = (char)key;
                if(c >= 'A' && c <= 'Z')
                    c = (char)(c - 'A' + 'a');

                if((c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') ||
                   c == '_' || c == '-') {
                    typed[len++] = c;
                    typed[len] = 0;
                    accounts_draw(selected, typed, 1);
                }
            }
        } else {
            if(key == KEY_UP && selected > 0) {
                --selected;
                accounts_draw(selected, typed, 0);
            } else if(key == KEY_DOWN &&
                      selected + 1 < accounts_count()) {
                ++selected;
                accounts_draw(selected, typed, 0);
            } else if(key == 'n' || key == 'N') {
                typed[0] = 0;
                len = 0;
                accounts_draw(selected, typed, 1);

                for(;;) {
                    key = keyboard_getkey();
                    if(key == KEY_NONE)
                        continue;

                    if(key == KEY_ESC)
                        break;

                    if(key == KEY_BACKSPACE) {
                        if(len > 0)
                            typed[--len] = 0;
                        accounts_draw(selected, typed, 1);
                    } else if(key == KEY_ENTER) {
                        if(accounts_create(typed))
                            return accounts_login(accounts_count() - 1);

                        vga_print("Invalid name, duplicate, or storage full.",
                                  14, 18, 0x4F);
                    } else if(key >= 32 && key <= 126 &&
                              len < ACCOUNT_NAME_MAX) {
                        char c = (char)key;
                        if(c >= 'A' && c <= 'Z')
                            c = (char)(c - 'A' + 'a');

                        if((c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-') {
                            typed[len++] = c;
                            typed[len] = 0;
                            accounts_draw(selected, typed, 1);
                        }
                    }
                }

                accounts_draw(selected, typed, 0);
            } else if(key == KEY_ENTER) {
                return accounts_login(selected);
            }
        }
    }
}


/* Delete descendants before their parent; VFS only deletes empty directories. */
static int system_delete_tree(vfs_node_t *node)
{
    if(!node)
        return 0;

    while(node->child) {
        if(!system_delete_tree(node->child))
            return 0;
    }

    return vfs_delete(node);
}

static void system_console_draw(int selected, int confirm)
{
    vga_clear(0x04);
    vga_print("$SYSTEM$", 1, 2, 0x4F);
    vga_print("RESTRICTED ACCOUNT REMOVAL CONSOLE", 3, 2, 0x4F);
    vga_print("----------------------------------", 4, 2, 0x4F);
    vga_print("WARNING: DELETIONS ARE PERMANENT", 6, 2, 0x4F);

    for(int i = 0; i < account_count; ++i) {
        const char *name = accounts[i].name;
        vga_print(i == selected ? ">" : " ", 8 + i, 3, 0x4F);
        vga_print(name, 8 + i, 5,
                  same(name, username) ? 0x4F : 0x4C);
        if(same(name, username))
            vga_print("[ACTIVE - PROTECTED]", 8 + i, 18, 0x4F);
    }

    if(confirm) {
        vga_print("DELETE SELECTED ACCOUNT AND ALL ITS FILES?", 20, 2, 0x4F);
        vga_print("Y = CONFIRM     N/ESC = CANCEL", 22, 2, 0x4F);
    } else {
        vga_print("UP/DOWN SELECT  ENTER DELETE  ESC EXIT", 21, 2, 0x4F);
    }

    vga_cursor_hide();
}

void accounts_system_console(void)
{
    int selected = 0;
    int confirm = 0;

    if(account_count <= 0)
        return;

    system_console_draw(selected, confirm);

    for(;;) {
        int key = keyboard_getkey();
        if(key == KEY_NONE) {
            __asm__ volatile("hlt");
            continue;
        }

        if(confirm) {
            if(key == KEY_ESC || key == 'n' || key == 'N') {
                confirm = 0;
                system_console_draw(selected, confirm);
            } else if(key == 'y' || key == 'Y') {
                const char *target = accounts[selected].name;

                /* Never remove the currently logged-in account. */
                if(!same(target, username)) {
                    vfs_node_t *user_dir = vfs_find(users_dir, target);
                    if(user_dir && system_delete_tree(user_dir)) {
                        for(int i = selected; i < account_count - 1; ++i)
                            accounts[i] = accounts[i + 1];

                        --account_count;
                        accounts[account_count].name[0] = 0;
                        save_registry();

                        if(account_count == 0)
                            selected = 0;
                        else if(selected >= account_count)
                            selected = account_count - 1;
                    }
                }

                confirm = 0;
                if(account_count <= 0)
                    return;
                system_console_draw(selected, confirm);
            }
            continue;
        }

        if(key == KEY_ESC)
            return;

        if(key == KEY_UP && selected > 0) {
            --selected;
            system_console_draw(selected, confirm);
        } else if(key == KEY_DOWN && selected + 1 < account_count) {
            ++selected;
            system_console_draw(selected, confirm);
        } else if(key == KEY_ENTER) {
            if(!same(accounts[selected].name, username)) {
                confirm = 1;
                system_console_draw(selected, confirm);
            } else {
                vga_print("ACTIVE ACCOUNT CANNOT BE DELETED", 23, 2, 0x4F);
            }
        }
    }
}
