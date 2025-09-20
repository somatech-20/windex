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
#include "sqlite3.h"
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#define DB_D "%s/.windex"
#define MAX_PATH 4096
#define MAX_NAME 256

int _dbp(const char *home_dir, char *db_path);
int init_db(const char *db_path, sqlite3 **db);
int is_excluded(const char *path);
long get_db_mtime(sqlite3 *db, const char *path);
void index_entry(sqlite3 *db, const char *path, struct stat *st);
void index_files(sqlite3 *db, const char *root);
void search_files(sqlite3 *db, const char *pattern);


/*
 frankly, I am not sure if this header file is necessary for such a small project.
    But I am keeping it for future expansion and it's 'good' practice.
*/