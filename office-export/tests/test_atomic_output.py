from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from docx import Document
from docx.document import Document as DocumentClass
from openpyxl import Workbook

from familypdf_office_export.docx_writer import write_docx
from familypdf_office_export.model import ExtractedDocument
from familypdf_office_export.xlsx_writer import write_xlsx


class AtomicOutputTest(unittest.TestCase):
    def test_docx_writes_to_temporary_path_before_replacement(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output.docx"
            output.write_bytes(b"original-docx")

            saved_paths: list[Path] = []

            def save_to_path(document: Document, path: str | Path) -> None:
                saved_paths.append(Path(path))
                Path(path).write_bytes(b"replacement-docx")

            with patch.object(DocumentClass, "save", save_to_path):
                write_docx(ExtractedDocument(pages=[]), output)

            self.assertEqual(output.read_bytes(), b"replacement-docx")
            self.assertEqual(len(saved_paths), 1)
            self.assertNotEqual(saved_paths[0], output)
            self.assertFalse(saved_paths[0].exists())

    def test_xlsx_writes_to_temporary_path_before_replacement(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output.xlsx"
            output.write_bytes(b"original-xlsx")

            saved_paths: list[Path] = []

            def save_to_path(workbook: Workbook, path: str | Path) -> None:
                saved_paths.append(Path(path))
                Path(path).write_bytes(b"replacement-xlsx")

            with patch.object(Workbook, "save", save_to_path):
                write_xlsx(ExtractedDocument(pages=[]), output)

            self.assertEqual(output.read_bytes(), b"replacement-xlsx")
            self.assertEqual(len(saved_paths), 1)
            self.assertNotEqual(saved_paths[0], output)
            self.assertFalse(saved_paths[0].exists())


if __name__ == "__main__":
    unittest.main()
