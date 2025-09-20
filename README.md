# Windex: A Fast File Indexing and Search Tool

**Windex** is a lightweight utility for indexing and searching files on Windows drives (C:\ or /mnt/). We have built it with efficiency in mind, Windex uses SQLite to store file metadata and offers a modern alternative to traditional tools like `locate`, with features like incremental indexing and case-insensitive search.

Inspired by the open-source philosophy of sharing knowledge and tools, Windex is designed to help you quickly locate files and directories while providing detailed metadata. Whether you're a developer, power user, or just need a better way to organize and find files, Windex has you covered.

## Features

- **File and Directory Indexing**: Captures path, name, type, size, and modification time.
- **Incremental Indexing**: Only indexes new or modified files, saving time and resources.
- **Powerful Search**: Supports partial, case-insensitive matching for quick file lookup.
- **Limited Results**: Returns up to 100 results, sorted by modification time for relevance.
- **System Directory Exclusion**: Skips common system directories (e.g., `System Volume Information`, `Windows`) to reduce clutter.
- **SQLite Backend**: Efficient storage and fast querying with indexed database.
<!-- - **Cross-Platform**: Primarily tested on Windows with MinGW64 Bash, with plans for broader compatibility. -->
- **Better Than `locate`**: Offers metadata, flexible search, and controlled result limits.

## Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/somatech-20/windex.git
   ```
  > I compiled this utility using MinGW GCC, specifically the Winlibs distribution. The MSVC CL compiler is currently not functional for this project.

2. So, ensure you have SQLite and MinGW64 (or equivalent) installed.
3. Compile the program:
   ```bash
   gcc -o windex windex.c -L./3rds/libs/ -lsqlite3 -Wall -Werror
   ```
   
## Usage

Run Windex from the command line with one of the following commands:

```bash
windex index           # Indexes (or Indices ☝️) files and directories from C:\ or /mnt/
windex search <pattern> # Searches for files matching the pattern
windex --help          # Displays help information
```

The database is stored at `~/.windex/.winindex.db` or rather `%HOME%\.windex\.winindex.db` in your home directory, with optimized indexes for fast searching.

### Examples

- Index all files:
  ```bash
  windex index
  ```
- Search for files containing "project":
  ```bash
  windex search project
  ```

## Why Windex?

Windex was created to address the limitations of traditional file search tools. Unlike `locate`, it provides detailed metadata, supports partial and case-insensitive searches, and limits results to keep things manageable. It's lightweight, avoids complex dependencies (like WSL's Hyper-V), and is built for speed and simplicity.

## Future Plans

This is just the beginning for Windex! Planned improvements include:
- Enhanced help documentation and CLI options.
- Support for additional platforms (Linux, macOS).
- Advanced search filters (e.g., by size, type, or date).
- Different languages(translations).
<!--- Optional GUI for easier interaction.-->

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests to improve Windex.

<!-- Check out the [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. -->

## License

Windex is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

Inspired by [locate](https://en.wikipedia.org/wiki/Locate_(Unix))

> Frankly, this project have taught how painful MS dev environment is and i will get back to my old `I use Arch btw` way.
