from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from docx import Document
from docx.shared import Pt

from .model import ExtractedDocument


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
        for block in page.blocks:
            paragraph = document.add_paragraph()
            for source_run in block.runs:
                run = paragraph.add_run(source_run.text)
                run.bold = source_run.bold
                run.italic = source_run.italic
                if source_run.font_size is not None:
                    run.font.size = Pt(source_run.font_size)
                if source_run.font_name:
                    run.font.name = source_run.font_name
            paragraphs_exported += 1

        if page_index + 1 < len(extracted.pages):
            document.add_page_break()

    document.save(target)
    return DocxExportReport(
        pages_exported=len(extracted.pages),
        paragraphs_exported=paragraphs_exported,
    )
