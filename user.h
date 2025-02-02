#ifndef USER_H
#define USER_H

#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

typedef struct {
    char hero_color[10];
    char difficulty[10];
} Settings;

typedef struct {
    char name[50];
    char email[100];
    char password[100];
    int score;
    Settings settings;
} User;

cJSON *settings_to_json(const Settings *settings);
int file_exists(const char *filename);
void* create_user_file(const char *filename);
void save_last_user(const char *filename, const char* username);
char* get_last_user(const char *filename);
int add_user_to_file(const char *filename, User new_user);
int is_username_taken(const char *filename, const char* username);
User get_user_by_name(const char *filename, const char *name);
void update_last_user(const char *filename, const char *username);
int update_user_settings(const char *filename, User user);
int is_email_valid(const char* email);
int is_password_valid(const char* password);

#endif
