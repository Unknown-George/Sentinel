#!/usr/bin/env python3
"""Reject a deployed macOS bundle that still loads build-machine libraries."""
from pathlib import Path
from functools import lru_cache
import os
import re
import subprocess
import sys


MACH_MAGICS = {
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca",
}


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


@lru_cache(maxsize=None)
def load_commands(binary):
    return run("/usr/bin/otool", "-l", str(binary))


def rpaths(binary):
    lines = load_commands(binary).splitlines()
    result = []
    for index, line in enumerate(lines):
        if line.strip() != "cmd LC_RPATH":
            continue
        for following in lines[index + 1:index + 5]:
            match = re.match(r"\s*path (.*?) \(offset \d+\)", following)
            if match:
                result.append(match.group(1))
                break
    return result


def expand(value, binary, executable):
    value = value.replace("@loader_path", str(binary.parent))
    value = value.replace("@executable_path", str(executable.parent))
    return Path(value).resolve()


def is_system(value):
    return value.startswith("/System/Library/") or value.startswith("/usr/lib/")


def version_tuple(value):
    if not re.fullmatch(r"\d+(?:\.\d+){0,2}", value):
        sys.exit(f"Invalid deployment version: {value}")
    return tuple((list(map(int, value.split("."))) + [0, 0])[:3])


def deployment_versions(binary):
    commands = load_commands(binary)
    modern = re.findall(r"cmd LC_BUILD_VERSION\s+cmdsize \d+\s+platform \d+\s+minos ([\d.]+)", commands)
    legacy = re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version ([\d.]+)", commands)
    return modern + legacy


def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: verify_bundle.py /path/to/Sentinel.app")
    app = Path(sys.argv[1]).resolve()
    executable = app / "Contents/MacOS/Sentinel"
    if not executable.is_file():
        sys.exit(f"Missing executable: {executable}")
    inherited_paths = rpaths(executable)
    maximum_version = os.environ.get("SENTINEL_MAX_DEPLOYMENT_TARGET", "14.0")
    maximum_tuple = version_tuple(maximum_version)
    errors = []
    binaries = set()
    for directory, _, files in os.walk(app):
        for name in files:
            candidate = Path(directory, name)
            try:
                with candidate.open("rb") as source:
                    if source.read(4) in MACH_MAGICS:
                        binaries.add(candidate.resolve())
            except OSError as error:
                errors.append(f"Cannot inspect {candidate}: {error}")
    for binary in sorted(binaries):
        architecture = subprocess.run(["/usr/bin/lipo", str(binary), "-verify_arch", "arm64"],
                                      capture_output=True, text=True)
        if architecture.returncode:
            errors.append(f"{binary.relative_to(app)}: missing ARM64 architecture")
        versions = deployment_versions(binary)
        if not versions:
            errors.append(f"{binary.relative_to(app)}: no macOS deployment minimum found")
        for version in versions:
            if version_tuple(version) > maximum_tuple:
                errors.append(f"{binary.relative_to(app)}: minimum macOS {version} exceeds {maximum_version}")
        search_paths = [(binary, path) for path in rpaths(binary)]
        search_paths += [(executable, path) for path in inherited_paths]
        for entry in run("/usr/bin/otool", "-L", str(binary)).splitlines()[1:]:
            # A universal binary includes per-architecture heading lines.
            if not entry.startswith(("\t", " ")):
                continue
            dependency = entry.strip().split(" (compatibility version", 1)[0]
            if is_system(dependency):
                continue
            if dependency.startswith("@rpath/"):
                suffix = dependency[len("@rpath/"):]
                candidates = [expand(path, origin, executable) / suffix for origin, path in search_paths]
            elif dependency.startswith(("@loader_path/", "@executable_path/")):
                candidates = [expand(dependency, binary, executable)]
            else:
                candidates = [Path(dependency)]
            resolved = [path.resolve() for path in candidates if path.is_file()]
            if not any(app in path.parents for path in resolved):
                errors.append(f"{binary.relative_to(app)}: external/unresolved dependency {dependency}")
        for path in rpaths(binary):
            expanded = expand(path, binary, executable)
            if "@rpath" not in path and not is_system(str(expanded) + "/") and expanded != app and app not in expanded.parents:
                errors.append(f"{binary.relative_to(app)}: external runtime search path {path}")
    if errors:
        sys.exit("Bundle dependency verification failed:\n" + "\n".join(errors))
    print(f"Verified {len(binaries)} ARM64 Mach-O files: self-contained dependencies and minimum macOS <= {maximum_version}.")


if __name__ == "__main__":
    main()
