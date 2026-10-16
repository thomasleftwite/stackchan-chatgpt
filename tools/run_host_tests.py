"""Compile and run the actual portable C++ rules. No device or API is used."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / ".pio" / "host-tests"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--suite', choices=['rules', 'gemini-ui'], default='rules')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests" / ("gemini_ui_test.cpp" if args.suite == 'gemini-ui' else "conversation_rules_test.cpp")
    binary = OUT / (source.stem + (".exe" if os.name == "nt" else ""))
    includes = [ROOT / 'include']
    if args.suite == 'gemini-ui': includes.append(ROOT / 'tests/firmware_ui_stubs')
    compiler = next((shutil.which(c) for c in ("c++", "g++", "clang++") if shutil.which(c)), None)
    env = os.environ.copy()
    if compiler:
        command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                   *[flag for folder in includes for flag in ('-I', str(folder))], str(source), "-o", str(binary)]
    else:
        compiler = shutil.which("cl")
        if not compiler:
            vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                raise SystemExit("Install a C++ compiler or run in a Visual Studio developer terminal.")
            install = subprocess.check_output([str(vswhere), "-latest", "-products", "*",
                                               "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                               "-property", "installationPath"], text=True).strip()
            if not install:
                raise SystemExit("Visual Studio C++ tools were not found.")
            setup = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            # Capture the environment internally; never print environment variables.
            output = subprocess.check_output(
                f'cmd.exe /d /s /c ""{setup}" >nul && set"', text=True)
            for line in output.splitlines():
                name, separator, value = line.partition("=")
                if separator and name:
                    env[name] = value
            compiler = shutil.which("cl", path=env.get("Path", env.get("PATH", "")))
        if not compiler:
            raise SystemExit("MSVC compiler was not found.")
        command = [compiler, "/nologo", "/EHsc", "/std:c++17", "/W4", "/WX", "/utf-8",
                   *['/I' + str(folder) for folder in includes], str(source), "/Fe:" + str(binary),
                   "/Fo:" + str(OUT / (source.stem + ".obj"))]
    subprocess.run(command, cwd=OUT, env=env, check=True)
    subprocess.run([str(binary)], cwd=OUT, check=True)


if __name__ == "__main__":
    main()
