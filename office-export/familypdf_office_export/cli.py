from __future__ import annotations

import argparse
import json
from dataclasses import asdict
from pathlib import Path
from typing import Sequence

from .docx_writer import write_docx
from .extract import extract_document
from .xlsx_writer import write_xlsx

EXIT_ERROR = 1
EXIT_MISSING_TEXT_LAYER = 3


def parse_page_range(value: str) -> list[int] | None:
    """Parse a 1-based page expression such as ``1-3,5``."""
    if not value.strip():
        return None

    pages: list[int] = []
    seen: set[int] = set()
    try:
        for raw_part in value.split(","):
            part = raw_part.strip()
            if not part:
                raise ValueError
            if "-" in part:
                raw_start, raw_end = part.split("-", maxsplit=1)
                start = int(raw_start)
                end = int(raw_end)
                if start < 1 or end < start:
                    raise ValueError
                values = range(start, end + 1)
            else:
                page = int(part)
                if page < 1:
                    raise ValueError
                values = (page,)
            for page in values:
                if page not in seen:
                    pages.append(page)
                    seen.add(page)
    except (TypeError, ValueError) as error:
        raise ValueError(
            f"Invalid page range '{value}'. Use a value such as 1-3,5."
        ) from error
    return pages


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="FamilyPDFOfficeExport",
        description="Export searchable PDF text to editable DOCX or XLSX.",
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--format", required=True, choices=("docx", "xlsx"))
    parser.add_argument(
        "--pages",
        default="",
        help="1-based pages, for example: 1-3,5",
    )
    parser.add_argument(
        "--allow-empty-text",
        action="store_true",
        help="Create output even when all selected pages need OCR.",
    )
    return parser


def _emit(payload: dict) -> None:
    print(json.dumps(payload, ensure_ascii=False))


def run(arguments: Sequence[str] | None = None) -> int:
    parser = _build_parser()
    try:
        args = parser.parse_args(arguments)
        if not args.input.is_file():
            raise FileNotFoundError(f"Input PDF does not exist: {args.input}")
        page_numbers = parse_page_range(args.pages)
        extracted = extract_document(args.input, page_numbers)

        pages_with_text = sum(page.has_text_layer for page in extracted.pages)
        if pages_with_text == 0 and not args.allow_empty_text:
            _emit(
                {
                    "status": "needs_ocr",
                    "message": (
                        "Selected pages have no searchable text layer. "
                        "Run FamilyPDF OCR, then export again."
                    ),
                    "warnings": extracted.warnings,
                }
            )
            return EXIT_MISSING_TEXT_LAYER

        if args.format == "docx":
            export_report = write_docx(extracted, args.output)
        else:
            export_report = write_xlsx(extracted, args.output)

        _emit(
            {
                "status": "ok",
                "format": args.format,
                "output": str(args.output.resolve()),
                **asdict(export_report),
                "warnings": extracted.warnings,
            }
        )
        return 0
    except SystemExit:
        raise
    except Exception as error:
        _emit(
            {
                "status": "error",
                "message": str(error),
                "error_type": type(error).__name__,
            }
        )
        return EXIT_ERROR


def main() -> None:
    raise SystemExit(run())


if __name__ == "__main__":
    main()
