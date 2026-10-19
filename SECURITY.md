# Security

## What fsvng touches

- **Reads** the metadata of every file and folder under the folder you point it at: names, sizes,
  timestamps, types, owners. It never reads file contents.
- **Writes** only its own settings: `config.json` and `imgui.ini` in `%APPDATA%\fsvng` (Windows),
  `~/Library/Application Support/fsvng` (macOS) or `~/.config/fsvng` (Linux). They hold paths you
  scanned, your default path, and display preferences.
- **Runs** other programs only when you ask: on Windows, the right-click Open item hands the file
  to the shell (`ShellExecute`), which opens it with its default program, the same as
  double-clicking it in Explorer. That includes running `.exe` files. The right-click "Windows
  Context Menu" item shows Explorer's own menu for the file, and runs whichever command you pick
  from it.
- **No network.** The desktop app makes no connections. (The build downloads its dependencies;
  see [docs/building.md](docs/building.md).)

## Reporting a problem

Open a [GitHub security advisory](https://github.com/sp00nznet/fsvng/security/advisories/new)
rather than a public issue. Include the version (Help > About, or the release you downloaded),
your OS, and steps to reproduce.
