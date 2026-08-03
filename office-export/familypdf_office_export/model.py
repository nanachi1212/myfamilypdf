from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass(slots=True)
class TextRun:
    text: str
    bold: bool = False
    italic: bool = False
    font_size: float | None = None
    font_name: str | None = None


@dataclass(slots=True)
class TextBlock:
    runs: list[TextRun] = field(default_factory=list)
    column: int = 0
    column_span: int = 1
    top: float = 0.0
    left: float = 0.0

    @property
    def text(self) -> str:
        return "".join(run.text for run in self.runs)


@dataclass(slots=True)
class ExtractedImage:
    data: bytes
    top: float
    left: float
    width_points: float
    height_points: float
    column: int = 0
    column_span: int = 1


LayoutItem = TextBlock | ExtractedImage


@dataclass(slots=True, frozen=True)
class TableMerge:
    min_row: int
    min_column: int
    max_row: int
    max_column: int


@dataclass(slots=True)
class ExtractedTable:
    rows: list[list[Any]] = field(default_factory=list)
    merges: list[TableMerge] = field(default_factory=list)


@dataclass(slots=True)
class ExtractedPage:
    number: int
    blocks: list[TextBlock] = field(default_factory=list)
    images: list[ExtractedImage] = field(default_factory=list)
    layout_items: list[LayoutItem] = field(default_factory=list)
    tables: list[ExtractedTable] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    has_text_layer: bool = True
    column_count: int = 1
    column_width_ratios: list[float] = field(default_factory=lambda: [1.0])
    column_split_x: float | None = None


@dataclass(slots=True)
class ExtractedDocument:
    pages: list[ExtractedPage] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
