#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <time.h>
#include <math.h>
#include <ncursesw/curses.h>
#include <wchar.h>
#include "user.h"

const char *user_file_name = "user.json";
User current_user;


typedef struct {
    int y, x;
    char side; // 'u': up / 'd': down / 'r': right / 'l': left
    int value; // For gold
} Object;

typedef struct {
    int y, x; // Top Left position
    int width, height;
    Object doors[8];
    int door_count;
    Object pillars[9];
    int pillar_count;
    Object gold[4];
    int gold_count;
    char kind[30];
} Room;

typedef struct {
    int first_x, first_y; // Top Left position
    int final_x, final_y; // Bottom Right postion
    Room *room;
} Region;

typedef struct {
    int room1_idx;
    int room2_idx;
    int cost; // Manhattan distance
} Edge;

int WIDTH, HEIGHT;
const int HORIZENTAL = 4, VERTICAL = 3;
Region regions[3][4]; // Divide screen into 12 regions
int start_region;


typedef struct {
    char symbol[6];
    attr_t attributes;
    int y, x;
    int level;
    int health;
    int gold;
} Character;
Character hero;
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

void show_logo(int which)
{
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
        " ╚══╝╚══╝ ╚══════╝╚══════╝ ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚══════╝"};
    const char *settings[] = {
        "███████╗███████╗████████╗████████╗██╗███╗   ██╗ ██████╗ ███████╗",
        "██╔════╝██╔════╝╚══██╔══╝╚══██╔══╝██║████╗  ██║██╔════╝ ██╔════╝",
        "███████╗█████╗     ██║      ██║   ██║██╔██╗ ██║██║  ███╗███████╗",
        "╚════██║██╔══╝     ██║      ██║   ██║██║╚██╗██║██║   ██║╚════██║",
        "███████║███████╗   ██║      ██║   ██║██║ ╚████║╚██████╔╝███████║",
        "╚══════╝╚══════╝   ╚═╝      ╚═╝   ╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚══════╝"};

    if (which == 0)
    {
        for (int i = 0; i < 6; i++)
        {
            mvprintw(2 + i, (WIDTH - 43) / 2, "%s", rogue[i]);
        }
    }
    else if (which == 1)
    {
        for (int i = 0; i < 6; i++)
        {
            mvprintw(2 + i, (WIDTH - 62) / 2, "%s", welcome[i]);
        }
    }
    else if (which == 2)
    {
        for (int i = 0; i < 6; i++)
        {
            mvprintw(2 + i, (WIDTH - 64) / 2, "%s", settings[i]);
        }
    }

    attroff(COLOR_PAIR(3));
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

int main_menu()
{
    // ROGUE
    clear();
    draw_border();
    show_logo(0);

    // Footer
    attron(COLOR_PAIR(1) | A_BOLD);
    const char *footer = "Created by: SepiMoti | Version: 2.0.0";
    mvaddstr(HEIGHT - 1, (WIDTH - strlen(footer)) / 2, footer);
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Current user
    if (current_user.name[0] != '\0')
    {
        char *message = (char *)calloc(100, sizeof(char));
        strcat(message, "You are Logged In as ");
        strcat(message, current_user.name);
        show_message(message);
        free(message);
    }

    refresh();

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
    for (int i = 0; i < WIDTH; i++) {
        screen[i] = (Screen *)calloc(WIDTH, sizeof(Screen));
    }
}

int find_region(Room *room) {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZENTAL; j++) {
            if ((regions[i][j].first_x <= room->x) && (room->x <= regions[i][j].final_x) 
             && (regions[i][j].first_y <= room->y) && (room->y <= regions[i][j].final_y)) {
                return i * HORIZENTAL + j;
            }
        }
    }
}

int find_region_by_coordinate(int y, int x) {
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZENTAL; j++) {
            if ((regions[i][j].first_x <= x) && (x <= regions[i][j].final_x) 
             && (regions[i][j].first_y <= y) && (y <= regions[i][j].final_y)) {
                return i * HORIZENTAL + j;
            }
        }
    }
}

// Set the rooms in regions
void room_condition_maker(int has_room[VERTICAL][HORIZENTAL]) {
    // At least we'll have 6 rooms but there is a chance to have up to 3 more rooms

    // Randomly set one of every two neighbor regions to 1
    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZENTAL; j += 2) {
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
        int random = rand() % (HORIZENTAL * VERTICAL);
        int y = random / HORIZENTAL;
        int x = random % HORIZENTAL;
        if (has_room[y][x] == 0)
            has_room[y][x] = 1;
    }
}

void random_room_generator(Region *region) {
    int height = region->final_y - region->first_y + 1;
    int width = region->final_x - region->first_x + 1;

    region->room->y = region->first_y + rand() % (height - 6 + 1);
    region->room->height = 6 + rand() % (region->final_y - region->room->y - 6 + 2);

    region->room->x = region->first_x + rand() % (width - 6 + 1);
    region->room->width = 6 + rand() % (region->final_x - region->room->x - 6 + 2);

    region->room->door_count = 0;

    region->room->pillar_count = 0;

    region->room->gold_count = 0;
}

void initialize_regions() {
    int x_sep = (WIDTH - 4) / HORIZENTAL;
    int y_sep = (HEIGHT - 6) / VERTICAL;

    int has_room[VERTICAL][HORIZENTAL];
    room_condition_maker(has_room);

    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZENTAL; j++) {
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

    int start = rand() % 4;
    int a, b;
    switch (start) {
    case 0:
        a = (regions[0][0].room != NULL) ? 0 : 1;
        strcpy(regions[0][a].room->kind, "start");

        b = (regions[2][2].room != NULL) ? 2 : 3;
        strcpy(regions[2][b].room->kind, "stair");

        start_region = a;
        break;
    case 1:
        a = (regions[0][2].room != NULL) ? 2 : 3;
        strcpy(regions[0][a].room->kind, "start");

        b = (regions[2][0].room != NULL) ? 0 : 1;
        strcpy(regions[2][b].room->kind, "stair");

        start_region = a;
        break;
    case 2:
        a = (regions[2][0].room != NULL) ? 0 : 1;
        strcpy(regions[2][a].room->kind, "start");

        b = (regions[0][2].room != NULL) ? 2 : 3;
        strcpy(regions[0][b].room->kind, "stair");

        start_region = 8 + a;
        break;
    case 3:
        a = (regions[2][2].room != NULL) ? 2 : 3;
        strcpy(regions[2][a].room->kind, "start");

        b = (regions[0][0].room != NULL) ? 0 : 1;
        strcpy(regions[0][b].room->kind, "stair");

        start_region = 8 + a;
        break;
    }
}

void set_cor(int y, int x) {
    strcpy(screen[y][x].symbol, "░");
    screen[y][x].attributes = COLOR_PAIR(1);
}

void draw_corridors(Room *room1, Room *room2) {
    int position1 = find_region(room1);
    int position2 = find_region(room2);

    Object door1, door2;

    if ((position1 % HORIZENTAL) == (position2 % HORIZENTAL)) {
        // If two rooms are in same column they'll be connected on their up and down sides
        door1.y = (position1 < position2) ? (room1->y + room1->height - 1) : (room1->y);
        door1.x = (room1->x + 1) + rand() % (room1->width - 2);
        door1.side = (position1 < position2) ? 'd' : 'u';

        door2.y = (position1 < position2) ? room2->y : (room2->y + room2->height - 1);
        door2.x = (room2->x + 1) + rand() % (room2->width - 2);
        door2.side = (position1 < position2) ? 'u' : 'd';

        int max_y = (door1.y > door2.y) ? door1.y : door2.y;
        int min_y = (door1.y > door2.y) ? door2.y : door1.y;

        int max_x = (door1.x > door2.x) ? door1.x : door2.x;
        int min_x = (door1.x > door2.x) ? door2.x : door1.x;

        for (int y = min_y; y <= (min_y + (max_y - min_y) / 2); y++) {
            if (door1.y < door2.y)
                set_cor(y, door1.x);
            else
                set_cor(y, door2.x);
        }
        for (int y = (min_y + (max_y - min_y) / 2); y <= max_y; y++) {
            if (door1.y < door2.y)
                set_cor(y, door2.x);
            else
                set_cor(y, door1.x);
        }
        for (int x = min_x; x <= max_x; x++) {
            set_cor((min_y + (max_y - min_y) / 2), x);
        }
    }
    else if ((position1 / HORIZENTAL) == (position2 / HORIZENTAL)) {
        // If two rooms are in same row they'll be connected on their right and left sides
        door1.y = (room1->y + 1) + rand() % (room1->height - 2);
        door1.x = (position1 < position2) ? (room1->x + room1->width - 1) : room1->x;
        door1.side = (position1 < position2) ? 'r' : 'l';

        door2.y = (room2->y + 1) + rand() % (room2->height - 2);
        door2.x = (position1 < position2) ? room2->x : (room2->x + room2->width - 1);
        door2.side = (position1 < position2) ? 'l' : 'r';

        int max_y = (door1.y > door2.y) ? door1.y : door2.y;
        int min_y = (door1.y > door2.y) ? door2.y : door1.y;

        int max_x = (door1.x > door2.x) ? door1.x : door2.x;
        int min_x = (door1.x > door2.x) ? door2.x : door1.x;

        for (int x = min_x; x <= (min_x + (max_x - min_x) / 2); x++) {
            if (door1.x < door2.x)
                set_cor(door1.y, x);
            else
                set_cor(door2.y, x);
        }
        for (int x = (min_x + (max_x - min_x) / 2); x <= max_x; x++) {
            if (door1.x < door2.x)
                set_cor(door2.y, x);
            else
                set_cor(door1.y, x);
        }
        for (int y = min_y; y <= max_y; y++) {
            set_cor(y, (min_x + (max_x - min_x) / 2));
        }
    }
    else {
        door1.x = (room1->x < room2->x) ? (room1->x + room1->width - 1) : (room1->x);
        door1.y = (room1->y + 1) + rand() % (room1->height - 2);
        door1.side = (room1->x < room2->x) ? 'r' : 'l';

        door2.x = (room2->x + 1) + rand() % (room2->width - 2);
        door2.y = (room1->y < room2->y) ? (room2->y) : (room2->y + room2->height - 1);
        door2.side = (room1->x < room2->x) ? 'u' : 'd';

        int min_x = door1.x < door2.x ? door1.x : door2.x;
        int max_x = door1.x > door2.x ? door1.x : door2.x;
        for (int x = min_x; x <= max_x; x++) {
            set_cor(door1.y, x);
        }
        int min_y = door1.y < door2.y ? door1.y : door2.y;
        int max_y = door1.y > door2.y ? door1.y : door2.y;
        for (int y = min_y; y <= max_y; y++) {
            set_cor(y, door2.x);
        }
    }

    room1->doors[room1->door_count++] = door1;
    room2->doors[room2->door_count++] = door2;
}

int compare_edges(const void *a, const void *b) {
    Edge *edge1 = (Edge *)a;
    Edge *edge2 = (Edge *)b;
    return edge1->cost - edge2->cost;
}

void connect_all_rooms() {
    // Collect all rooms
    Room *rooms[VERTICAL * HORIZENTAL];
    int room_count = 0;

    for (int i = 0; i < VERTICAL; i++) {
        for (int j = 0; j < HORIZENTAL; j++) {
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

int is_there_object(int y, int x, char* symbol) {
    if (!strcmp(screen[y - 1][x].symbol, symbol) && !screen[y - 1][x].condition) return 1;
    if (!strcmp(screen[y][x + 1].symbol, symbol) && !screen[y][x + 1].condition) return 2;
    if (!strcmp(screen[y + 1][x].symbol, symbol) && !screen[y + 1][x].condition) return 3;
    if (!strcmp(screen[y][x - 1].symbol, symbol) && !screen[y][x - 1].condition) return 4;
    return 0;
}

void add_room_stuff(Room* room) {
    // Add pillars
    int b = (room->height - 4) / 3;
    int a = (room->width  - 4) / 3;
    for (int i = 0; i < b; i++) {
        for (int j = 0; j < a; j++) {
            if ((rand() % 3) == 0) {
                int* index = &room->pillar_count;
                room->pillars[*index].y = (room->y + 1 + (3 * i)) + rand() % 3;
                room->pillars[*index].x = (room->x + 1 + (3 * j)) + rand() % 3;
                (*index)++;
            }
        }
    }
    
    // Add gold
    int chance = rand() % 4;
    while (chance != 0) {
        int y = (room->y + 1) + rand() % (room->height - 2);
        int x = (room->x + 1) + rand() % (room->width  - 2);

        int conflict = 0;
        for (int i = 0; i < room->pillar_count; i++) {
            if ((room->pillars[i].y == y) && (room->pillars[i].x == x)) {
                conflict = 1;
            }
        }

        if (!conflict) {
            room->gold[room->gold_count].y = y;
            room->gold[room->gold_count].x = x;
            room->gold[room->gold_count].value = 20 + 5 * (rand() % 5);
            room->gold_count++;

            chance--;
        }
    }
}

void set_room_index(int y, int x, char* symbol, attr_t attribute) {
    strcpy(screen[y][x].symbol, symbol);
    screen[y][x].attributes = attribute;
}

void set_room(Room *room) {
    int x = room->x;
    int y = room->y;
    int width = room->width;
    int height = room->height;

    attr_t color;
    if (!strcmp(room->kind, "start")) 
        color = COLOR_PAIR(6);
    else if(!strcmp(room->kind, "stair"))
        color = COLOR_PAIR(5);
    else
        color = COLOR_PAIR(3);

    // Walls and Corners
    set_room_index(y, x, "╔", color);
    set_room_index(y, x + width - 1, "╗", color);
    set_room_index(y + height - 1, x, "╚", color);
    set_room_index(y + height - 1, x + width - 1, "╝", color);
    for (int i = x + 1; i < x + width - 1; i++) {
        set_room_index(y, i, "═", color);
        set_room_index(y + height - 1, i, "═", color);
    }
    for (int i = y + 1; i < y + height - 1; i++) {
        set_room_index(i, x, "║", color);
        set_room_index(i, x + width - 1, "║", color);
    }
    for (int i = 0; i < room->door_count; i++) {
        set_room_index(room->doors[i].y, room->doors[i].x, "╬", color);
    }

    // Floor
    for (int i = y + 1; i < y + height - 1; i++) {
        for (int j = x + 1; j < x + width - 1; j++) {
            set_room_index(i, j, ".", COLOR_PAIR(5));
        }
    }

    // Pillar
    for (int i = 0; i < room->pillar_count; i++) {
        set_room_index(room->pillars[i].y, room->pillars[i].x, "■", color);
    }

    // Gold
    for (int i = 0; i < room->gold_count; i++) {
        set_room_index(room->gold[i].y, room->gold[i].x, "🜚", COLOR_PAIR(3) | A_BOLD);
    }
}

void draw_room(Room *room) {
    for (int y = room->y; y < room->y + room->height; y++) {
        for (int x = room->x; x < room->x + room->width; x++) {
            attron(screen[y][x].attributes);
            mvprintw(y, x , "%s", screen[y][x].symbol);
            attroff(screen[y][x].attributes);

            screen[y][x].condition = 1;
        }
    }
    
    refresh();
}

void set_hero() {
    int y = hero.y;
    int x = hero.x;
    strcpy(screen[y][x].symbol, hero.symbol);
    screen[y][x].attributes = hero.attributes;
    screen[y][x].condition = 1;
}

void init_hero() {
    // Set hero
    strcpy(hero.symbol, "☻");

    if (!strcmp(current_user.settings.hero_color, "yellow"))
        hero.attributes = COLOR_PAIR(3);
    else if (!strcmp(current_user.settings.hero_color, "green"))
        hero.attributes = COLOR_PAIR(5);
    else if (!strcmp(current_user.settings.hero_color, "red"))
        hero.attributes = COLOR_PAIR(4);
    else if (!strcmp(current_user.settings.hero_color, "blue"))
        hero.attributes = COLOR_PAIR(6);

    Region start = regions[start_region / HORIZENTAL][start_region % HORIZENTAL];
    hero.y = start.room->y + 1;
    hero.x = start.room->x + 1;
    // This code added to avoid confilct between hero and pillar
    if (!strcmp(screen[hero.y][hero.x].symbol, "■")) hero.y++;

    hero.level = 0;
    hero.health = 100;
    hero.gold = 0;

    // Set previous location of hero
    strcpy(previous.symbol, ".");
    previous.attributes = A_BOLD | COLOR_PAIR(5);
    previous.y = start.room->y + 1;
    previous.x = start.room->x + 1;
    previous.level = 0;

    // Draw hero
    attron(hero.attributes);
    mvprintw(hero.y, hero.x, "%s", hero.symbol);
    attroff(hero.attributes);
    refresh();
}

void show_health() {
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(2, 3, "HEALTH: ");
    attroff(COLOR_PAIR(3) | A_BOLD);

    attron(COLOR_PAIR(4));
    int health_bar = (hero.health % 10 == 0) ? (hero.health / 10) : ((int)(hero.health / 10) + 1);
    for (int i = 0; i < health_bar; i++) {
        printw("\u2665 ");
    }
    for (int i = health_bar; i < 10; i++) {
        printw("\u2661 ");
    }
    printw("%d", hero.health);
    attroff(COLOR_PAIR(4));
}

void show_gold() {
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(2, 37, "GOLD: ");
    attroff(COLOR_PAIR(3) | A_BOLD);

    attron(COLOR_PAIR(5));
    printw("%d", hero.gold);
    attroff(COLOR_PAIR(5));
}

void guide() {
    show_health();

    attron(COLOR_PAIR(3) | A_BOLD);
    printw(" ┇ ");
    attroff(COLOR_PAIR(3) | A_BOLD);

    show_gold();

    refresh();
}

int is_walkable(int y, int x) {
    if (screen[y][x].symbol[0] == '\0') return 0;

    return (strcmp(screen[y][x].symbol, "■")
         && strcmp(screen[y][x].symbol, "║") && strcmp(screen[y][x].symbol, "═")
         && strcmp(screen[y][x].symbol, "╔") && strcmp(screen[y][x].symbol, "╗")
         && strcmp(screen[y][x].symbol, "╝") && strcmp(screen[y][x].symbol, "╚"));
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
    hero.y = new_y;
    hero.x = new_x;

    // Draw the hero at the new position
    attron(hero.attributes);
    mvprintw(hero.y, hero.x, "%s", hero.symbol);
    attroff(hero.attributes);

    // Set the condition to 1
    screen[hero.y][hero.x].condition = 1;

    refresh();
}

void move_player() {
    int show_map = 0;

    while (1) {
        int ch = getchar();
        int new_y = hero.y, new_x = hero.x;

        switch (ch) {
        case 'w': new_y--; break; // Move up
        case 's': new_y++; break; // Move down
        case 'a': new_x--; break; // Move left
        case 'd': new_x++; break; // Move right
        case 'q': new_y--; new_x--; break; // Up-left
        case 'e': new_y--; new_x++; break; // Up-right
        case 'z': new_y++; new_x--; break; // Down-left
        case 'c': new_y++; new_x++; break; // Down-right
        case 'm': show_map = !show_map; break;
        case 27: return; // Exit loop
        default:
            show_message("Invalid Input!");
            break;
        }

        if (ch == 'm') {
            for (int i = 4; i < HEIGHT - 1; i++) {
                for (int j = 1; j < WIDTH - 1; j++) {
                    if (screen[i][j].symbol[0] != '\0') {
                        attron(screen[i][j].attributes);
                        if (show_map || screen[i][j].condition)
                            mvprintw(i, j, "%s", screen[i][j].symbol);
                        else
                            mvprintw(i, j, " ");
                        attroff(screen[i][j].attributes);
                    }
                }
            }
            refresh();
        }

        // Check if the new position is walkable
        if (is_walkable(new_y, new_x)) {
            update_previous(new_y, new_x);

            // Earn gold
            if (!strcmp(previous.symbol, "🜚")) {
                int reg = find_region_by_coordinate(new_y, new_x);
                Room* room = regions[reg / HORIZENTAL][reg % HORIZENTAL].room;
                for (int i = 0; i < room->gold_count; i++) {
                    if ((room->gold[i].y == previous.y) && (room->gold[i].x == previous.x)) {
                        char message[50];
                        sprintf(message, "You've earned %d golds!", room->gold[i].value);
                        show_message(message);

                        hero.gold += room->gold[i].value;
                        show_gold();

                        strcpy(previous.symbol, ".");
                        previous.attributes = COLOR_PAIR(5);
                        set_room_index(previous.y, previous.x, ".", COLOR_PAIR(5));
                    }
                }
            }

            // Show corridor
            int cor = is_there_object(hero.y, hero.x, "░");
            switch (cor) {
            case 1:
                mvprintw(hero.y - 1, hero.x, "░");
                screen[hero.y - 1][hero.x].condition = 1;
                break;
            case 2:
                mvprintw(hero.y, hero.x + 1, "░");
                screen[hero.y][hero.x + 1].condition = 1;
                break;
            case 3:
                mvprintw(hero.y + 1, hero.x, "░");
                screen[hero.y + 1][hero.x].condition = 1;
                break;
            case 4:
                mvprintw(hero.y, hero.x - 1, "░");
                screen[hero.y][hero.x - 1].condition = 1;
                break;
            default:
                break;
            }
            refresh();

            // Show new room
            if (!strcmp(screen[new_y][new_x].symbol, "╬")) {
                int reg = find_region_by_coordinate(new_y, new_x);
                Region region = regions[reg / HORIZENTAL][reg % HORIZENTAL];
                if (screen[region.room->y][region.room->x].condition == 0) {
                    draw_room(region.room);
                }
            }
        }
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
        for (int j = 0; j < HORIZENTAL; j++) {
            if (regions[i][j].room != NULL) {
                add_room_stuff(regions[i][j].room);

                set_room(regions[i][j].room);

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
            show_message("You have selected page 3");
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
