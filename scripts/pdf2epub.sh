#!/usr/bin/env bash

# Uso:
# ./pdf2epub.sh arquivo.pdf "Título do Livro"

set -e

PDF="$1"
TITLE="$2"

if [ -z "$PDF" ]; then
  echo "Usage: $0 file.pdf \"Title of the Book\""
  exit 1
fi

BASENAME=$(basename "$PDF" .pdf)
WORKDIR=$(mktemp -d)

echo "📂 file: $PDF"
echo "📁 Temp Dir: $WORKDIR"

# Verifica se o PDF tem texto selecionável
if pdftotext "$PDF" - | grep -q '[[:alnum:]]'; then
  echo "✅ PDF  has text convertint to  HTML..."
  pdftohtml -s -noframes "$PDF" "$WORKDIR/$BASENAME.html"
  INPUT="$WORKDIR/$BASENAME.html"
else
  echo "🔎 PDF scanned pdf ..."
  tesseract "$PDF" "$WORKDIR/$BASENAME" -l por pdf
  pdftotext "$WORKDIR/$BASENAME.pdf" "$WORKDIR/$BASENAME.txt"
  INPUT="$WORKDIR/$BASENAME.txt"
fi

echo "📚 Generating EPUB..."
pandoc "$INPUT" \
  -o "$BASENAME.epub" \
  --toc \
  --metadata title="$TITLE"

echo "✅ EPUB generated: $BASENAME.epub"

# Limpeza
rm -rf "$WORKDIR"
