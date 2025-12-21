import lit.formats
from pathlib import Path

if 'config' not in globals():
    config = object()

config.name = 'ParaCL'
config.test_format = lit.formats.ShTest(True)

config.suffixes = ['.pcl']
config.excludes = ['CMakeLists.txt', 'scripts', 'build', 'crashes', 'gen']

current_dir = Path(__file__).resolve().parent
project_root = current_dir.parent
build_dir = project_root / 'build'

config.test_source_root = str(current_dir)
config.test_exec_root = str(build_dir / 'tests')

compiler_path = build_dir / 'src' / 'paracl'

if not compiler_path.exists():
    print(f"WARNING: Compiler not found at {compiler_path}")

config.substitutions.append(('%paracl', str(compiler_path)))
