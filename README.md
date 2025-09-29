# Windex: A (Fast) File Indexing and Search Tool

**Windex** is a lightweight utility for indexing and searching files on Windows drives (C:\ or /mnt/). We developed with efficiency in mind, Windex uses SQLite to store file metadata and offers a modern alternative to traditional tools like `locate`, with features like incremental indexing and case-insensitive search.

Inspired by the open-source philosophy of sharing knowledge and tools, Windex is designed to help you quickly locate files and directories while providing detailed metadata. Whether you're a developer, power user, or just need a better way to organize and find files, Windex has you covered.

### Examples

- Index all files:
  ```bash
  windex index  # Iterative indexing with SQLlite transactions
  ```
- Search for files containing "windows.h":
  ```bash
  windex search windows.h
  ```

## Why Windex?

Windex was created to address the limitations of traditional file search tools. Unlike `locate`, it provides detailed metadata, supports partial and case-insensitive searches, and limits results to keep things manageable. It's lightweight, avoids complex dependencies (like WSL's Hyper-V), and is built for speed and simplicity.

## Future Plans

This is just the beginning for Windex! Planned improvements include:
- Enhance indexing time, rather optimze.
- Enhanced help documentation and CLI options.
- Support for additional platforms (Linux, macOS).
- Advanced search filters (e.g., by size, type, or date).
<!-- - Different languages(translations). -->
<!--- Optional GUI for easier interaction.-->

## Contributing

Contributions are welcome! Feel free to open issues or submit pull requests to improve Windex.

<!-- Check out the [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. -->

## License

Windex is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

Inspired by [locate](https://en.wikipedia.org/wiki/Locate_(Unix))

<!-- > Frankly, this project reminded me of how painful MS dev environment is said to be and i will get back to my old `I use Arch btw` way. -->
