const {app,BrowserWindow,ipcMain,dialog,safeStorage,nativeImage}=require('electron');
const {writeFile,readFile,mkdir}=require('fs/promises');
const {join}=require('path');
const {spawn}=require('child_process');

function createWindow(){
  const win=new BrowserWindow({width:1500,height:920,minWidth:1180,minHeight:760,icon:join(__dirname,'assets','app-icon.png'),webPreferences:{preload:join(__dirname,'preload.js'),contextIsolation:true}});
  win.loadFile(join(__dirname,'renderer/index.html'));
}
app.whenReady().then(()=>{createWindow();app.on('activate',()=>BrowserWindow.getAllWindows().length||createWindow())});
app.on('window-all-closed',()=>process.platform!=='darwin'&&app.quit());

const safeName=s=>(s||'制度展板').replace(/[\\/:*?"<>|]/g,'_').slice(0,80);
function runPS(script,args=[]){
  return new Promise((resolve,reject)=>{
    const p=spawn('powershell.exe',['-NoProfile','-ExecutionPolicy','Bypass','-File',script,...args]);
    let out='',err='';p.stdout.on('data',d=>out+=d);p.stderr.on('data',d=>err+=d);
    p.on('close',code=>code?reject(new Error(err||out||'PowerShell执行失败')):resolve(out.trim()));
  });
}
async function saveBuffer(ext,filters,data){
  const {canceled,filePath}=await dialog.showSaveDialog({defaultPath:`制度展板.${ext}`,filters});
  if(canceled||!filePath)return null;await writeFile(filePath,Buffer.from(data));return filePath;
}
async function htmlToPDF(filePath,html){
  const w=new BrowserWindow({show:false});await w.loadURL('data:text/html;charset=utf-8,'+encodeURIComponent(html));
  const pdf=await w.webContents.printToPDF({printBackground:true,preferCSSPageSize:true});await writeFile(filePath,pdf);w.destroy();
}
ipcMain.handle('save-svg',(_,svg)=>saveBuffer('svg',[{name:'SVG矢量图',extensions:['svg']}],svg));
ipcMain.handle('save-image',(_,arg)=>saveBuffer(arg.ext,[{name:arg.ext.toUpperCase(),extensions:[arg.ext]}],Buffer.from(arg.data.split(',')[1],'base64')));
ipcMain.handle('save-pdf',async(_,arg)=>{
  const {canceled,filePath}=await dialog.showSaveDialog({defaultPath:'制度展板.pdf',filters:[{name:'PDF印刷文件',extensions:['pdf']}]});
  if(canceled||!filePath)return null;await htmlToPDF(filePath,arg.html||arg);return filePath;
});
ipcMain.handle('save-cdr',async(_,arg)=>{
  const svg=typeof arg==='string'?arg:arg.svg,cmyk=typeof arg==='object'&&arg.cmyk;
  const {canceled,filePath}=await dialog.showSaveDialog({defaultPath:'制度展板.cdr',filters:[{name:'CorelDRAW文件',extensions:['cdr']}]});
  if(canceled||!filePath)return null;
  const temp=join(app.getPath('temp'),`board-${Date.now()}.svg`);await writeFile(temp,svg);
  await runPS(join(__dirname,'scripts','corel-export.ps1'),['-SvgPath',temp,'-CdrPath',filePath,'-ConvertCMYK',String(!!cmyk)]);return filePath;
});

const configPath=()=>join(app.getPath('userData'),'ai-config.json');
const templatesPath=()=>join(app.getPath('userData'),'templates.json');
ipcMain.handle('ai-config-get',async()=>{try{const c=JSON.parse(await readFile(configPath(),'utf8'));if(c.apiKey&&c.encrypted&&safeStorage.isEncryptionAvailable())c.apiKey=safeStorage.decryptString(Buffer.from(c.apiKey,'base64'));return c}catch{return {endpoint:'https://api.openai.com/v1/responses',model:'gpt-5.6-luna'}}});
ipcMain.handle('ai-config-save',async(_,cfg)=>{const copy={...cfg};if(copy.apiKey&&safeStorage.isEncryptionAvailable()){copy.apiKey=safeStorage.encryptString(copy.apiKey).toString('base64');copy.encrypted=true}await writeFile(configPath(),JSON.stringify(copy,null,2));return true});
ipcMain.handle('ai-generate',async(_,arg)=>{
  const endpoint=arg.endpoint||'https://api.openai.com/v1/responses';
  const board={type:'object',properties:{title:{type:'string'},body:{type:'string'},theme:{type:'string',enum:['red','blue','green','coffee']},recommended_width_cm:{type:'number'},recommended_height_cm:{type:'number'},review_note:{type:'string'}},required:['title','body','theme','recommended_width_cm','recommended_height_cm','review_note'],additionalProperties:false};
  const schema={type:'object',properties:{boards:{type:'array',items:board}},required:['boards'],additionalProperties:false};
  const requested=(arg.types||[]).filter(Boolean),policyName=arg.policyName||requested[0]||'管理制度';
  const prompt=`你是服务图文广告公司的中国企事业单位制度展板内容编辑与排版助手。客户只提供了制度名称“${policyName}”。${arg.autoMode?'请自动判断适用行业、使用场景和常见展板规格。':`行业模板：${arg.industry}；制度类型：${requested.join('、')}。`}生成${arg.count||1}张，每张约${arg.words||450}字；落款：${arg.company||'未指定'}；补充要求：${arg.requirements||'无'}。标题优先严格采用客户提供的制度名称；正文整理为6—10条适合上墙展示的简洁条款，使用中文序号和换行，不要Markdown。theme应按行业视觉习惯自动选择；recommended_width_cm和recommended_height_cm应选择图文店常用成品尺寸。不要虚构证照、法规条文编号或具体责任人；涉及法规、医疗、消防、危化、食品等内容时在review_note明确提醒交付前人工复核。`;
  const headers={'Content-Type':'application/json'};if(arg.apiKey)headers.Authorization=`Bearer ${arg.apiKey}`;
  const payload={model:arg.model||'gpt-5.6-luna',input:[{role:'system',content:'输出严格符合JSON Schema的制度展板数据。'},{role:'user',content:prompt}],text:{format:{type:'json_schema',name:'board_set',strict:true,schema}},max_output_tokens:12000};
  const res=await fetch(endpoint,{method:'POST',headers,body:JSON.stringify(payload)});const raw=await res.text();if(!res.ok)throw new Error(`AI接口错误 ${res.status}：${raw.slice(0,300)}`);
  const data=JSON.parse(raw);const output=data.output_text||data.output?.flatMap(x=>x.content||[]).find(x=>x.type==='output_text')?.text;if(!output)throw new Error('AI未返回可用内容');return JSON.parse(output);
});

async function readTemplates(){try{return JSON.parse(await readFile(templatesPath(),'utf8'))}catch{return []}}
ipcMain.handle('templates-list',()=>readTemplates());
ipcMain.handle('template-save',async(_,item)=>{const list=await readTemplates(),i=list.findIndex(x=>x.id===item.id);const saved={...item,id:item.id||String(Date.now()),updatedAt:new Date().toISOString()};if(i>=0)list[i]=saved;else list.unshift(saved);await writeFile(templatesPath(),JSON.stringify(list,null,2));return saved});
ipcMain.handle('template-delete',async(_,id)=>{const list=(await readTemplates()).filter(x=>x.id!==id);await writeFile(templatesPath(),JSON.stringify(list,null,2));return true});
ipcMain.handle('template-favorite',async(_,id)=>{const list=await readTemplates(),x=list.find(x=>x.id===id);if(x)x.favorite=!x.favorite;await writeFile(templatesPath(),JSON.stringify(list,null,2));return x});

ipcMain.handle('ocr-image',async(_,dataURL)=>{
  const {createWorker}=require('tesseract.js');const worker=await createWorker(['chi_sim','eng']);
  try{const {data}=await worker.recognize(Buffer.from(dataURL.split(',')[1],'base64'));return {text:data.text||'',confidence:data.confidence||0}}finally{await worker.terminate()}
});
ipcMain.handle('corel-test',async()=>{
  if(process.platform!=='win32')throw new Error('CorelDRAW兼容测试只能在Windows电脑运行');
  return runPS(join(__dirname,'scripts','test-corel-versions.ps1'));
});
ipcMain.handle('batch-export',async(_,arg)=>{
  const pick=await dialog.showOpenDialog({title:'选择批量导出文件夹',properties:['openDirectory','createDirectory']});if(pick.canceled)return null;
  const dir=pick.filePaths[0];await mkdir(dir,{recursive:true});const formats=arg.formats||['svg'];const report=[];
  const cdrManifest=[];
  for(let i=0;i<arg.items.length;i++){
    const item=arg.items[i],base=`${String(i+1).padStart(2,'0')}-${safeName(item.title)}`;
    if(formats.includes('svg')){await writeFile(join(dir,base+'.svg'),item.svg);report.push(base+'.svg')}
    if(formats.includes('pdf')){await htmlToPDF(join(dir,base+'.pdf'),item.pdfHtml);report.push(base+'.pdf')}
    if(formats.includes('png')||formats.includes('jpg')){
      const src='data:image/svg+xml;base64,'+Buffer.from(item.rasterSvg).toString('base64'),img=nativeImage.createFromDataURL(src);
      if(formats.includes('png')){await writeFile(join(dir,base+'.png'),img.toPNG());report.push(base+'.png')}
      if(formats.includes('jpg')){await writeFile(join(dir,base+'.jpg'),img.toJPEG(95));report.push(base+'.jpg')}
    }
    if(formats.includes('cdr')){const svgPath=join(app.getPath('temp'),`batch-${Date.now()}-${i}.svg`);await writeFile(svgPath,item.svg);cdrManifest.push({svgPath,cdrPath:join(dir,base+'.cdr')});report.push(base+'.cdr')}
  }
  if(cdrManifest.length){const manifest=join(app.getPath('temp'),`cdr-manifest-${Date.now()}.json`);await writeFile(manifest,JSON.stringify(cdrManifest));await runPS(join(__dirname,'scripts','corel-batch-export.ps1'),['-ManifestPath',manifest,'-ConvertCMYK',String(!!arg.cmyk)])}
  return {directory:dir,files:report};
});
