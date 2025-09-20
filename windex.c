/* windex.c - Main implementation file for windex program
 *
 * This program indexes files and directories from Windows drives (C:\ or /mnt/)
 * and stores the information in a SQLite database.
 * It supports incremental indexing and provides a search functionality with metadata.
 *
 *
 * Features (for now*):
 *   * - Indexes files and directories, storing path, name, type, size, and modification time.
 *   * - Incremental indexing: only new or modified entries are indexed.
 *   * - Search functionality with partial and case-insensitive matching.
 *   * - Limits search results to 100 entries, sorted by modification time.
 *   * - Excludes common system directories to avoid unnecessary indexing.
 *   * - Uses SQLite for efficient storage and querying.
 *   * - Cross-platform compatibility (primarily tested on Windows with MingW64 Bash).
 *   *    (should have been WSL at the very least But Nope, I don't like its complexity with hyper-v).
 *   * - Better than traditional locate: includes metadata, partial/case-insensitive search, and limits results.
 *   
 *
 * Usage: windex index | search <pattern> | --help
 *     The database is stored in the user's home directory under .windex/.winindex.db
 *     with appropriate indexes for fast searching.
 *     
 * Copyright (C) 2025 MM
 *
*/

#include "3rds/includes/windex.h"

// // Depricated
// // #define DB_F "%s/.winindex.db"
// int _dbdir(const char *dir_path) {
//     struct stat st = {0};
//     if (stat(dir_path, &st) == -1) {
//         if (mkdir(dir_path) == -1) {
//             perror("mkdir failed");
//             return -1;
//         }
//     }
//     return 0;
// }
// // Depricated

int _dbp(const char *home_dir, char *db_path) {
    char db_dir_path[MAX_PATH];
    char db_file_path[MAX_PATH];

    // Construct the directory path: <home_dir>/.windex
    snprintf(db_dir_path, MAX_PATH, DB_D, home_dir);
    // Construct the file path: <home_dir>/.windex/.winindex.db
    int res_buf = snprintf(db_file_path, MAX_PATH, "%s/.winindex.db", db_dir_path);
    if (res_buf < 0 || res_buf >= MAX_PATH) {
        fprintf(stderr, "Error constructing database file path.\n");
        return -1;
    }

    // Check if directory exists, create it if necessary
    struct stat st = {0};
    if (stat(db_dir_path, &st) == -1) {
        if (mkdir(db_dir_path) == -1) { // on unix-like systems, we would use 0700 for permissions
            perror("mkdir failed for directory");
            return -1;
        }
    }

    // Check and create file if necessary
    FILE *db_file = fopen(db_file_path, "r");
    if (!db_file) {
        db_file = fopen(db_file_path, "w+");
        if (!db_file) {
            perror("Failed to create the database file");
            return -1;
        }
    } else {
        fclose(db_file);
    }

    // Copy the correct file path to db_path
    snprintf(db_path, MAX_PATH, "%s", db_file_path);
    return 0;
}

// Excluded dirs
const char *excluded_dirs[] = {
    "System Volume Information",
    "$RECYCLE.BIN",
    "Windows",
    "Program Files",
    "Program Files (x86)",
    NULL
};

// Check if path contains an excluded directory
int is_excluded(const char *path) {
    for (int i = 0; excluded_dirs[i]; i++) {
        if (strstr(path, excluded_dirs[i])) return 1;
    }
    return 0;
}

// Initialize SQLite database
int init_db(const char *db_path, sqlite3 **db) {
    if (sqlite3_open(db_path, db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(*db));
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
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(*db);
        return 1;
    }
    return 0;
}

// Get existing mtime from database
long get_db_mtime(sqlite3 *db, const char *path) {
    sqlite3_stmt *stmt;
    long mtime = 0;
    const char *sql = "SELECT mtime FROM files WHERE full_path = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            mtime = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return mtime;
}

// Index a single file or directory
void index_entry(sqlite3 *db, const char *path, struct stat *st) {
    char name[MAX_NAME];
    snprintf(name, MAX_NAME, "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
    const char *type = S_ISDIR(st->st_mode) ? "dir" : "file";
    long mtime = (long)st->st_mtime;
    long db_mtime = get_db_mtime(db, path);

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
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

// Recursively index files and directories
void index_files(sqlite3 *db, const char *root) {
    DIR *dir = opendir(root);
    if (!dir) return;

    int count = 0;
    struct dirent *entry;
    char path[MAX_PATH];
    struct stat st;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, MAX_PATH, "%s/%s", root, entry->d_name);
        if (is_excluded(path)) continue;

        if (stat(path, &st) == 0) {
            index_entry(db, path, &st);
            count++;
            if (S_ISDIR(st.st_mode)) {
                index_files(db, path);
            }
        }
    }
    closedir(dir);
    printf("Indexed %d new or modified entries.\n", count);
}

// Search the index
void search_files(sqlite3 *db, const char *pattern) {
    sqlite3_stmt *stmt;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT full_path, type, size, mtime FROM files "
        "WHERE lower(name) LIKE '%%%s%%' OR lower(full_path) LIKE '%%%s%%' "
        "ORDER BY mtime DESC LIMIT 100;", pattern, pattern);

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
    }
}

int main(int argc, char *argv[]) {
    const char *hommy = getenv("HOME") ? getenv("HOME") : ".";

    char db_path[MAX_PATH];

    if (_dbp(hommy, db_path) != 0) return 1;

    sqlite3 *db;
    if (init_db(db_path, &db)) return 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s index | search <pattern> | --help\n", argv[0]);
        sqlite3_close(db);
        return 1;
    }

    if (strcmp(argv[1], "index") == 0) {
        const char *root = access("/mnt/", F_OK) == 0 ? "/mnt/" : "C:\\";
        index_files(db, root);
    } else if (strcmp(argv[1], "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: Search pattern required.\n");
            sqlite3_close(db);
            return 1;
        }
        search_files(db, argv[2]);
    } else if (strcmp(argv[1], "--help") == 0) {
        // TODO: Make this better.
        printf("Usage: %s index | search <pattern> | --help\n", argv[0]);
        printf("Indexes files/folders from Windows drives (C:\\ or /mnt/).\n");
        printf("Stores in SQLite DB at %s with indexes for fast search.\n", db_path);
        printf("Incremental indexing: only new/modified entries are indexed.\n");
        printf("Better than locate: Includes metadata, partial/case-insensitive search, limits results.\n");
    } else {
        fprintf(stderr, "Invalid command. Use --help for usage.\n");
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}

/* End of windex.c
*
* I might keep improving it.
*
*/
