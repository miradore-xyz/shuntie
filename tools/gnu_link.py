import os,shlex,subprocess,sys
from pathlib import Path
arguments=[]
for arg in sys.argv[1:]:
    arguments.extend(shlex.split(Path(arg[1:].strip('"')).read_text(encoding='utf-8-sig'),posix=True) if arg.startswith('@') else [arg])
arguments=[arg for arg in arguments if arg not in ('-lgcc','-lgcc_eh')]
arguments.extend(['-Wl,-Bstatic','-lunwind','-Wl,-Bdynamic'])
raise SystemExit(subprocess.call([os.environ['SHUNTIE_GNU_LINKER'],*arguments]))
