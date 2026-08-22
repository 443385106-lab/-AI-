$ErrorActionPreference='Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
Write-Host '1/3 安装项目依赖';npm install
Write-Host '2/3 构建Windows安装包';npm run dist
Write-Host '3/3 完成。请在 dist 文件夹中查找 ZhiduBoardStudio-0.3.0-Setup.exe'
