const $=id=>document.getElementById(id);
let logo='',reference='',aiBoards=[],analysis={borderInset:14,logoPosition:'top-left'};
const themes={red:{main:'#b51f24',light:'#fff7f3',line:'#e8c4b6'},blue:{main:'#1261a0',light:'#f1f8ff',line:'#bad6ed'},green:{main:'#26804a',light:'#f2fff6',line:'#b8dcc7'},coffee:{main:'#7a5034',light:'#fffaf3',line:'#ddcab4'}};
const escapeXML=s=>(s||'').replace(/[<>&'"]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;',"'":'&apos;','"':'&quot;'}[c]));
const dataURL=file=>new Promise(resolve=>{const r=new FileReader();r.onload=()=>resolve(r.result);r.readAsDataURL(file)});
function wrap(text,max){const out=[];(text||'').split(/\n/).forEach(p=>{if(!p){out.push('');return}let s='';for(const c of p){s+=c;if(s.length>=max){out.push(s);s=''}}if(s)out.push(s)});return out}
function currentData(extra={}){return {title:$('title').value,body:$('body').value,sign:$('sign').value,width:+$('width').value,height:+$('height').value,theme:$('theme').value,padding:+$('padding').value,titleSize:+$('titleSize').value,bodySize:+$('bodySize').value,dpi:+$('dpi').value,bleed:+$('bleed').value,cropMarks:$('cropMarks').checked,cmyk:$('cmyk').checked,logo,reference,opacity:+$('opacity').value,analysis,...extra}}
function layoutFor(d){
  const W=800,H=Math.round(W*d.height/d.width),bleedPx=(d.bleed/10)*(W/d.width),ox=bleedPx,oy=bleedPx,pad=d.padding||25;
  let bs=d.bodySize||21,lines,lineH,overflow=false;
  while(bs>=10){const chars=Math.max(12,Math.floor((W-pad*2-64)/bs));lines=wrap(d.body,chars);lineH=bs*1.68;if(oy+175+lines.length*lineH<=oy+H-90)break;bs--}
  if(bs<10){bs=10;overflow=true;lines=wrap(d.body,Math.max(12,Math.floor((W-pad*2-64)/bs)));lineH=bs*1.68}
  let ts=d.titleSize||42;while(ts>20&&[...d.title].length*ts>W-180)ts--;
  return {W,H,bleedPx,FW:W+bleedPx*2,FH:H+bleedPx*2,ox,oy,pad,bs,ts,lines,lineH,overflow};
}
function makeSVG(d,opts={}){
  const l=layoutFor(d),t=themes[d.theme]||themes.red,bi=Math.max(8,d.analysis?.borderInset||14),logoPos=d.analysis?.logoPosition||'top-left';
  const lx=logoPos.includes('right')?l.ox+l.W-110:l.ox+48,ly=logoPos.includes('bottom')?l.oy+l.H-125:l.oy+48;
  const totalWcm=d.width+d.bleed/5,totalHcm=d.height+d.bleed/5;
  const attrW=opts.raster?`${Math.round(totalWcm/2.54*(opts.dpi||d.dpi||300))}px`:`${totalWcm}cm`,attrH=opts.raster?`${Math.round(totalHcm/2.54*(opts.dpi||d.dpi||300))}px`:`${totalHcm}cm`;
  const crop=d.cropMarks&&l.bleedPx>0?`<g stroke="#111" stroke-width="1"><path d="M0 ${l.oy}H${l.ox-3}M${l.ox+l.W+3} ${l.oy}H${l.FW}M0 ${l.oy+l.H}H${l.ox-3}M${l.ox+l.W+3} ${l.oy+l.H}H${l.FW}M${l.ox} 0V${l.oy-3}M${l.ox} ${l.oy+l.H+3}V${l.FH}M${l.ox+l.W} 0V${l.oy-3}M${l.ox+l.W} ${l.oy+l.H+3}V${l.FH}</g>`:'';
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${attrW}" height="${attrH}" viewBox="0 0 ${l.FW} ${l.FH}"><metadata>{\"trim_cm\":\"${d.width}x${d.height}\",\"bleed_mm\":${d.bleed},\"dpi\":${d.dpi},\"color_target\":\"${d.cmyk?'CMYK':'RGB'}\"}</metadata><rect width="${l.FW}" height="${l.FH}" fill="${t.light}"/>${d.reference?`<image href="${d.reference}" x="${l.ox}" y="${l.oy}" width="${l.W}" height="${l.H}" preserveAspectRatio="xMidYMid slice" opacity="${d.opacity/100}"/>`:''}<rect x="${l.ox+bi}" y="${l.oy+bi}" width="${l.W-bi*2}" height="${l.H-bi*2}" rx="4" fill="none" stroke="${t.main}" stroke-width="8"/><rect x="${l.ox+32}" y="${l.oy+32}" width="${l.W-64}" height="${l.H-64}" fill="none" stroke="${t.line}" stroke-width="2"/><rect x="${l.ox}" y="${l.oy}" width="${l.W}" height="${l.H}" fill="none" stroke="#555" stroke-width="1" stroke-dasharray="6 5"/><path d="M${l.ox+48} ${l.oy+122}H${l.ox+l.W-48}" stroke="${t.main}" stroke-width="3"/><text x="${l.ox+l.W/2}" y="${l.oy+98}" text-anchor="middle" font-family="Microsoft YaHei" font-weight="700" font-size="${l.ts}" fill="${t.main}">${escapeXML(d.title)}</text>${d.logo?`<image href="${d.logo}" x="${lx}" y="${ly}" width="62" height="62" preserveAspectRatio="xMidYMid meet"/>`:''}<g font-family="Microsoft YaHei" font-size="${l.bs}" fill="#20242a">${l.lines.map((x,i)=>`<text x="${l.ox+l.pad+32}" y="${l.oy+175+i*l.lineH}">${escapeXML(x)}</text>`).join('')}</g><text x="${l.ox+l.W-l.pad-32}" y="${l.oy+l.H-55}" text-anchor="end" font-family="Microsoft YaHei" font-size="${l.bs*.8}" fill="#333">${escapeXML(d.sign)}</text>${crop}</svg>`;
}
function render(){
  const d=currentData(),l=layoutFor(d),parsed=new DOMParser().parseFromString(makeSVG(d),'image/svg+xml').documentElement,b=$('board');
  ['width','height','viewBox'].forEach(a=>b.setAttribute(a,parsed.getAttribute(a)));b.innerHTML=parsed.innerHTML;b.style.aspectRatio=`${l.FW}/${l.FH}`;
  $('overflowState').className='small-note '+(l.overflow?'overflow':'ok');$('overflowState').textContent=l.overflow?'内容仍可能溢出，请减少文字或增大展板':'文字空间正常；实际正文字号 '+l.bs;
}
function pdfHtml(svg,d){const w=d.width+d.bleed/5,h=d.height+d.bleed/5;return `<style>@page{size:${w}cm ${h}cm;margin:0}body{margin:0}svg{width:100%;height:100%}</style>${svg}`}
function setStatus(s){$('status').textContent=s}
async function rasterData(d,type){const svg=makeSVG(d,{raster:true,dpi:d.dpi}),img=new Image();await new Promise((ok,fail)=>{img.onload=ok;img.onerror=fail;img.src='data:image/svg+xml;base64,'+btoa(unescape(encodeURIComponent(svg)))});const c=document.createElement('canvas');c.width=img.width;c.height=img.height;c.getContext('2d').drawImage(img,0,0);return c.toDataURL(type==='jpg'?'image/jpeg':'image/png',.95)}

document.querySelectorAll('input,textarea,select').forEach(e=>e.addEventListener('input',()=>{if(!['templateSearch','apiKey','endpoint','model'].includes(e.id))render()}));
$('logo').onchange=async e=>{if(e.target.files[0]){logo=await dataURL(e.target.files[0]);render()}};
$('reference').onchange=async e=>{if(e.target.files[0]){reference=await dataURL(e.target.files[0]);render();$('analysisResult').textContent='样图已载入，可执行OCR和版式识别'}};

document.querySelectorAll('[data-export]').forEach(b=>b.onclick=async()=>{try{const type=b.dataset.export,d=currentData(),svg=makeSVG(d);setStatus('正在导出 '+type.toUpperCase()+'…');if(type==='svg')await boardAPI.saveSVG(svg);else if(type==='cdr')await boardAPI.saveCDR({svg,cmyk:d.cmyk});else if(type==='pdf')await boardAPI.savePDF({html:pdfHtml(svg,d)});else await boardAPI.saveImage({ext:type,data:await rasterData(d,type)});setStatus('导出完成')}catch(e){setStatus('导出失败：'+e.message)}});

function fillPolicyTypes(){const types=INDUSTRIES[$('industry').value]||[];$('policyTypes').innerHTML=types.map((x,i)=>`<option ${i===0?'selected':''}>${x}</option>`).join('');if(!$('policyName').value&&types[0])$('policyName').value=types[0]}
function applyBoard(b){$('title').value=b.title;$('body').value=b.body;$('theme').value=b.theme||'red';$('width').value=b.recommended_width_cm||40;$('height').value=b.recommended_height_cm||60;$('sign').value=$('company').value;render();setStatus(b.review_note?`已排版；复核提示：${b.review_note}`:'AI内容已排版')}
function showAIResults(){const box=$('aiResults');box.innerHTML=aiBoards.map((b,i)=>`<div class="result" data-index="${i}"><b>${i+1}. ${escapeXML(b.title)}</b><span>点击载入</span>${b.review_note?`<div class="review">需复核：${escapeXML(b.review_note)}</div>`:''}</div>`).join('');box.querySelectorAll('.result').forEach(x=>x.onclick=()=>applyBoard(aiBoards[+x.dataset.index]));if(aiBoards[0])applyBoard(aiBoards[0])}
Object.keys(INDUSTRIES).forEach(x=>$('industry').add(new Option(x,x)));fillPolicyTypes();$('industry').addEventListener('change',fillPolicyTypes);$('policyTypes').addEventListener('change',()=>{const x=$('policyTypes').selectedOptions[0];if(x)$('policyName').value=x.value});
$('saveAI').onclick=async()=>{await boardAPI.saveAIConfig({endpoint:$('endpoint').value.trim(),model:$('model').value.trim(),apiKey:$('apiKey').value.trim()});setStatus('AI连接设置已保存在本机')};
async function generateBoards(advanced=false){
  const name=$('policyName').value.trim(),selected=[...$('policyTypes').selectedOptions].map(x=>x.value),types=advanced?selected:(name?[name]:[]);
  if(!types.length)return setStatus('请输入需要制作的制度名称');
  if(!$('apiKey').value.trim())return setStatus('请先展开“AI连接设置”并填写API密钥');
  const button=advanced?$('aiGenerate'):$('quickGenerate');
  try{button.disabled=true;setStatus(`正在生成“${name||types[0]}”并自动排版…`);const data=await boardAPI.generateAI({endpoint:$('endpoint').value.trim(),model:$('model').value.trim(),apiKey:$('apiKey').value.trim(),policyName:name,autoMode:!advanced,industry:advanced?$('industry').value:'自动识别',types,count:advanced?+$('boardCount').value:1,words:+$('words').value,company:$('company').value.trim(),requirements:$('requirements').value.trim()});aiBoards=(data.boards||[]).slice(0,20);showAIResults();setStatus(`已生成并排版 ${aiBoards.length} 张展板`)}catch(e){setStatus('AI生成失败：'+e.message)}finally{button.disabled=false}
}
$('quickGenerate').onclick=()=>generateBoards(false);
$('aiGenerate').onclick=()=>generateBoards(true);
$('policyName').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();generateBoards(false)}});
document.querySelectorAll('[data-policy]').forEach(b=>b.onclick=()=>{$('policyName').value=b.dataset.policy;$('policyName').focus()});

$('ocrButton').onclick=async()=>{if(!reference)return setStatus('请先上传样图');try{$('ocrButton').disabled=true;setStatus('正在进行中文OCR识别…');const r=await boardAPI.ocrImage(reference),lines=r.text.split(/\n/).map(x=>x.trim()).filter(Boolean);if(lines.length){if(lines[0].length<=32)$('title').value=lines.shift();$('body').value=lines.join('\n');render()}setStatus(`OCR完成，平均置信度 ${Math.round(r.confidence)}%`)}catch(e){setStatus('OCR失败：'+e.message)}finally{$('ocrButton').disabled=false}};
function colorDistance(a,b){return Math.hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2])}
async function analyzeReference(){
  if(!reference)throw new Error('请先上传样图');const img=new Image();await new Promise((ok,fail)=>{img.onload=ok;img.onerror=fail;img.src=reference});
  const scale=Math.min(1,900/Math.max(img.width,img.height)),w=Math.round(img.width*scale),h=Math.round(img.height*scale),c=document.createElement('canvas');c.width=w;c.height=h;const ctx=c.getContext('2d');ctx.drawImage(img,0,0,w,h);const px=ctx.getImageData(0,0,w,h).data,map=new Map();
  for(let y=0;y<h;y+=8)for(let x=0;x<w;x+=8){const i=(y*w+x)*4,key=[px[i],px[i+1],px[i+2]].map(v=>Math.round(v/32)*32).join(',');map.set(key,(map.get(key)||0)+1)}
  const bg=[...map.entries()].sort((a,b)=>b[1]-a[1])[0][0].split(',').map(Number);let accent=[100,50,40],best=0;
  for(let y=0;y<h;y+=6)for(let x=0;x<w;x+=6){const i=(y*w+x)*4,r=px[i],g=px[i+1],b=px[i+2],d=colorDistance([r,g,b],bg),sat=Math.max(r,g,b)-Math.min(r,g,b);if(d>70&&sat>best){best=sat;accent=[r,g,b]}}
  const boxDensity=(x0,y0,x1,y1)=>{let n=0,k=0;for(let y=y0;y<y1;y+=4)for(let x=x0;x<x1;x+=4){const i=(y*w+x)*4;k++;if(colorDistance([px[i],px[i+1],px[i+2]],bg)>60)n++}return n/k};
  const colDensity=x=>boxDensity(x,h*.08,Math.min(w,x+3),h*.92),rowDensity=y=>boxDensity(w*.08,y,w*.92,Math.min(h,y+3));let bx=.018,by=.018;
  for(let x=1;x<w*.18;x+=2){if(colDensity(x)>.38){bx=x/w;break}}for(let y=1;y<h*.18;y+=2){if(rowDensity(y)>.38){by=y/h;break}}
  const corners=[['top-left',0,0,.25,.25],['top-right',.75,0,1,.25],['bottom-left',0,.75,.25,1],['bottom-right',.75,.75,1,1]].map(a=>[a[0],boxDensity(a[1]*w,a[2]*h,a[3]*w,a[4]*h)]).sort((a,b)=>b[1]-a[1]);
  let titleY=0,titleScore=0;for(let y=Math.round(h*.04);y<h*.42;y+=3){const s=boxDensity(w*.12,y,w*.88,Math.min(h,y+6));if(s>titleScore){titleScore=s;titleY=y}}
  const [r,g,b]=accent,theme=g>r*1.15&&g>b?'green':b>r*1.08?'blue':r>g*1.18?'red':'coffee';
  analysis={borderInset:Math.max(8,Math.min(80,Math.round((bx+by)/2*800))),logoPosition:corners[0][0],background:`rgb(${bg.join(',')})`,accent:`rgb(${accent.join(',')})`,titleYRatio:titleY/h};$('theme').value=theme;
  $('analysisResult').className='small-note analyzed';$('analysisResult').textContent=`底色 ${analysis.background}；主色 ${analysis.accent}；边框内缩约 ${Math.round((bx+by)/2*100)}%；标题区约在顶部 ${Math.round(titleY/h*100)}%；LOGO候选：${analysis.logoPosition}`;render();
}
$('analyzeButton').onclick=()=>analyzeReference().catch(e=>setStatus(e.message));

async function refreshTemplates(){const q=$('templateSearch').value.trim().toLowerCase(),list=await boardAPI.listTemplates(),filtered=list.filter(x=>!q||[x.name,x.industry,x.data?.title].join(' ').toLowerCase().includes(q));$('templateList').innerHTML=filtered.map(x=>`<div class="template-card" data-id="${x.id}"><b>${x.favorite?'★ ':''}${escapeXML(x.name)}</b><div>${escapeXML(x.industry||'未分类')} · ${escapeXML(x.data?.title||'')}</div><div class="template-actions"><button data-a="use">使用</button><button data-a="fav">收藏</button><button data-a="del">删除</button></div></div>`).join('');$('templateList').querySelectorAll('.template-card').forEach(card=>card.onclick=async e=>{const item=list.find(x=>x.id===card.dataset.id),a=e.target.dataset.a;if(a==='use'){const d=item.data;Object.entries({title:d.title,body:d.body,sign:d.sign,width:d.width,height:d.height,theme:d.theme,padding:d.padding,titleSize:d.titleSize,bodySize:d.bodySize,dpi:d.dpi,bleed:d.bleed}).forEach(([k,v])=>{if($(k)&&v!==undefined)$(k).value=v});analysis=d.analysis||analysis;logo=d.logo||'';render()}else if(a==='fav'){await boardAPI.favoriteTemplate(item.id);refreshTemplates()}else if(a==='del'){await boardAPI.deleteTemplate(item.id);refreshTemplates()}})}
$('saveTemplate').onclick=async()=>{const name=$('templateName').value.trim()||$('title').value;await boardAPI.saveTemplate({name,industry:$('templateIndustry').value.trim()||$('industry').value,favorite:false,data:currentData({reference:''})});setStatus('模板已保存');refreshTemplates()};$('templateSearch').oninput=refreshTemplates;

$('batchExport').onclick=async()=>{const formats=[...document.querySelectorAll('.batchFormat:checked')].map(x=>x.value);if(!formats.length)return setStatus('请至少选择一种批量格式');const source=aiBoards.length?aiBoards:[{title:$('title').value,body:$('body').value,theme:$('theme').value,recommended_width_cm:+$('width').value,recommended_height_cm:+$('height').value}];try{setStatus(`正在准备 ${source.length} 张批量文件…`);const items=source.slice(0,20).map(b=>{const d=currentData({title:b.title,body:b.body,theme:b.theme||$('theme').value,width:b.recommended_width_cm||+$('width').value,height:b.recommended_height_cm||+$('height').value,sign:$('company').value||$('sign').value,reference:''}),svg=makeSVG(d);return {title:b.title,svg,rasterSvg:makeSVG(d,{raster:true,dpi:d.dpi}),pdfHtml:pdfHtml(svg,d)}});const r=await boardAPI.batchExport({items,formats,cmyk:$('cmyk').checked});setStatus(r?`批量导出完成：${r.files.length}个文件，目录 ${r.directory}`:'已取消批量导出')}catch(e){setStatus('批量导出失败：'+e.message)}};
$('corelTest').onclick=async()=>{try{$('corelResult').textContent='检测中…';const raw=await boardAPI.corelTest(),data=JSON.parse(raw);$('corelResult').textContent=data.map(x=>`${x.Version}：${x.CreateAndSave?'通过':'未通过'}（${x.Message}）`).join('\n')}catch(e){$('corelResult').textContent=e.message}};

boardAPI.getAIConfig().then(c=>{if(c.endpoint)$('endpoint').value=c.endpoint;if(c.model)$('model').value=c.model;if(c.apiKey)$('apiKey').value=c.apiKey});refreshTemplates();render();
