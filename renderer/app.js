const $=id=>document.getElementById(id);
let logo='',reference='',aiBoards=[],visibleLocalTemplates=[],previewZoom=1,activeBoardIndex=0,isRestoring=false,history=[],historyIndex=-1,historyTimer,draftTimer,analysis={borderInset:14,logoPosition:'top-left'};
let customObjects=[],selectedObjectIds=[],activeTool='select',drawState=null,objectSequence=1,smartGuides=true;
const themes={red:{main:'#b51f24',light:'#fff7f3',line:'#e8c4b6'},blue:{main:'#1261a0',light:'#f1f8ff',line:'#bad6ed'},green:{main:'#26804a',light:'#f2fff6',line:'#b8dcc7'},coffee:{main:'#7a5034',light:'#fffaf3',line:'#ddcab4'}};
const escapeXML=s=>(s||'').replace(/[<>&'"]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;',"'":'&apos;','"':'&quot;'}[c]));
const dataURL=file=>new Promise(resolve=>{const r=new FileReader();r.onload=()=>resolve(r.result);r.readAsDataURL(file)});
function wrap(text,max){const out=[];(text||'').split(/\n/).forEach(p=>{if(!p){out.push('');return}let s='';for(const c of p){s+=c;if(s.length>=max){out.push(s);s=''}}if(s)out.push(s)});return out}
const cnNumbers=['一','二','三','四','五','六','七','八','九','十','十一','十二','十三','十四','十五','十六','十七','十八','十九','二十'];
function clauseOrder(s){if(/总则|适用|目的|原则|负责|职责/.test(s))return 0;if(/培训|教育|上岗|资格/.test(s))return 1;if(/操作|执行|使用|作业|流程/.test(s))return 2;if(/检查|巡查|维护|记录|台账|考核/.test(s))return 3;if(/异常|事故|应急|报告|处置/.test(s))return 4;if(/复核|修订|附则/.test(s))return 5;return 2}
function normalizeBody(text){
  const rows=(text||'').replace(/\r/g,'\n').split(/\n+/).map(x=>x.trim()).filter(Boolean).map((x,index)=>({text:x.replace(/^(?:第?[一二三四五六七八九十百\d]+(?:条|[、.．）)]))\s*/,'').replace(/^[-•·*]+\s*/,'').trim(),index})).filter(x=>x.text);
  rows.sort((a,b)=>clauseOrder(a.text)-clauseOrder(b.text)||a.index-b.index);
  return [...new Set(rows.map(x=>x.text))].slice(0,20).map((x,i)=>`${cnNumbers[i]||i+1}、${x.replace(/[；。]?$/,'。')}`).join('\n\n');
}
function requestedPolicyNames(){return [...new Set($('policyName').value.split(/[\n；;]+/).map(x=>x.trim()).filter(Boolean))].slice(0,20)}
function currentData(extra={}){return {title:$('title').value,body:$('body').value,sign:$('sign').value,width:+$('width').value,height:+$('height').value,theme:$('theme').value,padding:+$('padding').value,titleSize:+$('titleSize').value,bodySize:+$('bodySize').value,dpi:+$('dpi').value,bleed:+$('bleed').value,cropMarks:$('cropMarks').checked,cmyk:$('cmyk').checked,logo,reference,opacity:+$('opacity').value,analysis,customObjects:customObjects.map(x=>structuredClone(x)),...extra}}
const draftFields=['policyName','title','body','sign','width','height','theme','padding','titleSize','bodySize','dpi','bleed','cropMarks','cmyk','company','words','boardCount','requirements'];
function captureState(){const fields={};draftFields.forEach(id=>{const e=$(id);fields[id]=e.type==='checkbox'?e.checked:e.value});return {fields,analysis:{...analysis},aiBoards:aiBoards.map(b=>({...b})),activeBoardIndex,customObjects:customObjects.map(x=>structuredClone(x)),objectSequence}}
function applyState(state){if(!state?.fields)return;isRestoring=true;draftFields.forEach(id=>{const e=$(id),v=state.fields[id];if(v===undefined)return;if(e.type==='checkbox')e.checked=!!v;else e.value=v});analysis={...(state.analysis||analysis)};aiBoards=(state.aiBoards||[]).map(b=>({...b}));activeBoardIndex=Math.min(state.activeBoardIndex||0,Math.max(0,aiBoards.length-1));customObjects=(state.customObjects||[]).map(x=>structuredClone(x));objectSequence=state.objectSequence||customObjects.length+1;selectedObjectIds=[];isRestoring=false;showAIResults(false);render();renderObjectList();updateObjectInspector()}
function saveDraft(){try{localStorage.setItem('zhidu-board-draft-v1',JSON.stringify(captureState()));$('draftState').textContent=`已自动保存 ${new Date().toLocaleTimeString([], {hour:'2-digit',minute:'2-digit'})}`}catch{$('draftState').textContent='自动保存空间不足，请保存为模板'}}
function restoreDraft(){try{const raw=localStorage.getItem('zhidu-board-draft-v1');if(!raw)return false;applyState(JSON.parse(raw));setStatus('已恢复上次未完成的设计');return true}catch{return false}}
function updateUndoButtons(){$('undoDesign').disabled=historyIndex<=0;$('redoDesign').disabled=historyIndex>=history.length-1}
function recordHistory(){if(isRestoring)return;const state=captureState(),serialized=JSON.stringify(state),current=history[historyIndex];if(current&&current.serialized===serialized)return;history=history.slice(0,historyIndex+1);history.push({state,serialized});if(history.length>40)history.shift();historyIndex=history.length-1;updateUndoButtons()}
function queueStateSave(){clearTimeout(historyTimer);clearTimeout(draftTimer);historyTimer=setTimeout(recordHistory,450);draftTimer=setTimeout(saveDraft,650)}
function commitState(message){render();recordHistory();saveDraft();if(message)setStatus(message)}
function inferTheme(text){return /食品|餐饮|药品|农业|卫生|环保/.test(text)?'green':/安全|消防|设备|电气|学校|医疗|科技|数据/.test(text)?'blue':/酒店|美容|文化|旅游|服务/.test(text)?'coffee':'red'}
function updateDesignAudit(d,l){
  let score=100;const issues=[],chars=[...d.body.replace(/\s/g,'')].length,clauses=d.body.split(/\n+/).filter(x=>x.trim()).length;
  if(!d.title.trim()){score-=30;issues.push('缺少标题')}else if([...d.title].length>24){score-=8;issues.push('标题较长，建议压缩')}
  if(chars<120){score-=18;issues.push('正文偏少，建议补充条款')}if(chars>900){score-=15;issues.push('正文过多，建议拆成两张')}if(clauses<4){score-=12;issues.push('条款少于4条')}
  if(l.overflow){score-=30;issues.push('正文发生溢出')}if(d.dpi<300){score-=10;issues.push('印刷分辨率低于300dpi')}if(d.bleed<3){score-=8;issues.push('出血不足3mm')}if(!d.cmyk){score-=10;issues.push('未启用CMYK目标')}if(!d.sign.trim()){score-=4;issues.push('未填写署名')}
  score=Math.max(0,score);$('designScore').textContent=score;$('scoreRing').style.setProperty('--score',score);$('scoreRing').className='score-ring '+(score>=90?'excellent':score>=70?'good':'warning');$('designIssues').innerHTML=(issues.length?issues:['排版与印刷参数正常']).map(x=>`<li>${escapeXML(x)}</li>`).join('');
}
function layoutFor(d){
  const W=800,H=Math.round(W*d.height/d.width),bleedPx=(d.bleed/10)*(W/d.width),ox=bleedPx,oy=bleedPx,pad=d.padding||25;
  let bs=d.bodySize||21,lines,lineH,overflow=false;
  while(bs>=10){const chars=Math.max(12,Math.floor((W-pad*2-64)/bs));lines=wrap(d.body,chars);lineH=bs*1.68;if(oy+175+lines.length*lineH<=oy+H-90)break;bs--}
  if(bs<10){bs=10;overflow=true;lines=wrap(d.body,Math.max(12,Math.floor((W-pad*2-64)/bs)));lineH=bs*1.68}
  let ts=d.titleSize||42;while(ts>20&&[...d.title].length*ts>W-180)ts--;
  return {W,H,bleedPx,FW:W+bleedPx*2,FH:H+bleedPx*2,ox,oy,pad,bs,ts,lines,lineH,overflow};
}
function objectBounds(o){
  if(o.type==='path'&&o.points?.length){const xs=o.points.map(p=>p.x),ys=o.points.map(p=>p.y);return {x:Math.min(...xs),y:Math.min(...ys),w:Math.max(1,Math.max(...xs)-Math.min(...xs)),h:Math.max(1,Math.max(...ys)-Math.min(...ys))}}
  const x=Math.min(o.x,o.x+(o.width||0)),y=Math.min(o.y,o.y+(o.height||0));
  return {x,y,w:Math.max(1,Math.abs(o.width||1)),h:Math.max(1,Math.abs(o.height||1))};
}
function customObjectSVG(o,selected=false,preview=false){
  const b=objectBounds(o),cx=b.x+b.w/2,cy=b.y+b.h/2,rotation=Number(o.rotation)||0,transform=rotation?` transform="rotate(${rotation} ${cx} ${cy})"`:'';
  const fill=o.fill||'none',stroke=o.stroke||'none',sw=Number(o.strokeWidth??2),common=`class="editable-object${o.locked?' locked':''}" data-object-id="${o.id}"${transform}`;
  let shape='';
  if(o.type==='rectangle')shape=`<rect ${common} x="${b.x}" y="${b.y}" width="${b.w}" height="${b.h}" rx="${o.radius||0}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}"/>`;
  else if(o.type==='ellipse')shape=`<ellipse ${common} cx="${cx}" cy="${cy}" rx="${b.w/2}" ry="${b.h/2}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}"/>`;
  else if(o.type==='line')shape=`<line ${common} x1="${o.x}" y1="${o.y}" x2="${o.x+(o.width||0)}" y2="${o.y+(o.height||0)}" stroke="${stroke==='none'?'#222222':stroke}" stroke-width="${Math.max(1,sw)}"/>`;
  else if(o.type==='text')shape=`<text ${common} x="${o.x}" y="${o.y+(o.fontSize||28)}" font-family="Microsoft YaHei" font-size="${o.fontSize||28}" font-weight="${o.fontWeight||400}" fill="${fill==='none'?'#222222':fill}" stroke="${stroke}" stroke-width="${sw}">${escapeXML(o.text||'文字')}</text>`;
  else if(o.type==='path'){const d=(o.points||[]).map((p,i)=>`${i?'L':'M'}${p.x} ${p.y}`).join(' ');shape=`<path ${common} d="${d}" fill="none" stroke="${stroke==='none'?'#222222':stroke}" stroke-width="${Math.max(1,sw)}" stroke-linecap="round" stroke-linejoin="round"/>`}
  if(!preview||!selected)return shape;
  const handles=[['nw',b.x,b.y],['ne',b.x+b.w,b.y],['sw',b.x,b.y+b.h],['se',b.x+b.w,b.y+b.h]].map(([a,x,y])=>`<rect class="selection-handle" data-object-id="${o.id}" data-resize="${a}" x="${x-5}" y="${y-5}" width="10" height="10"/>`).join('');
  const nodes=activeTool==='node'&&o.type==='path'?(o.points||[]).map((p,i)=>`<circle class="node-handle" data-object-id="${o.id}" data-node-index="${i}" cx="${p.x}" cy="${p.y}" r="5"/>`).join(''):'';
  return `${shape}<rect class="selection-box" x="${b.x}" y="${b.y}" width="${b.w}" height="${b.h}"${transform}/>${handles}${nodes}`;
}
function makeSVG(d,opts={}){
  const l=layoutFor(d),t=themes[d.theme]||themes.red,bi=Math.max(8,d.analysis?.borderInset||14),logoPos=d.analysis?.logoPosition||'top-left';
  const lx=logoPos.includes('right')?l.ox+l.W-110:l.ox+48,ly=logoPos.includes('bottom')?l.oy+l.H-125:l.oy+48;
  const totalWcm=d.width+d.bleed/5,totalHcm=d.height+d.bleed/5;
  const attrW=opts.raster?`${Math.round(totalWcm/2.54*(opts.dpi||d.dpi||300))}px`:`${totalWcm}cm`,attrH=opts.raster?`${Math.round(totalHcm/2.54*(opts.dpi||d.dpi||300))}px`:`${totalHcm}cm`;
  const crop=d.cropMarks&&l.bleedPx>0?`<g stroke="#111" stroke-width="1"><path d="M0 ${l.oy}H${l.ox-3}M${l.ox+l.W+3} ${l.oy}H${l.FW}M0 ${l.oy+l.H}H${l.ox-3}M${l.ox+l.W+3} ${l.oy+l.H}H${l.FW}M${l.ox} 0V${l.oy-3}M${l.ox} ${l.oy+l.H+3}V${l.FH}M${l.ox+l.W} 0V${l.oy-3}M${l.ox+l.W} ${l.oy+l.H+3}V${l.FH}</g>`:'';
  const customLayer=`<g id="custom-object-layer">${(d.customObjects||[]).map(o=>customObjectSVG(o,selectedObjectIds.includes(o.id),!!opts.preview)).join('')}</g>`;
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${attrW}" height="${attrH}" viewBox="0 0 ${l.FW} ${l.FH}"><metadata>{\"trim_cm\":\"${d.width}x${d.height}\",\"bleed_mm\":${d.bleed},\"dpi\":${d.dpi},\"color_target\":\"${d.cmyk?'CMYK':'RGB'}\"}</metadata><rect width="${l.FW}" height="${l.FH}" fill="${t.light}"/>${d.reference?`<image href="${d.reference}" x="${l.ox}" y="${l.oy}" width="${l.W}" height="${l.H}" preserveAspectRatio="xMidYMid slice" opacity="${d.opacity/100}"/>`:''}<rect x="${l.ox+bi}" y="${l.oy+bi}" width="${l.W-bi*2}" height="${l.H-bi*2}" rx="4" fill="none" stroke="${t.main}" stroke-width="8"/><rect x="${l.ox+32}" y="${l.oy+32}" width="${l.W-64}" height="${l.H-64}" fill="none" stroke="${t.line}" stroke-width="2"/><rect x="${l.ox}" y="${l.oy}" width="${l.W}" height="${l.H}" fill="none" stroke="#555" stroke-width="1" stroke-dasharray="6 5"/><path d="M${l.ox+48} ${l.oy+122}H${l.ox+l.W-48}" stroke="${t.main}" stroke-width="3"/><text x="${l.ox+l.W/2}" y="${l.oy+98}" text-anchor="middle" font-family="Microsoft YaHei" font-weight="700" font-size="${l.ts}" fill="${t.main}">${escapeXML(d.title)}</text>${d.logo?`<image href="${d.logo}" x="${lx}" y="${ly}" width="62" height="62" preserveAspectRatio="xMidYMid meet"/>`:''}<g font-family="Microsoft YaHei" font-size="${l.bs}" fill="#20242a" letter-spacing=".4">${l.lines.map((x,i)=>{const lead=/^(?:[一二三四五六七八九十百]+|\d+)、/.test(x),indent=x&&!lead?26:0;return `<text x="${l.ox+l.pad+32+indent}" y="${l.oy+175+i*l.lineH}" font-weight="${lead?600:400}">${escapeXML(x)}</text>`}).join('')}</g>${customLayer}<text x="${l.ox+l.W-l.pad-32}" y="${l.oy+l.H-55}" text-anchor="end" font-family="Microsoft YaHei" font-size="${l.bs*.8}" fill="#333">${escapeXML(d.sign)}</text>${crop}</svg>`;
}
function applyPreviewZoom(){
  const stage=$('stage'),b=$('board'),l=layoutFor(currentData()),fit=Math.max(120,Math.min((stage.clientWidth-40)*.92,(stage.clientHeight-40)*.92*l.FW/l.FH));
  b.style.width=`${fit*previewZoom}px`;b.style.height='auto';b.style.maxWidth='none';b.style.maxHeight='none';$('zoomValue').textContent=`${Math.round(previewZoom*100)}%`;$('zoomRange').value=Math.round(previewZoom*100);
}
function setPreviewZoom(value){previewZoom=Math.max(.25,Math.min(2,value));applyPreviewZoom()}
function render(){
  const d=currentData(),l=layoutFor(d),parsed=new DOMParser().parseFromString(makeSVG(d,{preview:true}),'image/svg+xml').documentElement,b=$('board');
  ['width','height','viewBox'].forEach(a=>b.setAttribute(a,parsed.getAttribute(a)));b.innerHTML=parsed.innerHTML;b.style.aspectRatio=`${l.FW}/${l.FH}`;
  $('overflowState').className='small-note '+(l.overflow?'overflow':'ok');$('overflowState').textContent=l.overflow?'内容仍可能溢出，请减少文字或增大展板':'文字空间正常；实际正文字号 '+l.bs;
  updateDesignAudit(d,l);
  requestAnimationFrame(applyPreviewZoom);
}
function pdfHtml(svg,d){const w=d.width+d.bleed/5,h=d.height+d.bleed/5;return `<style>@page{size:${w}cm ${h}cm;margin:0}body{margin:0}svg{width:100%;height:100%}</style>${svg}`}
function setStatus(s){$('status').textContent=s}
async function rasterData(d,type){const svg=makeSVG(d,{raster:true,dpi:d.dpi}),img=new Image();await new Promise((ok,fail)=>{img.onload=ok;img.onerror=fail;img.src='data:image/svg+xml;base64,'+btoa(unescape(encodeURIComponent(svg)))});const c=document.createElement('canvas');c.width=img.width;c.height=img.height;c.getContext('2d').drawImage(img,0,0);return c.toDataURL(type==='jpg'?'image/jpeg':'image/png',.95)}

const toolInfo={select:['选择工具','拖动对象；Shift 多选；Delete 删除'],node:['形状/节点工具','拖动手绘路径节点调整造型'],freehand:['钢笔/手绘工具','按住鼠标在画布上绘制路径'],rectangle:['矩形工具','在画布上拖动绘制矩形'],ellipse:['椭圆工具','在画布上拖动绘制椭圆'],line:['直线工具','在画布上拖动绘制直线'],text:['文本工具','单击画布添加文字，双击文字可编辑'],zoom:['缩放工具','单击放大，Shift+单击缩小'],eyedropper:['吸管工具','单击对象吸取其填充颜色']};
const paletteColors=['none','#000000','#ffffff','#7f8c8d','#e74c3c','#c0392b','#f39c12','#f1c40f','#27ae60','#16a085','#00a8e8','#2980b9','#2c3eae','#8e44ad','#d81b60','#795548','#b51f24','#1261a0','#26804a','#7a5034'];
const selectedObjects=()=>customObjects.filter(o=>selectedObjectIds.includes(o.id));
const nextObjectId=()=>`obj-${Date.now()}-${objectSequence++}`;
function setActiveTool(tool){
  activeTool=tool;document.querySelectorAll('[data-tool]').forEach(b=>b.classList.toggle('active',b.dataset.tool===tool));
  const info=toolInfo[tool]||toolInfo.select;$('activeToolName').textContent=info[0];$('toolHint').textContent=info[1];
  $('board').className.baseVal='';if(['freehand','rectangle','ellipse','line'].includes(tool))$('board').classList.add('draw-crosshair');if(tool==='text')$('board').classList.add('tool-text');if(tool==='zoom')$('board').classList.add('tool-zoom');render();
}
function selectObject(id,additive=false){
  const clicked=customObjects.find(o=>o.id===id);if(!clicked)return;
  const ids=clicked.groupId?customObjects.filter(o=>o.groupId===clicked.groupId).map(o=>o.id):[id];
  if(additive){ids.forEach(x=>{const i=selectedObjectIds.indexOf(x);if(i>=0)selectedObjectIds.splice(i,1);else selectedObjectIds.push(x)})}else selectedObjectIds=ids;
  render();renderObjectList();updateObjectInspector();
}
function renderObjectList(){
  $('objectCount').textContent=`${customObjects.length} 个对象`;
  $('objectList').innerHTML=customObjects.length?[...customObjects].reverse().map(o=>{const icon={rectangle:'□',ellipse:'○',line:'╱',text:'字',path:'✎'}[o.type]||'◇',name=o.type==='text'?(o.text||'文字'):{rectangle:'矩形',ellipse:'椭圆',line:'直线',path:'手绘路径'}[o.type]||o.type;return `<div class="object-row ${selectedObjectIds.includes(o.id)?'active':''}" data-list-object="${o.id}"><span class="object-icon">${icon}</span><span class="object-name">${escapeXML(name)}</span><span class="object-lock">${o.groupId?'组合 ':''}${o.locked?'🔒':''}</span></div>`}).join(''):'<div class="empty-object">使用左侧工具在画布上绘制</div>';
  $('objectList').querySelectorAll('[data-list-object]').forEach(row=>row.onclick=e=>selectObject(row.dataset.listObject,e.shiftKey));
}
function updateObjectInspector(){
  const items=selectedObjects(),o=items[0],fields=['objectX','objectY','objectW','objectH','objectRotation','objectFill','objectStroke','objectStrokeWidth','applyObjectProps'];
  fields.forEach(id=>$(id).disabled=!o);$('selectionStatus').textContent=o?`已选择 ${items.length} 个对象${o.locked?' · 已锁定':''}`:'未选择对象';
  $('objectText').disabled=!(items.length===1&&o?.type==='text');$('objectText').value=items.length===1&&o?.type==='text'?o.text||'':'';
  if(!o)return;const b=objectBounds(o);$('objectX').value=Math.round(b.x);$('objectY').value=Math.round(b.y);$('objectW').value=Math.round(b.w);$('objectH').value=Math.round(b.h);$('objectRotation').value=o.rotation||0;$('objectFill').value=/^#[0-9a-f]{6}$/i.test(o.fill||'')?o.fill:'#ffffff';$('objectStroke').value=/^#[0-9a-f]{6}$/i.test(o.stroke||'')?o.stroke:'#222222';$('objectStrokeWidth').value=o.strokeWidth??2;
}
function commitVector(message){render();renderObjectList();updateObjectInspector();recordHistory();saveDraft();setStatus(message)}
function svgPoint(e){const svg=$('board'),p=svg.createSVGPoint();p.x=e.clientX;p.y=e.clientY;const q=p.matrixTransform(svg.getScreenCTM().inverse());return {x:q.x,y:q.y}}
function snap(v){return smartGuides?Math.round(v/5)*5:v}
function defaultObject(type,x,y){
  const base={id:nextObjectId(),type,x:snap(x),y:snap(y),width:1,height:1,rotation:0,fill:'#f4c542',stroke:'#222222',strokeWidth:2,locked:false};
  if(type==='line'||type==='path')base.fill='none';if(type==='text')Object.assign(base,{width:180,height:38,text:'双击编辑文字',fontSize:28,fill:'#222222',stroke:'none',strokeWidth:0});return base;
}
function deleteSelected(){if(!selectedObjectIds.length)return;customObjects=customObjects.filter(o=>!selectedObjectIds.includes(o.id));selectedObjectIds=[];commitVector('已删除所选对象')}
function duplicateSelected(){const items=selectedObjects();if(!items.length)return;const map=new Map(),copies=items.map(o=>{const c=structuredClone(o);c.id=nextObjectId();c.x=(c.x||0)+18;c.y=(c.y||0)+18;if(c.points)c.points=c.points.map(p=>({x:p.x+18,y:p.y+18}));if(c.groupId){if(!map.has(c.groupId))map.set(c.groupId,`group-${Date.now()}-${map.size}`);c.groupId=map.get(c.groupId)}return c});customObjects.push(...copies);selectedObjectIds=copies.map(o=>o.id);commitVector(`已复制 ${copies.length} 个对象`)}
function arrangeSelected(action){
  if(!selectedObjectIds.length)return;const chosen=customObjects.filter(o=>selectedObjectIds.includes(o.id)),rest=customObjects.filter(o=>!selectedObjectIds.includes(o.id));
  if(action==='front')customObjects=[...rest,...chosen];else if(action==='back')customObjects=[...chosen,...rest];else for(const o of [...chosen]){let i=customObjects.indexOf(o),j=action==='up'?Math.min(customObjects.length-1,i+1):Math.max(0,i-1);[customObjects[i],customObjects[j]]=[customObjects[j],customObjects[i]]}commitVector('已调整对象层级');
}
function alignSelected(kind){
  const items=selectedObjects();if(!items.length)return;const bounds=items.map(objectBounds),page=layoutFor(currentData()),target=items.length===1?{x:page.ox,y:page.oy,w:page.W,h:page.H}:{x:Math.min(...bounds.map(b=>b.x)),y:Math.min(...bounds.map(b=>b.y)),w:Math.max(...bounds.map(b=>b.x+b.w))-Math.min(...bounds.map(b=>b.x)),h:Math.max(...bounds.map(b=>b.y+b.h))-Math.min(...bounds.map(b=>b.y))};
  items.forEach((o,i)=>{const b=bounds[i];let dx=0,dy=0;if(kind==='left')dx=target.x-b.x;if(kind==='center')dx=target.x+target.w/2-(b.x+b.w/2);if(kind==='right')dx=target.x+target.w-(b.x+b.w);if(kind==='top')dy=target.y-b.y;if(kind==='middle')dy=target.y+target.h/2-(b.y+b.h/2);if(kind==='bottom')dy=target.y+target.h-(b.y+b.h);moveObject(o,dx,dy)});commitVector(`已执行${kind}对齐`);
}
function moveObject(o,dx,dy){if(o.points)o.points=o.points.map(p=>({x:p.x+dx,y:p.y+dy}));else{o.x+=dx;o.y+=dy}}
function groupSelected(){const items=selectedObjects();if(items.length<2)return setStatus('请至少选择两个对象再组合');const id=`group-${Date.now()}`;items.forEach(o=>o.groupId=id);commitVector(`已组合 ${items.length} 个对象`)}
function ungroupSelected(){const items=selectedObjects();if(!items.length)return;items.forEach(o=>delete o.groupId);commitVector('已取消对象组合')}
function setSelectedColor(kind,color){const items=selectedObjects();if(!items.length)return setStatus('请先选择对象');items.forEach(o=>o[kind]=color);commitVector(kind==='fill'?'已更改对象填充色':'已更改对象轮廓色')}

function canvasPointerDown(e){
  if(e.button!==0)return;const p=svgPoint(e),target=e.target.closest?.('[data-object-id]'),id=target?.dataset.objectId;
  if(activeTool==='zoom'){setPreviewZoom(previewZoom+(e.shiftKey?-.15:.15));return}
  if(activeTool==='eyedropper'){const o=customObjects.find(x=>x.id===id);if(o){$('objectFill').value=/^#[0-9a-f]{6}$/i.test(o.fill)?o.fill:'#ffffff';setStatus(`已吸取颜色 ${o.fill}`)}return}
  if(activeTool==='text'){const o=defaultObject('text',p.x,p.y);customObjects.push(o);selectedObjectIds=[o.id];commitVector('已添加文字对象');return}
  if(activeTool==='select'||activeTool==='node'){
    if(!id){selectedObjectIds=[];render();renderObjectList();updateObjectInspector();return}if(!selectedObjectIds.includes(id))selectObject(id,e.shiftKey);const o=customObjects.find(x=>x.id===id);if(!o||o.locked)return;
    if(target.dataset.nodeIndex!==undefined){drawState={mode:'node',id,nodeIndex:+target.dataset.nodeIndex};}
    else if(target.dataset.resize){drawState={mode:'resize',id,handle:target.dataset.resize,start:p,original:structuredClone(o),bounds:objectBounds(o)};}
    else{const ids=selectedObjectIds.includes(id)?selectedObjectIds:[id];drawState={mode:'move',start:p,originals:ids.map(x=>structuredClone(customObjects.find(o=>o.id===x))).filter(Boolean)};}
    $('board').setPointerCapture(e.pointerId);return;
  }
  const type=activeTool==='freehand'?'path':activeTool,o=defaultObject(type,p.x,p.y);if(type==='path')o.points=[{x:snap(p.x),y:snap(p.y)}];customObjects.push(o);selectedObjectIds=[o.id];drawState={mode:'draw',id:o.id,start:p};$('board').setPointerCapture(e.pointerId);render();renderObjectList();updateObjectInspector();
}
function canvasPointerMove(e){
  let p;try{p=svgPoint(e)}catch{return}$('cursorPosition').textContent=`X: ${Math.round(p.x)}　Y: ${Math.round(p.y)}`;if(!drawState)return;
  if(drawState.mode==='move'){const dx=snap(p.x-drawState.start.x),dy=snap(p.y-drawState.start.y);drawState.originals.forEach(orig=>{const o=customObjects.find(x=>x.id===orig.id);if(!o)return;Object.assign(o,structuredClone(orig));moveObject(o,dx,dy)})}
  else if(drawState.mode==='node'){const o=customObjects.find(x=>x.id===drawState.id);if(o?.points?.[drawState.nodeIndex])o.points[drawState.nodeIndex]={x:snap(p.x),y:snap(p.y)}}
  else if(drawState.mode==='resize'){const o=customObjects.find(x=>x.id===drawState.id),b=drawState.bounds;if(o){let x1=b.x,y1=b.y,x2=b.x+b.w,y2=b.y+b.h;if(drawState.handle.includes('w'))x1=snap(p.x);else x2=snap(p.x);if(drawState.handle.includes('n'))y1=snap(p.y);else y2=snap(p.y);const nb={x:Math.min(x1,x2),y:Math.min(y1,y2),w:Math.max(2,Math.abs(x2-x1)),h:Math.max(2,Math.abs(y2-y1))};if(o.points){o.points=drawState.original.points.map(pt=>({x:nb.x+(pt.x-b.x)/b.w*nb.w,y:nb.y+(pt.y-b.y)/b.h*nb.h}))}else Object.assign(o,{x:nb.x,y:nb.y,width:nb.w,height:nb.h})}}
  else if(drawState.mode==='draw'){const o=customObjects.find(x=>x.id===drawState.id);if(o?.type==='path'){const last=o.points[o.points.length-1];if(Math.hypot(p.x-last.x,p.y-last.y)>4)o.points.push({x:snap(p.x),y:snap(p.y)})}else if(o){o.width=snap(p.x-drawState.start.x);o.height=snap(p.y-drawState.start.y)}}render();updateObjectInspector();
}
function canvasPointerUp(e){if(!drawState)return;const mode=drawState.mode;drawState=null;try{$('board').releasePointerCapture(e.pointerId)}catch{}const o=selectedObjects()[0];if(mode==='draw'&&o&&o.type!=='path'&&Math.abs(o.width)<3&&Math.abs(o.height)<3){o.width=80;o.height=o.type==='line'?0:60}commitVector(mode==='draw'?'已创建矢量对象':'已更新对象')}

$('board').addEventListener('pointerdown',canvasPointerDown);$('board').addEventListener('pointermove',canvasPointerMove);$('board').addEventListener('pointerup',canvasPointerUp);$('board').addEventListener('pointercancel',canvasPointerUp);
$('board').addEventListener('dblclick',e=>{const id=e.target.closest?.('[data-object-id]')?.dataset.objectId,o=customObjects.find(x=>x.id===id);if(o?.type==='text'){const text=prompt('编辑文字',o.text||'');if(text!==null){o.text=text;o.width=Math.max(80,[...text].length*(o.fontSize||28));selectedObjectIds=[o.id];commitVector('文字内容已更新')}}});
document.querySelectorAll('[data-tool]').forEach(b=>b.onclick=()=>setActiveTool(b.dataset.tool));
$('gridToggle').onclick=()=>{$('stage').classList.toggle('show-grid');$('gridToggle').classList.toggle('active');setStatus($('stage').classList.contains('show-grid')?'已显示网格':'已隐藏网格')};
$('guidesToggle').onclick=()=>{smartGuides=!smartGuides;$('stage').classList.toggle('smart-guides',smartGuides);$('guidesToggle').classList.toggle('active',smartGuides);setStatus(smartGuides?'智能参考线与5单位吸附已开启':'智能参考线已关闭')};
$('colorPalette').innerHTML=paletteColors.map(c=>`<button class="color-swatch ${c==='none'?'none':''}" data-color="${c}" title="左键填充；右键轮廓${c==='none'?'：无色':`：${c}`}" style="${c==='none'?'':`background:${c}`}"></button>`).join('');
$('colorPalette').querySelectorAll('[data-color]').forEach(b=>{b.onclick=()=>setSelectedColor('fill',b.dataset.color);b.oncontextmenu=e=>{e.preventDefault();setSelectedColor('stroke',b.dataset.color)}});
$('applyObjectProps').onclick=()=>{const items=selectedObjects();if(!items.length)return;const o=items[0],b=objectBounds(o),nx=+$('objectX').value,ny=+$('objectY').value,nw=Math.max(1,+$('objectW').value),nh=Math.max(1,+$('objectH').value);items.forEach(item=>{if(item===o){moveObject(item,nx-b.x,ny-b.y);if(!item.points){item.width=nw;item.height=nh}}item.rotation=+$('objectRotation').value;item.fill=$('objectFill').value;item.stroke=$('objectStroke').value;item.strokeWidth=+$('objectStrokeWidth').value});commitVector('对象属性已应用')};
$('objectText').oninput=e=>{const o=selectedObjects()[0];if(o?.type==='text'){o.text=e.target.value;o.width=Math.max(80,[...o.text].length*(o.fontSize||28));render();queueStateSave()}};
$('objectText').onchange=()=>commitVector('文字内容已更新');$('duplicateObject').onclick=duplicateSelected;$('deleteObject').onclick=deleteSelected;document.querySelectorAll('[data-arrange]').forEach(b=>b.onclick=()=>arrangeSelected(b.dataset.arrange));document.querySelectorAll('[data-align]').forEach(b=>b.onclick=()=>alignSelected(b.dataset.align));$('groupObjects').onclick=groupSelected;$('ungroupObjects').onclick=ungroupSelected;
$('lockObject').onclick=()=>{const items=selectedObjects();if(!items.length)return;const lock=!items.every(o=>o.locked);items.forEach(o=>o.locked=lock);commitVector(lock?'对象已锁定':'对象已解锁')};
$('clearObjects').onclick=()=>{if(customObjects.length&&confirm('确定清空所有自绘矢量对象吗？')){customObjects=[];selectedObjectIds=[];commitVector('已清空自绘对象')}};
$('newBoard').onclick=()=>{if(confirm('新建空白设计？当前草稿仍可通过撤销恢复。')){customObjects=[];selectedObjectIds=[];$('title').value='新制度展板';$('body').value='一、请输入制度内容。';$('sign').value='';commitVector('已新建制度展板')}};$('undoTop').onclick=()=>$('undoDesign').click();$('redoTop').onclick=()=>$('redoDesign').click();
document.querySelectorAll('[data-menu]').forEach(b=>b.onclick=()=>{const tips={file:'文件导出位于右上角，可生成CDR、SVG、PDF和高清图片',edit:'使用顶部按钮或快捷键撤销、重做、复制和删除',view:'可切换网格、参考线与缩放比例',layout:'使用顶部对齐和层级按钮整理版式',object:'右侧“对象与图层”可组合、锁定和编辑对象',effect:'封套、阴影、轮廓图等深度效果请送入CorelDRAW 2020',bitmap:'PowerTRACE、抠图与位图滤镜请送入CorelDRAW 2020',text:'左侧“字”工具可新增美术字并编辑内容',table:'表格对象将在后续版本加入',tools:'左侧提供选择、节点、钢笔、图形、文字和吸管工具',window:'当前为单文档设计工作区',help:'快捷键：V选择、N节点、P手绘、R矩形、E椭圆、L直线、T文字、Z缩放'};setStatus(tips[b.dataset.menu]||'就绪')});

document.querySelectorAll('input,textarea,select').forEach(e=>e.addEventListener('input',()=>{if(!['templateSearch','localTemplateSearch','localIndustry','apiKey','endpoint','model','policyName','zoomRange'].includes(e.id))render();if(!['templateSearch','localTemplateSearch','localIndustry','apiKey','endpoint','model','zoomRange'].includes(e.id))queueStateSave()}));
$('logo').onchange=async e=>{if(e.target.files[0]){logo=await dataURL(e.target.files[0]);commitState('LOGO已载入')}};
$('reference').onchange=async e=>{if(e.target.files[0]){reference=await dataURL(e.target.files[0]);commitState('样图已载入，可执行OCR和版式识别');$('analysisResult').textContent='样图已载入，可执行OCR和版式识别'}};

document.querySelectorAll('[data-export]').forEach(b=>b.onclick=async()=>{try{const type=b.dataset.export,d=currentData(),svg=makeSVG(d);setStatus('正在导出 '+type.toUpperCase()+'…');if(type==='svg')await boardAPI.saveSVG(svg);else if(type==='cdr')await boardAPI.saveCDR({svg,cmyk:d.cmyk});else if(type==='pdf')await boardAPI.savePDF({html:pdfHtml(svg,d)});else await boardAPI.saveImage({ext:type,data:await rasterData(d,type)});setStatus('导出完成')}catch(e){setStatus('导出失败：'+e.message)}});

function fillPolicyTypes(){const types=INDUSTRIES[$('industry').value]||[];$('policyTypes').innerHTML=types.map((x,i)=>`<option ${i===0?'selected':''}>${x}</option>`).join('');if(!$('policyName').value&&types[0])$('policyName').value=types[0]}
function applyBoard(b,index=aiBoards.indexOf(b)){activeBoardIndex=Math.max(0,index);$('title').value=b.title;$('body').value=normalizeBody(b.body);$('theme').value=b.theme||'red';$('width').value=b.recommended_width_cm||40;$('height').value=b.recommended_height_cm||60;$('sign').value=$('company').value;document.querySelectorAll('.result').forEach((x,i)=>x.classList.toggle('active',i===activeBoardIndex));commitState(b.review_note?`已整理排版；复核提示：${b.review_note}`:'AI内容已自动整理排版')}
function showAIResults(loadFirst=true){
  const box=$('aiResults');box.innerHTML=aiBoards.length?`<div class="result-summary">批量队列 ${aiBoards.length} 张 · 可调整顺序</div>`+aiBoards.map((b,i)=>`<div class="result ${i===activeBoardIndex?'active':''}" data-index="${i}"><div class="result-main"><b>${i+1}. ${escapeXML(b.title)}</b><span>点击载入编辑</span>${b.review_note?`<div class="review">需复核：${escapeXML(b.review_note)}</div>`:''}</div><div class="result-actions"><button data-result-action="up" title="上移">↑</button><button data-result-action="down" title="下移">↓</button><button data-result-action="delete" title="删除">×</button></div></div>`).join(''):'';
  box.querySelectorAll('.result').forEach(card=>card.onclick=e=>{const i=+card.dataset.index,a=e.target.dataset.resultAction;if(a){e.stopPropagation();if(a==='delete'){aiBoards.splice(i,1);activeBoardIndex=Math.min(activeBoardIndex,Math.max(0,aiBoards.length-1))}else{const target=a==='up'?i-1:i+1;if(target>=0&&target<aiBoards.length){[aiBoards[i],aiBoards[target]]=[aiBoards[target],aiBoards[i]];activeBoardIndex=target}}showAIResults(false);recordHistory();saveDraft();setStatus(`批量队列剩余 ${aiBoards.length} 张`)}else applyBoard(aiBoards[i],i)});
  if(loadFirst&&aiBoards[0])applyBoard(aiBoards[0],0);
}
Object.keys(INDUSTRIES).forEach(x=>$('industry').add(new Option(x,x)));fillPolicyTypes();$('industry').addEventListener('change',fillPolicyTypes);$('policyTypes').addEventListener('change',()=>{const x=$('policyTypes').selectedOptions[0];if(x)$('policyName').value=x.value});
Object.keys(INDUSTRIES).forEach(x=>$('localIndustry').add(new Option(x,x)));
function applyLocalTemplate(t){
  $('policyName').value=t.title;$('title').value=t.title;$('body').value=normalizeBody(t.body);$('theme').value=t.theme;$('width').value=t.width;$('height').value=t.height;
  $('industry').value=t.industry;fillPolicyTypes();$('templateIndustry').value=t.industry;aiBoards=[];commitState(`已套用本地模板：${t.industry} / ${t.title}；交付前请人工复核`);
}
function renderLocalTemplates(){
  const industry=$('localIndustry').value,q=$('localTemplateSearch').value.trim().toLowerCase();
  visibleLocalTemplates=LOCAL_TEMPLATES.filter(t=>(!industry||t.industry===industry)&&(!q||`${t.industry} ${t.title}`.toLowerCase().includes(q)));
  $('localTemplateCount').textContent=`共 ${LOCAL_TEMPLATES.length} 套；当前显示 ${visibleLocalTemplates.length} 套`;
  $('localTemplateList').innerHTML=visibleLocalTemplates.slice(0,100).map((t,i)=>`<div class="local-template-card" data-local-index="${i}"><div><b>${escapeXML(t.title)}</b><span>${escapeXML(t.industry)} · ${t.width}×${t.height}cm · 本地</span></div><button type="button">套用</button></div>`).join('');
  $('localTemplateList').querySelectorAll('.local-template-card').forEach(card=>card.onclick=()=>applyLocalTemplate(visibleLocalTemplates[+card.dataset.localIndex]));
}
$('localIndustry').onchange=renderLocalTemplates;$('localTemplateSearch').oninput=renderLocalTemplates;renderLocalTemplates();
$('saveAI').onclick=async()=>{await boardAPI.saveAIConfig({endpoint:$('endpoint').value.trim(),model:$('model').value.trim(),apiKey:$('apiKey').value.trim()});setStatus('AI连接设置已保存在本机')};
async function generateBoards(advanced=false){
  const names=requestedPolicyNames(),selected=[...$('policyTypes').selectedOptions].map(x=>x.value),types=advanced?selected:names;
  if(!types.length)return setStatus('请输入需要制作的制度名称');
  if(!$('apiKey').value.trim())return setStatus('请先展开“AI连接设置”并填写API密钥');
  const button=advanced?$('aiGenerate'):$('quickGenerate');
  try{button.disabled=true;setStatus(`正在生成 ${types.length} 张制度牌并自动整理排版…`);const data=await boardAPI.generateAI({endpoint:$('endpoint').value.trim(),model:$('model').value.trim(),apiKey:$('apiKey').value.trim(),policyNames:types,autoMode:!advanced,industry:advanced?$('industry').value:'自动识别',types,count:advanced?Math.max(types.length,+$('boardCount').value):types.length,words:+$('words').value,company:$('company').value.trim(),requirements:$('requirements').value.trim()});aiBoards=(data.boards||[]).slice(0,20).map(b=>({...b,body:normalizeBody(b.body)}));showAIResults();setStatus(`已生成、排序并排版 ${aiBoards.length} 张展板`)}catch(e){setStatus('AI生成失败：'+e.message)}finally{button.disabled=false}
}
$('quickGenerate').onclick=()=>generateBoards(false);
$('aiGenerate').onclick=()=>generateBoards(true);
$('policyName').addEventListener('keydown',e=>{if(e.key==='Enter'&&(e.ctrlKey||e.metaKey)){e.preventDefault();generateBoards(false)}});
document.querySelectorAll('[data-policy]').forEach(b=>b.onclick=()=>{$('policyName').value=b.dataset.policy;$('policyName').focus()});
$('sortText').onclick=()=>{$('body').value=normalizeBody($('body').value);commitState('正文已按职责、执行、检查、应急顺序整理并美化')};
$('smartOptimize').onclick=()=>{
  const first=requestedPolicyNames()[0];if(!$('title').value.trim()&&first)$('title').value=first;$('body').value=normalizeBody($('body').value);
  const text=$('title').value+$('body').value,chars=[...$('body').value.replace(/\s/g,'')].length;$('theme').value=inferTheme(text);
  const size=chars>700?[60,80]:chars>480?[50,70]:[40,60];$('width').value=size[0];$('height').value=size[1];$('titleSize').value=[...$('title').value].length>18?34:42;$('bodySize').value=chars>650?18:chars>450?20:22;$('padding').value=25;$('dpi').value=300;$('bleed').value=3;$('cropMarks').checked=true;$('cmyk').checked=true;commitState(`智能优化完成：${size[0]}×${size[1]}cm · ${$('theme').selectedOptions[0].text} · 300dpi`);
};
document.querySelectorAll('[data-size]').forEach(b=>b.onclick=()=>{const [w,h]=b.dataset.size.split('x').map(Number);$('width').value=w;$('height').value=h;commitState(`已切换常用尺寸 ${w}×${h}cm`)});
$('undoDesign').onclick=()=>{if(historyIndex<=0)return;historyIndex--;applyState(history[historyIndex].state);updateUndoButtons();saveDraft();setStatus('已撤销上一步')};
$('redoDesign').onclick=()=>{if(historyIndex>=history.length-1)return;historyIndex++;applyState(history[historyIndex].state);updateUndoButtons();saveDraft();setStatus('已重做上一步')};
document.addEventListener('keydown',e=>{if(!(e.ctrlKey||e.metaKey))return;if(e.key.toLowerCase()==='z'){e.preventDefault();e.shiftKey?$('redoDesign').click():$('undoDesign').click()}else if(e.key.toLowerCase()==='y'){e.preventDefault();$('redoDesign').click()}else if(e.key.toLowerCase()==='s'){e.preventDefault();saveDraft();setStatus('当前设计已保存到本机草稿')}else if(e.key.toLowerCase()==='d'){e.preventDefault();duplicateSelected()}});
document.addEventListener('keydown',e=>{
  if(e.ctrlKey||e.metaKey||/INPUT|TEXTAREA|SELECT/.test(document.activeElement?.tagName||''))return;
  if(e.key==='Delete'||e.key==='Backspace'){e.preventDefault();deleteSelected();return}if(e.key==='Escape'){selectedObjectIds=[];setActiveTool('select');renderObjectList();updateObjectInspector();return}
  const shortcuts={v:'select',n:'node',p:'freehand',r:'rectangle',e:'ellipse',l:'line',t:'text',z:'zoom',i:'eyedropper'},tool=shortcuts[e.key.toLowerCase()];if(tool){e.preventDefault();setActiveTool(tool);return}
  if(['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(e.key)&&selectedObjectIds.length){e.preventDefault();const step=e.shiftKey?10:1,dx=e.key==='ArrowLeft'?-step:e.key==='ArrowRight'?step:0,dy=e.key==='ArrowUp'?-step:e.key==='ArrowDown'?step:0;selectedObjects().filter(o=>!o.locked).forEach(o=>moveObject(o,dx,dy));commitVector('已微移所选对象')}
});
$('zoomRange').oninput=e=>setPreviewZoom(+e.target.value/100);$('zoomOut').onclick=()=>setPreviewZoom(previewZoom-.1);$('zoomIn').onclick=()=>setPreviewZoom(previewZoom+.1);$('zoomFit').onclick=()=>setPreviewZoom(1);window.addEventListener('resize',applyPreviewZoom);

$('ocrButton').onclick=async()=>{if(!reference)return setStatus('请先上传样图');try{$('ocrButton').disabled=true;setStatus('正在进行中文OCR识别…');const r=await boardAPI.ocrImage(reference),lines=r.text.split(/\n/).map(x=>x.trim()).filter(Boolean);if(lines.length){if(lines[0].length<=32)$('title').value=lines.shift();$('body').value=normalizeBody(lines.join('\n'));commitState(`OCR完成，平均置信度 ${Math.round(r.confidence)}%`)}else setStatus('OCR未识别到文字')}catch(e){setStatus('OCR失败：'+e.message)}finally{$('ocrButton').disabled=false}};
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
  $('analysisResult').className='small-note analyzed';$('analysisResult').textContent=`底色 ${analysis.background}；主色 ${analysis.accent}；边框内缩约 ${Math.round((bx+by)/2*100)}%；标题区约在顶部 ${Math.round(titleY/h*100)}%；LOGO候选：${analysis.logoPosition}`;commitState('样图版式分析完成');
}
$('analyzeButton').onclick=()=>analyzeReference().catch(e=>setStatus(e.message));

async function refreshTemplates(){const q=$('templateSearch').value.trim().toLowerCase(),list=await boardAPI.listTemplates(),filtered=list.filter(x=>!q||[x.name,x.industry,x.data?.title].join(' ').toLowerCase().includes(q));$('templateList').innerHTML=filtered.map(x=>`<div class="template-card" data-id="${x.id}"><b>${x.favorite?'★ ':''}${escapeXML(x.name)}</b><div>${escapeXML(x.industry||'未分类')} · ${escapeXML(x.data?.title||'')}</div><div class="template-actions"><button data-a="use">使用</button><button data-a="fav">收藏</button><button data-a="del">删除</button></div></div>`).join('');$('templateList').querySelectorAll('.template-card').forEach(card=>card.onclick=async e=>{const item=list.find(x=>x.id===card.dataset.id),a=e.target.dataset.a;if(a==='use'){const d=item.data;Object.entries({title:d.title,body:d.body,sign:d.sign,width:d.width,height:d.height,theme:d.theme,padding:d.padding,titleSize:d.titleSize,bodySize:d.bodySize,dpi:d.dpi,bleed:d.bleed}).forEach(([k,v])=>{if($(k)&&v!==undefined)$(k).value=v});analysis=d.analysis||analysis;logo=d.logo||'';commitState(`已载入我的模板：${item.name}`)}else if(a==='fav'){await boardAPI.favoriteTemplate(item.id);refreshTemplates()}else if(a==='del'){await boardAPI.deleteTemplate(item.id);refreshTemplates()}})}
$('saveTemplate').onclick=async()=>{const name=$('templateName').value.trim()||$('title').value;await boardAPI.saveTemplate({name,industry:$('templateIndustry').value.trim()||$('industry').value,favorite:false,data:currentData({reference:''})});setStatus('模板已保存');refreshTemplates()};$('templateSearch').oninput=refreshTemplates;

$('batchExport').onclick=async()=>{const formats=[...document.querySelectorAll('.batchFormat:checked')].map(x=>x.value);if(!formats.length)return setStatus('请至少选择一种批量格式');const source=aiBoards.length?aiBoards:[{title:$('title').value,body:$('body').value,theme:$('theme').value,recommended_width_cm:+$('width').value,recommended_height_cm:+$('height').value}];try{setStatus(`正在准备 ${source.length} 张批量文件…`);const items=source.slice(0,20).map(b=>{const d=currentData({title:b.title,body:b.body,theme:b.theme||$('theme').value,width:b.recommended_width_cm||+$('width').value,height:b.recommended_height_cm||+$('height').value,sign:$('company').value||$('sign').value,reference:''}),svg=makeSVG(d);return {title:b.title,svg,rasterSvg:makeSVG(d,{raster:true,dpi:d.dpi}),pdfHtml:pdfHtml(svg,d)}});const r=await boardAPI.batchExport({items,formats,cmyk:$('cmyk').checked});setStatus(r?`批量导出完成：${r.files.length}个文件，目录 ${r.directory}`:'已取消批量导出')}catch(e){setStatus('批量导出失败：'+e.message)}};
function formatCorelResult(result){
  if(result.preflight){const p=result.preflight;return [`印前预检：${p.pages}页 / ${p.shapes}个对象`,`文字对象：${p.text}　位图：${p.bitmaps}　低于300dpi：${p.lowResolutionBitmaps}`,`非CMYK均匀色/轮廓：${p.nonCMYKColors}　复杂填充：${p.complexFills}`,`文档分辨率：${p.documentDpi}dpi${p.minimumBitmapDpi?`　最低位图：${p.minimumBitmapDpi}dpi`:''}`,'',...p.warnings.map(x=>'• '+x)].join('\n')}
  return [result.message,result.file?`文件：${result.file}`:'',result.programPath?`程序：${result.programPath}`:''].filter(Boolean).join('\n');
}
async function runCorelTool(action){
  const buttons=[$('corelOpen'),...document.querySelectorAll('[data-corel-action]')],badge=$('corelBadge'),d=currentData();
  try{
    buttons.forEach(x=>x.disabled=true);badge.className='corel-badge busy';badge.textContent='处理中';$('corelResult').textContent='正在连接 CorelDRAW 2020（64-Bit）…';setStatus('正在执行 CorelDRAW 2020 专业联动…');
    const raw=await boardAPI.corelTool({action,svg:makeSVG(d),title:d.title,pageWidthCM:d.width+d.bleed/5,pageHeightCM:d.height+d.bleed/5,dpi:d.dpi,bleedMM:d.bleed,cropMarks:d.cropMarks}),result=JSON.parse(raw);
    if(result.cancelled){$('corelResult').textContent='已取消保存';setStatus('已取消CorelDRAW保存');return}
    badge.className='corel-badge ok';badge.textContent='2020已连接';$('corelResult').textContent=formatCorelResult(result);setStatus(result.message||'CorelDRAW操作完成');
  }catch(e){badge.className='corel-badge error';badge.textContent='连接失败';$('corelResult').textContent=e.message;setStatus('CorelDRAW操作失败：'+e.message)}finally{buttons.forEach(x=>x.disabled=false)}
}
$('corelOpen').onclick=()=>runCorelTool('Import');
document.querySelectorAll('[data-corel-action]').forEach(button=>button.onclick=()=>runCorelTool(button.dataset.corelAction));
$('corelTest').onclick=async()=>{try{$('corelResult').textContent='检测中…';const raw=await boardAPI.corelTest(),data=JSON.parse(raw);$('corelResult').textContent=data.map(x=>`${x.Version}：${x.CreateAndSave?'通过':'未通过'}（${x.Message}）`).join('\n')}catch(e){$('corelResult').textContent=e.message}};

boardAPI.getAIConfig().then(c=>{if(c.endpoint)$('endpoint').value=c.endpoint;if(c.model)$('model').value=c.model;if(c.apiKey)$('apiKey').value=c.apiKey});refreshTemplates();if(!restoreDraft())render();renderObjectList();updateObjectInspector();$('stage').classList.add('smart-guides');$('guidesToggle').classList.add('active');recordHistory();updateUndoButtons();
