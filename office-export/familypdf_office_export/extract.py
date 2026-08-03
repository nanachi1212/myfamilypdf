from __future__ import annotations

from collections.abc import Iterable, Sequence
from io import BytesIO
from pathlib import Path
from typing import Any

import pdfplumber

from .model import (
    ExtractedDocument,
    ExtractedImage,
    ExtractedPage,
    ExtractedTable,
    LayoutItem,
    TextBlock,
    TextRun,
)


def _is_bold(font_name: str) -> bool:
    lowered = font_name.lower()
    return "bold" in lowered or "black" in lowered or "semibold" in lowered


def _is_italic(font_name: str) -> bool:
    lowered = font_name.lower()
    return "italic" in lowered or "oblique" in lowered


def _cluster_column_starts(
    block_positions: Sequence[tuple[TextBlock, float, float, float]],
    page_width: float,
) -> list[tuple[float, int]]:
    tolerance = page_width * 0.06
    clusters: list[list[float]] = []
    for start in sorted(position[1] for position in block_positions):
        if not clusters or abs(start - sum(clusters[-1]) / len(clusters[-1])) > tolerance:
            clusters.append([start])
        else:
            clusters[-1].append(start)
    return [
        (sum(cluster) / len(cluster), len(cluster))
        for cluster in clusters
        if len(cluster) >= 2
    ]


def _detect_column_starts(
    block_positions: Sequence[tuple[TextBlock, float, float, float]],
    page_width: float,
) -> list[float]:
    clusters = _cluster_column_starts(block_positions, page_width)
    if len(clusters) >= 3:
        three_candidates = sorted(
            sorted(clusters, key=lambda cluster: cluster[1], reverse=True)[:3],
            key=lambda cluster: cluster[0],
        )
        if all(
            three_candidates[index + 1][0] - three_candidates[index][0]
            >= page_width * 0.22
            for index in range(2)
        ):
            return [candidate[0] for candidate in three_candidates]

    pairs = [
        (left, right)
        for left_index, left in enumerate(clusters)
        for right in clusters[left_index + 1 :]
        if right[0] - left[0] >= page_width * 0.20
    ]
    if not pairs:
        return []
    left, right = max(
        pairs,
        key=lambda pair: (
            pair[0][1] + pair[1][1],
            pair[1][0] - pair[0][0],
        ),
    )
    return [left[0], right[0]]


def _assign_layout_column(
    x0: float,
    x1: float,
    column_count: int,
    column_boundaries: Sequence[float],
) -> tuple[int, int]:
    if (
        column_count > 1
        and column_boundaries
        and x0 < column_boundaries[0]
        and x1 > column_boundaries[-1]
    ):
        return 0, column_count
    midpoint = (x0 + x1) / 2.0
    column = sum(midpoint >= boundary for boundary in column_boundaries)
    return min(column, column_count - 1), 1


def _group_words_into_blocks(
    words: Iterable[dict[str, Any]],
    page_width: float,
    detect_columns: bool = True,
) -> tuple[list[TextBlock], int, list[float], list[float]]:
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
                left = min(
                    float(word.get("x0", 0.0))
                    for word in segment
                )
                block.top = top
                block.left = left
                block_positions.append(
                    (
                        block,
                        left,
                        max(float(word.get("x1", 0.0)) for word in segment),
                        top,
                    )
                )

    column_count = 1
    column_width_ratios = [1.0]
    column_boundaries: list[float] = []
    column_starts = (
        _detect_column_starts(block_positions, page_width)
        if detect_columns
        else []
    )
    if len(column_starts) in (2, 3):
        left_edge = column_starts[0]
        inferred_right_edge = page_width - left_edge
        column_boundaries = [
            start - page_width * 0.03
            for start in column_starts[1:]
        ]
        width_edges = [
            left_edge,
            *column_boundaries,
            inferred_right_edge,
        ]
        column_widths = [
            width_edges[index + 1] - width_edges[index]
            for index in range(len(width_edges) - 1)
        ]
        width_total = sum(column_widths)
        if width_total > 0.0 and all(width > 0.0 for width in column_widths):
            column_width_ratios = [
                width / width_total for width in column_widths
            ]
        else:
            column_width_ratios = [
                1.0 / len(column_starts) for _ in column_starts
            ]
        column_count = len(column_starts)
        for block, x0, x1, _ in block_positions:
            block.column, block.column_span = _assign_layout_column(
                x0,
                x1,
                column_count,
                column_boundaries,
            )

    if column_count == 1:
        block_positions = []
        for line in grouped_lines:
            block = _create_text_block(line)
            if block is not None:
                top = float(line[0].get("top", 0.0))
                left = min(
                    float(word.get("x0", 0.0))
                    for word in line
                )
                block.top = top
                block.left = left
                block_positions.append(
                    (
                        block,
                        left,
                        max(float(word.get("x1", 0.0)) for word in line),
                        top,
                    )
                )

    blocks = [item[0] for item in block_positions]
    return (
        _order_layout_items(blocks, column_count),
        column_count,
        column_width_ratios,
        column_boundaries,
    )


def _order_layout_items(
    items: Sequence[LayoutItem],
    column_count: int,
) -> list[LayoutItem]:
    if column_count <= 1:
        return sorted(items, key=lambda item: (item.top, item.left))

    spanning_items = sorted(
        (item for item in items if item.column_span > 1),
        key=lambda item: (item.top, item.left),
    )
    column_items = [item for item in items if item.column_span == 1]
    ordered_items: list[LayoutItem] = []
    previous_top = float("-inf")
    for spanning in spanning_items:
        ordered_items.extend(
            sorted(
                (
                    item
                    for item in column_items
                    if previous_top <= item.top < spanning.top
                ),
                key=lambda item: (item.column, item.top, item.left),
            )
        )
        ordered_items.append(spanning)
        previous_top = spanning.top
    ordered_items.extend(
        sorted(
            (
                item
                for item in column_items
                if item.top >= previous_top
            ),
            key=lambda item: (item.column, item.top, item.left),
        )
    )
    return ordered_items


def _extract_images(
    page,
    column_count: int,
    column_boundaries: Sequence[float],
    page_number: int,
) -> tuple[list[ExtractedImage], list[str]]:
    if not page.images:
        return [], []

    try:
        rendered = page.to_image(resolution=144).original.convert("RGB")
    except Exception as error:
        return [], [
            f"Page {page_number} images could not be rendered: {error}"
        ]

    images: list[ExtractedImage] = []
    warnings: list[str] = []
    scale = 2.0
    for image_index, source in enumerate(page.images, start=1):
        try:
            x0 = float(source.get("x0", 0.0))
            x1 = float(source.get("x1", 0.0))
            top = float(source.get("top", 0.0))
            bottom = float(source.get("bottom", 0.0))
            width = x1 - x0
            height = bottom - top
            if width < 12.0 or height < 12.0:
                continue

            crop_box = (
                max(0, round(x0 * scale)),
                max(0, round(top * scale)),
                min(rendered.width, round(x1 * scale)),
                min(rendered.height, round(bottom * scale)),
            )
            if crop_box[0] >= crop_box[2] or crop_box[1] >= crop_box[3]:
                continue
            crop = rendered.crop(crop_box)
            buffer = BytesIO()
            crop.save(buffer, format="PNG")

            column = 0
            column_span = 1
            if column_count > 1:
                column, column_span = _assign_layout_column(
                    x0,
                    x1,
                    column_count,
                    column_boundaries,
                )
            images.append(
                ExtractedImage(
                    data=buffer.getvalue(),
                    top=top,
                    left=x0,
                    width_points=width,
                    height_points=height,
                    column=column,
                    column_span=column_span,
                )
            )
        except Exception as error:
            warnings.append(
                f"Page {page_number} image {image_index} was skipped: {error}"
            )
    images.sort(key=lambda image: (image.top, image.left))
    return images, warnings


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
            (
                blocks,
                column_count,
                column_width_ratios,
                column_boundaries,
            ) = _group_words_into_blocks(
                words,
                float(page.width),
                detect_columns=not tables,
            )
            images, image_warnings = _extract_images(
                page,
                column_count,
                column_boundaries,
                page_number,
            )
            layout_items = _order_layout_items(
                [*blocks, *images],
                column_count,
            )
            has_text_layer = bool(page.chars) and bool(blocks)
            warnings = list(image_warnings)
            document_warnings.extend(image_warnings)
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
                    images=images,
                    layout_items=layout_items,
                    column_count=column_count,
                    column_width_ratios=column_width_ratios,
                    column_boundaries=column_boundaries,
                    tables=tables,
                    warnings=warnings,
                    has_text_layer=has_text_layer,
                )
            )

    return ExtractedDocument(pages=pages, warnings=document_warnings)
