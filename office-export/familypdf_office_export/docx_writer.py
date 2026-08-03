from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from docx import Document
from docx.shared import Pt

from .model import ExtractedDocument


def _write_block(paragraph, block) -> None:
    for source_run in block.runs:
        run = paragraph.add_run(source_run.text)
        run.bold = source_run.bold
        run.italic = source_run.italic
        if source_run.font_size is not None:
            run.font.size = Pt(source_run.font_size)
        if source_run.font_name:
            run.font.name = source_run.font_name


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
            table = document.add_table(rows=1, cols=page.column_count)
            for column_index, cell in enumerate(table.rows[0].cells):
                column_blocks = [
                    block
                    for block in page.blocks
                    if block.column == column_index
                ]
                for block_index, block in enumerate(column_blocks):
                    paragraph = (
                        cell.paragraphs[0]
                        if block_index == 0
                        else cell.add_paragraph()
                    )
                    _write_block(paragraph, block)
                    paragraphs_exported += 1
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
