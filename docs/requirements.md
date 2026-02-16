# EPUB Translation Plugin Requirements

## 1. Overview
This document outlines the functional and non-functional requirements for an EPUB translation plugin. The tool utilizes Large Language Models (LLMs) to translate ebook content while strictly preserving the original formatting, structure, and metadata.

**Technical Stack:** Developed in **C** for **Unix-like** systems, adhering to **Clean Code** principles.

---

## 2. Functional Requirements

### 2.1 File Handling
2.1.1 **Input**: Support for valid `.epub` files (OEBPS container format).
2.1.2 **Output**: Generation of a new `.epub` file containing the fully translated content.
2.1.3 **Structure Preservation**: The output must be structurally identical to the input. This includes:
- **Navigation**: Table of Contents (NCX and NAV documents).
- **Spine**: Chapter order and separation.
- **Assets**: Preservation of images, fonts, and scripts.
- **Styling**: All CSS rules and HTML class mapping must remain intact.
- **Metadata**: Title, Author, and Description should be translated while keeping standard identifiers (ISBN, UUID).

### 2.2 Configuration System
2.2.1 **Configuration Sources**:
- **File**: `./conf/config.json` for default settings.
- **CLI**: Command-line arguments for overriding specific values.
2.2.2 **Precedence**: Command-line parameters take precedence over values defined in the configuration file.
2.2.3 **Language Selection**:
- Default output language defined in `config.json`.
- Supported codes (ISO 639-1): `en`, `es`, `fr`, `de`, `it`, `pt-br`, `ru`, `zh`, `ja`, `ko`, `ar`, `hi`, etc.

### 2.3 LLM Integration
2.3.1 **Provider Support**: Extensible architecture to support multiple providers (OpenAI, Anthropic, Google Gemini, Ollama).
2.3.2 **Security**: Secure handling of API keys (passed via environment variables or encrypted in config, never logged).
2.3.3 **Prompting**: System prompts and translation tone (e.g., literal vs. creative) must be configurable.

### 2.4 Translation Process
2.4.1 **Batch Processing**: 
- Content split into manageable chunks based on token limits.
- **Context Window**: Window size must be configurable via `config.json` or CLI.
2.4.2 **Context Maintenance (Alternatives)**:
- **A. Sliding Window**: Include the last 100-200 words of the previous translated chapter in the context of the next prompt to maintain flow.
- **B. Terminology Bootstrap**: Pre-scan the book for key character names and places to create a "Translation Memory" passed with every chunk.
- **C. Chapter Summary**: Maintain a rolling summary (max 500 tokens) of the book's plot to provide to the LLM for long-form consistency.
2.4.3 **Glossary support**: Force specific translations for user-defined terms.
2.4.4 **Error Handling**: Implement exponential backoff for rate limits and robust handling of network timeouts.

### 2.5 User Interface
2.5.1 **Execution**: Primarily CLI-based for Unix-like environments.
2.5.2 **Feedback**: Real-time progress bar (UTF-8) showing percentage of chapters/words translated.
2.5.3 **Logging**: Standard error (stderr) for diagnostic logs, standard output (stdout) for progress.

---

## 3. Non-Functional Requirements

### 3.1 Performance
3.1.1 **Resource Usage**: Written in C to ensure low memory footprint compared to Python/Node.js alternatives.
3.1.2 **Concurrency**: Utilize threads (e.g., pthreads) to translate multiple chapters in parallel, respecting provider rate limits.

### 3.2 Reliability
3.2.1 **Validation**: The resulting file must pass `epubcheck` validation.
3.2.2 **Fault Tolerance**: In case of a crash, the plugin should allow resuming from the last successfully translated chapter.

### 3.3 Portability
3.3.1 **Unix-like**: Compatible with Linux, macOS, and WSL.
3.3.2 **Dependencies**: Minimize external dependencies; prefer standard libraries where possible.

---

## 4. Technical Constraints (C Implementation)
- **ZIP Handling**: `libzip` or `miniz`.
- **XML/HTML Parsing**: `libxml2` (for DOM manipulation) or `expat`.
- **JSON**: `json-c` for configuration.
- **Networking**: `libcurl` for API communication.
- **Build System**: `Makefile` or `CMake`.
