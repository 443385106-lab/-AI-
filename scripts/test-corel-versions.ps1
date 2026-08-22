$ErrorActionPreference='Continue'
$targets=@(@{Name='CorelDRAW X7';ProgID='CorelDRAW.Application.17'},@{Name='CorelDRAW 2018';ProgID='CorelDRAW.Application.20'},@{Name='CorelDRAW 2019';ProgID='CorelDRAW.Application.21'},@{Name='CorelDRAW 2020';ProgID='CorelDRAW.Application.22'})
$results=@()
foreach($t in $targets){$app=$null;$doc=$null;try{$app=New-Object -ComObject $t.ProgID;$app.Visible=$false;$doc=$app.CreateDocument();$tmp=Join-Path $env:TEMP ("corel-test-"+[guid]::NewGuid().ToString()+".cdr");$doc.SaveAs($tmp);$ok=Test-Path $tmp;if($ok){Remove-Item $tmp -Force};$results+=[pscustomobject]@{Version=$t.Name;Installed=$true;CreateAndSave=$ok;Message='通过'}}catch{$results+=[pscustomobject]@{Version=$t.Name;Installed=$false;CreateAndSave=$false;Message=$_.Exception.Message}}finally{if($doc){$doc.Close()};if($app){$app.Quit()}}}
$results|ConvertTo-Json -Compress
