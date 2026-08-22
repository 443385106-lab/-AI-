param(
  [Parameter(Mandatory=$true)][ValidateSet('Detect','Import','ConvertCurves','ConvertCMYK','Preflight','SaveCDR','PublishPDF')][string]$Action,
  [string]$InputPath='',
  [string]$OutputPath='',
  [double]$PageWidthCM=0,
  [double]$PageHeightCM=0,
  [int]$Dpi=300,
  [double]$BleedMM=3,
  [string]$CropMarks='True'
)

$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.Encoding]::UTF8

function Write-Result([hashtable]$Data){
  $Data.action=$Action
  $Data.version='CorelDRAW 2020'
  $Data.architecture='64-Bit'
  $Data|ConvertTo-Json -Depth 6 -Compress
}

function Get-Corel2020 {
  try {$instance=New-Object -ComObject CorelDRAW.Application.22}
  catch {throw '未检测到 CorelDRAW 2020（64-Bit）。请确认已安装完整桌面版，并至少手动启动一次。'}
  if([int]$instance.VersionMajor -ne 22){throw "连接到的不是 CorelDRAW 2020（检测到主版本 $($instance.VersionMajor)）。"}
  return $instance
}

function Get-ActiveDocument($App){
  if([int]$App.Documents.Count -lt 1){throw 'CorelDRAW 2020 中没有打开的文档。请先点击“送入 CorelDRAW 编辑”。'}
  return $App.ActiveDocument
}

function Visit-Shapes($Shapes,[scriptblock]$Visitor){
  for($i=[int]$Shapes.Count;$i -ge 1;$i--){
    $shape=$Shapes.Item($i)
    & $Visitor $shape
    if([int]$shape.Type -eq 7){Visit-Shapes $shape.Shapes $Visitor}
    try {if($shape.PowerClip -and [int]$shape.PowerClip.Shapes.Count -gt 0){Visit-Shapes $shape.PowerClip.Shapes $Visitor}} catch {}
  }
}

function Visit-Document($Document,[scriptblock]$Visitor){
  for($p=1;$p -le [int]$Document.Pages.Count;$p++){Visit-Shapes $Document.Pages.Item($p).Shapes $Visitor}
}

function Convert-DocumentColors($Document){
  $script:convertedColors=0
  Visit-Document $Document {
    param($shape)
    if([int]$shape.Type -eq 7){return}
    try {
      if([int]$shape.Fill.Type -eq 1 -and -not [bool]$shape.Fill.UniformColor.IsCMYK){$shape.Fill.UniformColor.ConvertToCMYK();$script:convertedColors++}
    } catch {}
    try {
      if([int]$shape.Outline.Type -ne 0 -and -not [bool]$shape.Outline.Color.IsCMYK){$shape.Outline.Color.ConvertToCMYK();$script:convertedColors++}
    } catch {}
  }
  return $script:convertedColors
}

function Get-Preflight($Document){
  $stats=[ordered]@{shapes=0;text=0;bitmaps=0;lowResolutionBitmaps=0;minimumBitmapDpi=$null;nonCMYKColors=0;complexFills=0;pages=[int]$Document.Pages.Count;documentDpi=[int]$Document.Resolution;warnings=@()}
  Visit-Document $Document {
    param($shape)
    $stats.shapes++
    if([int]$shape.Type -eq 6){$stats.text++}
    if([int]$shape.Type -eq 5){
      $stats.bitmaps++
      try {
        $bitmapDpi=[Math]::Min([int]$shape.Bitmap.ResolutionX,[int]$shape.Bitmap.ResolutionY)
        if($null -eq $stats.minimumBitmapDpi -or $bitmapDpi -lt $stats.minimumBitmapDpi){$stats.minimumBitmapDpi=$bitmapDpi}
        if($bitmapDpi -lt 300){$stats.lowResolutionBitmaps++}
      } catch {}
    }
    if([int]$shape.Type -ne 7){
      try {if([int]$shape.Fill.Type -eq 1 -and -not [bool]$shape.Fill.UniformColor.IsCMYK){$stats.nonCMYKColors++}elseif([int]$shape.Fill.Type -gt 1){$stats.complexFills++}} catch {}
      try {if([int]$shape.Outline.Type -ne 0 -and -not [bool]$shape.Outline.Color.IsCMYK){$stats.nonCMYKColors++}} catch {}
    }
  }
  if($stats.text -gt 0){$stats.warnings+=("仍有 {0} 个文字对象，交付前建议转曲或确认字体嵌入。" -f $stats.text)}
  if($stats.nonCMYKColors -gt 0){$stats.warnings+=("发现 {0} 处非 CMYK 均匀色/轮廓色。" -f $stats.nonCMYKColors)}
  if($stats.lowResolutionBitmaps -gt 0){$stats.warnings+=("发现 {0} 张低于 300dpi 的位图；放大不会产生真实细节。" -f $stats.lowResolutionBitmaps)}
  if($stats.complexFills -gt 0){$stats.warnings+=("发现 {0} 个渐变/图样等复杂填充，请在分色预览中复核。" -f $stats.complexFills)}
  if($stats.documentDpi -lt 300){$stats.warnings+=('文档分辨率低于 300dpi。')}
  if($stats.warnings.Count -eq 0){$stats.warnings+=('未发现文字、均匀色和位图分辨率问题；仍需按印厂色彩配置进行最终分色复核。')}
  return $stats
}

$corel=Get-Corel2020
$corel.Visible=$true

switch($Action){
  'Detect' {
    Write-Result @{ok=$true;message='CorelDRAW 2020（64-Bit）自动化接口连接正常';build=[int]$corel.VersionBuild;programPath=[string]$corel.ProgramPath}
  }
  'Import' {
    if(-not (Test-Path -LiteralPath $InputPath)){throw '没有找到待导入的 SVG 文件。'}
    $doc=$corel.CreateDocument()
    $doc.Unit=3
    if($PageWidthCM -gt 0 -and $PageHeightCM -gt 0){$doc.ActivePage.SetSize($PageWidthCM,$PageHeightCM)}
    $doc.Resolution=$Dpi
    $filter=$doc.ActiveLayer.ImportEx($InputPath)
    $filter.Finish()
    $doc.ClearSelection()
    $doc.Activate()
    $corel.Refresh()
    Write-Result @{ok=$true;message='当前展板已送入 CorelDRAW 2020，可继续节点、造型、文字、PowerTRACE 和图层编辑';document=[string]$doc.Title;dpi=$Dpi;pageCM=@($PageWidthCM,$PageHeightCM)}
  }
  'ConvertCurves' {
    $doc=Get-ActiveDocument $corel
    $converted=0
    $doc.BeginCommandGroup('制度展板-文字转曲')
    try {Visit-Document $doc {param($shape) if([int]$shape.Type -eq 6){$shape.ConvertToCurves();$script:converted++}}} finally {$doc.EndCommandGroup()}
    $corel.Refresh()
    Write-Result @{ok=$true;message=("已将 {0} 个文字对象转为曲线，可在 CorelDRAW 中撤销" -f $converted);converted=$converted}
  }
  'ConvertCMYK' {
    $doc=Get-ActiveDocument $corel
    $doc.BeginCommandGroup('制度展板-颜色转CMYK')
    try {$converted=Convert-DocumentColors $doc} finally {$doc.EndCommandGroup()}
    $corel.Refresh()
    Write-Result @{ok=$true;message=("已转换 {0} 处均匀填充/轮廓色为 CMYK；复杂填充与位图请用印前预检复核" -f $converted);converted=$converted}
  }
  'Preflight' {
    $doc=Get-ActiveDocument $corel
    $stats=Get-Preflight $doc
    Write-Result @{ok=$true;message='印前预检完成';preflight=$stats}
  }
  'SaveCDR' {
    if([string]::IsNullOrWhiteSpace($OutputPath)){throw '没有指定 CDR 保存位置。'}
    $doc=Get-ActiveDocument $corel
    $doc.SaveAs($OutputPath)
    Write-Result @{ok=$true;message='CorelDRAW 2020 CDR 文件保存完成';file=$OutputPath}
  }
  'PublishPDF' {
    if([string]::IsNullOrWhiteSpace($OutputPath)){throw '没有指定 PDF 保存位置。'}
    $doc=Get-ActiveDocument $corel
    $settings=$doc.PDFSettings
    $settings.Reset()
    $settings.ColorMode=1
    $settings.ColorResolution=$Dpi
    $settings.GrayResolution=$Dpi
    $settings.DownsampleColor=$false
    $settings.DownsampleGray=$false
    $settings.EmbedFonts=$true
    $settings.ProtectedTextAsCurves=$true
    $settings.CropMarks=($CropMarks -eq 'True')
    $settings.IncludeBleed=($BleedMM -gt 0)
    if($BleedMM -gt 0){$settings.Bleed=$corel.CorelScriptTools.FromInches($BleedMM/25.4)}
    $doc.PublishToPDF($OutputPath)
    Write-Result @{ok=$true;message=("CMYK 印刷 PDF 已输出：{0}dpi、{1}mm 出血、裁切线 {2}" -f $Dpi,$BleedMM,$CropMarks);file=$OutputPath;dpi=$Dpi;bleedMM=$BleedMM;cropMarks=($CropMarks -eq 'True')}
  }
}
