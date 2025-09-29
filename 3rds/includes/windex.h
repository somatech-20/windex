/* windex.h - Header file for windex program
 *
 * This file contains function declarations and macros used in the windex program.
 * It provides functionality for indexing and searching files in a directory.
 *
 * Copyright (C) 2025 MM
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
// #include <pthread.h>
// #ifdef _WIN32
// #include <windows.h>
// #else
#include <dirent.h>
// #endif

#include "sqlite3.h"
#include <ilogg.h>


#define DB_D "%s/.windex"
#define MAX_PATH 4096
#define MAX_NAME 256

// // Excluded directories
// const char *default_excluded_dirs[] = { "/Windows", "/Program Files", "/Program Files (x86)", "/ProgramData", NULL };
// char **excluded_dirs = NULL;
// int num_excluded_dirs = 0;

// Excluded dirs (dynamic array for user-defined excludes)
static const char *default_excluded_dirs[] = {
    "System Volume Information",
    "$RECYCLE.BIN",
    "Windows",
    "Program Files",
    "Program Files (x86)",
    NULL
};
static char **excluded_dirs = NULL;
static int num_excluded_dirs = 0;

// int _dbp(const char *home_dir, char *db_path);
int _dbp(const char *home_dir, const char *custom_db, char *db_path);
int _init_db(const char *db_path, sqlite3 **db);

void _init_excluded_dirs(void);
void _add_exclude_dir(const char *dir);
void _free_excluded_dirs(void);
int _is_excluded(const char *path);

long _get_db_mtime(sqlite3 *db, const char *path);

void _index_entry(sqlite3 *db, const char *path, struct stat *st);
void _prune_stale_entries(sqlite3 *db, const char *root);
void _index_files_dynamic(sqlite3 *db, const char *root);

void _search_files(sqlite3 *db, const char *pattern);

// int init_db(const char *db_path, sqlite3 **db);
// int is_excluded(const char *path);
// long get_db_mtime(sqlite3 *db, const char *path);
// void index_entry(sqlite3 *db, const char *path, struct stat *st);
// void index_files(sqlite3 *db, const char *root);
// void search_files(sqlite3 *db, const char *pattern);


/*
 frankly, I am not sure if this header file is necessary for such a small project.
    But I am keeping it for future expansion and it's 'good' practice.
*/