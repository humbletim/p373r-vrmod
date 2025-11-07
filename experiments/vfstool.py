#!/usr/bin/env python3
import argparse
import json
import sys
import os
from pathlib import Path
import yaml

# Try to use LibYAML for speed if available, otherwise fallback to pure python
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    from yaml import Loader, Dumper

class VFSMerger:
    def __init__(self):
        self.master_meta = None
        self.merged_roots = []
        self.cwd = Path.cwd()

    def _normalize_roots(self, data, source_path):
        """
        Convert all paths in a single overlay data block to absolute paths 
        based on its specific overlay-relative setting.
        """
        is_relative = data.get('overlay-relative', False)
        # If true case-sensitive is string "true"/"false", sometimes boolean in standard YAML. Handle both.
        if isinstance(is_relative, str):
             is_relative = is_relative.lower() == 'true'

        base_dir = source_path.parent.absolute() if is_relative else self.cwd

        normalized_roots = []
        for entry in data.get('roots', []):
            new_entry = entry.copy()
            # Normalize 'name' (virtual path)
            # VFS usually treats 'name' as absolute or CWD-relative. 
            # We will internalize it as absolute for consistent merging.
            name_path = Path(new_entry['name'])
            if not name_path.is_absolute():
                 # If it was already relative, it's relative to CWD (standard LLVM) 
                 # OR the overlay if overlay-relative is true.
                 new_entry['_abs_name'] = (base_dir / name_path).absolute()
            else:
                 new_entry['_abs_name'] = name_path.absolute()

            # Normalize 'external-contents' (real path)
            if 'external-contents' in new_entry:
                ext_path = Path(new_entry['external-contents'])
                if not ext_path.is_absolute():
                    new_entry['_abs_external'] = (base_dir / ext_path).absolute()
                else:
                    new_entry['_abs_external'] = ext_path.absolute()
            
            normalized_roots.append(new_entry)
        return normalized_roots

    def add_file(self, path_str, force=False):
        path = Path(path_str).absolute()
        with open(path, 'r') as f:
            # Use safe_load to handle both JSON and YAML transparently
            data = yaml.safe_load(f)

        if self.master_meta is None:
            # First file dictates metadata
            self.master_meta = {
                'version': data.get('version', 0),
                'case-sensitive': data.get('case-sensitive', 'true'),
                'overlay-relative': data.get('overlay-relative', 'false')
            }
        else:
            # Validate metadata unless forced
            if not force:
                for key in ['version', 'case-sensitive']:
                    # standardizes on string comparison for robustness against "true" vs true
                    val_master = str(self.master_meta.get(key)).lower()
                    val_current = str(data.get(key, self.master_meta.get(key))).lower()
                    
                    if val_master != val_current:
                        print(f"ERROR: Metadata conflict in '{path.name}' for key '{key}'. "
                              f"Expected '{val_master}', got '{val_current}'. Use --force to ignore.", file=sys.stderr)
                        sys.exit(1)

        self.merged_roots.extend(self._normalize_roots(data, path))

    def _finalize_roots(self, output_path=None):
        """
        Re-relativize roots based on master metadata and intended output location.
        """
        final_roots = []
        
        # Determine the target base directory for relative paths
        is_overlay_relative = str(self.master_meta['overlay-relative']).lower() == 'true'
        
        if is_overlay_relative:
            if not output_path:
                 raise ValueError("Output path required for overlay-relative finalized generation.")
            target_base = output_path.parent.absolute()
        else:
            # User requested CWD-relative preferred over Absolute when not using overlay-relative
            target_base = self.cwd

        for entry in self.merged_roots:
            final_entry = {}
            for k, v in entry.items():
                if k.startswith('_abs_'): continue
                final_entry[k] = v

            # Re-calculate paths relative to the target base
            # We use os.path.relpath for robust cross-platform relative pathing
            # from one arbitrary directory to another.
            
            # Handle Name
            try:
                final_entry['name'] = os.path.relpath(entry['_abs_name'], target_base)
            except ValueError:
                # Fallback for different drives on Windows where relpath fails
                 final_entry['name'] = str(entry['_abs_name'])

            # Handle External Contents
            if '_abs_external' in entry:
                try:
                    final_entry['external-contents'] = os.path.relpath(entry['_abs_external'], target_base)
                except ValueError:
                    final_entry['external-contents'] = str(entry['_abs_external'])

            final_roots.append(final_entry)
            
        return final_roots

    def get_merged_data(self, output_path=None):
        data = self.master_meta.copy()
        data['roots'] = self._finalize_roots(output_path)
        return data

    def resolve_path(self, virtual_path):
        # For resolution, we test against the absolute internal paths 
        # to simulate what LLVM does after it loads the overlay.
        search_path = Path(virtual_path).absolute()
        
        # LLVM VFS iterates linearly and takes the first match.
        # Our merged_roots is already in prioritized order.
        
        # Standardizing case-sensitivity for lookup based on master meta
        case_sensitive = str(self.master_meta.get('case-sensitive', 'true')).lower() == 'true'

        for entry in self.merged_roots:
            entry_name = entry['_abs_name']
            
            match = False
            if case_sensitive:
                match = (search_path == entry_name)
            else:
                match = (str(search_path).lower() == str(entry_name).lower())

            if match:
                return entry.get('_abs_external')
                
        return None

def cmd_merge(args):
    merger = VFSMerger()
    for inp in args.inputs:
        merger.add_file(inp, force=args.force)

    output_path = Path(args.output).absolute()
    result_data = merger.get_merged_data(output_path=output_path)

    # Ensure output directory exists
    output_path.parent.mkdir(parents=True, exist_ok=True)

    is_yaml = output_path.suffix.lower() in ['.yaml', '.yml']
    
    with open(output_path, 'w') as f:
        if is_yaml:
            # Use a customized dump to avoid excessive quoting if desired, 
            # but standard safe_dump is usually fine.
            yaml.safe_dump(result_data, f, default_flow_style=False, sort_keys=False)
        else:
            json.dump(result_data, f, indent=2)
    
def cmd_resolve(args):
    merger = VFSMerger()
    for inp in args.inputs:
        merger.add_file(inp, force=args.force)

    result = merger.resolve_path(args.path)
    
    if result:
        print(f"FOUND: {result}")
        # Optionally print normalized relative path to CWD for easier reading:
        # print(f"Relative: {os.path.relpath(result, Path.cwd())}")
    else:
        print(f"NOT FOUND: {args.path}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="LLVM VFS Overlay merge and test utility.")
    subparsers = parser.add_subparsers(dest='command', required=True)

    # Merge command
    parser_merge = subparsers.add_parser('merge', help='Combine multiple VFS overlays.')
    parser_merge.add_argument('inputs', nargs='+', help='Input overlay files (priority highest to lowest).')
    parser_merge.add_argument('-o', '--output', required=True, help='Output file path (.json or .yaml).')
    parser_merge.add_argument('--force', action='store_true', help='Ignore metadata conflicts, use first file\'s metadata.')
    parser_merge.set_defaults(func=cmd_merge)

    # Resolve command
    parser_resolve = subparsers.add_parser('resolve', help='Test path resolution against merged overlays.')
    parser_resolve.add_argument('inputs', nargs='+', help='Input overlay files.')
    parser_resolve.add_argument('--path', required=True, help='Virtual path to resolve.')
    parser_resolve.add_argument('--force', action='store_true', help='Ignore metadata conflicts during load.')
    parser_resolve.set_defaults(func=cmd_resolve)

    args = parser.parse_args()
    args.func(args)

if __name__ == '__main__':
    main()