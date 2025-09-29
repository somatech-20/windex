/* windex.c - Main implementation file for windex program
 *
 * This program indexes files and directories from Windows drives (C:\ or /mnt/)
 * and stores the information in a SQLite database.
 * It supports incremental indexing and provides a search functionality with metadata.
 *
 * Features:
 *   - Indexes files and directories, storing path, name, type, size, and modification time.
 *   - Incremental indexing: only new or modified entries are indexed.
 *   - Search functionality with partial and case-insensitive matching.
 *   - Limits search results to 100 entries, sorted by modification time.
 *   - Excludes common system directories to avoid unnecessary indexing.
 *   - Uses SQLite for efficient storage and querying.
 *   - Better than traditional locate: includes metadata, partial/case-insensitive search, and limits results.
 *   - Configurable root directory and excludes via command-line options.
 *   - Removes stale entries during indexing.
 *   
 * Usage: windex [--root <path>] [--exclude <dir>] [--db <path>] index | search <pattern> | --help
 *     The database is stored in the user's home directory under .windex/.winindex.db by default
 *     with appropriate indexes for fast searching.
 *     
 * Copyright (C) 2025 MM
 */

#include "3rds/includes/windex.h"

// Initialize excluded dirs
void _init_excluded_dirs(void) {
    int i;
    for (i = 0; default_excluded_dirs[i]; i++) {}
    num_excluded_dirs = i;
    excluded_dirs = malloc((i + 1) * sizeof(char *));
    if (!excluded_dirs) {
        LOG_ERROR("Failed to allocate memory for excluded dirs");
        exit(1);
    }
    for (i = 0; default_excluded_dirs[i]; i++) {
        excluded_dirs[i] = strdup(default_excluded_dirs[i]);
        if (!excluded_dirs[i]) {
            LOG_ERROR("Failed to allocate memory for exclude dir %s", default_excluded_dirs[i]);
            exit(1);
        }
    }
    excluded_dirs[i] = NULL;
}

// Add user-defined exclude dir
void _add_exclude_dir(const char *dir) {
    excluded_dirs = realloc(excluded_dirs, (num_excluded_dirs + 2) * sizeof(char *));
    if (!excluded_dirs) {
        LOG_ERROR("Failed to reallocate memory for exclude dir %s", dir);
        exit(1);
    }
    excluded_dirs[num_excluded_dirs] = strdup(dir);
    if (!excluded_dirs[num_excluded_dirs]) {
        LOG_ERROR("Failed to allocate memory for exclude dir %s", dir);
        exit(1);
    }
    num_excluded_dirs++;
    excluded_dirs[num_excluded_dirs] = NULL;
}

// Free excluded dirs
void _free_excluded_dirs(void) {
    for (int i = 0; i < num_excluded_dirs; i++) {
        free(excluded_dirs[i]);
    }
    free(excluded_dirs);
}

// Check if path contains an excluded directory
int _is_excluded(const char *path) {
    for (int i = 0; excluded_dirs[i]; i++) {
        if (strstr(path, excluded_dirs[i])) return 1;
    }
    return 0;
}

// Construct database path
int _dbp(const char *home_dir, const char *custom_db, char *db_path) {
    char db_dir_path[MAX_PATH];
    char db_file_path[MAX_PATH];

    if (custom_db) {
        snprintf(db_file_path, MAX_PATH, "%s", custom_db);
    } else {
        snprintf(db_dir_path, MAX_PATH, "%s/.windex", home_dir);
        int res_buf = snprintf(db_file_path, MAX_PATH, "%s/.winindex.db", db_dir_path);
        if (res_buf < 0 || res_buf >= MAX_PATH) {
            LOG_ERROR("Error constructing database file path");
            return -1;
        }

        struct stat st = {0};
        if (stat(db_dir_path, &st) == -1) {
            if (mkdir(db_dir_path) == -1) {
                LOG_ERROR("mkdir failed for directory %s: %s", db_dir_path, strerror(errno));
                return -1;
            }
        }
    }

    FILE *db_file = fopen(db_file_path, "r");
    if (!db_file) {
        db_file = fopen(db_file_path, "w+");
        if (!db_file) {
            LOG_ERROR("Failed to create database file %s: %s", db_file_path, strerror(errno));
            return -1;
        }
    } else {
        fclose(db_file);
    }

    snprintf(db_path, MAX_PATH, "%s", db_file_path);
    LOG_INFO("Database path set to %s", db_path);
    return 0;
}

// Initialize SQLite database
int _init_db(const char *db_path, sqlite3 **db) {
    if (sqlite3_open(db_path, db) != SQLITE_OK) {
        LOG_ERROR("Cannot open database %s: %s", db_path, sqlite3_errmsg(*db));
        return 1;
    }
    char *err_msg = NULL;
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "full_path TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL,"
        "type TEXT NOT NULL,"
        "size INTEGER,"
        "mtime INTEGER);"
        "CREATE INDEX IF NOT EXISTS idx_name ON files(name);"
        "CREATE INDEX IF NOT EXISTS idx_path ON files(full_path);";
    if (sqlite3_exec(*db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        LOG_ERROR("SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(*db);
        return 1;
    }
    LOG_INFO("Database initialized successfully");
    return 0;
}

// Get existing mtime from database
long _get_db_mtime(sqlite3 *db, const char *path) {
    sqlite3_stmt *stmt;
    long mtime = 0;
    const char *sql = "SELECT mtime FROM files WHERE full_path = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            mtime = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    } else {
        LOG_ERROR("Failed to prepare mtime query: %s", sqlite3_errmsg(db));
    }
    return mtime;
}

// Index a single file or directory
void _index_entry(sqlite3 *db, const char *path, struct stat *st) {
    char name[MAX_NAME];
    snprintf(name, MAX_NAME, "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    const char *type = S_ISDIR(st->st_mode) ? "dir" : "file";
    long mtime = (long)st->st_mtime;
    long db_mtime = _get_db_mtime(db, path);

    if (db_mtime == mtime) return; // Skip unchanged entries

    sqlite3_stmt *stmt;
    const char *sql = db_mtime == 0
        ? "INSERT OR IGNORE INTO files (full_path, name, type, size, mtime) VALUES (?, ?, ?, ?, ?);"
        : "UPDATE files SET name = ?, type = ?, size = ?, mtime = ? WHERE full_path = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (db_mtime == 0) {
            sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, type, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 4, (sqlite3_int64)st->st_size);
            sqlite3_bind_int64(stmt, 5, (sqlite3_int64)mtime);
        } else {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 3, (sqlite3_int64)st->st_size);
            sqlite3_bind_int64(stmt, 4, (sqlite3_int64)mtime);
            sqlite3_bind_text(stmt, 5, path, -1, SQLITE_STATIC);
        }
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            LOG_ERROR("Failed to execute index statement for %s: %s", path, sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
        LOG_INFO("Indexed entry: %s", path);
    } else {
        LOG_ERROR("Failed to prepare index statement: %s", sqlite3_errmsg(db));
    }
}

// Prune stale entries
void _prune_stale_entries(sqlite3 *db, const char *root) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT full_path FROM files WHERE full_path LIKE ?;";
    char pattern[MAX_PATH];
    snprintf(pattern, MAX_PATH, "%s%%", root);
    int deleted = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *path = (const char *)sqlite3_column_text(stmt, 0);
            struct stat st;
            if (stat(path, &st) != 0) {
                sqlite3_stmt *del_stmt;
                const char *del_sql = "DELETE FROM files WHERE full_path = ?;";
                if (sqlite3_prepare_v2(db, del_sql, -1, &del_stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(del_stmt, 1, path, -1, SQLITE_STATIC);
                    if (sqlite3_step(del_stmt) == SQLITE_DONE) {
                        deleted++;
                        LOG_INFO("Deleted stale entry: %s", path);
                    }
                    sqlite3_finalize(del_stmt);
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    LOG_INFO("Pruned %d stale entries", deleted);
}

// Iterative indexing with transactions
void _index_files_dynamic(sqlite3 *db, const char *root) {
    char **stack = malloc(1000 * sizeof(char *));
    if (!stack) {
        LOG_ERROR("Failed to allocate stack for indexing");
        return;
    }
    int top = 0;
    int capacity = 1000;
    int total_count = 0;
    
    stack[top++] = strdup(root);
    if (!stack[0]) {
        LOG_ERROR("Failed to allocate memory for root path");
        free(stack);
        return;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    
    while (top > 0) {
        char *current = stack[--top];
        DIR *dir = opendir(current);
        
        if (!dir) {
            LOG_ERROR("Failed to open directory %s: %s", current, strerror(errno));
            free(current);
            continue;
        }
        
        struct dirent *entry;
        char path[MAX_PATH];
        struct stat st;
        
        while ((entry = readdir(dir))) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
                continue;
            
            snprintf(path, MAX_PATH, "%s/%s", current, entry->d_name);
            if (_is_excluded(path)) continue;
            
            if (stat(path, &st) == 0) {
                _index_entry(db, path, &st);
                total_count++;
                
                if (S_ISDIR(st.st_mode)) {
                    if (top >= capacity) {
                        capacity *= 2;
                        char **new_stack = realloc(stack, capacity * sizeof(char *));
                        if (!new_stack) {
                            LOG_ERROR("Failed to reallocate stack");
                            closedir(dir);
                            free(current);
                            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                            for (int i = 0; i < top; i++) free(stack[i]);
                            free(stack);
                            return;
                        }
                        stack = new_stack;
                    }
                    stack[top] = strdup(path);
                    if (!stack[top]) {
                        LOG_ERROR("Failed to allocate memory for path %s", path);
                        continue;
                    }
                    top++;
                }
            } else {
                LOG_ERROR("Failed to stat %s: %s", path, strerror(errno));
            }
        }
        
        closedir(dir);
        free(current);
    }
    
    _prune_stale_entries(db, root);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    free(stack);
    LOG_INFO("Indexed %d new or modified entries", total_count);
    // printf("Indexed %d new or modified entries.\n", total_count);
}

// Convert string to lowercase
char *_to_lower(const char *str) {
    char *lower = strdup(str);
    if (!lower) {
        LOG_ERROR("Failed to allocate memory for lowercase pattern");
        return NULL;
    }
    for (char *p = lower; *p; p++) *p = tolower(*p);
    return lower;
}

// Search the index
void _search_files(sqlite3 *db, const char *pattern) {
    char *lower_pattern = _to_lower(pattern);
    if (!lower_pattern) return;

    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT full_path, type, size, mtime FROM files "
        "WHERE lower(name) LIKE '%%%s%%' OR lower(full_path) LIKE '%%%s%%' "
        "ORDER BY mtime DESC LIMIT 100;", lower_pattern, lower_pattern);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *path = (const char *)sqlite3_column_text(stmt, 0);
            const char *type = (const char *)sqlite3_column_text(stmt, 1);
            sqlite3_int64 size = sqlite3_column_int64(stmt, 2);
            sqlite3_int64 mtime = sqlite3_column_int64(stmt, 3);
            char mtime_str[32];
            time_t t = (time_t)mtime;
            strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", localtime(&t));
            printf("Path: %s\nType: %s\nSize: %lld bytes\nModified: %s\n\n", path, type, size, mtime_str);
        }
        sqlite3_finalize(stmt);
    } else {
        LOG_ERROR("Failed to prepare search query: %s", sqlite3_errmsg(db));
    }
    free(lower_pattern);
}

int main(int argc, char *argv[]) {
    if (logger_init("logs/windex.log", LOG_DEBUG, 1) != 0) {
        fprintf(stderr, "Logger initialization failed. Exiting.\n");
        return 1;
    }

    const char *hommy = getenv("HOME") ? getenv("HOME") : ".";
    const char *root = access("/mnt/", F_OK) == 0 ? "/mnt/" : "C:\\";
    const char *custom_db = NULL;

    // Parse command-line options
    struct option long_options[] = {
        {"root", required_argument, 0, 'r'},
        {"exclude", required_argument, 0, 'e'},
        {"db", required_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt;
    _init_excluded_dirs();
    while ((opt = getopt_long(argc, argv, "r:e:d:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                root = optarg;
                break;
            case 'e':
                _add_exclude_dir(optarg);
                break;
            case 'd':
                custom_db = optarg;
                break;
            case 'h':
                printf("Usage: %s [--root <path>] [--exclude <dir>] [--db <path>] index | search <pattern> | --help\n", argv[0]);
                printf("Options:\n");
                printf("  --root <path>    Set root directory to index (default: %s)\n", root);
                printf("  --exclude <dir>  Add directory to exclude from indexing\n");
                printf("  --db <path>      Set custom database file path (default: ~/.windex/.winindex.db)\n");
                printf("  --help           Show this help message\n");
                printf("Commands:\n");
                printf("  index            Index files from the root directory\n");
                printf("  search <pattern> Search for files matching the pattern\n");
                printf("Description:\n");
                printf("  Indexes files/folders from Windows drives. Stores in SQLite DB at %s\n",
                       custom_db ? custom_db : "~/.windex/.winindex.db");
                printf("  Incremental indexing: only new/modified entries are indexed.\n");
                printf("  Search is case-insensitive with partial matching, limited to 100 results.\n");
                _free_excluded_dirs();
                return 0;
            default:
                fprintf(stderr, "Invalid option. Use --help for usage.\n");
                _free_excluded_dirs();
                return 1;
        }
    }

    char db_path[MAX_PATH];
    if (_dbp(hommy, custom_db, db_path) != 0) {
        _free_excluded_dirs();
        return 1;
    }

    sqlite3 *db;
    if (_init_db(db_path, &db)) {
        _free_excluded_dirs();
        return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "Usage: %s [--root <path>] [--exclude <dir>] [--db <path>] index | search <pattern> | --help\n", argv[0]);
        sqlite3_close(db);
        _free_excluded_dirs();
        return 1;
    }

    if (strcmp(argv[optind], "index") == 0) {
        LOG_EXECUTION(_index_files_dynamic(db, root));
    } else if (strcmp(argv[optind], "search") == 0) {
        if (optind + 1 >= argc) {
            fprintf(stderr, "Error: Search pattern required.\n");
            sqlite3_close(db);
            _free_excluded_dirs();
            return 1;
        }
        _search_files(db, argv[optind + 1]);
    } else {
        fprintf(stderr, "Invalid command. Use --help for usage.\n");
        sqlite3_close(db);
        _free_excluded_dirs();
        return 1;
    }

    sqlite3_close(db);
    _free_excluded_dirs();
    return 0;
}