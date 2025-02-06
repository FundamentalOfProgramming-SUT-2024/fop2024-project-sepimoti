#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <ncursesw/curses.h>
#include "user.h"

const char *user_file_name = "user.json";
User current_user;


typedef struct {
    char symbol[6];
    attr_t attributes;
    int y, x;  // Used for hero
    int value; // Used for rooms
} Character;

typedef struct {
    char name[25];
    Character character;
    int health;
    int damage;
    int can_follow;
    int walk; // For giant and undeed to avoid walking after 5 blocks
    Character previous;
} Monster;

typedef struct {
    char kind[30];
    int y, x; // Top Left position
    int width, height;
    Character** map;
    Monster monsters[5];
    int monster_count;
} Room;

typedef struct {
    int first_x, first_y; // Top Left position
    int final_x, final_y; // Bottom Right postion
    Room *room;
} Region;

// Used to connect rooms
typedef struct {
    int room1_idx;
    int room2_idx;
    int cost; // Manhattan distance
} Edge;

int WIDTH, HEIGHT;
const int HORIZONTAL = 4, VERTICAL = 3;
Region regions[3][4]; // Divide screen into 12 regions
int start_region;

typedef struct {
    int value;
    int number;
    int first_consume; // For magic/supreme food and potions
} Resource;

typedef struct {
    Resource foods[3];   // 0: normal or rotten | 1: magic | 2: supreme
    Resource weapons[5]; // 0: mace | 1: dagger | 2: wand  | 3: arrow | 4: sword
    int which_weapon;
    Resource potions[3]; // 0: health | 1: speed | 2: damage
} Inventory;

typedef struct {
    Character character;
    Inventory inventory;
    int walk;
    int level;
    int health;
    int hunger;
    int gold;
    int healing_speed;
    int speed;
    int base_power;
} Hero;
Hero hero;
Character previous;

typedef struct {
    char symbol[6];
    attr_t attributes;
    int condition;
} Screen;
Screen **screen;


int find_max(const char *strings[], size_t size)
{
    int max = strlen(strings[0]);
    for (int i = 1; i < size; i++)
    {
        if (strlen(strings[i]) > max)
            max = strlen(strings[i]);
    }
    return max;
}

void draw_border()
{
    attron(COLOR_PAIR(3));

    mvprintw(0, 0, "╔");
    mvprintw(0, WIDTH - 1, "╗");
    mvprintw(HEIGHT - 1, 0, "╚");
    mvprintw(HEIGHT - 1, WIDTH - 1, "╝");

    for (int i = 1; i < WIDTH - 1; i++)
    {
        mvprintw(0, i, "═");
        mvprintw(HEIGHT - 1, i, "═");
    }
    for (int i = 1; i < HEIGHT - 1; i++)
    {
        mvprintw(i, 0, "║");
        mvprintw(i, WIDTH - 1, "║");
    }

    attroff(COLOR_PAIR(3));
    refresh();
}

void show_message(const char *message)
{
    attron(COLOR_PAIR(1) | A_BOLD);
    mvaddch(0, 2, ' ');
    mvaddstr(0, 3, message);
    mvaddch(0, strlen(message) + 3, ' ');
    attroff(COLOR_PAIR(1) | A_BOLD);

    attron(COLOR_PAIR(3) | A_BOLD);
    for (int i = strlen(message) + 4; i < WIDTH - 1; i++)
        mvprintw(0, i, "═");
    attron(COLOR_PAIR(3) | A_BOLD);

    refresh();
}

void show_error(int y, int x, const char *message)
{
    attron(COLOR_PAIR(4));
    mvprintw(y, x, "%s", message);
    attroff(COLOR_PAIR(4));

    attron(COLOR_PAIR(1));
    for (int i = x + strlen(message); i < WIDTH - 2; i++)
        mvprintw(y, i, " ");
    attron(COLOR_PAIR(1));

    refresh();
}

void show_logo(int which){
    attron(COLOR_PAIR(3));
    const char *rogue[] = {
        "██████╗  ██████╗  ██████╗ ██╗   ██╗███████╗",
        "██╔══██╗██╔═══██╗██╔════╝ ██║   ██║██╔════╝",
        "██████╔╝██║   ██║██║  ███╗██║   ██║█████╗  ",
        "██╔══██╗██║   ██║██║   ██║██║   ██║██╔══╝ ",
        "██║  ██║╚██████╔╝╚██████╔╝╚██████╔╝███████╗",
        "╚═╝  ╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚══════╝",
    };
    const char *welcome[] = {
        "██╗    ██╗███████╗██╗      ██████╗ ██████╗ ███╗   ███╗███████╗",
        "██║    ██║██╔════╝██║     ██╔════╝██╔═══██╗████╗ ████║██╔════╝",
        "██║ █╗ ██║█████╗  ██║     ██║     ██║   ██║██╔████╔██║█████╗",
        "██║███╗██║██╔══╝  ██║     ██║     ██║   ██║██║╚██╔╝██║██╔══╝",
        "╚███╔███╔╝███████╗███████╗╚██████╗╚██████╔╝██║ ╚═╝ ██║███████╗",
        " ╚══╝╚══╝ ╚══════╝╚══════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚══════╝"
    };
    const char *settings[] = {
        "███████╗███████╗████████╗████████╗██╗███╗   ██╗ ██████╗ ███████╗",
        "██╔════╝██╔════╝╚══██╔══╝╚══██╔══╝██║████╗  ██║██╔════╝ ██╔════╝",
        "███████╗█████╗     ██║      ██║   ██║██╔██╗ ██║██║  ███╗███████╗",
        "╚════██║██╔══╝     ██║      ██║   ██║██║╚██╗██║██║   ██║╚════██║",
        "███████║███████╗   ██║      ██║   ██║██║ ╚████║╚██████╔╝███████║",
        "╚══════╝╚══════╝   ╚═╝      ╚═╝   ╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚══════╝"
    };
    const char *game_over[] = {
        "  ▄████  ▄▄▄       ███▄ ▄███▓▓█████     ▒█████   ██▒   █▓▓█████  ██▀███  ",
        " ██▒ ▀█▒▒████▄    ▓██▒▀█▀ ██▒▓█   ▀    ▒██▒  ██▒▓██░   █▒▓█   ▀ ▓██ ▒ ██▒",
        "▒██░▄▄▄░▒██  ▀█▄  ▓██    ▓██░▒███      ▒██░  ██▒ ▓██  █▒░▒███   ▓██ ░▄█ ▒",
        "░▓█  ██▓░██▄▄▄▄██ ▒██    ▒██ ▒▓█  ▄    ▒██   ██░  ▒██ █░░▒▓█  ▄ ▒██▀▀█▄  ",
        "░▒▓███▀▒ ▓█   ▓██▒▒██▒   ░██▒░▒████▒   ░ ████▓▒░   ▒▀█░  ░▒████▒░██▓ ▒██▒",
        " ░▒   ▒  ▒▒   ▓▒█░░ ▒░   ░  ░░░ ▒░ ░   ░ ▒░▒░▒░    ░ ▐░  ░░ ▒░ ░░ ▒▓ ░▒▓░",
        "  ░   ░   ▒   ▒▒ ░░  ░      ░ ░ ░  ░     ░ ▒ ▒░    ░ ░░   ░ ░  ░  ░▒ ░ ▒░",
        "░ ░   ░   ░   ▒   ░      ░      ░      ░ ░ ░ ▒       ░░     ░     ░░   ░ ",
        "      ░       ░  ░       ░      ░  ░       ░ ░        ░     ░  ░   ░     ",
        "                                                     ░                   "
    };
    const char *won[] = {
        "██    ██  ██████  ██    ██     ██     ██  ██████  ███    ██",
        " ██  ██  ██    ██ ██    ██     ██     ██ ██    ██ ████   ██",
        "  ████   ██    ██ ██    ██     ██  █  ██ ██    ██ ██ ██  ██",
        "   ██    ██    ██ ██    ██     ██ ███ ██ ██    ██ ██  ██ ██",
        "   ██     ██████   ██████       ███ ███   ██████  ██   ████"
    };

    if (which == 0) {
        for (int i = 0; i < 6; i++) {
            mvprintw(2 + i, (WIDTH - 43) / 2, "%s", rogue[i]);
        }
    }
    else if (which == 1) {
        for (int i = 0; i < 6; i++) {
            mvprintw(2 + i, (WIDTH - 62) / 2, "%s", welcome[i]);
        }
    }
    else if (which == 2) {
        for (int i = 0; i < 6; i++) {
            mvprintw(2 + i, (WIDTH - 64) / 2, "%s", settings[i]);
        }
    }

    attroff(COLOR_PAIR(3));

    if (which == 3) {
        attron(COLOR_PAIR(4));
        for (int i = 0; i < 10; i++) {
            mvprintw(2 + i, (WIDTH - 73) / 2, "%s", game_over[i]);
        }
        attroff(COLOR_PAIR(4));
    }

    if (which == 4) {
        attron(COLOR_PAIR(5));
        for (int i = 0; i < 5; i++) {
            mvprintw(2 + i, (WIDTH - 59) / 2, "%s", won[i]);
        }
        attroff(COLOR_PAIR(5));
    }
}

int change_menu(const char *menu[], int len, int y, int x, int which_menu)
{
    int current = 0;
    int ch = 0;

    while (1)
    {
        for (int i = 0; i < len; i++)
        {
            if (i == current)
            {
                attron(COLOR_PAIR(2) | A_BOLD);
                mvprintw(y + i * 2, x, "  >>> %s <<<  ", menu[i]);
                attroff(COLOR_PAIR(2) | A_BOLD);
            }
            else
            {
                attron(COLOR_PAIR(1) | A_BOLD);
                mvprintw(y + i * 2, x, "      %s      ", menu[i]);
                attroff(COLOR_PAIR(1) | A_BOLD);
            }
        }
        refresh();

        ch = getch();
        if (ch == '\n')
            return current + 1;
        if (ch == 27)
            return -1; // ESC key

        if (current_user.name[0] == '\0' && which_menu == 0)
        {
            show_message("You must log in first!");
        }
        else
        {
            if (ch == KEY_UP || ch == 'w')
                current = (current + len - 1) % len;
            else if (ch == KEY_DOWN || ch == 's')
                current = (current + 1) % len;
            else
                show_message("Please use key_up-key_down or w-s");
        }
    }
}

void menu_clear() {
    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            mvprintw(y, x, " ");
        }
    }
}

int main_menu() {
    // ROGUE
    menu_clear();
    show_logo(0);
    
    // Footer
    attron(COLOR_PAIR(1) | A_BOLD);
    const char *footer = "Created by: SepiMoti | Version: 2.0.0";
    mvaddstr(HEIGHT - 1, (WIDTH - strlen(footer)) / 2, footer);
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Menu
    const char *menu[] = {
        "[1] Profile",
        "[2] New Game",
        "[3] Load Game",
        "[4] Settings",
        "[5] Score Table"};
    return change_menu(menu, 5, 10, WIDTH / 2 - 12, 0);
}

int profile_menu()
{
    // ROGUE
    clear();
    draw_border();
    show_logo(0);

    // Menu
    const char *menu[] = {
        "[1] Sign Up",
        "[2] Log In",
        "[3] Guest"};
    return change_menu(menu, 3, 10, WIDTH / 2 - 11, 1);
}

void draw_table(int init_x, int final_x, int init_y, int final_y)
{
    attron(COLOR_PAIR(3));
    mvprintw(init_y, init_x, "┏");
    mvprintw(init_y, final_x, "┓");
    mvprintw(final_y, init_x, "┗");
    mvprintw(final_y, final_x, "┛");

    for (int i = init_x + 1; i < final_x; i++)
    {
        mvprintw(init_y, i, "━");
        mvprintw(final_y, i, "━");
    }
    for (int i = init_y + 1; i < final_y; i++)
    {
        if ((i - init_y) % 4 == 0)
        {
            mvprintw(i, init_x, "┠");
            mvprintw(i, final_x, "┨");
            for (int j = init_x + 1; j < final_x; j++)
            {
                mvprintw(i, j, "─");
            }
        }
        else
        {
            mvprintw(i, init_x, "┃");
            mvprintw(i, final_x, "┃");
        }
    }
    attroff(COLOR_PAIR(3));
    refresh();
}

void fill_table(const char *options[], size_t op_size, int y, int x, int end_x)
{
    int max_len = find_max(options, op_size);

    attron(COLOR_PAIR(3));
    for (int i = 0; i < op_size; i++)
    {
        mvaddstr(y + 4 * i, x + (max_len - strlen(options[i])) / 2, options[i]);
    }
    attroff(COLOR_PAIR(3));

    refresh();
}

void clear_input(int y, int x, int len, char *input)
{
    input[0] = '\0';

    attron(COLOR_PAIR(1));
    for (int i = x; i < x + len; i++)
        mvaddstr(y, i, " ");
    attroff(COLOR_PAIR(1));

    refresh();
}

int signup_page()
{
    User user;
    int name = 0, email = 0, password = 0;

    clear();
    draw_border();
    show_logo(1);

    int table_width = 60;
    int init_x = (WIDTH - table_width) / 2;
    int final_x = (WIDTH + table_width) / 2;
    int init_y = 9;
    int final_y = 25;

    // Draw and Fill table
    draw_table(init_x, final_x, init_y, final_y);
    const char *options[] = {
        "UserName:",
        "E-Mail:",
        "Password:",
        "Confirm Password:"};
    size_t op_size = sizeof(options) / sizeof(options[0]);
    fill_table(options, op_size, init_y + 2, init_x + 2, final_x - 2);

    // Initialize input parts
    int max_len = find_max(options, op_size);
    char inputs[4][50] = {"", "", "", ""};
    int field = 0;

    echo();
    curs_set(TRUE);
    while (1)
    {
        // Display the fields
        for (size_t i = 0; i < op_size; i++)
        {
            if ((int)i == field)
            {
                attron(COLOR_PAIR(1) | A_REVERSE);
                mvprintw(init_y + 2 + 4 * i, init_x + max_len + 2, "%s", inputs[i]);
                attroff(COLOR_PAIR(1) | A_REVERSE);
            }
            else
            {
                attron(COLOR_PAIR(1));
                mvprintw(init_y + 2 + 4 * i, init_x + max_len + 2, "%s", inputs[i]);
                attroff(COLOR_PAIR(1));
            }
        }
        refresh();
        move(init_y + 2 + 4 * field, init_x + max_len + 2 + strlen(inputs[field]));

        int ch = getch();

        if (ch == 27)
        { // ESC key
            noecho();
            curs_set(FALSE);
            return -1;
        }
        else if (ch == '\n')
        {
            if (field == 0 && strlen(inputs[0]) != 0)
            {
                int a = is_username_taken(user_file_name, inputs[0]);
                if (a == -1)
                {
                    show_error(init_y + 2, final_x + 2, "Failed to read user file!");
                    clear_input(init_y + 2, init_x + max_len + 2, table_width - max_len - 3, inputs[0]);
                }
                else if (a == 1)
                {
                    show_error(init_y + 2, final_x + 2, "Username has already taken!");
                    clear_input(init_y + 2, init_x + max_len + 2, table_width - max_len - 3, inputs[0]);
                }
                else
                {
                    strcpy(user.name, inputs[0]);
                    show_message("Username accepted!");
                    show_error(init_y + 2, final_x + 2, "");
                    field++;
                    name = 1;
                }
            }
            else if (field == 1 && strlen(inputs[1]) != 0)
            {
                if (is_email_valid(inputs[1]))
                {
                    strcpy(user.email, inputs[1]);
                    show_message("E-Mail accepted!");
                    show_error(init_y + 6, final_x + 2, "");
                    field++;
                    email = 1;
                }
                else
                {
                    show_error(init_y + 6, final_x + 2, "E-Mail format is not valid!");
                    clear_input(init_y + 6, init_x + max_len + 2, table_width - max_len - 3, inputs[1]);
                }
            }
            else if (field == 2)
            {
                int a = is_password_valid(inputs[2]);
                if (a == -1)
                {
                    show_error(init_y + 10, final_x + 2, "Password length must be at least 7!");
                    clear_input(init_y + 10, init_x + max_len + 2, table_width - max_len - 3, inputs[2]);
                }
                else if (a == 0)
                {
                    show_error(init_y + 10, final_x + 2, "Password must include lower, upper, and digits!");
                    clear_input(init_y + 10, init_x + max_len + 2, table_width - max_len - 3, inputs[2]);
                }
                else
                {
                    show_error(init_y + 10, final_x + 2, "");
                    field++;
                }
            }
            else if (field == 3)
            {
                if (strcmp(inputs[2], inputs[3]) == 0)
                {
                    strcpy(user.password, inputs[2]);
                    show_message("Password accepted!");
                    show_error(init_y + 14, final_x + 2, "");
                    field = 0;
                    password = 1;
                }
                else
                {
                    show_error(init_y + 14, final_x + 2, "Password not confirmed!");
                    clear_input(init_y + 14, init_x + max_len + 2, table_width - max_len - 3, inputs[3]);
                }
            }

            if (name & email & password)
            {
                user.score = 0;
                strcpy(user.settings.hero_color, "yellow");
                strcpy(user.settings.difficulty, "normal");

                if (add_user_to_file(user_file_name, user) == 1)
                {
                    current_user = get_user_by_name(user_file_name, inputs[0]);
                    curs_set(FALSE);
                    noecho();
                    return 1;
                }
                else
                {
                    show_message("Failed to add user to the file! Please Try again.");
                    for (int i = 0; i < op_size; i++)
                        clear_input(init_y + 2 * i, init_x + max_len + 2, table_width - max_len - 3, inputs[i]);
                }
            }
        }
        else if (ch == KEY_UP)
        {
            field = (field - 1 + op_size) % op_size;
        }
        else if (ch == KEY_DOWN)
        {
            field = (field + 1) % op_size;
        }
        else if (ch == KEY_BACKSPACE)
        {
            int index = strlen(inputs[field]) - 1;
            inputs[field][index] = '\0';
            // Clear from screen
            attron(COLOR_PAIR(1));
            mvprintw(init_y + 2 + 4 * field, init_x + max_len + 2 + index, " ");
            attroff(COLOR_PAIR(1));
        }
        else if (isgraph(ch))
        {
            char temp[2];
            temp[0] = (char)ch;
            temp[1] = '\0';
            strcat(inputs[field], temp);
        }
        else if (ch != EOF)
        {
            show_message("Invalid input!");
        }
    }
}

int login_page()
{
    User user;
    int name = 0;

    clear();
    draw_border();
    show_logo(1);

    int table_width = 60;
    int init_x = (WIDTH - table_width) / 2;
    int final_x = (WIDTH + table_width) / 2;
    int init_y = 9;
    int final_y = 17;

    // Draw and Fill table
    draw_table(init_x, final_x, init_y, final_y);
    const char *options[] = {
        "UserName:",
        "Password:"};
    size_t op_size = 2;
    fill_table(options, op_size, init_y + 2, init_x + 2, final_x - 2);

    // Initialize input parts
    int max_len = find_max(options, op_size);
    char inputs[2][100] = {"", ""};
    int field = 0;
    int is_recovery = 0;

    while (1)
    {
        if (field == 2 && is_recovery == 0)
        { // Display the recovery option
            noecho();
            curs_set(FALSE);
            attron(COLOR_PAIR(2) | A_BOLD);
            mvaddstr(19, WIDTH / 2 - 14, "  >>> Forgot Password? <<<  ");
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        else
        { // Display the fields
            if (is_recovery == 0)
            {
                attron(COLOR_PAIR(1));
                mvaddstr(19, WIDTH / 2 - 14, "      Forgot Password?      ");
                attroff(COLOR_PAIR(1));
            }

            echo();
            curs_set(TRUE);
            for (size_t i = 0; i < op_size; i++)
            {
                if ((int)i == field)
                {
                    attron(COLOR_PAIR(1) | A_REVERSE);
                    mvprintw(init_y + 2 + 4 * i, init_x + max_len + 2, "%s", inputs[i]);
                    attroff(COLOR_PAIR(1) | A_REVERSE);
                }
                else
                {
                    attron(COLOR_PAIR(1));
                    mvprintw(init_y + 2 + 4 * i, init_x + max_len + 2, "%s", inputs[i]);
                    attroff(COLOR_PAIR(1));
                }
            }
            move(init_y + 2 + 4 * field, init_x + max_len + 2 + strlen(inputs[field]));
        }
        refresh();

        int ch = getch();

        if (ch == 27)
        { // ESC key
            noecho();
            curs_set(FALSE);
            return -1;
        }
        else if (ch == '\n')
        {
            if (field == 0 && strlen(inputs[0]) != 0)
            {
                int a = is_username_taken(user_file_name, inputs[0]);
                if (a == -1)
                {
                    show_error(init_y + 2, final_x + 2, "Failed to read user file!");
                    clear_input(init_y + 2, init_x + max_len + 2, table_width - max_len - 3, inputs[0]);
                }
                else if (a == 1)
                {
                    show_error(init_y + 2, final_x + 2, "");
                    user = get_user_by_name(user_file_name, inputs[0]);
                    field++;
                    name = 1;
                }
                else
                {
                    show_error(init_y + 2, final_x + 2, "There is no such username!");
                    clear_input(init_y + 2, init_x + max_len + 2, table_width - max_len - 3, inputs[0]);
                }
            }
            else if (field == 1 && strlen(inputs[1]) != 0)
            {
                if (name == 0)
                {
                    show_error(init_y + 6, final_x + 2, "Please enter your username first!");
                    clear_input(init_y + 6, init_x + max_len + 2, table_width - max_len - 3, inputs[1]);
                    field = 0;
                }
                else
                {
                    if (is_recovery == 0)
                    {
                        if (strcmp(user.password, inputs[1]) == 0)
                        {
                            current_user = get_user_by_name(user_file_name, inputs[0]);
                            update_last_user(user_file_name, inputs[0]);
                            noecho();
                            curs_set(FALSE);
                            return 1;
                        }
                        else
                        {
                            show_error(init_y + 6, final_x + 2, "Password doesn't match!");
                            clear_input(init_y + 6, init_x + max_len + 2, table_width - max_len - 3, inputs[1]);
                        }
                    }
                    else
                    {
                        if (strcmp(user.email, inputs[1]) == 0)
                        {
                            current_user = get_user_by_name(user_file_name, inputs[0]);
                            update_last_user(user_file_name, inputs[0]);
                            noecho();
                            curs_set(FALSE);
                            return 1;
                        }
                        else
                        {
                            show_error(init_y + 6, final_x + 2, "E-Mail doesn't match!");
                            clear_input(init_y + 6, init_x + max_len + 2, table_width - max_len - 3, inputs[1]);
                        }
                    }
                }
            }
            else if (field == 2)
            {
                is_recovery = 1;

                attron(COLOR_PAIR(1));
                mvaddstr(final_y - 2, init_x + 2, "         ");               // Clear "Password:"
                mvaddstr(19, WIDTH / 2 - 14, "                            "); // Clear "Forgot Password?"
                attroff(COLOR_PAIR(1));

                show_message("Please enter your e-mail instead of your password.");
                const char *new_options[] = {
                    "UserName:",
                    "E-Mail:"};
                fill_table(new_options, op_size, init_y + 2, init_x + 2, final_x - 2);

                field = 1;
            }
        }
        else if (ch == KEY_UP)
        {
            int k = is_recovery ? 2 : 3;
            field = (field - 1 + k) % k;
        }
        else if (ch == KEY_DOWN)
        {
            int k = is_recovery ? 2 : 3;
            field = (field + 1) % k;
        }
        else if (ch == KEY_BACKSPACE)
        {
            int index = strlen(inputs[field]) - 1;
            inputs[field][index] = '\0';
            // Clear from screen
            attron(COLOR_PAIR(1));
            mvprintw(init_y + 2 + 4 * field, init_x + max_len + 2 + index, " ");
            attroff(COLOR_PAIR(1));
        }
        else if (isgraph(ch))
        {
            char temp[2];
            temp[0] = (char)ch;
            temp[1] = '\0';
            strcat(inputs[field], temp);
        }
        else if (ch != EOF)
        {
            show_message("Invalid input!");
        }
    }
}

void set_guest()
{
    strcpy(current_user.name, "GUEST");
    strcpy(current_user.settings.difficulty, "normal");
    strcpy(current_user.settings.hero_color, "yellow");
    current_user.score = -1;
}

int settings_index_finder(const char *menu[], size_t len, const char *current)
{
    for (int i = 0; i < len; i++)
    {
        if (strcmp(menu[i], current) == 0)
        {
            return i;
        }
    }
    return 0;
}

void settings_page()
{
    clear();
    draw_border();
    show_logo(2); // SETTINGS

    attron(COLOR_PAIR(3));
    mvaddstr(9, (WIDTH - 64) / 2, "Hero Color");
    mvaddstr(11, (WIDTH - 64) / 2, "Difficulty");
    attroff(COLOR_PAIR(3));

    const char *color[] = {
        "yellow",
        "green",
        "red",
        "blue"};
    int current_color = settings_index_finder(color, 4, current_user.settings.hero_color);

    const char *difficulty[] = {
        "easy",
        "normal",
        "hard"};
    int current_difficulty = settings_index_finder(difficulty, 3, current_user.settings.difficulty);

    int which = 0;
    int ch = 0;

    while (1)
    {
        if (which == 0)
        {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(9, (WIDTH + 64) / 2 - 19, "  <<<         >>>  ");
            mvprintw(9, (WIDTH + 64) / 2 - 13, "%s", color[current_color]);
            attroff(COLOR_PAIR(2) | A_BOLD);

            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(11, (WIDTH + 64) / 2 - 19, "                   ");
            mvprintw(11, (WIDTH + 64) / 2 - 13, "%s", difficulty[current_difficulty]);
            attroff(COLOR_PAIR(1) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(9, (WIDTH + 64) / 2 - 19, "                   ");
            mvprintw(9, (WIDTH + 64) / 2 - 13, "%s", color[current_color]);
            attroff(COLOR_PAIR(1) | A_BOLD);

            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(11, (WIDTH + 64) / 2 - 19, "  <<<         >>>  ");
            mvprintw(11, (WIDTH + 64) / 2 - 13, "%s", difficulty[current_difficulty]);
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        refresh();

        ch = getch();
        if (ch == 27)
        {
            return;
        }
        else if (ch == '\n')
        {
            if (current_user.score < 0)
            { // Guest mode
                strcpy(current_user.settings.hero_color, color[current_color]);
                strcpy(current_user.settings.difficulty, difficulty[current_difficulty]);
            }
            else
            {
                User user = current_user;
                strcpy(user.settings.hero_color, color[current_color]);
                strcpy(user.settings.difficulty, difficulty[current_difficulty]);
                if (update_user_settings(user_file_name, user) == 1)
                {
                    current_user = user;
                    return;
                }
                else
                {
                    show_message("Failed to save the settings. Please try again.");
                }
            }
        }
        else if (ch == KEY_UP || ch == 'w' || ch == KEY_DOWN || ch == 's')
        {
            which = !which;
        }
        else if (ch == KEY_RIGHT || ch == 'd')
        {
            if (which == 0)
                current_color = (current_color + 1) % 4;
            else
                current_difficulty = (current_difficulty + 1) % 3;
        }
        else if (ch == KEY_LEFT || ch == 'a')
        {
            if (which == 0)
                current_color = (current_color - 1 + 4) % 4;
            else
                current_difficulty = (current_difficulty - 1 + 3) % 3;
        }
    }
}

void initialize_screen() {
    screen = (Screen **)calloc(HEIGHT, sizeof(Screen *));
    for (int i = 0; i < HEIGHT; i++) {
        screen[i] = (Screen *)calloc(WIDTH, sizeof(Screen));
    }
}

void free_screen() {
    for (int i = 0; i < HEIGHT; i++) {
        free(screen[i]);
    }
    free(screen);
}

int find_region(Room *room) {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            if ((regions[i][j].first_x <= room->x) && (room->x <= regions[i][j].final_x) 
             && (regions[i][j].first_y <= room->y) && (room->y <= regions[i][j].final_y)) {
                return i * HORIZONTAL + j;
            }
        }
    }
}

int find_region_by_coordinate(int y, int x) {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            if ((regions[i][j].first_x <= x) && (x <= regions[i][j].final_x) 
             && (regions[i][j].first_y <= y) && (y <= regions[i][j].final_y)) {
                return i * HORIZONTAL + j;
            }
        }
    }
}

// Set the rooms in regions
void room_condition_maker(int has_room[VERTICAL][HORIZONTAL]) {
    // At least we'll have 6 rooms but there is a chance to have up to 3 more rooms

    // Randomly set one of every two neighbor regions to 1
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j += 2) {
            int random = rand() % 2;
            has_room[i][j + random] = 1;
            has_room[i][j + !random] = 0;
        }
    }

    // Three more chances to have more rooms
    int more = 3;
    // Check that rooms aren't too far
    for (int i = 0; i < VERTICAL; i++) {
        if (has_room[i][1] == 0 && has_room[i][2] == 0) {
            int random = rand() % 2;
            has_room[i][1 + random] = 1;
            has_room[i][1 + !random] = 0;
            more--;
        }
    }
    // Apply the remaining chances
    for (int i = 0; i < more; i++) {
        int random = rand() % (HORIZONTAL * VERTICAL);
        int y = random / HORIZONTAL;
        int x = random % HORIZONTAL;
        if (has_room[y][x] == 0)
            has_room[y][x] = 1;
    }
}

void random_room_generator(Region *region) {
    // The rooms are normal by default
    strcpy(region->room->kind, "normal");
    
    int height = region->final_y - region->first_y + 1;
    int width = region->final_x - region->first_x + 1;

    region->room->y = region->first_y + rand() % (height - 6 + 1);
    region->room->height = 6 + rand() % (region->final_y - region->room->y - 6 + 2);

    region->room->x = region->first_x + rand() % (width - 6 + 1);
    region->room->width = 6 + rand() % (region->final_x - region->room->x - 6 + 2);

    // Build the room map
    region->room->map = (Character **)malloc(region->room->height * sizeof(Character *));
    for (int i = 0; i < region->room->height; i++) {
        region->room->map[i] = (Character *)calloc(region->room->width, sizeof(Character));
    }

    region->room->monster_count = 0;
}

void initialize_regions() {
    int x_sep = (WIDTH - 4) / HORIZONTAL;
    int y_sep = (HEIGHT - 6) / VERTICAL;

    int has_room[VERTICAL][HORIZONTAL];
    room_condition_maker(has_room);

    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            regions[i][j].first_y = i * y_sep + 4;
            regions[i][j].first_x = j * x_sep + 2;
            regions[i][j].final_y = (i + 1) * y_sep + 2;
            regions[i][j].final_x = (j + 1) * x_sep;

            if (has_room[i][j] == 0) {
                regions[i][j].room = NULL;
            }
            else {
                regions[i][j].room = (Room *)malloc(sizeof(Room));
                random_room_generator(&regions[i][j]);
            }
        }
    }

    int a = (regions[0][0].room != NULL) ? 0 : 1;
    strcpy(regions[0][a].room->kind, "start");

    int b = (regions[2][2].room != NULL) ? 2 : 3;
    strcpy(regions[2][b].room->kind, "stair");

    start_region = a;
}

void set_cor(int y, int x) {
    strcpy(screen[y][x].symbol, "░");
    screen[y][x].attributes = COLOR_PAIR(1);
}

void draw_corridors(Room *room1, Room *room2) {
    int position1 = find_region(room1);
    int position2 = find_region(room2);

    int y1, x1; // Position of the door 1 in the screen
    int y2, x2; // Position of the door 2 in the screen

    if ((position1 % HORIZONTAL) == (position2 % HORIZONTAL)) {
        // If two rooms are in same column they'll be connected on their up and down sides
        y1 = (position1 < position2) ? (room1->y + room1->height - 1) : (room1->y);
        x1 = (room1->x + 1) + rand() % (room1->width - 2);

        y2 = (position1 < position2) ? room2->y : (room2->y + room2->height - 1);
        x2 = (room2->x + 1) + rand() % (room2->width - 2);

        int max_y = (y1 > y2) ? y1 : y2;
        int min_y = (y1 > y2) ? y2 : y1;

        int max_x = (x1 > x2) ? x1 : x2;
        int min_x = (x1 > x2) ? x2 : x1;

        for (int y = min_y; y <= (min_y + (max_y - min_y) / 2); y++) {
            if (y1 < y2)
                set_cor(y, x1);
            else
                set_cor(y, x2);
        }
        for (int y = (min_y + (max_y - min_y) / 2); y <= max_y; y++) {
            if (y1 < y2)
                set_cor(y, x2);
            else
                set_cor(y, x1);
        }
        for (int x = min_x; x <= max_x; x++) {
            set_cor((min_y + (max_y - min_y) / 2), x);
        }
    }
    else if ((position1 / HORIZONTAL) == (position2 / HORIZONTAL)) {
        // If two rooms are in same row they'll be connected on their right and left sides
        y1 = (room1->y + 1) + rand() % (room1->height - 2);
        x1 = (position1 < position2) ? (room1->x + room1->width - 1) : room1->x;

        y2 = (room2->y + 1) + rand() % (room2->height - 2);
        x2 = (position1 < position2) ? room2->x : (room2->x + room2->width - 1);

        int max_y = (y1 > y2) ? y1 : y2;
        int min_y = (y1 > y2) ? y2 : y1;

        int max_x = (x1 > x2) ? x1 : x2;
        int min_x = (x1 > x2) ? x2 : x1;

        for (int x = min_x; x <= (min_x + (max_x - min_x) / 2); x++) {
            if (x1 < x2)
                set_cor(y1, x);
            else
                set_cor(y2, x);
        }
        for (int x = (min_x + (max_x - min_x) / 2); x <= max_x; x++) {
            if (x1 < x2)
                set_cor(y2, x);
            else
                set_cor(y1, x);
        }
        for (int y = min_y; y <= max_y; y++) {
            set_cor(y, (min_x + (max_x - min_x) / 2));
        }
    }
    else {
        x1 = (room1->x < room2->x) ? (room1->x + room1->width - 1) : (room1->x);
        y1 = (room1->y + 1) + rand() % (room1->height - 2);

        x2 = (room2->x + 1) + rand() % (room2->width - 2);
        y2 = (room1->y < room2->y) ? (room2->y) : (room2->y + room2->height - 1);

        int min_x = x1 < x2 ? x1 : x2;
        int max_x = x1 > x2 ? x1 : x2;
        for (int x = min_x; x <= max_x; x++) {
            set_cor(y1, x);
        }
        int min_y = y1 < y2 ? y1 : y2;
        int max_y = y1 > y2 ? y1 : y2;
        for (int y = min_y; y <= max_y; y++) {
            set_cor(y, x2);
        }
    }

    strcpy(room1->map[y1 - room1->y][x1 - room1->x].symbol, "╬");
    strcpy(room2->map[y2 - room2->y][x2 - room2->x].symbol, "╬");
}

int compare_edges(const void *a, const void *b) {
    Edge *edge1 = (Edge *)a;
    Edge *edge2 = (Edge *)b;
    return edge1->cost - edge2->cost;
}

void connect_all_rooms() {
    // Collect all rooms
    Room *rooms[VERTICAL * HORIZONTAL];
    int room_count = 0;

    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            if (regions[i][j].room != NULL) {
                rooms[room_count++] = regions[i][j].room;
            }
        }
    }

    // Generate edges based on Manhattan distance
    Edge edges[room_count * (room_count - 1) / 2];
    int edge_count = 0;

    for (int i = 0; i < room_count; i++) {
        for (int j = i + 1; j < room_count; j++) {
            int x1 = rooms[i]->x + rooms[i]->width / 2;
            int y1 = rooms[i]->y + rooms[i]->height / 2;
            int x2 = rooms[j]->x + rooms[j]->width / 2;
            int y2 = rooms[j]->y + rooms[j]->height / 2;

            edges[edge_count++] = (Edge){
                .room1_idx = i,
                .room2_idx = j,
                .cost = abs(x1 - x2) + abs(y1 - y2)};
        }
    }

    // Sort edges by cost (Manhattan distance)
    qsort(edges, edge_count, sizeof(Edge), compare_edges);

    // Create MST using Kruskal's Algorithm
    int parent[room_count];
    for (int i = 0; i < room_count; i++)
        parent[i] = i;

    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]);
        return parent[x];
    }

    void union_set(int x, int y) {
        parent[find(x)] = find(y);
    }

    for (int i = 0; i < edge_count; i++) {
        int root1 = find(edges[i].room1_idx);
        int root2 = find(edges[i].room2_idx);

        if (root1 != root2) {
            union_set(root1, root2);
            draw_corridors(rooms[edges[i].room1_idx], rooms[edges[i].room2_idx]);
        }
    }
}

void update_room_map(Room* room, int y, int x, const char* symbol, attr_t color) {
    strcpy(room->map[y][x].symbol, symbol);
    room->map[y][x].attributes = color;
}

void create_room_map(Room* room) {
    int chance = 0;

    // Room theme
    attr_t color;
    if (!strcmp(room->kind, "start")) 
        color = COLOR_PAIR(7);
    else if (!strcmp(room->kind, "stair"))
        color = COLOR_PAIR(5);
    else if (!strcmp(room->kind, "treasure"))
        color = COLOR_PAIR(1);
    else
        color = COLOR_PAIR(3);

    // Add walls and corners
    update_room_map(room, 0, 0, "╔", color);
    update_room_map(room, 0, room->width - 1, "╗", color);
    update_room_map(room, room->height - 1, 0, "╚", color);
    update_room_map(room, room->height - 1, room-> width - 1, "╝", color);
    for (int x = 1; x < room->width - 1; x++) {
        if (!strcmp(room->map[0][x].symbol, "╬"))
            room->map[0][x].attributes = color;
        else
            update_room_map(room, 0, x, "═", color);

        if (!strcmp(room->map[room->height - 1][x].symbol, "╬"))
            room->map[room->height - 1][x].attributes = color;
        else
            update_room_map(room, room->height - 1, x, "═", color);
    }
    for (int y = 1; y < room->height - 1; y++) {
        if (!strcmp(room->map[y][0].symbol, "╬"))
            room->map[y][0].attributes = color;
        else
            update_room_map(room, y, 0, "║", color);

        if (!strcmp(room->map[y][room->width - 1].symbol, "╬"))
            room->map[y][room->width - 1].attributes = color;
        else
            update_room_map(room, y, room->width - 1, "║", color);
    }

    // Add window
    chance = (rand() % 5 == 0) ? 1 : 0;
    while (chance != 0) {
        int y = 1 + (rand() % (room->height - 2));
        int x = (rand() % 2 == 0) ? 0 : (room->width - 1);
        if (strcmp(room->map[y][x].symbol, "╬")) {
            update_room_map(room, y, x, "◊", color);
            chance--;
        }
    }

    // Add pillars
    int b = (room->height - 4) / 3;
    int a = (room->width  - 4) / 3;
    for (int i = 0; i < b; i++) {
        for (int j = 0; j < a; j++) {
            if ((rand() % 2) == 0) {
                int y = (2 + (3 * i)) + rand() % 3;
                int x = (2 + (3 * j)) + rand() % 3;
                update_room_map(room, y, x, "■", color); 
            }
        }
    }

    // Add stair
    if (!strcmp(room->kind, "stair")) chance = 1;
    while (chance != 0) {
        int y = 2 + rand() % (room->height - 4);
        int x = 2 + rand() % (room->width  - 4);

        if (room->map[y][x].symbol[0] == '\0') {
            update_room_map(room, y, x, "≡", COLOR_PAIR(5) | A_BOLD);
            chance--;
        }
    }
    
    // Add gold
    chance = rand() % 4;
    while (chance != 0) {
        int y = 1 + rand() % (room->height - 2);
        int x = 1 + rand() % (room->width  - 2);

        if (room->map[y][x].symbol[0] == '\0') {
            update_room_map(room, y, x, "🜚", COLOR_PAIR(3) | A_BOLD);
            room->map[y][x].value = 10 + 5 * (rand() % 3);
            chance--;
        }
    }

    // Add black gold
    if (rand() % 20 == 0) {
        int y = 1 + rand() % (room->height - 2);
        int x = 1 + rand() % (room->width  - 2);

        if (room->map[y][x].symbol[0] == '\0') {
            update_room_map(room, y, x, "ᴥ", COLOR_PAIR(1));
            room->map[y][x].value = 100;
        }
    }

    // Add food
    if (!strcmp(room->kind, "start")) chance = 1;
    else chance = rand() % 3;
    while (chance != 0) {
        int y = 1 + rand() % (room->height - 2);
        int x = 1 + rand() % (room->width  - 2);

        if (room->map[y][x].symbol[0] == '\0') {
            int which = rand() % 16;
            if (0 <= which && which <= 13) // normal or rotten
                update_room_map(room, y, x, "֍", COLOR_PAIR(1));
            else if (which == 14) // magic
                update_room_map(room, y, x, "♣", COLOR_PAIR(6));
            else // supreme
                update_room_map(room, y, x, "♦", COLOR_PAIR(8));
            
            chance--;
        }
    }

    // Add weapon
    if (rand() % 8 == 0) {
        int y = 1 + rand() % (room->height - 2);
        int x = 1 + rand() % (room->width  - 2);

        if (room->map[y][x].symbol[0] == '\0') {
            chance = rand() % 10;
            if (0 <= chance && chance <= 2) { // Dagger
                update_room_map(room, y, x, "⸸", COLOR_PAIR(8));
            } else if (chance == 3) { // Magic Wand
                update_room_map(room, y, x, "|", COLOR_PAIR(8));
            } else if (chance == 4) { // Sword
                update_room_map(room, y, x, "⚔", COLOR_PAIR(8));
            } else { // Arrow
                update_room_map(room, y, x, "➳", COLOR_PAIR(8));
            }
        }
    }

    // Add potion
    if (rand() % 10 == 0) {
        int y = 1 + rand() % (room->height - 2);
        int x = 1 + rand() % (room->width  - 2);

        if (room->map[y][x].symbol[0] == '\0') {
            chance = rand() % 5;
            if (chance == 0 || chance == 1) { // Health
                update_room_map(room, y, x, "♥", COLOR_PAIR(4));
            } else if (chance == 2 || chance == 3) { // Speed
                update_room_map(room, y, x, "♯", COLOR_PAIR(6));
            } else if (chance == 4) { // Damage
                update_room_map(room, y, x, "●", COLOR_PAIR(8));
            }
        }
    }

    // Add trap
    chance = (strcmp(room->kind, "treasure") == 0) ? 3 : (rand() % 2);
    while (chance != 0) {
        int y = 1 + rand() % (room->height - 2);
        int x = 1 + rand() % (room->width  - 2);
        if (room->map[y][x].symbol[0] == '\0') {
            update_room_map(room, y, x, ".", COLOR_PAIR(5));
            chance--;
        }
    }

    // Add floor
    for (int i = 1; i < room->height - 1; i++) {
        for (int j = 1; j < room->width - 1; j++) {
            if (room->map[i][j].symbol[0] == '\0') {
                update_room_map(room, i, j, "·", COLOR_PAIR(5));
            }
        }
    }
}

void update_room_in_screen(int y, int x, char* symbol, attr_t attribute) {
    strcpy(screen[y][x].symbol, symbol);
    screen[y][x].attributes = attribute;
}

void create_room_in_screen(Room *room) {
    for (int i = 0; i < room->height; i++) {
        for (int j = 0; j < room->width; j++) {
            update_room_in_screen(i + room->y, j + room->x, room->map[i][j].symbol, room->map[i][j].attributes);
        }
    }
}

int is_there_monster(Room* room, int y, int x) {
    for (int i = 0; i < room->monster_count; i++) {
        if ((room->monsters[i].character.y == y) && (room->monsters[i].character.x == x)) {
            return 1;
        }
    }
    return 0;
}

void init_demon(Room* room, int monster_index) {
    Monster* monster = &room->monsters[monster_index];

    strcpy(monster->name, "DEMON");
    strcpy(monster->character.symbol, "D");
    monster->character.attributes = COLOR_PAIR(1);

    monster->health = 5;
    monster->damage = 2;
    monster->can_follow = 0;
}

void init_fire(Room* room, int monster_index) {
    Monster* monster = &room->monsters[monster_index];

    strcpy(monster->name, "FIRE BREATHING MONSTER");
    strcpy(monster->character.symbol, "F");
    monster->character.attributes = COLOR_PAIR(3);

    monster->health = 10;
    monster->damage = 5;
    monster->can_follow = 0;
}

void init_giant(Room* room, int monster_index) {
    Monster* monster = &room->monsters[monster_index];

    strcpy(monster->name, "GIANT");
    strcpy(monster->character.symbol, "G");
    monster->character.attributes = COLOR_PAIR(1);

    monster->health = 15;
    monster->damage = 10;
    monster->can_follow = 1;
    monster->walk = 0;
}

void init_snake(Room* room, int monster_index) {
    Monster* monster = &room->monsters[monster_index];

    strcpy(monster->name, "SNAKE");
    strcpy(monster->character.symbol, "S");
    monster->character.attributes = COLOR_PAIR(1);

    monster->health = 20;
    monster->damage = 15;
    monster->can_follow = 1;
}

void init_undeed(Room* room, int monster_index) {
    Monster* monster = &room->monsters[monster_index];

    strcpy(monster->name, "UNDEED");
    strcpy(monster->character.symbol, "U");
    monster->character.attributes = COLOR_PAIR(1);

    monster->health = 30;
    monster->damage = 20;
    monster->can_follow = 1;
    monster->walk = 0;
}

void set_monster(Room* room, int monster_index) {
    Monster* monster = &room->monsters[monster_index];
    while (1) {
        int y = (room->y + 1) + rand() % (room->height - 2);
        int x = (room->x + 1) + rand() % (room->width - 2);
        if (!is_there_monster(room, y, x) && strcmp(room->map[y - room->y][x - room->x].symbol, "■")) {
            monster->character.y = y;
            monster->character.x = x;

            monster->previous.y = y;
            monster->previous.x = x;

            // Set previous map index of the monster
            strcpy(monster->previous.symbol, room->map[y - room->y][x - room->x].symbol);
            monster->previous.attributes = room->map[y - room->y][x - room->x].attributes;

            // Set the screen location of monster
            strcpy(screen[y][x].symbol, monster->character.symbol);
            screen[y][x].attributes = monster->character.attributes;

            return;
        }
    }
}

void add_monsters(Room* room) {
    int random_number = ((int)((room->height - 2) / 5)  + 1) * ((int)((room->width - 2) / 5) + 1);
    if (random_number > 5) random_number = 5;
    
    int total_monster = 1 + rand() % random_number;
    if (!strcmp(room->kind, "start")) {
        init_demon(room, 0);
        set_monster(room, 0);
        room->monster_count++;
        return;
    } else if (!strcmp(room->kind, "start")) {
        init_undeed(room, 0);
        set_monster(room, 0);

        init_snake(room, 1);
        set_monster(room, 1);

        init_snake(room, 2);
        set_monster(room, 2);

        init_undeed(room, 3);
        set_monster(room, 3);

        init_giant(room, 4);
        set_monster(room, 4);

        room->monster_count = 5;
        return;
    }

    for (int i = 0; i < total_monster; i++) {
        int which = rand() % 100;
        if (0 <= which && which <= 39) { // Demon %40 chance
            init_demon(room, i);
        } else if (40 <= which && which <= 74) { // Fire Breathing Monster %35 chance
            init_fire(room, i);
        } else if (75 <= which && which <= 89) { // Giant %15 chance
            init_giant(room, i);
        } else { // Snake %10 chance
            init_snake(room, i);
        }
        set_monster(room, i);
        room->monster_count++;
    }
}

void draw_room(Room *room) {
    for (int y = room->y; y < room->y + room->height; y++) {
        for (int x = room->x; x < room->x + room->width; x++) {
            attron(screen[y][x].attributes);
            mvprintw(y, x, "%s", screen[y][x].symbol);
            attroff(screen[y][x].attributes);

            screen[y][x].condition = 1;
        }
    }
    refresh();
}

void init_inventory() {
    // Food
    // regular or rotten
    hero.inventory.foods[0].number = 0;
    hero.inventory.foods[0].value  = 10 + rand() % 6;
    // magic
    hero.inventory.foods[1].number = 0;
    hero.inventory.foods[1].value  = 15 + rand() % 6;
    // supreme
    hero.inventory.foods[2].number = 0;
    hero.inventory.foods[3].value  = 15 + rand() % 6;

    // Weapon
    hero.inventory.which_weapon = 0;
    // mace
    hero.inventory.weapons[0].number = 1;
    hero.inventory.weapons[0].value  = 5;
    // dagger
    hero.inventory.weapons[1].number = 0;
    hero.inventory.weapons[1].value  = 12;
    // wand
    hero.inventory.weapons[2].number = 0;
    hero.inventory.weapons[2].value  = 15;
    // arrow
    hero.inventory.weapons[3].number = 0;
    hero.inventory.weapons[3].value  = 5;
    // sword
    hero.inventory.weapons[4].number = 0;
    hero.inventory.weapons[4].value  = 10;

    // Potion
    // health
    hero.inventory.potions[0].number = 0;
    // speed
    hero.inventory.potions[1].number = 0;
    // damage
    hero.inventory.potions[2].number = 0;
}

void set_hero() {
    int y = hero.character.y;
    int x = hero.character.x;
    strcpy(screen[y][x].symbol, hero.character.symbol);
    screen[y][x].attributes = hero.character.attributes;
    screen[y][x].condition = 1;
}

void init_hero() {
    // Set hero
    // Character
    strcpy(hero.character.symbol, "☻");

    //Attributes
    if (!strcmp(current_user.settings.hero_color, "yellow"))
        hero.character.attributes = COLOR_PAIR(3);
    else if (!strcmp(current_user.settings.hero_color, "green"))
        hero.character.attributes = COLOR_PAIR(5);
    else if (!strcmp(current_user.settings.hero_color, "red"))
        hero.character.attributes = COLOR_PAIR(4);
    else if (!strcmp(current_user.settings.hero_color, "blue"))
        hero.character.attributes = COLOR_PAIR(6);

    // Position
    Region start = regions[start_region / HORIZONTAL][start_region % HORIZONTAL];
    hero.character.y = start.room->y + 1;
    hero.character.x = start.room->x + 1;

    // Inventory
    init_inventory();

    // Other
    hero.walk = 0;
    hero.level = 0;
    if (!strcmp(current_user.settings.difficulty, "easy")) {
        hero.health = 100;
        hero.hunger = 100;
    } else if (!strcmp(current_user.settings.difficulty, "normal")) {
        hero.health = 85;
        hero.hunger = 85;
    } else {
        hero.health = 70;
        hero.hunger = 70;
    }
    hero.gold = 0;
    hero.healing_speed = 1;
    hero.speed = 1;
    hero.base_power = 0;

    // Set previous location of hero
    strcpy(previous.symbol, "·");
    previous.attributes = A_BOLD | COLOR_PAIR(5);
    previous.y = start.room->y + 1;
    previous.x = start.room->x + 1;

    // Draw hero
    attron(hero.character.attributes);
    mvprintw(hero.character.y, hero.character.x, "%s", hero.character.symbol);
    attroff(hero.character.attributes);
    refresh();
}

void show_health() {
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(2, 3, "HEALTH: ");
    attroff(COLOR_PAIR(3) | A_BOLD);

    attron(COLOR_PAIR(4));
    int health_bar = (hero.health % 10 == 0) ? (hero.health / 10) : ((int)(hero.health / 10) + 1);
    for (int i = 0; i < health_bar; i++) {
        printw("♥ ");
    }
    for (int i = health_bar; i < 10; i++) {
        printw("♡ ");
    }
    printw("%d", hero.health);
    attroff(COLOR_PAIR(4));
}

void show_hunger() {
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(2, 37, "HUNGER: ");
    attroff(COLOR_PAIR(3) | A_BOLD);

    attron(COLOR_PAIR(5));
    int hunger = (hero.hunger % 10 == 0) ? (hero.hunger / 10) : ((int)(hero.hunger / 10) + 1);
    for (int i = 0; i < hunger; i++) {
        printw("■ ");
    }
    for (int i = hunger; i < 10; i++) {
        printw("□ ");
    }
    printw("%d", hero.hunger);
    attroff(COLOR_PAIR(5));
}

void show_gold() {
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(2, 71, "GOLD: ");
    attroff(COLOR_PAIR(3) | A_BOLD);

    attron(COLOR_PAIR(1));
    printw("%d", hero.gold);
    attroff(COLOR_PAIR(1));
}

void clear_inventory_bar() {
    for (int i = 84; i < WIDTH - 3; i++) {
            mvprintw(2, i, " ");
    }
}

void consume_food() {
    show_message("I or ESC: quit | ENTER: eat");
    clear_inventory_bar();
    int max_width = (WIDTH - 4) - 84;

    char* foods[20] = {
        "🥔",
        "✨",
        "🥞"
    };

    int which = 0;
    int ch = 0;
    while (1) {
            move(2, 84 + (max_width - 24) / 2);
            for (int i = 0; i < 3; i++) {
                if (i == which) {
                    attron(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
                    printw(" %s (%d) ", foods[i], hero.inventory.foods[i].number);
                    attroff(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
                }
                else {
                    attron(COLOR_PAIR(1));
                    printw(" %s (%d) ", foods[i], hero.inventory.foods[i].number);
                    attroff(COLOR_PAIR(1));
                }
            }

            ch = getch();
            if (ch == 'i' || ch == 27) {
                clear_inventory_bar();
                return;
            }
            else if (ch == KEY_RIGHT || ch == 'd') 
                which = (which + 1) % 3;
            else if (ch == KEY_LEFT || ch == 'a')
                which = (which - 1 + 3) % 3;
            else if (ch == '\n') {
                if (hero.inventory.foods[which].number == 0)
                    show_message("You have nothing of the chosen food!");
                else {
                    switch (which) {
                    case 0:
                        if (rand() % 5 == 0) {
                            show_message("OOPS!! The food was rotten!");
                            hero.hunger -= hero.inventory.foods[0].value / 2;
                            hero.health -= hero.inventory.foods[0].value / 2;
                            show_health();
                        } else {
                            show_message("Hmm! That was good!");
                            hero.hunger += hero.inventory.foods[0].value;
                        }
                        break;
                    
                    case 1:
                        show_message("WOW! I feel energized!");
                        hero.hunger += hero.inventory.foods[1].value;
                        hero.speed = 2;
                        hero.inventory.foods[1].first_consume = hero.walk;
                        break;

                    case 3:
                        show_message("My GOD! I can feel the strength in my muscles!");
                        hero.hunger += hero.inventory.foods[2].value;
                        hero.base_power = 5;
                        hero.inventory.foods[2].first_consume = hero.walk;
                        break;
                    }

                    if (hero.hunger > 100) hero.hunger = 100;
                    else if (hero.hunger < 5) hero.hunger = 4;

                    show_hunger();
                    hero.inventory.foods[which].number--;
                }
            }
    }
}

void choose_weapon() {
    show_message("I or ESC: quit | ENTER: choose");
    clear_inventory_bar();
    int max_width = (WIDTH - 4) - 84;

    char* weapons[20] = {
        "🪓",
        "🗡",
        "🪄",
        "➳",
        "⚔"
    };

    int which = hero.inventory.which_weapon;
    int ch = 0;
    while (1) {
            move(2, 84 + (max_width - 40) / 2);
            for (int i = 0; i < 5; i++) {
                if (i == which) {
                    attron(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
                    printw(" %s (%d) ", weapons[i], hero.inventory.weapons[i].number);
                    attroff(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
                }
                else {
                    attron(COLOR_PAIR(1));
                    printw(" %s (%d) ", weapons[i], hero.inventory.weapons[i].number);
                    attroff(COLOR_PAIR(1));
                }
            }

            ch = getch();
            if (ch == 'i' || ch == 27) {
                clear_inventory_bar();
                return;
            }
            else if (ch == KEY_RIGHT || ch == 'd') 
                which = (which + 1) % 5;
            else if (ch == KEY_LEFT || ch == 'a')
                which = (which - 1 + 5) % 5;
            else if (ch == '\n') {
                if (hero.inventory.weapons[which].number == 0)
                    show_message("You don't have the chosen weapon!");
                else {
                    hero.inventory.which_weapon = which;
                    show_message("The weapon has been selected.");
                }
            }
    }
}

void consume_potion() {
    show_message("I or ESC: quit | ENTER: eat");
    clear_inventory_bar();
    int max_width = (WIDTH - 4) - 84;

    char* potions[20] = {
        "❤️",
        "🚀",
        "💪"
    };

    int which = 0;
    int ch = 0;
    while (1) {
            move(2, 84 + (max_width - 24) / 2);
            for (int i = 0; i < 3; i++) {
                if (i == which) {
                    attron(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
                    printw(" %s (%d) ", potions[i], hero.inventory.potions[i].number);
                    attroff(COLOR_PAIR(1) | A_BOLD | A_UNDERLINE);
                }
                else {
                    attron(COLOR_PAIR(1));
                    printw(" %s (%d) ", potions[i], hero.inventory.potions[i].number);
                    attroff(COLOR_PAIR(1));
                }
            }

            ch = getch();
            if (ch == 'i' || ch == 27) {
                clear_inventory_bar();
                return;
            }
            else if (ch == KEY_RIGHT || ch == 'd') 
                which = (which + 1) % 3;
            else if (ch == KEY_LEFT || ch == 'a')
                which = (which - 1 + 3) % 3;
            else if (ch == '\n') {
                if (hero.inventory.potions[which].number == 0)
                    show_message("You have nothing of the chosen potion!");
                else {
                    switch (which) {
                    case 0:
                        hero.healing_speed = 2;
                        show_message("Now you are healing faster.");
                        break;
                    
                    case 1:
                        hero.speed = 3;
                        show_message("Now you are faster.");
                        break;

                    case 2:
                        hero.base_power = -2;
                        show_message("Now you are stronger.");
                        break;
                    }
                    hero.inventory.potions[which].first_consume = hero.walk;
                    hero.inventory.potions[which].number--;
                }
            }
    }
}

void show_inventory(int condition) {
    clear_inventory_bar();
    int max_width = (WIDTH - 4) - 84;

    if (condition == 0) {
        const char* message = "PRESS 'I' TO OPEN YOUR INVENTORY";
        int x = 84 + (max_width - strlen(message)) / 2;

        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(2, x, "%s", message);
        attroff(COLOR_PAIR(3) | A_BOLD);
    }
    else {
        const char* menu[] = {
            "FOOD",
            "WEAPON",
            "POTION"
        };

        int which = 0;
        int ch = 0;
        while (1) {
            move(2, 84 + (max_width - 32) / 2);
            for (int i = 0; i < 3; i++) {
                if (i == which) {
                    attron(COLOR_PAIR(2) | A_BOLD);
                    printw("<< %s >>", menu[i]);
                    attroff(COLOR_PAIR(2) | A_BOLD);
                }
                else {
                    attron(COLOR_PAIR(3) | A_BOLD);
                    printw("   %s   ", menu[i]);
                    attroff(COLOR_PAIR(3) | A_BOLD);
                }
            }

            ch = getch();
            if ((ch == 27) || (ch == 'i')) {
                show_inventory(0);
                return;
            }
            else if (ch == KEY_RIGHT || ch == 'd') 
                which = (which + 1) % 3;
            else if (ch == KEY_LEFT || ch == 'a')
                which = (which - 1 + 3) % 3;
            else if (ch == '\n') {
                switch (which) {
                case 0:
                    consume_food();
                    break;
                case 1:
                    choose_weapon();
                    break;
                case 2:
                    consume_potion();
                    break;
                }
            }
        }
    }
    refresh();
}

void guide() {
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(2, 35, "┇");
    mvprintw(2, 69, "┇");
    mvprintw(2, 82, "┇");
    attroff(COLOR_PAIR(3) | A_BOLD);

    show_health();
    show_hunger();
    show_gold();
    show_inventory(0);

    refresh();
}

void earn_gold(Room* room, int new_y, int new_x) {
    int y = new_y - room->y;
    int x = new_x - room->x;

    char message[50];
    sprintf(message, "You've earned %d golds!", room->map[y][x].value);
    show_message(message);

    hero.gold += room->map[y][x].value;
    show_gold();

    strcpy(previous.symbol, "·");
    previous.attributes = COLOR_PAIR(5);

    update_room_in_screen(previous.y, previous.x, "·", COLOR_PAIR(5));
    update_room_map(room, y, x, "·", COLOR_PAIR(5));
}

void earn_food(Room* room, int new_y, int new_x, int which_food) {
    int y = new_y - room->y;
    int x = new_x - room->x;

    switch (which_food) {
    case 0:
        show_message("You've got a normal food!");
        hero.inventory.foods[0].number++;
        break;
    case 1:
        show_message("You've got a magic food!");
        hero.inventory.foods[1].number++;
        break;
    case 2:
        show_message("You've got a supreme food!");
        hero.inventory.foods[2].number++;
        break;
    }

    strcpy(previous.symbol, "·");
    previous.attributes = COLOR_PAIR(5);

    update_room_in_screen(previous.y, previous.x, "·", COLOR_PAIR(5));
    update_room_map(room, y, x, "·", COLOR_PAIR(5));
}

void delete_all_swords() {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            Room* room = regions[i][j].room;
            if (room != NULL) {
                for (int y = 1; y < room->height - 1; y++) {
                    for (int x = 1; x < room->width - 1; x++) {
                        if (!strcmp(room->map[y][x].symbol, "⚔")) {
                            update_room_in_screen(room->y + y, room->x + x, "·", COLOR_PAIR(5));
                            update_room_map(room, y, x, "·", COLOR_PAIR(5));

                            if (screen[room->y + y][room->x + x].condition) {
                                attron(COLOR_PAIR(5));
                                mvprintw(room->y + y, room->x + x, "·");
                                attroff(COLOR_PAIR(5));
                            }
                        }
                    }
                }
            }
        }
    }
    // Draw Hero
    attron(hero.character.attributes);
    mvprintw(hero.character.y, hero.character.x, "%s", hero.character.symbol);
    attroff(hero.character.attributes);
}

void get_weapon(Room* room, int new_y, int new_x, int which_weapon) {
    show_message("Press enter to get the weapon");
    int ch = getch();
    if (ch != '\n') return;

    int y = new_y - room->y;
    int x = new_x - room->x;

    switch (which_weapon) {
    case 1:
        show_message("You've got 5 daggers.");
        hero.inventory.weapons[1].number += 5;
        break;
    case 2:
        show_message("You've got 4 magic wands.");
        hero.inventory.weapons[2].number += 4;
        break;
    case 3:
        show_message("You've got 10 arrows.");
        hero.inventory.weapons[3].number += 10;
        break;
    case 4:
        show_message("Well done! You've got a sword.");
        hero.inventory.weapons[4].number = 1;
        delete_all_swords();
        break;
    }

    strcpy(previous.symbol, "·");
    previous.attributes = COLOR_PAIR(5);

    update_room_in_screen(previous.y, previous.x, "·", COLOR_PAIR(5));
    update_room_map(room, y, x, "·", COLOR_PAIR(5));

    refresh();
}

void get_potion(Room* room, int new_y, int new_x, int which_potion) {
    show_message("Press enter to get the potion");
    int ch = getch();
    if (ch != '\n') return;

    int y = new_y - room->y;
    int x = new_x - room->x;

    switch (which_potion) {
    case 0:
        show_message("You've got a health potion.");
        break;
    case 1:
        show_message("You've got a speed potion.");
        break;
    case 2:
        show_message("You've got a damage potion.");
        break;
    }
    hero.inventory.potions[which_potion].number++;

    strcpy(previous.symbol, "·");
    previous.attributes = COLOR_PAIR(5);

    update_room_in_screen(previous.y, previous.x, "·", COLOR_PAIR(5));
    update_room_map(room, y, x, "·", COLOR_PAIR(5));
}

void trap(Room* room, int new_y, int new_x) {
    int y = new_y - room->y;
    int x = new_x - room->x;

    printf("\a");
    show_message("You stepped on a trap!");
    hero.health -= 5;
    show_health();

    strcpy(previous.symbol, "^");
    previous.attributes = COLOR_PAIR(5);

    update_room_in_screen(previous.y, previous.x, "^", COLOR_PAIR(5));
    update_room_map(room, y, x, "^", COLOR_PAIR(5));
}

void update_previous(int new_y, int new_x) {
    // Restore the previous tile before moving
    attron(previous.attributes);
    mvprintw(previous.y, previous.x, "%s", previous.symbol);
    attroff(previous.attributes);

    // Update previous with the hero's next position before moving
    previous.y = new_y;
    previous.x = new_x;
    strcpy(previous.symbol, screen[new_y][new_x].symbol);
    previous.attributes = screen[new_y][new_x].attributes;

    // Move the hero
    hero.character.y = new_y;
    hero.character.x = new_x;

    // Draw the hero at the new position
    attron(hero.character.attributes);
    mvprintw(hero.character.y, hero.character.x, "%s", hero.character.symbol);
    attroff(hero.character.attributes);

    // Set the condition to 1
    screen[hero.character.y][hero.character.x].condition = 1;

    refresh();
}

void show_screen(int condition) {
    for (int i = 4; i < HEIGHT - 1; i++) {
        for (int j = 1; j < WIDTH - 1; j++) {
            if (screen[i][j].symbol[0] != '\0') {
                attron(screen[i][j].attributes);
                if (condition || screen[i][j].condition)
                    mvprintw(i, j, "%s", screen[i][j].symbol);
                else
                    mvprintw(i, j, " ");
                attroff(screen[i][j].attributes);
            }
        }
    }

    // Draw hero
    attron(hero.character.attributes);
    mvprintw(hero.character.y, hero.character.x, "%s", hero.character.symbol);
    attroff(hero.character.attributes);

    refresh();
}

// For hero
int is_walkable(int y, int x) {
    if (!strcmp(screen[y][x].symbol, "◊")) return -1;
    else if (screen[y][x].symbol[0] == '\0') return 0;

    return ((screen[y][x].symbol[0] != 'D') && (screen[y][x].symbol[0] != 'F')
         && (screen[y][x].symbol[0] != 'G') && (screen[y][x].symbol[0] != 'S')
         && (screen[y][x].symbol[0] != 'U') && strcmp(screen[y][x].symbol, "■")
         && strcmp(screen[y][x].symbol, "║") && strcmp(screen[y][x].symbol, "═")
         && strcmp(screen[y][x].symbol, "╔") && strcmp(screen[y][x].symbol, "╗")
         && strcmp(screen[y][x].symbol, "╝") && strcmp(screen[y][x].symbol, "╚"));
}

// For monsters
int is_walkable_monster(int y, int x) {
    if (screen[y][x].symbol[0] == '\0') return 0;

    return (strcmp(screen[y][x].symbol, hero.character.symbol)
         && strcmp(screen[y][x].symbol, "■") && strcmp(screen[y][x].symbol, "◊")
         && strcmp(screen[y][x].symbol, "║") && strcmp(screen[y][x].symbol, "═")
         && strcmp(screen[y][x].symbol, "╔") && strcmp(screen[y][x].symbol, "╗")
         && strcmp(screen[y][x].symbol, "╝") && strcmp(screen[y][x].symbol, "╚"));
}

// For showing new corridors
int is_there_corridor(int y, int x) {
    if (!strcmp(screen[y - 1][x].symbol, "░") && !screen[y - 1][x].condition) return 1;
    if (!strcmp(screen[y][x + 1].symbol, "░") && !screen[y][x + 1].condition) return 2;
    if (!strcmp(screen[y + 1][x].symbol, "░") && !screen[y + 1][x].condition) return 3;
    if (!strcmp(screen[y][x - 1].symbol, "░") && !screen[y][x - 1].condition) return 4;
    return 0;
}

// For long-range weapons
int is_it_block(int y, int x) {
    if (screen[y][x].symbol[0] == '\0') return 1;

    return (!strcmp(screen[y][x].symbol, "■")
         || !strcmp(screen[y][x].symbol, "║") || !strcmp(screen[y][x].symbol, "═")
         || !strcmp(screen[y][x].symbol, "╔") || !strcmp(screen[y][x].symbol, "╗")
         || !strcmp(screen[y][x].symbol, "╝") || !strcmp(screen[y][x].symbol, "╚"));
}

int monster_index_finder(Room* room, int y, int x) {
    for (int i = 0; i < room->monster_count; i++) {
        if ((room->monsters[i].character.y == y) 
         && (room->monsters[i].character.x == x) 
         && (room->monsters[i].health > 0))
            return i;
    }
    return -1;
}

void attack_monster(Monster* monster, int which_weapon) {
    int weapon_damage = hero.inventory.weapons[which_weapon].value;
    int damage = (hero.base_power == -2) ? (2 * weapon_damage) : (hero.base_power + weapon_damage);
    monster->health -= damage;
    if (monster->health > 0) {
        char message[60];
        sprintf(message, "%s health: %d", monster->name, monster->health);
        show_message(message);

        if (monster->can_follow && which_weapon == 2) {
            getch();
            monster->can_follow = 0;
            char new_message[70];
            sprintf(new_message, "The %s walking ability has been disabled.", monster->name);
            show_message(new_message);
        }
    } else {
        char message[60];
        sprintf(message, "You killed the %s", monster->name);
        show_message(message);
        
        int y = monster->character.y;
        int x = monster->character.x;
        update_room_in_screen(y, x, monster->previous.symbol, monster->previous.attributes);
        attron(screen[y][x].attributes);
        mvprintw(y, x, "%s", screen[y][x].symbol);
        attroff(screen[y][x].attributes);
    }

    getch();
    show_message(" ");
}

void use_short_range(Room* room, int which) {
    int flag = 0;
    for (int y = hero.character.y - 1; y <= hero.character.y + 1; y++) {
        for (int x = hero.character.x - 1; x <= hero.character.x + 1; x++) {
            int index = monster_index_finder(room, y, x);
            if (index != -1) {
                attack_monster(&room->monsters[index], which);
                flag++;
            }
        }
    }

    if (!flag) {
        show_message("There is no monster nearby!");
    }
}

void use_long_range(Room* room, int which) {
    int range = (which == 2) ? 10 : 5;
    char* symbol = (which == 1) ? "⸸" : ((which == 2) ? "/" : "➳");

    int y = hero.character.y;
    int x = hero.character.x;

    show_message("Select the way you want to shoot!");
    int ch = getch();
    int flag = 0;
    while (range != 0) {
        switch (ch) {
            case 'd': case KEY_RIGHT: x++; break;
            case 'a': case KEY_LEFT: x--; break;
            case 's': case KEY_DOWN: y++; break;
            case 'w': case KEY_UP: y--; break;
            default: show_message("Invalid Input!"); return;
        }

        if (is_it_block(y, x))  {
            show_message("You missed the monster.");
            break;
        }

        int index = monster_index_finder(room, y, x);
        if (index != -1) {
            attack_monster(&room->monsters[index], which);
            flag++;
            break;
        }

        attron(COLOR_PAIR(8));
        mvprintw(y, x, "%s", symbol);
        attroff(COLOR_PAIR(8));
        refresh();

        usleep(150000);

        attron(screen[y][x].attributes);
        mvprintw(y, x, "%s", screen[y][x].symbol);
        attroff(screen[y][x].attributes);
        refresh();

        range--;
    }

    if (!flag) {
        show_message("You missed the monster.");
    }

    if (--hero.inventory.weapons[which].number == 0) {
        hero.inventory.which_weapon = 0;
    }
}

void use_weapon() {
    int reg = find_region_by_coordinate(hero.character.y, hero.character.x);
    Room* room = regions[reg / HORIZONTAL][reg % HORIZONTAL].room;

    switch (hero.inventory.which_weapon) {
    case 0: 
        use_short_range(room, 0);
        break;
    case 1:
        use_long_range(room, 1);
        break;
    case 2:
        use_long_range(room, 2);
        break;
    case 3:
        use_long_range(room, 3);
        break;
    case 4:
        use_short_range(room, 4);
        break;
    }
}

void game_over(int over_code) {
    show_message("!OH NO!");
    for (int i = 0; i < 3; i++) {
        int y = hero.character.y;
        int x = hero.character.x;

        attron(hero.character.attributes);
        mvprintw(y, x, " ");
        refresh();
        usleep(250000);

        mvprintw(y, x, "%s", hero.character.symbol);
        refresh();
        usleep(250000);
        attroff(hero.character.attributes);
    }

    clear();
    draw_border();
    show_logo(3);

    curs_set(TRUE);

    attron(COLOR_PAIR(5));
    switch (over_code) {
    case 0:
        mvprintw(13, (WIDTH - 21) / 2, "YOU DIED FROM HUNGER!");
        break;
    case 1:
        mvprintw(13, (WIDTH - 22) / 2, "YOU STEPPED ON A TRAP!");
        break;
    case 2:
        mvprintw(13, (WIDTH - 29) / 2, "YOU WERE KILLED BY THE DEMON!");
        break;
    case 3:
        mvprintw(13, (WIDTH - 46) / 2, "YOU WERE KILLED BY THE FIRE BREATHING MONSTER!");
        break;
    case 4:
        mvprintw(13, (WIDTH - 29) / 2, "YOU WERE KILLED BY THE GIANT!");
        break;
    case 5:
        mvprintw(13, (WIDTH - 29) / 2, "YOU WERE KILLED BY THE SNAKE!");
        break;
    case 6:
        mvprintw(13, (WIDTH - 30) / 2, "YOU WERE KILLED BY THE UNDEED!");
        break;
    }
    attroff(COLOR_PAIR(5));

    attron(COLOR_PAIR(1));
    mvprintw(15, (WIDTH - 26) / 2, "Press ENTER to continue...");
    attroff(COLOR_PAIR(1));

    refresh();

    free_screen();

    int ch = getch();
    while (ch != '\n') ch = getch();
    curs_set(FALSE);
}

int save_regions(FILE *file) {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            Region *reg = &regions[i][j];

            // Save basic Region data
            fwrite(&reg->first_x, sizeof(int), 1, file);
            fwrite(&reg->first_y, sizeof(int), 1, file);
            fwrite(&reg->final_x, sizeof(int), 1, file);
            fwrite(&reg->final_y, sizeof(int), 1, file);

            // Check if room exists and save flag
            int has_room = (reg->room != NULL) ? 1 : 0;
            fwrite(&has_room, sizeof(int), 1, file);

            if (has_room) {
                Room *room = reg->room;

                // Save Room data
                fwrite(room, sizeof(Room), 1, file);

                // Save map
                for (int y = 0; y < room->height; y++) {
                    fwrite(room->map[y], sizeof(Character), room->width, file);
                }
            }
        }
    }
    return 1;
}

int save_game(const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for saving");
        return 0;
    }

    // Save HEIGHT and WIDTH
    fwrite(&HEIGHT, sizeof(int), 1, file);
    fwrite(&WIDTH, sizeof(int), 1, file);

    // Save Hero
    fwrite(&hero, sizeof(Hero), 1, file);

    // Save previous Character
    fwrite(&previous, sizeof(Character), 1, file);

    // Save Regions
    save_regions(file);

    // Save start_region
    fwrite(&start_region, sizeof(int), 1, file);

    // Save Screen
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            fwrite(&screen[i][j], sizeof(Screen), 1, file);
        }
    }
    free_screen();

    fclose(file);

    return 1;
}

int save_bar() {
    clear_inventory_bar();
    int max_width = (WIDTH - 4) - 84;

    const char* menu[] = {
        "EXIT",
        "SAVE & EXIT"
    };

    int which = 0;
    int ch = 0;
    while (1) {
        move(2, 84 + (max_width - 27) / 2);
        for (int i = 0; i < 2; i++) {
            if (i == which) {
                attron(COLOR_PAIR(2) | A_BOLD);
                printw("<< %s >>", menu[i]);
                attroff(COLOR_PAIR(2) | A_BOLD);
            }
            else {
                attron(COLOR_PAIR(3) | A_BOLD);
                printw("   %s   ", menu[i]);
                attroff(COLOR_PAIR(3) | A_BOLD);
            }
        }
        refresh();

        ch = getch();
        if (ch == 27) {
            show_inventory(0);
            return -1;
        }
        else if (ch == KEY_RIGHT || ch == 'd') 
            which = (which + 1) % 2;
        else if (ch == KEY_LEFT || ch == 'a')
            which = (which - 1 + 2) % 2;
        else if (ch == '\n') {
            switch (which) {
            case 0:
                return 0;
            case 1: {
                char file[55];
                sprintf(file, "%s.dat", current_user.name);
                return save_game(file);
            }
            }
        }
    }
}

int is_hero_in_room(Room* room) {
    if (!room) return 0;

    int y1 = room->y;
    int y2 = room->y + room->height - 1;

    int x1 = room->x;
    int x2 = room->x + room->width - 1;

    return ((y1 < hero.character.y) && (hero.character.y < y2)
         && (x1 < hero.character.x) && (hero.character.x < x2));
}

void move_monster(Monster* monster) {
    int dy = monster->character.y - hero.character.y;
    int dx = monster->character.x - hero.character.x;

    int new_y = monster->character.y;
    int new_x = monster->character.x;

    if ((dy > 1) && is_walkable_monster(new_y - 1, new_x)) {
        new_y--;
    } else if ((dy < -1) && is_walkable_monster(new_y + 1, new_x)) {
        new_y++;
    }

    if ((dx > 1) && is_walkable_monster(new_y, new_x - 1)) {
        new_x--;
    } else if ((dx < -1) && is_walkable_monster(new_y, new_x + 1)) {
        new_x++;
    }

    // Giant and Undeed stop following the hero after the mentioned block
    if ((new_y != monster->character.y) || (new_x != monster->character.x)) {
        if ((monster->character.symbol[0] == 'G') && (++monster->walk == 10)) {
            monster->can_follow = 0;
        } else if ((monster->character.symbol[0] == 'U') && (++monster->walk == 10)) {
            monster->can_follow = 0;
        }
    }

    // Draw previous
    Character p = monster->previous;
    attron(p.attributes);
    mvprintw(p.y, p.x, "%s", p.symbol);
    attroff(p.attributes);
    update_room_in_screen(p.y, p.x, p.symbol, p.attributes);
    // Update the location
    monster->previous.y = new_y;
    monster->previous.x = new_x;

    // Draw monster
    Character c = monster->character;
    attron(c.attributes);
    mvprintw(new_y, new_x, "%s", c.symbol);
    attroff(c.attributes);
    update_room_in_screen(new_y, new_x, c.symbol, c.attributes);
    // Update the location
    monster->character.y = new_y;
    monster->character.x = new_x;

    refresh();
}

int monster_attacks(Monster* monster) {
    int dy = monster->character.y - hero.character.y;
    int dx = monster->character.x - hero.character.x;

    if (((dy == 1) || (dy == -1))
     && ((dx == 1) || (dx == -1))) {
        hero.health -= monster->damage;
        printf("\a");

        if (hero.health > 0) {
            char message[75];
            sprintf(message, "The %s hit you! Press any key to continue...", monster->name);
            show_message(message);
            getch();
        } else {
            switch (monster->character.symbol[0]) {
            case 'D':
                game_over(2);
                return -1;
            case 'F':
                game_over(3);
                return -1;
            case 'G':
                game_over(4);
                return -1;
            case 'S':
                game_over(5);
                return -1;
            case 'U':
                game_over(6);
                return -1;
            }
        }

        show_health();
    }

    return 1;
}

int process_monster() {
    int reg = find_region_by_coordinate(hero.character.y, hero.character.x);
    Room* room = regions[reg / HORIZONTAL][reg % HORIZONTAL].room;
    if (!room) return 0;

    if (is_hero_in_room(room)) {
        for (int i = 0; i < room->monster_count; i++) {
            if (room->monsters[i].health <= 0) continue;

            Monster* monster = &room->monsters[i];
            if (monster->can_follow) {
                move_monster(monster);
            }
            if(monster_attacks(monster) == -1) {
                return -1;
            }
        }
    }
    
    return 1;
}

void game_win() {
    clear();
    draw_border();
    show_logo(4);

    curs_set(TRUE);

    attron(COLOR_PAIR(8));
    mvprintw(8, (WIDTH - 41) / 2, "CONGRATULATIONS! YOU'VE DONE A GREAT JOB.");
    attroff(COLOR_PAIR(8));

    attron(COLOR_PAIR(1));
    mvprintw(10, (WIDTH - 26) / 2, "Press ENTER to continue...");
    attroff(COLOR_PAIR(1));

    refresh();

    int ch = getch();
    while (ch != '\n') ch = getch();
    curs_set(FALSE);

    char filename[60];
    sprintf(filename, "%s.dat", current_user.name);
    if (file_exists(filename)) remove(filename);
}

int use_stair() {
    show_message("Press ENTER to end the game");
    int ch = getch();
    if (ch != '\n') return 0;

    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            if (regions[i][j].room != NULL) {
                Room* room = regions[i][j].room;
                for (int k = 0; k < room->monster_count; k++) {
                    if (room->monsters[k].health > 0) {
                        return -1;
                    }
                }
            }
        }
    }
    game_win();
    return 1;
}

void move_player() {
    int show_map = 0;

    while (1) {
        int ch = getchar();

        if (ch == 27) {
            if (save_bar() == -1) continue;
            return;
        }
        else if (ch == 'm') {
            show_map = !show_map;
            show_screen(show_map);
            continue;
        }
        else if (ch == 'i') {
            show_inventory(1);
            continue;
        }
        else if (ch == ' ') {
            use_weapon();
            continue;
        }

        int new_y = hero.character.y, new_x = hero.character.x;
        for (int i = 0; i < hero.speed; i++) {
            int temp_y = new_y, temp_x = new_x;

            switch (ch) {
            case 'w': temp_y--; break; // Move up
            case 's': temp_y++; break; // Move down
            case 'a': temp_x--; break; // Move left
            case 'd': temp_x++; break; // Move right
            case 'q': temp_y--; temp_x--; break; // Up-left
            case 'e': temp_y--; temp_x++; break; // Up-right
            case 'z': temp_y++; temp_x--; break; // Down-left
            case 'c': temp_y++; temp_x++; break; // Down-right
            default:
                show_message("Invalid Input!");
                break;
            }

            if (ch == 'm' || ch == 'i') break;

            // Find current room
            int reg = find_region_by_coordinate(temp_y, temp_x);
            Room* room = regions[reg / HORIZONTAL][reg % HORIZONTAL].room;

            int condition = is_walkable(temp_y, temp_x);
            if (condition == -1) { // Apply window
                int ver = reg / HORIZONTAL;
                int hor = reg % HORIZONTAL;
                while (1) {
                    if (temp_x == room->x) hor--;
                    else hor++;

                    if ((0 <= hor) && (hor <= 3)) {
                        if (regions[ver][hor].room != NULL) {
                            draw_room(regions[ver][hor].room);
                            break;
                        }
                    } else break;
                }
                break;
            } else if (!condition) break;

            new_y = temp_y;
            new_x = temp_x;

            update_previous(new_y, new_x);

            hero.walk++;

            // Health and Hunger
            int freq = (hero.healing_speed == 1) ? 20 : 5;
            int min_healing = (hero.healing_speed == 1) ? 75 : 60;
            if (hero.walk % freq == 0) {
                if (hero.hunger > 10) {
                    hero.hunger -= 2;
                    show_hunger();
                }
                if (hero.hunger < 50) {
                    printf("\a");
                    show_message("You are hungry!");
                    hero.health -= (10 - (hero.hunger / 5));
                    if (hero.health < 5) {
                        game_over(0);
                        return;
                    }
                    show_health();
                }
                if (hero.hunger > min_healing) {
                    show_message("You are healing!");
                    hero.health += (hero.hunger - min_healing + 5) / 5;
                    if (hero.health > 100) hero.health = 100;
                    show_health();
                }
            }

            if (hero.walk % 200 == 0) {
                if (hero.inventory.foods[1].number != 0) {
                    show_message("The magic food became normal!");
                    hero.inventory.foods[1].number--;
                    hero.inventory.foods[0].number++;
                }
            }
            if (hero.walk % 250 == 0) {
                if (hero.inventory.foods[2].number != 0) {
                    show_message("The supreme food became normal!");
                    hero.inventory.foods[2].number--;
                    hero.inventory.foods[0].number++;
                } 
            }

            // Health effect
            if ((hero.healing_speed != 1) && ((hero.walk - hero.inventory.potions[0].first_consume) > 20)) {
                hero.healing_speed = 1;
                show_message("The health potion effect has gone!");
            }

            // Speed effects
            if (hero.speed != 1) {
                if ((hero.speed == 2) && ((hero.walk - hero.inventory.foods[1].first_consume) > 30)) {
                    hero.speed = 1;
                    show_message("The magic food effect has gone!");
                }
                else if ((hero.speed == 3) && ((hero.walk - hero.inventory.potions[1].first_consume) > 10)) {
                    hero.speed = 1;
                    show_message("The speed potion effect has gone!");
                }
            }

            // Damage effects
            if ((hero.base_power > 0) && ((hero.walk - hero.inventory.foods[2].first_consume) > 30)) {
                hero.base_power = 0;
                show_message("The supreme food effect has gone!");
            } else if ((hero.base_power < 0) && ((hero.walk - hero.inventory.potions[2].first_consume) > 10)) {
                hero.base_power = 0;
                show_message("The damage potion effect has gone!");
            }

            // Check for stair
            if (!strcmp(previous.symbol, "≡")) {
                if (use_stair() == 1) return;
                else {
                    show_message("There are still alive monsters!");
                }
            }

            // Check for trap
            if (!strcmp(previous.symbol, ".")) {
                trap(room, new_y, new_x);
                if (hero.health < 5) {
                    game_over(1);
                    return;
                }
            }

            // Earn gold
            if (!strcmp(previous.symbol, "🜚")) {
                earn_gold(room, new_y, new_x);
            } else if (!strcmp(previous.symbol, "ᴥ")) {
                earn_gold(room, new_y, new_x);
            }

            // Earn food
            if (!strcmp(previous.symbol, "֍")) {
                earn_food(room, new_y, new_x, 0);
            } else if (!strcmp(previous.symbol, "♣")) {
                earn_food(room, new_y, new_x, 1);
            } else if (!strcmp(previous.symbol, "♦")) {
                earn_food(room, new_y, new_x, 2);
            }

            // Get weapon
            if (!strcmp(previous.symbol, "⸸")) { // Dagger
                get_weapon(room, new_y, new_x, 1);
            } else if (!strcmp(previous.symbol, "|")) { // Magic Wand
                get_weapon(room, new_y, new_x, 2);
            } else if (!strcmp(previous.symbol, "➳")) { // Arrow
                get_weapon(room, new_y, new_x, 3);
            } else if (!strcmp(previous.symbol, "⚔")) { // Sword
                get_weapon(room, new_y, new_x, 4);
            }

            // Get potion  
            if (!strcmp(previous.symbol, "♥")) { // Health
                get_potion(room, new_y, new_x, 0);
            } else if (!strcmp(previous.symbol, "♯")) { // Speed
                get_potion(room, new_y, new_x, 1);
            } else if (!strcmp(previous.symbol, "●")) { // Damage
                get_potion(room, new_y, new_x, 2);
            }

            // Show corridor
            int cor = is_there_corridor(hero.character.y, hero.character.x);
            switch (cor) {
            case 1:
                mvprintw(hero.character.y - 1, hero.character.x, "░");
                screen[hero.character.y - 1][hero.character.x].condition = 1;
                break;
            case 2:
                mvprintw(hero.character.y, hero.character.x + 1, "░");
                screen[hero.character.y][hero.character.x + 1].condition = 1;
                break;
            case 3:
                mvprintw(hero.character.y + 1, hero.character.x, "░");
                screen[hero.character.y + 1][hero.character.x].condition = 1;
                break;
            case 4:
                mvprintw(hero.character.y, hero.character.x - 1, "░");
                screen[hero.character.y][hero.character.x - 1].condition = 1;
                break;
            default:
                break;
            }
            refresh();

            // Show new room
            if (!strcmp(screen[new_y][new_x].symbol, "╬")) {
                if (screen[room->y][room->x].condition == 0) {
                    draw_room(room);
                }
            }
        }

        if (process_monster() == -1) return;
    }
}

void new_game() {
    clear();
    initialize_screen();

    draw_border();
    draw_table(1, WIDTH - 2, 1, 3);

    char message[50] = "Welcome to the ROGUE ";
    strcat(message, current_user.name);
    show_message(message);

    initialize_regions();
    connect_all_rooms();
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            if (regions[i][j].room != NULL) {
                create_room_map(regions[i][j].room);

                create_room_in_screen(regions[i][j].room);

                add_monsters(regions[i][j].room);

                if (!strcmp(regions[i][j].room->kind, "start")) {
                    draw_room(regions[i][j].room);
                }
            }
        }
    }

    init_hero();
    guide();

    move_player();
}

void load_regions(FILE *file) {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZONTAL; j++) {
            Region *reg = &regions[i][j];

            // Load Region data
            fread(&reg->first_x, sizeof(int), 1, file);
            fread(&reg->first_y, sizeof(int), 1, file);
            fread(&reg->final_x, sizeof(int), 1, file);
            fread(&reg->final_y, sizeof(int), 1, file);

            // Check if room exists
            int has_room;
            fread(&has_room, sizeof(int), 1, file);

            if (has_room) {
                // Allocate memory for room
                reg->room = (Room *)malloc(sizeof(Room));
                fread(reg->room, sizeof(Room), 1, file);

                Room *room = reg->room;

                // Allocate memory for map
                room->map = (Character **)malloc(room->height * sizeof(Character *));
                for (int y = 0; y < room->height; y++) {
                    room->map[y] = (Character *)malloc(room->width * sizeof(Character));
                    fread(room->map[y], sizeof(Character), room->width, file);
                }
            } else {
                reg->room = NULL;
            }
        }
    }
}

int load_game(const char* filename) {
    if (!file_exists(filename)) return 0;

    FILE* file = fopen(filename, "rb");
    if (!file) return -1;

    int h, w;
    fread(&h, sizeof(int), 1, file);
    fread(&w, sizeof(int), 1, file);
    if ((h != HEIGHT) || (w != WIDTH)) {
        fclose(file);
        return -2;
    }

    fread(&hero, sizeof(Hero), 1, file);
    fread(&previous, sizeof(Character), 1, file);

    load_regions(file);

    fread(&start_region, sizeof(int), 1, file);

    // Load Screen
    initialize_screen();
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            fread(&screen[i][j], sizeof(Screen), 1, file);
        }
    }

    fclose(file);

    clear();
    show_message("Welcome Back!");

    draw_border();
    draw_table(1, WIDTH - 2, 1, 3);
    show_screen(0);    

    guide();
    move_player();

    return 1;
}

int main() {
    // Ensure user.json exists
    if (!file_exists(user_file_name)) {
        if (create_user_file(user_file_name) == NULL) {
            printf("FAILED TO CREATE OR LOAD USER FILE!\n");
            printf("PLEASE TRY AGAIN AND RUN THE GAME.\n");
            return 1;
        }
    }

    // Seed random number generator
    srand(time(0));

    // Get current user
    current_user = get_user_by_name(user_file_name, get_last_user(user_file_name));

    // Set UTF-8 characters
    setlocale(LC_ALL, "");

    // Initialize ncurses
    initscr();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    getmaxyx(stdscr, HEIGHT, WIDTH);

    // Define colors
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_YELLOW);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_GREEN, COLOR_BLACK);
    init_pair(6, COLOR_BLUE, COLOR_BLACK);
    init_pair(7, COLOR_CYAN, COLOR_BLACK);
    init_pair(8, COLOR_MAGENTA, COLOR_BLACK);

    draw_border();
    // Current user
    if (current_user.name[0] != '\0') {
        char message[75];
        sprintf(message, "You are Logged In as %s", current_user.name);
        show_message(message);
    }

    while (1) {
        // Main Menu
        int page = main_menu();

        if (page == 1) { // Profile
            while (1) {
                int condition = profile_menu();
                if (condition == 1) { // Sign Up
                    signup_page();
                    break;
                }
                else if (condition == 2) { // Log In
                    login_page();
                    break;
                }
                else if (condition == 3) { // Guest
                    set_guest();
                    break;
                }
                else if (condition == -1) { // ESC
                    break;
                }
            }
        }
        else if (page == 2) { // New Game
            new_game();
        }
        else if (page == 3) { // Load Game
            char file[55];
            sprintf(file, "%s.dat", current_user.name);
            switch (load_game(file)) {
            case 0:
                show_message("You don't have any saved game yet.");
                break;
            case -1:
                show_message("Failed to load the saved game!");
                break;
            case -2:
                show_message("Please resize your terminal to the previous size.");
                break;
            }
        }
        else if (page == 4) { // Settings
            settings_page();
        }
        else if (page == 5) { // Score Table
            show_message("You have selected page 5");
        }
        else {
            break;
        }
    }

    endwin();
    return 0;
}
