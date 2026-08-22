param([Parameter(Mandatory=$true)][string]$ManifestPath,[string]$ConvertCMYK="False")
$ErrorActionPreference='Stop';$items=Get-Content -Raw -Encoding UTF8 $ManifestPath|ConvertFrom-Json
try{$corel=New-Object -ComObject CorelDRAW.Application}catch{throw '未检测到可用的 CorelDRAW。'};$corel.Visible=$false
function Convert-ShapesToCMYK($shapes){foreach($shape in @($shapes)){try{if($shape.Type -eq 7){Convert-ShapesToCMYK $shape.Shapes}else{if($shape.Fill.Type -ne 0){$shape.Fill.UniformColor.ConvertToCMYK()};if($shape.Outline.Type -ne 0){$shape.Outline.Color.ConvertToCMYK()}}}catch{}}}
try{foreach($item in $items){$doc=$corel.CreateDocument();try{$null=$doc.ActiveLayer.Import($item.svgPath);if($ConvertCMYK -eq "True"){Convert-ShapesToCMYK $doc.ActivePage.Shapes};$doc.SaveAs($item.cdrPath)}finally{if($doc){$doc.Close();$doc=$null}}}}finally{if($corel){$corel.Quit()}}
