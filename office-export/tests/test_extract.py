import tempfile
import unittest
from pathlib import Path

from pypdf import PdfReader, PdfWriter
from pypdf.generic import (
    DecodedStreamObject,
    DictionaryObject,
    NameObject,
)

from familypdf_office_export.extract import extract_document


def _write_text_and_table_pdf(path: Path) -> None:
    writer = PdfWriter()
    page = writer.add_blank_page(width=300, height=300)

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
BT /F1 12 Tf 40 260 Td (FamilyPDF export) Tj ET
1 w
40 200 m 260 200 l 260 120 l 40 120 l h S
40 160 m 260 160 l S
140 200 m 140 120 l S
BT /F1 10 Tf 50 175 Td (Item) Tj ET
BT /F1 10 Tf 150 175 Td (Qty) Tj ET
BT /F1 10 Tf 50 135 Td (Book) Tj ET
BT /F1 10 Tf 150 135 Td (2) Tj ET
"""
    )
    page[NameObject("/Contents")] = writer._add_object(content)
    with path.open("wb") as stream:
        writer.write(stream)


class ExtractTest(unittest.TestCase):
    def test_extracts_text_runs_and_ruled_table(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "table.pdf"
            _write_text_and_table_pdf(source)
            extracted = extract_document(source)

        self.assertEqual(len(extracted.pages), 1)
        page = extracted.pages[0]
        self.assertTrue(page.has_text_layer)
        self.assertEqual(page.column_count, 1)
        self.assertEqual(len(page.blocks), 3)
        all_text = "\n".join(block.text for block in page.blocks)
        self.assertIn("FamilyPDF export", all_text)
        self.assertGreaterEqual(len(page.tables), 1)
        flattened = [
            value
            for row in page.tables[0].rows
            for value in row
            if value is not None
        ]
        self.assertIn("Item", flattened)
        self.assertIn("Book", flattened)

    def test_marks_blank_page_as_missing_text_layer(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "blank.pdf"
            writer = PdfWriter()
            writer.add_blank_page(width=300, height=300)
            with source.open("wb") as stream:
                writer.write(stream)

            self.assertEqual(len(PdfReader(source).pages), 1)
            extracted = extract_document(source)

        self.assertFalse(extracted.pages[0].has_text_layer)
        self.assertTrue(
            any("OCR" in warning for warning in extracted.pages[0].warnings)
        )


if __name__ == "__main__":
    unittest.main()
