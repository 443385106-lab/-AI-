# AI制度展板生成工具 0.8（CorelDRAW 2020专业联动版）

软件品牌：**匠心图文**。Windows程序窗口、桌面快捷方式和安装包均使用项目内的红金“匠心图文”图标。

Windows 本地单机工具，适用于制度牌、操作规程、安全告知卡、岗位职责等批量排版。文字、边框和装饰以 SVG 矢量形式保存，可导出 CorelDRAW、SVG、PDF 和 300dpi 图片。

## 主要功能

- 本地OCR：识别中文、英文和数字，自动分离标题与正文
- 样图分析：估算底色、主色、边框内缩、标题区及四角LOGO候选位置
- 本地行业制度库：36个行业、210套模板，最多生成20张独立制度牌
- 模板库：保存、搜索、行业分类、收藏、删除和重复使用
- 文字溢出检测：根据画布高度自动缩小正文字号
- 批量导出：一次输出1—20张CDR、SVG、PDF、PNG或JPG
- 印刷参数：150/200/300dpi、出血、裁切线、CMYK目标
- CorelDRAW X7/2018/2019/2020自动兼容性检测
- CorelDRAW 2020（64-Bit）专业联动：当前展板直接导入、文字转曲、均匀色与轮廓转CMYK、印前预检、另存CDR、300dpi CMYK印刷PDF
- CorelDRAW原生编辑衔接：贝塞尔与节点、焊接/修剪/相交、封套/阴影/轮廓/调和、PowerTRACE、图框精确剪裁、图层/符号、批注审阅和字体管理
- Windows NSIS安装包配置和一键构建脚本

## Windows运行源码

1. 安装 Node.js 20 LTS。
2. 解压项目，在项目文件夹打开命令提示符。
3. 执行 `npm install`。
4. 执行 `npm start`。

OCR首次识别时需要取得Tesseract中文语言数据，之后由本机缓存复用。

## 构建双击安装的EXE

在项目文件夹中右键以PowerShell运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1
```

完成后安装程序位于 `dist/ZhiduBoardStudio-0.8.0-Setup.exe`。

项目同时包含 `.github/workflows/build-windows.yml`，推送到GitHub的 `main` 分支后会在Windows构建机自动生成安装包，并作为 `ZhiduBoardStudio-Windows-Setup` 构建产物提供下载。

## CorelDRAW兼容测试

软件内点击“检测CorelDRAW版本兼容性”，或执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-corel-versions.ps1
```

脚本分别探测X7、2018、2019、2020 COM接口，并验证新建文档和保存CDR。完整人工测试请按 `WINDOWS_TEST_CHECKLIST.md` 执行。

软件中的“CorelDRAW 2020（64-Bit）专业联动”工具箱只连接主版本22。先点击“送入 CorelDRAW 2020 编辑”，再执行转曲、CMYK、预检、保存或印刷PDF。转曲和颜色转换会作为CorelDRAW命令组执行，可在CorelDRAW中撤销。

## CMYK说明

- CDR导出会尝试把导入对象的填充和轮廓转换到CMYK。
- SVG以及PNG/JPG预览图仍使用RGB色彩。
- 顶部普通PDF由Chromium生成，不保证是真正的印刷CMYK PDF；工具箱中的“印刷PDF”由CorelDRAW 2020以CMYK、300dpi、出血和裁切线参数发布。
- 出血和裁切线以真实尺寸写入SVG/CDR/PDF页面。

## 安全与审核

- API密钥保存在本机用户数据目录；Windows支持时使用系统加密。
- AI生成制度属于辅助草稿。医疗、消防、食品、危化、建筑及引用法规的内容，上墙前需由专业人员审核。
- 上传样图时应确认具有使用权限；OCR结果需要校对错别字。
