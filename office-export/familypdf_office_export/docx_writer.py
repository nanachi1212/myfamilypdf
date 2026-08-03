from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from docx import Document
from docx.shared import Pt

from .model import ExtractedDocument, TextBlock


def _write_block(paragraph, block) -> None:
    for source_run in block.runs:
        run = paragraph.add_run(source_run.text)
        run.bold = source_run.bold
        run.italic = source_run.italic
        if source_run.font_size is not None:
            run.font.size = Pt(source_run.font_size)
        if source_run.font_name:
            run.font.name = source_run.font_name


def _write_column_table(
    document,
    columns: list[list[TextBlock]],
) -> int:
    if not any(columns):
        return 0

    table = document.add_table(rows=1, cols=len(columns))
    paragraphs_exported = 0
    for column_index, cell in enumerate(table.rows[0].cells):
        for block_index, block in enumerate(columns[column_index]):
            paragraph = (
                cell.paragraphs[0]
                if block_index == 0
                else cell.add_paragraph()
            )
            _write_block(paragraph, block)
            paragraphs_exported += 1
    return paragraphs_exported


@dataclass(slots=True, frozen=True)
class DocxExportReport:
    pages_exported: int
    paragraphs_exported: int


def write_docx(
    extracted: ExtractedDocument,
    output_path: str | Path,
) -> DocxExportReport:
    """Write editable paragraphs and basic run styles to a DOCX file."""
    target = Path(output_path)
    target.parent.mkdir(parents=True, exist_ok=True)

    document = Document()
    paragraphs_exported = 0
    for page_index, page in enumerate(extracted.pages):
        if page.column_count > 1:
            pending_columns: list[list[TextBlock]] = [
                [] for _ in range(page.column_count)
            ]
            for block in page.blocks:
                if block.column_span > 1:
                    paragraphs_exported += _write_column_table(
                        document,
                        pending_columns,
                    )
                    pending_columns = [
                        [] for _ in range(page.column_count)
                    ]
                    paragraph = document.add_paragraph()
                    _write_block(paragraph, block)
                    paragraphs_exported += 1
                else:
                    pending_columns[block.column].append(block)
            paragraphs_exported += _write_column_table(
                document,
                pending_columns,
            )
        else:
            for block in page.blocks:
                paragraph = document.add_paragraph()
                _write_block(paragraph, block)
                paragraphs_exported += 1

        if page_index + 1 < len(extracted.pages):
            document.add_page_break()

    document.save(target)
    return DocxExportReport(
        pages_exported=len(extracted.pages),
        paragraphs_exported=paragraphs_exported,
    )
