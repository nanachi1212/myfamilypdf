import tempfile
import unittest
from pathlib import Path

from docx import Document
from pypdf import PdfWriter
from pypdf.generic import (
    DecodedStreamObject,
    DictionaryObject,
    NameObject,
)

from familypdf_office_export.docx_writer import write_docx
from familypdf_office_export.extract import extract_document


def _write_two_column_pdf(path: Path) -> None:
    writer = PdfWriter()
    page = writer.add_blank_page(width=600, height=400)
    font = DictionaryObject(
        {
            NameObject("/Type"): NameObject("/Font"),
            NameObject("/Subtype"): NameObject("/Type1"),
            NameObject("/BaseFont"): NameObject("/Helvetica"),
        }
    )
    font_reference = writer._add_object(font)
    page[NameObject("/Resources")] = DictionaryObject(
        {
            NameObject("/Font"): DictionaryObject(
                {NameObject("/F1"): font_reference}
            )
        }
    )
    content = DecodedStreamObject()
    content.set_data(
        b"""
BT /F1 18 Tf 220 375 Td (Full width heading) Tj ET
BT /F1 12 Tf 40 340 Td (Left top) Tj ET
BT /F1 12 Tf 40 300 Td (Left bottom) Tj ET
BT /F1 12 Tf 340 340 Td (Right top) Tj ET
BT /F1 12 Tf 340 300 Td (Right bottom) Tj ET
"""
    )
    page[NameObject("/Contents")] = writer._add_object(content)
    with path.open("wb") as stream:
        writer.write(stream)


class MultiColumnExportTest(unittest.TestCase):
    def test_preserves_two_pdf_columns_as_editable_docx_columns(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "two-columns.pdf"
            target = Path(directory) / "two-columns.docx"
            _write_two_column_pdf(source)

            extracted = extract_document(source)
            report = write_docx(extracted, target)
            document = Document(target)

        self.assertEqual(extracted.pages[0].column_count, 2)
        heading = extracted.pages[0].blocks[0]
        self.assertEqual(heading.text, "Full width heading")
        self.assertEqual(heading.column_span, 2)
        self.assertEqual(document.paragraphs[0].text, "Full width heading")
        self.assertEqual(len(document.tables), 1)
        cells = document.tables[0].rows[0].cells
        self.assertEqual(len(cells), 2)
        self.assertEqual(cells[0].text.splitlines(), ["Left top", "Left bottom"])
        self.assertEqual(cells[1].text.splitlines(), ["Right top", "Right bottom"])
        self.assertEqual(report.paragraphs_exported, 5)


if __name__ == "__main__":
    unittest.main()
