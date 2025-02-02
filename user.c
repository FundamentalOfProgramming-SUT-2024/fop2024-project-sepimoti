#include "user.h"
#include "cJSON.h"
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>

char* last_user = NULL;

cJSON *settings_to_json(const Settings *settings) {
    cJSON *settings_json = cJSON_CreateObject();
    if (!settings_json) return NULL;

    cJSON_AddStringToObject(settings_json, "hero_color", settings->hero_color);
    cJSON_AddStringToObject(settings_json, "difficulty", settings->difficulty);

    return settings_json;
}


int file_exists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}


void* create_user_file(const char* filename) {
    FILE *file = fopen(filename, "w");
    if (file) {
        fprintf(file, "{\"users\": [], \"last_user\": \"\"}"); // Empty JSON structure
        fclose(file);
    }
    else {
        return NULL;
    }
}


void save_last_user(const char* filename, const char* username) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "last_user", username);

    char* json_string = cJSON_Print(root);
    fprintf(file, "%s", json_string);

    fclose(file);
    cJSON_Delete(root);
    free(json_string);
}


char* get_last_user(const char* filename) {
    if (!file_exists(filename)) {
        return NULL;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json_string = (char*)malloc(length + 1);
    fread(json_string, 1, length, file);
    fclose(file);
    json_string[length] = '\0';

    cJSON* root = cJSON_Parse(json_string);
    free(json_string);

    if (!root) {
        return NULL;
    }

    cJSON* last_user = cJSON_GetObjectItem(root, "last_user");
    if (!last_user || !cJSON_IsString(last_user)) {
        cJSON_Delete(root);
        return NULL;
    }

    char* result = strdup(last_user->valuestring);
    cJSON_Delete(root);

    if (result[0] == '\0') return NULL;

    return result;
}


int add_user_to_file(const char* filename, User new_user) {
    FILE *file = fopen(filename, "r");
    if (!file) 
        return 0;

    // Read existing JSON
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *json_string = (char *)malloc(length + 1);
    fread(json_string, 1, length, file);
    fclose(file);
    json_string[length] = '\0';

    // Parse JSON
    cJSON *root = cJSON_Parse(json_string);
    if (!root || !cJSON_IsObject(root)) {
        free(json_string);
        return 0;
    }

    cJSON *users_array = cJSON_GetObjectItem(root, "users");
    if (!users_array || !cJSON_IsArray(users_array)) {
        free(json_string);
        return 0;
    }

    // Create new user object
    cJSON *user_json = cJSON_CreateObject();
    cJSON_AddStringToObject(user_json, "name", new_user.name);
    cJSON_AddStringToObject(user_json, "email", new_user.email);
    cJSON_AddStringToObject(user_json, "password", new_user.password);
    cJSON_AddNumberToObject(user_json, "score", new_user.score);

    // Add settings
    cJSON *settings_json = settings_to_json(&new_user.settings);
    cJSON_AddItemToObject(user_json, "settings", settings_json);

    // Add to JSON array
    cJSON_AddItemToArray(users_array, user_json);

    // Update last user
    cJSON_ReplaceItemInObject(root, "last_user", cJSON_CreateString(new_user.name));

    // Write updated JSON back
    char *updated_json = cJSON_Print(root);
    file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%s", updated_json);
        fclose(file);
    } else {
        return 0;
    }

    // Cleanup
    cJSON_Delete(root);
    free(json_string);
    free(updated_json);

    return 1;
}


int is_username_taken(const char* filename, const char* username) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) return -1;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json_string = malloc(length + 1);
    fread(json_string, 1, length, file);
    fclose(file);
    json_string[length] = '\0';

    cJSON* root = cJSON_Parse(json_string);
    free(json_string);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON* users_array = cJSON_GetObjectItem(root, "users");
    if (!users_array || !cJSON_IsArray(users_array)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON* user = NULL;
    cJSON_ArrayForEach(user, users_array) {
        cJSON* name_json = cJSON_GetObjectItem(user, "name");
        if (name_json && strlen(name_json->valuestring) > 0 && strcmp(username, name_json->valuestring) == 0) {
            cJSON_Delete(root);
            return 1;
        }
    }

    cJSON_Delete(root);
    return 0;
}


User get_user_by_name(const char *filename, const char *name) {
    User found_user;
    memset(&found_user, 0, sizeof(User)); // Initialize with default values

    // Open the file and read its content
    FILE *file = fopen(filename, "r");
    if (!file) {
        found_user.name[0] = '\0';
        return found_user;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(length + 1);
    fread(json_string, 1, length, file);
    fclose(file);
    json_string[length] = '\0';

    // Parse the JSON
    cJSON *root = cJSON_Parse(json_string);
    free(json_string);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        found_user.name[0] = '\0';
        return found_user;
    }

    cJSON *users_array = cJSON_GetObjectItem(root, "users");
    if (!users_array || !cJSON_IsArray(users_array)) {
        cJSON_Delete(root);
        found_user.name[0] = '\0';
        return found_user;
    }

    // Search for the user by name
    cJSON *user = NULL;
    cJSON_ArrayForEach(user, users_array) {
        cJSON *name_json = cJSON_GetObjectItem(user, "name");
        if (name_json && cJSON_IsString(name_json) && strcmp(name, name_json->valuestring) == 0) {
            // Populate the User struct
            strncpy(found_user.name, name_json->valuestring, sizeof(found_user.name));

            cJSON *email_json = cJSON_GetObjectItem(user, "email");
            if (email_json && cJSON_IsString(email_json)) {
                strncpy(found_user.email, email_json->valuestring, sizeof(found_user.email));
            }

            cJSON *password_json = cJSON_GetObjectItem(user, "password");
            if (password_json && cJSON_IsString(password_json)) {
                strncpy(found_user.password, password_json->valuestring, sizeof(found_user.password));
            }

            cJSON *score_json = cJSON_GetObjectItem(user, "score");
            if (score_json && cJSON_IsNumber(score_json)) {
                found_user.score = score_json->valueint;
            }

            // Handle the settings field
            cJSON *settings_json = cJSON_GetObjectItem(user, "settings");
            if (settings_json && cJSON_IsObject(settings_json)) {
                cJSON *hero_color_json = cJSON_GetObjectItem(settings_json, "hero_color");
                if (hero_color_json && cJSON_IsString(hero_color_json)) {
                    strncpy(found_user.settings.hero_color, hero_color_json->valuestring, sizeof(found_user.settings.hero_color));
                }

                cJSON *difficulty_json = cJSON_GetObjectItem(settings_json, "difficulty");
                if (difficulty_json && cJSON_IsString(difficulty_json)) {
                    strncpy(found_user.settings.difficulty, difficulty_json->valuestring, sizeof(found_user.settings.difficulty));
                }
            }
            break;
        }
    }

    cJSON_Delete(root);

    return found_user;
}


void update_last_user(const char *filename, const char *username) {
    // Open the file and read its content
    FILE *file = fopen(filename, "r");
    if (!file) {
        return;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(length + 1);
    fread(json_string, 1, length, file);
    fclose(file);
    json_string[length] = '\0';

    // Parse the JSON
    cJSON *root = cJSON_Parse(json_string);
    free(json_string);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    // Update the "last_user" field
    cJSON_ReplaceItemInObject(root, "last_user", cJSON_CreateString(username));

    // Write the updated JSON back to the file
    file = fopen(filename, "w");
    if (file) {
        char *updated_json = cJSON_Print(root);
        fprintf(file, "%s", updated_json);
        fclose(file);
        free(updated_json);
    }

    // Cleanup
    cJSON_Delete(root);
}



int update_user_settings(const char *filename, User updated_user) {
    // Open the file and read its content
    FILE *file = fopen(filename, "r");
    if (!file) {
        return 0; // Failed to open the file
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(length + 1);
    fread(json_string, 1, length, file);
    fclose(file);
    json_string[length] = '\0';

    // Parse the JSON
    cJSON *root = cJSON_Parse(json_string);
    free(json_string);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return 0; // Failed to parse the JSON
    }

    cJSON *users_array = cJSON_GetObjectItem(root, "users");
    if (!users_array || !cJSON_IsArray(users_array)) {
        cJSON_Delete(root);
        return 0; // Users array not found
    }

    // Search for the user by name and update their settings
    cJSON *user = NULL;
    cJSON_ArrayForEach(user, users_array) {
        cJSON *name_json = cJSON_GetObjectItem(user, "name");
        if (name_json && cJSON_IsString(name_json) && strcmp(updated_user.name, name_json->valuestring) == 0) {
            // Found the user, update their settings
            cJSON *settings_json = cJSON_GetObjectItem(user, "settings");
            if (!settings_json) {
                settings_json = cJSON_CreateObject();
                cJSON_AddItemToObject(user, "settings", settings_json);
            }

            cJSON_ReplaceItemInObject(settings_json, "hero_color", cJSON_CreateString(updated_user.settings.hero_color));
            cJSON_ReplaceItemInObject(settings_json, "difficulty", cJSON_CreateString(updated_user.settings.difficulty));
            break;
        }
    }

    // Write the updated JSON back to the file
    char *updated_json = cJSON_Print(root);
    file = fopen(filename, "w");
    if (!file) {
        cJSON_Delete(root);
        free(updated_json);
        return 0; // Failed to open the file for writing
    }

    fprintf(file, "%s", updated_json);
    fclose(file);

    // Clean up
    cJSON_Delete(root);
    free(updated_json);

    return 1;
}


int is_email_valid(const char* email) {
    const char* at = strchr(email, '@');
    const char* dot = strrchr(email, '.');
    return at && dot && at < dot;
}


int is_password_valid(const char* password) {
    if (strlen(password) < 7) return -1;

    int has_digit = 0, has_upper = 0, has_lower = 0;
    for (const char* p = password; *p; ++p) {
        if (isdigit(*p)) has_digit = 1;
        if (isupper(*p)) has_upper = 1;
        if (islower(*p)) has_lower = 1;
    }

    return has_digit && has_upper && has_lower;
}
