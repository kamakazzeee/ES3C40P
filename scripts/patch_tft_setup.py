from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
setup_source = project_dir / "include" / "User_Setup.h"
setup_target = project_dir / ".pio" / "libdeps" / env.subst("$PIOENV") / "TFT_eSPI" / "User_Setup.h"

if setup_target.parent.exists():
    setup_target.write_text(setup_source.read_text(encoding="ascii"), encoding="ascii")
