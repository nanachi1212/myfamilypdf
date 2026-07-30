import tempfile
import unittest
import zipfile
from pathlib import Path
from xml.etree import ElementTree

from familypdf_office_export.docx_writer import write_docx
from familypdf_office_export.model import (
    ExtractedDocument,
    ExtractedPage,
    TextBlock,
    TextRun,
)


WORD_NS = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}


class DocxExportTest(unittest.TestCase):
    def test_writes_multilingual_styles_paragraphs_and_page_break(self) -> None:
        document = ExtractedDocument(
            pages=[
                ExtractedPage(
                    number=1,
                    blocks=[
                        TextBlock(
                            runs=[
                                TextRun("繁體中文", bold=True, font_size=14.0),
                                TextRun(" / "),
                                TextRun("简体中文", italic=True, font_size=12.0),
                            ]
                        ),
                        TextBlock(runs=[TextRun("FamilyPDF 123")]),
                    ],
                ),
                ExtractedPage(
                    number=2,
                    blocks=[TextBlock(runs=[TextRun("第二頁 / 第二页")])],
                ),
            ]
        )

        with tempfile.TemporaryDirectory() as directory:
            output_path = Path(directory) / "multilingual.docx"
            report = write_docx(document, output_path)

            self.assertEqual(report.pages_exported, 2)
            self.assertEqual(report.paragraphs_exported, 3)
            self.assertTrue(output_path.is_file())

            with zipfile.ZipFile(output_path) as archive:
                xml = archive.read("word/document.xml")

        root = ElementTree.fromstring(xml)
        text = "".join(
            node.text or "" for node in root.findall(".//w:t", WORD_NS)
        )
        self.assertIn("繁體中文", text)
        self.assertIn("简体中文", text)
        self.assertIn("FamilyPDF 123", text)
        self.assertIn("第二頁 / 第二页", text)
        self.assertIsNotNone(root.find(".//w:b", WORD_NS))
        self.assertIsNotNone(root.find(".//w:i", WORD_NS))
        page_breaks = [
            node
            for node in root.findall(".//w:br", WORD_NS)
            if node.attrib.get(f"{{{WORD_NS['w']}}}type") == "page"
        ]
        self.assertEqual(len(page_breaks), 1)


if __name__ == "__main__":
    unittest.main()
