from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse, os, shutil, subprocess, tempfile, time

parser=argparse.ArgumentParser(description="Exercise concurrent same-cookie sign-ons without fence or message commands.")
parser.add_argument("--home-base", type=Path, required=True)
parser.add_argument("--account", type=int, default=1)
parser.add_argument("--workers", type=int, default=10)
parser.add_argument("--rounds", type=int, default=10)
parser.add_argument("--cli", default="/home/devops/repos/1.ufsrv/ufsrvcli/build/dev/src/ufsrvcli")
args=parser.parse_args()
if min(args.account,args.workers,args.rounds)<1: parser.error("account, workers and rounds must be positive")
root=Path(tempfile.mkdtemp(prefix='same_cookie_repro_',dir='/tmp'))
root.chmod(0o700)
cli=args.cli
source=args.home_base/str(args.account)
for worker in range(args.workers):
 home=root/str(worker)/str(args.account); home.mkdir(parents=True,mode=0o700);home.parent.chmod(0o700)
 for filename in ('ufsrvcli.config','ufsrvcli.prefs'):
  shutil.copyfile(source/filename,home/filename);(home/filename).chmod(0o600)
def invoke(operation):
 round,worker=operation
 with (root/f'round_{round}_worker_{worker}.log').open('w') as log:
  try:
   result=subprocess.run([cli,'--headless','--home-base',str(root/str(worker)/str(args.account)),'--'],stdout=log,stderr=subprocess.STDOUT,timeout=5,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
   return result.returncode
  except subprocess.TimeoutExpired:return 124
print('Evidence:',root,flush=True)
for round in range(1,args.rounds+1):
 with ThreadPoolExecutor(max_workers=args.workers) as pool: results=list(pool.map(invoke,[(round,worker) for worker in range(args.workers)]))
 print('round',round,'exit codes',results,flush=True)
 if any(code not in (0,1) for code in results): raise SystemExit('FAIL: crashed, timed-out or unexpected CLI exit')
 if 0 not in results: raise SystemExit('FAIL: no accepted sign-on in this round; rejection exits alone cannot prove server availability')
 time.sleep(.15)
print("PASS: bounded sign-on responses across",args.rounds*args.workers,"attempts; no domain commands submitted")
