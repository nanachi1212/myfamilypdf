from __future__ import annotations

import argparse
from pathlib import Path

from familypdf_office_export.docx_writer import write_docx
from familypdf_office_export.model import (
    ExtractedDocument,
    ExtractedPage,
    ExtractedTable,
    TableMerge,
    TextBlock,
    TextRun,
)
from familypdf_office_export.xlsx_writer import write_xlsx


def build_docx_fixture(path: Path) -> None:
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
                    TextBlock(runs=[TextRun("FamilyPDF Office interoperability")]),
                ],
            ),
            ExtractedPage(
                number=2,
                blocks=[
                    TextBlock(runs=[TextRun("第二頁 / 第二页")]),
                    TextBlock(
                        runs=[
                            TextRun(
                                "多段落繁體內容",
                                bold=True,
                                font_size=18.0,
                            ),
                            TextRun(" / "),
                            TextRun(
                                "多段落简体内容",
                                italic=True,
                                font_size=9.0,
                            ),
                        ]
                    ),
                    TextBlock(
                        runs=[
                            TextRun(
                                "FamilyPDF preserves editable paragraphs "
                                "across explicit PDF page boundaries."
                            )
                        ]
                    ),
                ],
            ),
            ExtractedPage(
                number=3,
                blocks=[
                    TextBlock(
                        runs=[
                            TextRun(
                                "第三頁 / 第三页",
                                bold=True,
                                font_size=16.0,
                            )
                        ]
                    ),
                    TextBlock(runs=[TextRun("家庭文件摘要 / 家庭文件摘要")]),
                    TextBlock(
                        runs=[
                            TextRun("Mixed style: ", bold=True),
                            TextRun("editable", italic=True),
                            TextRun(" Office output"),
                        ]
                    ),
                ],
            ),
        ]
    )
    write_docx(document, path)


def build_xlsx_fixture(path: Path) -> None:
    document = ExtractedDocument(
        pages=[
            ExtractedPage(
                number=1,
                tables=[
                    ExtractedTable(
                        rows=[
                            ["項目", "数量", "金額"],
                            ["家庭測試", 2, 120],
                        ],
                        merges=[TableMerge(1, 1, 1, 2)],
                    ),
                    ExtractedTable(
                        rows=[
                            ["分類", "說明"],
                            ["附加", "第二個表格 / 第二个表格"],
                        ]
                    ),
                ],
            ),
            ExtractedPage(
                number=2,
                blocks=[
                    TextBlock(runs=[TextRun("無表格第一行")]),
                    TextBlock(runs=[TextRun("无表格第二行")]),
                    TextBlock(runs=[TextRun("FamilyPDF fallback row 3")]),
                    TextBlock(runs=[TextRun("長文字欄位 / 长文字栏位")]),
                ],
            ),
            ExtractedPage(
                number=3,
                tables=[
                    ExtractedTable(
                        rows=[
                            ["第三頁摘要 / 第三页摘要", None, None, None],
                            ["地區", "繁體", "简体", "Total"],
                            ["家庭", 10, 20, 30],
                        ],
                        merges=[TableMerge(1, 1, 1, 4)],
                    )
                ],
            ),
        ]
    )
    write_xlsx(document, path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create deterministic DOCX/XLSX fixtures for Microsoft Office smoke tests."
    )
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    docx_path = args.output_dir / "office-interop.docx"
    xlsx_path = args.output_dir / "office-interop.xlsx"
    build_docx_fixture(docx_path)
    build_xlsx_fixture(xlsx_path)
    print(docx_path)
    print(xlsx_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
