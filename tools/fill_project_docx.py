from pathlib import Path
import importlib.util


ROOT = Path(r"D:\table\jiuyuan")
PROJECT_DOCX = ROOT / "项目.docx"
OUTPUT_DOCX = ROOT / "项目-已填充.docx"
GENERATOR = ROOT / "tools" / "generate_starflash_report.py"


def main():
    spec = importlib.util.spec_from_file_location("starflash_report_generator", GENERATOR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    module.TEMPLATE = PROJECT_DOCX
    module.OUT = OUTPUT_DOCX
    out = module.build_report()
    print(out)


if __name__ == "__main__":
    main()
