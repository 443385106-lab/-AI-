param([Parameter(Mandatory=$true)][string]$SvgPath,[Parameter(Mandatory=$true)][string]$CdrPath,[string]$ConvertCMYK="False")
$ErrorActionPreference='Stop'
try { $corel=New-Object -ComObject CorelDRAW.Application } catch { throw '未检测到可用的 CorelDRAW。请安装 X7—2020，并至少手动启动一次。' }
$corel.Visible=$false;$doc=$corel.CreateDocument()
function Convert-ShapesToCMYK($shapes){
  foreach($shape in @($shapes)){
    try { if($shape.Type -eq 7){Convert-ShapesToCMYK $shape.Shapes}else{if($shape.Fill.Type -ne 0){$shape.Fill.UniformColor.ConvertToCMYK()};if($shape.Outline.Type -ne 0){$shape.Outline.Color.ConvertToCMYK()}} } catch {}
  }
}
try {
  $null=$doc.ActiveLayer.Import($SvgPath)
  if($ConvertCMYK -eq "True"){try{$doc.Unit=3}catch{};Convert-ShapesToCMYK $doc.ActivePage.Shapes}
  $doc.SaveAs($CdrPath)
} finally {if($doc){$doc.Close()};if($corel){$corel.Quit()}}
