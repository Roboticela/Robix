#!/usr/bin/env python3
"""Assemble UI/Rxu.rx from the Robix.UI source files."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "UI"
ORDER = [
    "RxuSyntax.rx",
    "RxuDocument.rx",
    "RxuElement.rx",
    "RxuResources.rx",
    "RxuBinding.rx",
    "RxuBackend.rx",
    "RxuLinux.rx",
    "RxuLayout.rx",
    "RxuMvp.rx",
    "RxuEvents.rx",
    "RxuTree.rx",
    "RxuFlow.rx",
    "RxuKit.rx",
    "RxuCatalog.rx",
    "RxuFrame.rx",
    "RxuNav.rx",
    "RxuProgram.rx",
]


def body(path: Path) -> str:
    lines = path.read_text().splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.startswith(("Public Class ", "Private Function ", "Public Function ")):
            text = "".join(lines[index:])
            if not text.endswith("\n"):
                text += "\n"
            return text + "\n"
    return ""


def main() -> None:
    current = (ROOT / "Rxu.rx").read_text().splitlines(keepends=True)
    cut = 0
    for index, line in enumerate(current):
        if line.startswith("Public Class "):
            cut = index
            break
    preamble = "".join(current[:cut])
    (ROOT / "Rxu.rx").write_text(preamble + "".join(body(ROOT / name) for name in ORDER))


if __name__ == "__main__":
    main()
