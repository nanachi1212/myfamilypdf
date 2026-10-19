from pathlib import Path
import subprocess, os, json

import tempfile
_temporary = tempfile.TemporaryDirectory(prefix='familypdf-ocr-security-')
root=Path(_temporary.name)
source=Path(__file__).resolve().parents[2] / 'scripts/ocr/FamilyPDF-OCR.cmd'
launcher=root/'FamilyPDF-OCR.cmd'
launcher.write_bytes(source.read_bytes())
stub=root/'FamilyPDF-OCR.ps1'
stub.write_text('''param([string]$InputFile,[string]$OutputFile,[string]$Mode="Auto",[string]$Pages)
$PSBoundParameters | ConvertTo-Json -Compress | Set-Content -LiteralPath $env:FAMILYPDF_TEST_RESULT -Encoding UTF8
if ($InputFile -eq 'fail.pdf') {exit 7}
exit 0
''',encoding='utf-8-sig')
cases=[['input&ver&.pdf'],['中文 空白!&%.pdf','輸出 空白!.pdf','-Mode','Simplified','-Pages','1-10'],['input.pdf','output.pdf','-Mode','Auto'],['fail.pdf']]
for i,args in enumerate(cases):
    result=root/f'case-{i}.json'
    env=dict(os.environ,FAMILYPDF_TEST_RESULT=str(result),FAMILYPDF_TEST_ARGS=subprocess.list2cmdline(args))
    command=f'{os.environ["ComSpec"]} /d /v:on /c ""{launcher}" !FAMILYPDF_TEST_ARGS!"'
    completed=subprocess.run(command,env=env,cwd=root,capture_output=True,timeout=20)
    assert completed.returncode==(7 if args[0]=='fail.pdf' else 0),(args,completed.returncode,completed.stdout,completed.stderr)
    actual=json.loads(result.read_text(encoding='utf-8-sig'))
    assert actual['InputFile']==args[0],(actual,args)
    if len(args)>1: assert actual['OutputFile']==args[1],(actual,args)
    if '-Mode' in args: assert actual['Mode']==args[args.index('-Mode')+1],(actual,args)
    if '-Pages' in args: assert actual['Pages']==args[args.index('-Pages')+1],(actual,args)
    assert not completed.stdout,(args,completed.stdout)
    print(f'PASS OCR case {i+1}: arguments and exit code preserved')
