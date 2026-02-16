# EPUB Translation Plugin (C)

A high-performance EPUB translation tool written in C for Unix-like systems. It uses Large Language Models (LLMs) to translate ebook content while strictly preserving formatting, CSS, and metadata structure.

Made with gemini 3.

## Prerequisites

You will need the following libraries installed on your system:
- `libzip`: For EPUB (ZIP) handling.
- `libxml2`: For XHTML and XML manifest parsing.
- `json-c`: For configuration file parsing.
- `libcurl`: For API communication with LLM providers.

**Ubuntu/Debian installation:**
```bash
sudo apt-get update
sudo apt-get install build-essential pkg-config libzip-dev libxml2-dev libjson-c-dev libcurl4-openssl-dev
```

**macOS installation (via Homebrew):**
```bash
brew install libzip libxml2 json-c curl pkg-config
```
*Note: You may need to set `PKG_CONFIG_PATH` if libraries are not found automatically.*

**Windows:**
This tool is designed for Unix-like environments. On Windows, please use **WSL (Windows Subsystem for Linux)** and follow the **Ubuntu/Debian** instructions above.

## Configuration

The plugin uses a JSON configuration file. It searches in the following order:
1.  Command-line argument (`-c <file>`)
2.  Local directory (`./conf/config.json`)
3.  System-wide directory (`/usr/local/etc/ebook-translator/config.json`)

You have do create a ./conf/config.json file ou point to location you are using you config.json.

### `config.json` Example:
```json
{
    "llm_provider": "openai",
    "model": "gpt-4o",
    "api_key": "YOUR_OPENAI_API_KEY",
    "target_language": "pt-br",
    "context_window": 4096,
    "context_file": "book_context.json",
    "tone": "literal"
}
```

### CLI Overrides:
Command-line parameters take precedence over the configuration file:
- `-l, --lang <code>`: Override the target language (e.g., `-l fr`).
- `-m, --model <name>`: Override the LLM model.
- `-c, --config <file>`: Use a custom configuration file path.

## Build

To compile the project, simply run `make` in the root directory:

```bash
make
```

This will create binary `epubtrans` in the current directory.

To install it system-wide (requires sudo):
```bash
sudo make install
```
Then you can run `epubtrans` from anywhere.

## Run

To translate an EPUB file:

```bash
./epubtrans input.epub output.epub
```

To translate with a specific language override:

```bash
./epubtrans -l pt-br input.epub translated_output.epub
```

To translate to Spanish:
```bash
./epubtrans -l es input.epub output_es.epub
```

To translate to French:
```bash
./epubtrans -l fr input.epub output_fr.epub
```

## Features
- **Structure Preservation**: Keeps all CSS, images, and HTML tags exactly as they were.
- **Context Maintenance**: Supports persistent context history (via `-C` flag) to maintain character and plot consistency across chapters.
- **Fast & Lightweight**: Developed in C for minimal memory footprint.

### Custom Prompts
You can customize the LLM prompts by editing the markdown files in `conf/`:
-   `prompt_context_init.md`: Used to extract initial context from the first chapter.
-   `prompt_context_update.md`: Used to update context with new chapter content.
-   `prompt_translation.md`: Used for the actual translation.

These files are loaded relative to the executable or from `/usr/local/etc/ebook-translator/`. If missing, built-in defaults are used.

### Context Awareness (New!)
To enable persistent context tracking (improves consistency but increases API usage/cost):

1.  Add `"context_file": "book_context.json"` to your `config.json`.
2.  Or use the `-C` flag: `epubtrans -c config.json -C context.json input.epub`

The tool will:
-   Initialize context from the first chapter (Summary, Characters, Locations).
-   Update the context file after translating each chapter.
-   Inject the current context into the LLM prompt for subsequent translations.

## Common Issues
## License

This project is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

[![CC BY-NC-SA 4.0](https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png)](http://creativecommons.org/licenses/by-nc-sa/4.0/)

View the full license at [http://creativecommons.org/licenses/by-nc-sa/4.0/](http://creativecommons.org/licenses/by-nc-sa/4.0/) or see the [LICENSE](LICENSE) file.
