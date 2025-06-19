#!/usr/bin/env python3
"""
DLL VFTable Parser - Extracts virtual function table information from DLL files
and generates C++ function declarations.

Requirements: pip install pefile
"""

import pefile
import struct
import sys
import re
import argparse
from typing import Literal
import pydemangler


FUNC_REGEX = re.compile(
    r"(?:(?P<visibility>public\:|protected\:|private\:)\s+)?"
    r"(?:(?P<virtual>virtual)\s+)?"
    r"(?P<ret>(?:const )*.+?)\s+"
    r"(?:(?P<decl>__thiscall|__cdecl|__stdcall|__fastcall|__clrcall)\s+)?"
    r"(?:(?P<ns>\S*)\:\:)?"
    r"(?P<clazz>(?:\S*)(?:(?:<.*?>)*))\:\:"
    r"(?P<name>(?:\S*?)(?:<.*?>)*)"
    r"\((?P<params>(?:(?:.*?(?:<.*?>)?)(?:,)?)*)\)"
    r"(?:\s+(?P<const>const))?"
)

class FuncDefinition:
    visibility: Literal["public:", "protected:", "private:"]
    virtual: bool
    ret: str
    decl: Literal["__thiscall", "__cdecl", "__stdcall", "__fastcall", "__clrcall"] | None
    ns: str | None
    clazz: str | None
    name: str
    params: list[str]
    const: bool
    def __init__(
        self,
        ret: str,
        name: str,
        params: str | list[str] | None = None,
        visibility: Literal["public:", "protected:", "private:"] | None = None,
        virtual: bool | str | None = True,
        decl: Literal["__thiscall", "__cdecl", "__stdcall", "__fastcall", "__clrcall"] | None = None,
        ns: str | None = None,
        clazz: str | None = None,
        const: bool | str | None = False,
    ):
        self.ret = ret
        self.name = name
        if params is None or params == "void":
            params = []
        if isinstance(params, str):
            params = [p.strip() for p in params.split(",")]
        self.params = params
        self.visibility = visibility or "public:"
        self.virtual = virtual if isinstance(virtual, bool) else (virtual is not None)
        self.decl = decl
        self.ns = ns
        self.clazz = clazz
        self.const = const if isinstance(const, bool) else (const is not None)

class Address:
    def __init__(self, pe: pefile.PE, rva: int):
        self._pe: pefile.PE = pe
        
        if rva < 0:
            raise ValueError(f"rva value '{hex(rva)}' was negative!")
        if rva > self._pe.OPTIONAL_HEADER.SizeOfImage:
            raise ValueError(f"rva value '{hex(rva)}' was greater than the image size '{hex(self._pe.OPTIONAL_HEADER.SizeOfImage)}'!")

        self._rva: int = rva
    
    @classmethod
    def from_va(cls, pe: pefile.PE, va: int) -> 'Address':
        return cls(pe, va - pe.OPTIONAL_HEADER.ImageBase)

    
    @property
    def va(self) -> int:
        return self._rva + self._pe.OPTIONAL_HEADER.ImageBase
    
    @property
    def rva(self) -> int:
        return self._rva
    
    def __eq__(self, value: object) -> bool:
        if not isinstance(value, Address):
            return False
        return self.va == value.va
    
    def __hash__(self) -> int:
        return hash(self.va)
    
    def __str__(self) -> str:
        return f"{hex(self.va)} ({hex(self.rva)})"
    
    def __repr__(self) -> str:
        return f"<Address {self})>"


class VFTableParser:
    def __init__(self, dll_path: str):
        self.pe = pefile.PE(dll_path)
        self.exports: dict[Address, str] = {}
        self.imports: dict[Address, str] = {}
        self.thunks: dict[Address, Address] = {}
        self._parse_exports()
        self._parse_imports()
    
    def _parse_exports(self):
        """Parse the export table to build a mapping of addresses to function names."""
        if not hasattr(self.pe, 'DIRECTORY_ENTRY_EXPORT'):
            print("No export table found in DLL")
            return
        
        export_dir = self.pe.DIRECTORY_ENTRY_EXPORT
        
        for export in export_dir.symbols:
            if export.address is not None:
                try:
                    name = export.name.decode('utf-8') if export.name else f"Ordinal_{export.ordinal}"
                    self.exports[Address(self.pe, rva=export.address)] = name
                except:
                    # Skip exports we can't resolve
                    continue
    
    def _parse_imports(self):
        """Parse the import table to build a mapping of addresses to function names."""
        if not hasattr(self.pe, 'DIRECTORY_ENTRY_IMPORT'):
            print("No import table found in DLL")
            return
        
        import_dirs = self.pe.DIRECTORY_ENTRY_IMPORT
        
        for imp_file in import_dirs:
            for imp in imp_file.imports:
                if imp.address is not None:
                    try:
                        name = imp.name.decode('utf-8') if imp.name else f"Ordinal_{imp.ordinal}"
                        self.imports[Address.from_va(self.pe, va=imp.address)] = name
                    except:
                        # Skip imports we can't resolve
                        continue
    
    def _check_addr_for_thunks(self, addr: Address) -> Address | None:
        """Scan a code section for thunk patterns."""
        if addr in self.thunks:
            return self.thunks[addr]
        
        addr_data = self.pe.get_data(addr.rva, 6)
        
        target_addr = None
        # Look for JMP [address] patterns (FF 25 xx xx xx xx)
        if addr_data[:2] == b'\xFF\x25':  # JMP [mem32]
            # Extract the target address
            jmp_dest: int = struct.unpack('<I', addr_data[2:6])[0]
            target_addr = Address.from_va(self.pe, jmp_dest)
        # Look for JMP rel32 patterns (E9 xx xx xx xx)
        elif addr_data[0] == 0xE9:
            rel_offset: int = struct.unpack('<i', addr_data[1:5])[0]
            target_addr = Address(self.pe, addr.rva + 5 + rel_offset)  # +5 for instruction length
        
        if target_addr:
            self.thunks[addr] = target_addr
        
        return target_addr
    
    def find_vftable(self, class_name: str) -> Address | None:
        """Find the vftable for the given class name."""
        vftable_symbol = f"??_7{class_name}@@6B@"
        
        # Look for exact match first
        for va, export_name in self.exports.items():
            if export_name == vftable_symbol:
                return va
        
        return None
    
    def read_vftable_entries(self, vftable_addr: Address, max_entries: int = 100) -> list[Address]:
        """Read function pointers from the vftable."""
        entries: list[Address] = []
        
        # Read the vftable entries (assuming 32-bit pointers for now)
        # Adjust for 64-bit if needed
        is_64bit = self.pe.FILE_HEADER.Machine == 0x8664
        ptr_size = 8 if is_64bit else 4
        ptr_format = 'Q' if is_64bit else 'L'
        
        for i in range(max_entries):
            entry_addr = Address(self.pe, vftable_addr.rva + (i * ptr_size))
            # If the address is in the exports, we've probably hit the end of this vftable
            if i != 0 and entry_addr in self.exports:
                break
            
            # Read pointer at current position
            data = self.pe.get_data(entry_addr.rva, ptr_size)
            ptr: int = struct.unpack('<' + ptr_format, data)[0]
            
            # Check if this looks like a valid code pointer
            if ptr == 0:
                break
            
            # Convert to VA if it's a RVA
            if ptr < self.pe.OPTIONAL_HEADER.ImageBase:
                ptr += self.pe.OPTIONAL_HEADER.ImageBase
            
            # Verify the RVA is within the image
            if (ptr - self.pe.OPTIONAL_HEADER.ImageBase) >= self.pe.OPTIONAL_HEADER.SizeOfImage:
                break
            
            entries.append(Address.from_va(self.pe, ptr))
        
        return entries

    def analyze_function_signature(self, func_addr: Address) -> FuncDefinition:
        """Analyze function at VA to determine return type and parameters.
        This is a simplified heuristic-based approach."""

        if thunk_target := self._check_addr_for_thunks(func_addr):
            func_addr = thunk_target
        
        # Check if we have export information for this function
        if func_name := (self.exports | self.imports).get(func_addr):
            if func_name == "_purecall":
                return FuncDefinition("void", f"Function_{func_addr.va:08X}_purecall")
            demangled = pydemangler.demangle(func_name)
            match = FUNC_REGEX.match(demangled)
            if match:
                groups = match.groupdict()
                return FuncDefinition(**groups)  # type: ignore
            else:
                print(f"Couldn't parse function def at 0x{func_addr.va:08X}: {demangled}, from {func_name}")

        return FuncDefinition("void", f"Function_{func_addr.va:08X}")

    def generate_definitions(self, class_name: str) -> list[FuncDefinition]:
        """Generate C++ virtual function declarations for the given class."""
        vftable_addr = self.find_vftable(class_name)
        
        if not vftable_addr:
            print(f"Could not find vftable for class '{class_name}'")
            return []
        
        print(f"Found vftable for {class_name} at: {vftable_addr}")
        
        # Read vftable entries
        try:
            entries = self.read_vftable_entries(vftable_addr)
        except Exception as e:
            print(f"Error reading vftable entries: {e}")
            return []
        
        if not entries:
            print("No vftable entries found")
            return []
        
        print(f"Found {len(entries)} vftable entries")
        
        definitions: list[FuncDefinition] = []
        
        for i, func_addr in enumerate(entries):
            try:
                func_def = self.analyze_function_signature(func_addr)
                
                # Format parameters
                param_str = ", ".join(func_def.params)
                
                # Generate declaration
                declaration = f"\t{func_def.ret} {func_def.name}({param_str});"
                definitions.append(func_def)
                
                print(f"  [{i:2d}]: {func_addr} -> {declaration}")
                
            except Exception as e:
                print(f"  [{i:2d}]: {func_addr} -> Error: {e}")
                raise RuntimeError(f"Error analyzing function at {func_addr}") from e
        
        return definitions

def main(dll_file: str, class_name: str):
    try:
        parser = VFTableParser(dll_file)
    except FileNotFoundError as e:
        raise RuntimeError(f"Error: Could not find DLL file '{dll_file}'") from e
    except pefile.PEFormatError as e:
        raise RuntimeError(f"Error: '{dll_file}' is not a valid PE file") from e
    
    definitions = parser.generate_definitions(class_name)
    
    if definitions:
        print(f"\nGenerated C++ declarations for {class_name}:")
        print(f"// Virtual function table for {class_name}")
        last_vis = ""
        for d in definitions:
            decl = ""
            if last_vis != d.visibility:
                print(d.visibility)
                last_vis = d.visibility
            if d.virtual:
                decl += "virtual "
            decl += f"{d.ret} "
            if d.decl:
                decl += f"{d.decl} "
            if d.ns:
                decl += f"{d.ns}::"
            if d.clazz:
                decl += f"{d.clazz}::"
            decl += f"{d.name}({' '.join(d.params)})"
            if d.const:
                decl += f" const"
            print(f"    {decl};")
        print("\n")
        print(f"// Virtual function table struct definition for {class_name}")
        print(f"struct {class_name}_vftable {{")
        for d in definitions:
            this_param = f"class {d.clazz or class_name}* `this`"
            param_str = ", ".join([this_param] + d.params)
            print(f"    {d.ret} ({d.decl or '__thiscall'} * {d.name})({param_str});")
        print("};")
    else:
        print("No definitions generated")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.description = "Scans for the given class's vftable and prints out C++ definitions and a struct definition of the vftable"
    parser.add_argument("dll_file", help="Path to the dll file to scan")
    parser.add_argument("class_name", help="Name of the class to scan")
    args = parser.parse_args()
    main(args.dll_file, args.class_name)