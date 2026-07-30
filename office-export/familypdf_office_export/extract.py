from __future__ import annotations

from collections.abc import Iterable, Sequence
from pathlib import Path
from typing import Any

import pdfplumber

from .model import (
    ExtractedDocument,
    ExtractedPage,
    ExtractedTable,
    TextBlock,
    TextRun,
)


def _is_bold(font_name: str) -> bool:
    lowered = font_name.lower()
    return "bold" in lowered or "black" in lowered or "semibold" in lowered


def _is_italic(font_name: str) -> bool:
    lowered = font_name.lower()
    return "italic" in lowered or "oblique" in lowered


def _group_words_into_blocks(words: Iterable[dict[str, Any]]) -> list[TextBlock]:
    sorted_words = sorted(
        words,
        key=lambda item: (
            round(float(item.get("top", 0.0)) / 3.0),
            float(item.get("x0", 0.0)),
        ),
    )
    grouped_lines: list[list[dict[str, Any]]] = []
    for word in sorted_words:
        top = float(word.get("top", 0.0))
        if not grouped_lines:
            grouped_lines.append([word])
            continue
        previous_top = float(grouped_lines[-1][0].get("top", 0.0))
        if abs(top - previous_top) <= 3.0:
            grouped_lines[-1].append(word)
        else:
            grouped_lines.append([word])

    blocks: list[TextBlock] = []
    for line in grouped_lines:
        line.sort(key=lambda item: float(item.get("x0", 0.0)))
        runs: list[TextRun] = []
        for index, word in enumerate(line):
            text = str(word.get("text", ""))
            if not text:
                continue
            if index > 0:
                text = f" {text}"
            font_name = str(word.get("fontname", ""))
            size_value = word.get("size")
            font_size = (
                float(size_value)
                if isinstance(size_value, (int, float))
                else None
            )
            runs.append(
                TextRun(
                    text=text,
                    bold=_is_bold(font_name),
                    italic=_is_italic(font_name),
                    font_size=font_size,
                    font_name=font_name or None,
                )
            )
        if runs:
            blocks.append(TextBlock(runs=runs))
    return blocks


def _extract_tables(page) -> list[ExtractedTable]:
    tables: list[ExtractedTable] = []
    for table in page.find_tables():
        rows = table.extract()
        if rows and any(any(value not in (None, "") for value in row) for row in rows):
            tables.append(ExtractedTable(rows=rows))
    return tables


def _select_page_numbers(
    page_count: int,
    page_numbers: Sequence[int] | None,
) -> list[int]:
    if page_numbers is None:
        return list(range(1, page_count + 1))

    selected: list[int] = []
    seen: set[int] = set()
    for page_number in page_numbers:
        if page_number < 1 or page_number > page_count:
            raise ValueError(
                f"Page {page_number} is outside the document range 1-{page_count}."
            )
        if page_number not in seen:
            selected.append(page_number)
            seen.add(page_number)
    return selected


def extract_document(
    input_path: str | Path,
    page_numbers: Sequence[int] | None = None,
) -> ExtractedDocument:
    """Extract editable text runs and tables from a machine-generated PDF."""
    source = Path(input_path)
    pages: list[ExtractedPage] = []
    document_warnings: list[str] = []

    with pdfplumber.open(source) as pdf:
        selected_page_numbers = _select_page_numbers(
            len(pdf.pages), page_numbers
        )
        for page_number in selected_page_numbers:
            page = pdf.pages[page_number - 1]
            words = page.extract_words(
                use_text_flow=True,
                keep_blank_chars=False,
                extra_attrs=["fontname", "size"],
            )
            blocks = _group_words_into_blocks(words)
            has_text_layer = bool(page.chars) and bool(blocks)
            warnings: list[str] = []
            if not has_text_layer:
                warning = (
                    f"Page {page_number} has no searchable text layer; "
                    "run OCR before Office export."
                )
                warnings.append(warning)
                document_warnings.append(warning)

            pages.append(
                ExtractedPage(
                    number=page_number,
                    blocks=blocks,
                    tables=_extract_tables(page),
                    warnings=warnings,
                    has_text_layer=has_text_layer,
                )
            )

    return ExtractedDocument(pages=pages, warnings=document_warnings)
