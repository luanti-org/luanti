#!/usr/bin/env python3
# Luanti
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 The Luanti Contributors
"""Print the memory a WebAssembly binary imports.

An imported memory must be provided with `initial >= declared minimum` and
`maximum <= declared maximum`. luanti.wasm declares the heap it was linked for
(-sINITIAL_MEMORY / -sMAXIMUM_MEMORY), and client/web/startup.js relies on
that rule to start the same binary on a smaller ceiling when the browser will
not reserve the full one (wasm_porting.md, "Heap ceiling probe"). This reads
the declaration out of the binary itself instead of trusting the flags::

	python3 util/wasm/memory_import.py build-wasm/bin/luanti.wasm
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

PAGE = 65536
IMPORT_SECTION = 2
KIND_FUNCTION, KIND_TABLE, KIND_MEMORY, KIND_GLOBAL, KIND_TAG = range(5)


class WasmError(Exception):
	pass


def _leb(data: bytes, position: int) -> tuple[int, int]:
	value = shift = 0
	while True:
		if position >= len(data):
			raise WasmError("truncated LEB128")
		byte = data[position]
		position += 1
		value |= (byte & 0x7F) << shift
		shift += 7
		if not byte & 0x80:
			return value, position


def _name(data: bytes, position: int) -> tuple[str, int]:
	length, position = _leb(data, position)
	return data[position:position + length].decode("utf-8"), position + length


def memory_imports(data: bytes) -> list[dict]:
	"""Return every imported memory as {module, name, min, max, shared} (pages)."""
	if data[:8] != b"\0asm\x01\0\0\0":
		raise WasmError("not a WebAssembly binary")
	position = 8
	found: list[dict] = []
	while position < len(data):
		section = data[position]
		size, position = _leb(data, position + 1)
		end = position + size
		if section == IMPORT_SECTION:
			count, cursor = _leb(data, position)
			for _ in range(count):
				module, cursor = _name(data, cursor)
				name, cursor = _name(data, cursor)
				kind = data[cursor]
				cursor += 1
				if kind in (KIND_FUNCTION, KIND_TAG):
					if kind == KIND_TAG:
						cursor += 1
					_, cursor = _leb(data, cursor)
				elif kind == KIND_TABLE:
					cursor += 1  # element type
					flags = data[cursor]
					cursor += 1
					_, cursor = _leb(data, cursor)
					if flags & 1:
						_, cursor = _leb(data, cursor)
				elif kind == KIND_MEMORY:
					flags = data[cursor]
					cursor += 1
					minimum, cursor = _leb(data, cursor)
					maximum = None
					if flags & 1:
						maximum, cursor = _leb(data, cursor)
					found.append({"module": module, "name": name, "min": minimum,
						"max": maximum, "shared": bool(flags & 2), "memory64": bool(flags & 4)})
				elif kind == KIND_GLOBAL:
					cursor += 2  # value type, mutability
				else:
					raise WasmError(f"unknown import kind {kind}")
			return found
		position = end
	return found


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__,
		formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("wasm", help="a .wasm file")
	args = parser.parse_args()
	try:
		memories = memory_imports(Path(args.wasm).read_bytes())
	except (WasmError, OSError) as error:
		print(f"memory_import: {error}", file=sys.stderr)
		return 1
	if not memories:
		print("no imported memory (the binary creates its own)")
	for memory in memories:
		maximum = "unbounded" if memory["max"] is None else \
			f"{memory['max'] * PAGE:d} bytes ({memory['max'] * PAGE / 1048576:.0f} MiB, {memory['max']} pages)"
		print(f"{memory['module']}.{memory['name']}: {'shared ' if memory['shared'] else ''}memory, "
			f"minimum {memory['min'] * PAGE} bytes ({memory['min'] * PAGE // 1048576} MiB, "
			f"{memory['min']} pages), maximum {maximum}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
