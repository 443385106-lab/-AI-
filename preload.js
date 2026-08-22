const {contextBridge,ipcRenderer}=require('electron');
contextBridge.exposeInMainWorld('boardAPI',{
 saveSVG:s=>ipcRenderer.invoke('save-svg',s),saveImage:a=>ipcRenderer.invoke('save-image',a),
 savePDF:h=>ipcRenderer.invoke('save-pdf',h),saveCDR:s=>ipcRenderer.invoke('save-cdr',s),
 getAIConfig:()=>ipcRenderer.invoke('ai-config-get'),saveAIConfig:c=>ipcRenderer.invoke('ai-config-save',c),generateAI:a=>ipcRenderer.invoke('ai-generate',a),
 ocrImage:d=>ipcRenderer.invoke('ocr-image',d),corelTest:()=>ipcRenderer.invoke('corel-test'),corelTool:a=>ipcRenderer.invoke('corel-tool',a),batchExport:a=>ipcRenderer.invoke('batch-export',a),
 listTemplates:()=>ipcRenderer.invoke('templates-list'),saveTemplate:t=>ipcRenderer.invoke('template-save',t),deleteTemplate:id=>ipcRenderer.invoke('template-delete',id),favoriteTemplate:id=>ipcRenderer.invoke('template-favorite',id)
});
