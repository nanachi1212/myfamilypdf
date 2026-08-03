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


def _group_words_into_blocks(
    words: Iterable[dict[str, Any]],
    page_width: float,
    detect_columns: bool = True,
) -> tuple[list[TextBlock], int]:
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

    block_positions: list[tuple[TextBlock, float, float, float]] = []
    for line in grouped_lines:
        line.sort(key=lambda item: float(item.get("x0", 0.0)))
        segments: list[list[dict[str, Any]]] = []
        for word in line:
            if not segments:
                segments.append([word])
                continue
            previous_x1 = float(segments[-1][-1].get("x1", 0.0))
            gap = float(word.get("x0", 0.0)) - previous_x1
            if gap > max(36.0, page_width * 0.12):
                segments.append([word])
            else:
                segments[-1].append(word)

        for segment in segments:
            block = _create_text_block(segment)
            if block is not None:
                top = float(segment[0].get("top", 0.0))
                block.top = top
                block_positions.append(
                    (
                        block,
                        min(float(word.get("x0", 0.0)) for word in segment),
                        max(float(word.get("x1", 0.0)) for word in segment),
                        top,
                    )
                )

    column_count = 1
    page_center = page_width / 2.0
    column_candidates = [
        position
        for position in block_positions
        if not (position[1] < page_center < position[2])
    ]
    if detect_columns and len(column_candidates) >= 4:
        starts = sorted(position[1] for position in column_candidates)
        gaps = [
            (starts[index + 1] - starts[index], index)
            for index in range(len(starts) - 1)
        ]
        largest_gap, gap_index = max(gaps, default=(0.0, 0))
        left_count = gap_index + 1
        right_count = len(starts) - left_count
        if (
            largest_gap >= page_width * 0.20
            and left_count >= 2
            and right_count >= 2
        ):
            split_x = (starts[gap_index] + starts[gap_index + 1]) / 2.0
            column_count = 2
            for block, x0, x1, _ in block_positions:
                if x0 < page_center < x1:
                    block.column = 0
                    block.column_span = 2
                else:
                    block.column = 0 if x0 < split_x else 1

    if column_count == 1:
        block_positions = []
        for line in grouped_lines:
            block = _create_text_block(line)
            if block is not None:
                top = float(line[0].get("top", 0.0))
                block.top = top
                block_positions.append(
                    (
                        block,
                        min(float(word.get("x0", 0.0)) for word in line),
                        max(float(word.get("x1", 0.0)) for word in line),
                        top,
                    )
                )

    if column_count > 1:
        spanning_positions = sorted(
            (
                item
                for item in block_positions
                if item[0].column_span > 1
            ),
            key=lambda item: (item[3], item[1]),
        )
        column_positions = [
            item
            for item in block_positions
            if item[0].column_span == 1
        ]
        ordered_positions: list[
            tuple[TextBlock, float, float, float]
        ] = []
        previous_top = float("-inf")
        for spanning in spanning_positions:
            ordered_positions.extend(
                sorted(
                    (
                        item
                        for item in column_positions
                        if previous_top <= item[3] < spanning[3]
                    ),
                    key=lambda item: (
                        item[0].column,
                        item[3],
                        item[1],
                    ),
                )
            )
            ordered_positions.append(spanning)
            previous_top = spanning[3]
        ordered_positions.extend(
            sorted(
                (
                    item
                    for item in column_positions
                    if item[3] >= previous_top
                ),
                key=lambda item: (
                    item[0].column,
                    item[3],
                    item[1],
                ),
            )
        )
        block_positions = ordered_positions
    else:
        block_positions.sort(key=lambda item: (item[3], item[1]))
    return [item[0] for item in block_positions], column_count


def _create_text_block(
    words: list[dict[str, Any]],
) -> TextBlock | None:
    runs: list[TextRun] = []
    for index, word in enumerate(words):
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
    return TextBlock(runs=runs) if runs else None


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
            tables = _extract_tables(page)
            blocks, column_count = _group_words_into_blocks(
                words,
                float(page.width),
                detect_columns=not tables,
            )
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
                    column_count=column_count,
                    tables=tables,
                    warnings=warnings,
                    has_text_layer=has_text_layer,
                )
            )

    return ExtractedDocument(pages=pages, warnings=document_warnings)
