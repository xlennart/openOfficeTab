import {t,setLocale,currentLocale,resolveLanguage,errorText,bindStaticTranslations} from './localization.js';
import {createIcons,ArrowUpRight,Check,CircleArrowUp,CircleCheck,CodeXml,House,Info,Minus,PackageCheck,SlidersHorizontal,Square,Star,Wrench,X} from 'lucide';

const root=document.getElementById('oot-design');
const $=id=>root.querySelector('#'+id.replace(/^#/,''));
const invoke=(name,args={})=>window.__TAURI__.core.invoke(name,name==='service'?{...args,request:{...args.request,language:currentLocale()}}:name==='export_diagnostics'?{...args,language:currentLocale()}:args);
const frame=window.__TAURI__.window.getCurrentWindow();
const defaults={theme:'auto',style:'open',width:120,widthMode:'shrink',autoHide:false,doubleClose:false};
const state={route:'installer',host:'Word',active:1,...defaults};
const files={Word:['项目计划.docx','会议纪要.docx','调研笔记.docx'],Excel:['年度预算.xlsx','采购清单.xlsx','项目统计.xlsx'],PowerPoint:['项目汇报.pptx','研究展示.pptx','团队介绍.pptx']};
const colors={Word:'#235bb4',Excel:'#107c41',PowerPoint:'#c43b1c'};
let snapshot=null,savedNative=null,savedAppearance={...defaults},busy=false,release=null,waitingUpdate=false,updateGeneration=0;
let languageOverride=null,toastState=null,updatePhase='checked',automaticCheckFailed=false,installedHosts=null;
const translateStatic=bindStaticTranslations(root);
let lastDiagnostics=null;
let captionUpdate=Promise.resolve();
function refreshCaption(){
 $('oot-caption').textContent=state.route==='installer'?t('安装与恢复'):t('管理中心');
 const title=state.route==='installer'?t('openOfficeTab 安装器'):t('openOfficeTab 管理中心');
 document.title=title;
 captionUpdate=captionUpdate.then(()=>frame.setTitle(title)).catch(()=>{});
}
function placeLanguage(){
 const slot=state.route==='installer'?($('oot-install-done').hidden?$('oot-install-language'):$('oot-done-language')):$('oot-center-language');
 slot.append(root.querySelector('.o-language'));
}
function applySnapshotLanguage(value){
 const saved=value.language||{mode:0,locale:resolveLanguage(0,navigator.language),systemLocale:navigator.language};
 const mode=languageOverride==null?saved.mode:({'auto':0,'zh-CN':1,en:2}[languageOverride]??saved.mode);
 const locale=languageOverride==null?saved.locale:resolveLanguage(mode,saved.systemLocale);
 const changed=currentLocale()!==locale||!root.dataset.languageReady;
 if(changed){setLocale(locale);translateStatic();refreshCaption();root.dataset.languageReady='true';invoke('set_update_language',{language:locale}).catch(()=>{});}
 $('oot-language').value=String(mode);return changed;
}
function renderUpdateResult(){
 const version=snapshot?.version||'…';
 $('oot-update-description').textContent=automaticCheckFailed?t('管理中心 {version} · 更新检查暂不可用',{version}):t('管理中心 {version}',{version});
 if(!release)return;
 $('oot-update-result').hidden=false;
 const titles={waiting:'更新已下载，等待 Office 退出',installed:'更新安装完成'};
 $('oot-update-result-title').textContent=titles[updatePhase]?t(titles[updatePhase]):release.available?t('发现新版本 {version}',{version:release.version}):t('当前无需更新');
 const notes={downloaded:'更新已下载，安装前会重新校验。',waiting:'你的文档不会被关闭。保持管理中心打开后将继续；已开启自动安装时也会在后台继续。',installed:'关闭并重新打开管理中心即可使用新版。'};
 $('oot-update-result-note').textContent=notes[updatePhase]?t(notes[updatePhase]):release.available?t('更新清单已验签；下载后核对 SHA-256，安装前检查 Office。'):t('所选通道版本：{version}。',{version:release.version||t('暂无可用版本')});
 $('oot-install-update').hidden=!release.available||updatePhase==='installed';
}
root.querySelectorAll('[data-brand]').forEach(image=>image.src='./assets/brand.png');
root.querySelectorAll('button,a,input,select,label').forEach(control=>control.classList.add('cursor-interaction'));
function toast(message,error=false){toastState={message,error};renderToast();}
function renderToast(){if(!toastState)return;root.querySelectorAll('[data-toast]').forEach(item=>{item.textContent=toastState.error?errorText(toastState.message):t(toastState.message);item.classList.toggle('is-error',toastState.error);});}
function setBusy(value){busy=value;root.setAttribute('aria-busy',String(value));root.querySelectorAll('button,input,select').forEach(control=>{if(!control.closest('.o-window-tools'))control.disabled=value;});if(!value)renderEnvironment();}
async function operation(action,request={},message=t('正在处理…')){
 if(busy)return null;setBusy(true);toast(message);
 try{return await invoke('service',{action,request});}catch(error){toast(String(error),true);return null;}finally{setBusy(false);}
}
function route(name){
 if(!['installer','overview','appearance','repair','updates'].includes(name))return;
 state.route=name;$('oot-installer').hidden=name!=='installer';$('oot-center').hidden=name==='installer';$('oot-caption').textContent=name==='installer'?t('安装与恢复'):t('管理中心');
 refreshCaption();
 placeLanguage();
 root.querySelectorAll('[id^="oot-page-"]').forEach(page=>page.hidden=page.id!=='oot-page-'+name);
 root.querySelectorAll('[data-route]').forEach(button=>{button.setAttribute('aria-pressed',String(button.dataset.route===name));if(button.closest('.o-nav')){if(button.dataset.route===name)button.setAttribute('aria-current','page');else button.removeAttribute('aria-current');}});
 toast('');if(name==='repair')void diagnostics();
}
function renderPreview(){
 const sample=$('oot-sample');sample.dataset.theme=state.theme;sample.dataset.style=state.style;sample.style.setProperty('--p-accent',colors[state.host]);sample.style.setProperty('--p-width',state.width+'px');
 const names=files[state.host].map(name=>t(name));if(state.active>=names.length)state.active=0;
 const list=$('oot-file-tabs');list.hidden=state.autoHide&&names.length===1;list.replaceChildren();
 names.forEach((name,index)=>{const button=document.createElement('button');button.type='button';button.id='oot-doc-'+index;button.className='o-file-tab cursor-interaction';button.setAttribute('role','tab');button.setAttribute('aria-controls','oot-preview-content');button.setAttribute('aria-selected',String(state.active===index));button.setAttribute('aria-label',name);button.style.flexBasis=(state.widthMode==='title'?Math.max(96,Math.min(220,name.length*8+30)):state.width)+'px';button.textContent=name;button.addEventListener('click',()=>{state.active=index;renderPreview();});list.append(button);});
 $('oot-preview-content').setAttribute('aria-labelledby','oot-doc-'+state.active);$('oot-sample-title').textContent=names[state.active];$('oot-width-value').textContent=state.width+' DIP';
 for(const [attribute,key] of [['preview-host','host'],['theme','theme'],['style','style']])root.querySelectorAll('button[data-'+attribute+']').forEach(button=>button.setAttribute('aria-pressed',String(button.getAttribute('data-'+attribute)===state[key])));
 $('oot-tab-width').value=state.width;$('oot-width-mode').value=state.widthMode;$('oot-auto-hide').checked=state.autoHide;$('oot-double-close').checked=state.doubleClose;
}
function appearanceFromNative(value){return {theme:['auto','light','dark','system'][value.themeMode]||'auto',style:value.barStyle===0?'panel':'open',width:value.tabWidth,widthMode:['fixed','title','shrink'][value.widthMode]||'shrink',autoHide:value.autoHide,doubleClose:value.doubleClose};}
function nativeAppearance(){return {themeMode:['auto','light','dark','system'].indexOf(state.theme),barStyle:state.style==='panel'?0:1,tabWidth:state.width,widthMode:['fixed','title','shrink'].indexOf(state.widthMode),autoHide:state.autoHide,doubleClose:state.doubleClose};}
function renderEnvironment(){
 const running=snapshot?.officeRunning;const compatible=snapshot?.compatible;
 $('oot-env').classList.toggle('blocked',Boolean(snapshot&&(!compatible||running)));
 $('oot-env-title').textContent=!snapshot?t('正在检查安装环境…'):!compatible?t('需要匹配的 Office 位数'):running?t('请先保存并退出 Office'):t('安装环境已就绪');
 $('oot-env-note').textContent=!snapshot?t('正在读取本机状态。'):!compatible?(snapshot.architecture==='conflict'?t('Office 位数信息存在冲突，请检查 Office 安装后重新检测。'):t('无法确认 Office 位数，请确认已安装受支持的桌面版 Office。')):running?t('Office 仍在运行。退出后点击“重新检测”，不会关闭你的文档。'):t('已确认 {bits} 位 Office，可安装所选组件。',{bits:snapshot.architecture==='x86'?32:64});
 $('oot-install-button').textContent=running?t('重新检测'):t('安装');
 $('oot-install-button').disabled=busy||!snapshot||!compatible||(!running&&root.querySelectorAll('[data-install-host]:checked').length===0);
}
function setSnapshot(value,loadAppearance=false){
 snapshot=value;const localeChanged=applySnapshotLanguage(value);
 root.querySelectorAll('[data-enabled-host]').forEach(input=>input.checked=Boolean(value.hosts.find(host=>host.name===input.dataset.enabledHost)?.enabled));
 root.querySelectorAll('.o-app-card').forEach((card,index)=>{
  const host=value.hosts[index];if(!host)return;const lines=card.querySelectorAll('.o-app-desc span');
  lines[0].textContent=!host.registered?t('组件未注册'):!host.dllExists?t('组件文件缺失'):t('组件已注册');
  lines[1].textContent=!host.running?t('运行状态：未启动'):!host.moduleReadable?t('运行载荷：未能核对'):!host.loaded?t('运行状态：未加载'):!host.matchesRegistration?t('运行载荷：等待重启'):t('运行状态：已加载');
 });
 const line=root.querySelector('.o-status-line');let copy=line.querySelector('span');if(!copy){copy=document.createElement('span');const textNodes=[...line.childNodes].filter(n=>n.nodeType===3);textNodes.forEach(n=>n.remove());line.append(copy);}
 copy.textContent=!value.installed?t('尚未安装加载组件'):!value.centerInstalled?t('加载项已注册，管理中心尚未安装'):value.officeRunning?t('正在核对 Office 运行状态'):t('组件已安装，等待下次启动 Office');
 line.dataset.status=value.installed?'ready':'warning';
 for(const [key,id] of [['autoCheck','oot-auto-check'],['autoDownload','oot-auto-download'],['autoInstall','oot-auto-install']])$(id).checked=Boolean(value.preferences[key]);
 $('oot-update-channel').value=value.preferences.updateChannel==='beta'?'beta':'stable';
 if(loadAppearance){savedNative={...value.appearance};savedAppearance=appearanceFromNative(value.appearance);Object.assign(state,savedAppearance);renderPreview();}
 renderEnvironment();if(localeChanged){renderPreview();renderUpdateResult();renderToast();if(lastDiagnostics)renderDiagnostics(lastDiagnostics);}
 if(installedHosts){$('oot-installed-list').textContent=t('{hosts} 组件已安装',{hosts:installedHosts.join(', ')});$('oot-finish-install').textContent=$('oot-open-center').checked?t('完成并打开管理中心'):t('完成');}
}
async function refreshState(){const result=await operation('state',{},t('正在读取状态…'));if(result){setSnapshot(result);toast(t('状态已更新。'));}return result;}
async function diagnostics(){
 const report=await operation('diagnostics',{},t('正在检查组件与运行载荷…'));if(!report)return;
 lastDiagnostics=report;renderDiagnostics(report);toast(t('检查完成。'));
}
function renderDiagnostics(report){
 const rows=root.querySelectorAll('.o-diagnostic .o-plain-row>span');rows[0].textContent=report.architecture==='x64'?t('64 位 · 匹配'):report.architecture==='x86'?t('32 位 · 匹配'):t('未能确认');rows[1].textContent=report.installed?(report.matchesThisPackage?t('与此安装包一致'):t('已注册，版本需核对')):t('未安装或文件缺失');
 const active=report.hosts.filter(host=>host.running);
 $('oot-loaded-state').textContent=!active.length?t('等待 Office 启动后核对'):active.some(host=>!host.moduleReadable)?t('部分进程载荷未能核对'):active.every(host=>host.loaded&&host.matchesRegistration)?t('已加载，路径与注册一致'):t('未加载或仍运行旧载荷，需重启 Office');
 const preview={...report};if(report.runtimeTimeline)preview.runtimeTimeline={...report.runtimeTimeline,eventCount:report.runtimeTimeline.events.length,events:report.runtimeTimeline.events.slice(-30)};
 $('oot-diagnostic-preview').textContent=JSON.stringify(preview,null,2);
}
root.querySelectorAll('[data-route]').forEach(button=>button.addEventListener('click',()=>route(button.dataset.route)));
root.querySelectorAll('[data-install-host]').forEach(input=>input.addEventListener('change',renderEnvironment));
$('oot-language').addEventListener('change',async()=>{
 const mode=Number($('oot-language').value);const previousOverride=languageOverride;
 const result=await operation('language',{mode},'');
 if(result){languageOverride=null;setSnapshot(result);toast(t('语言设置已保存。'));}
 else{languageOverride=previousOverride;if(snapshot)setSnapshot(snapshot);}
});
$('oot-install-button').addEventListener('click',async()=>{
 if(snapshot?.officeRunning){await refreshState();return;}
 root.querySelectorAll('.o-step').forEach(step=>step.classList.toggle('active',step.dataset.step==='2'));
 const hosts=[...root.querySelectorAll('[data-install-host]:checked')].map(input=>input.dataset.installHost);
 const result=await operation('install',{hosts,shortcut:$('oot-desktop-shortcut').checked},t('正在安装组件与管理中心，请稍候…'));
 root.querySelectorAll('.o-step').forEach(step=>step.classList.toggle('active',step.dataset.step===(result?'3':'1')));
 if(!result)return;installedHosts=hosts;setSnapshot(result,true);$('oot-install-ready').hidden=true;$('oot-install-done').hidden=false;placeLanguage();$('oot-installed-list').textContent=t('{hosts} 组件已安装',{hosts:hosts.join(', ')});$('oot-finish-install').textContent=$('oot-open-center').checked?t('完成并打开管理中心'):t('完成');toast(t('安装结果已校验。'));
});
$('oot-finish-install').addEventListener('click',()=>{$('oot-open-center').checked?route('overview'):frame.close();});
root.querySelectorAll('[data-preview-host]').forEach(button=>button.addEventListener('click',()=>{state.host=button.dataset.previewHost;renderPreview();}));
for(const [attribute,key] of [['theme','theme'],['style','style']])root.querySelectorAll('button[data-'+attribute+']').forEach(button=>button.addEventListener('click',()=>{state[key]=button.dataset[key];renderPreview();}));
$('oot-tab-width').addEventListener('input',event=>{state.width=Number(event.target.value);renderPreview();});
$('oot-width-mode').addEventListener('change',event=>{state.widthMode=event.target.value;renderPreview();});
$('oot-auto-hide').addEventListener('change',event=>{state.autoHide=event.target.checked;renderPreview();});
$('oot-double-close').addEventListener('change',event=>state.doubleClose=event.target.checked);
$('oot-reset-appearance').addEventListener('click',()=>{Object.assign(state,defaults);renderPreview();toast(t('已恢复默认预览，点击“应用设置”后生效。'));});
$('oot-cancel-appearance').addEventListener('click',()=>{Object.assign(state,savedAppearance);renderPreview();toast(t('已取消本次更改。'));});
$('oot-apply-appearance').addEventListener('click',async()=>{
 if(!savedNative)return;const changes=Object.fromEntries(Object.entries(nativeAppearance()).filter(([key,value])=>value!==savedNative[key]));
 const result=await operation('appearance',{changes},t('正在保存外观设置…'));if(result){setSnapshot(result,true);toast(t('设置已保存，正在运行的加载项会自动同步。'));}
});
$('oot-apply-hosts').addEventListener('click',async()=>{const hosts=[...root.querySelectorAll('[data-enabled-host]:checked')].map(input=>input.dataset.enabledHost);const result=await operation('hosts',{hosts},t('正在保存启用状态…'));if(result){setSnapshot(result);toast(t('已保存，下次启动对应 Office 应用时生效。'));}});
$('oot-run-diagnostics').addEventListener('click',diagnostics);
$('oot-export-preview').addEventListener('click',async()=>{if(busy)return;setBusy(true);try{if(await invoke('export_diagnostics'))toast(t('已导出诊断与脱敏操作时间线，可作为 issue 附件。'));}catch(error){toast(String(error),true);}finally{setBusy(false);}});
$('oot-repair').addEventListener('click',async()=>{const result=await operation('repair',{},t('正在修复组件与管理中心…'));if(result){setSnapshot(result);toast(t('修复结果已校验，请重新打开 Office。'));}});
async function savePreferences(changed){
 if(changed==='updateChannel')updateGeneration++;
 if(changed==='autoCheck'&&!$('oot-auto-check').checked){$('oot-auto-download').checked=false;$('oot-auto-install').checked=false;}
 if(changed==='autoDownload'){if($('oot-auto-download').checked)$('oot-auto-check').checked=true;else $('oot-auto-install').checked=false;}
 if(changed==='autoInstall'&&$('oot-auto-install').checked){$('oot-auto-check').checked=true;$('oot-auto-download').checked=true;}
 const request={autoCheck:$('oot-auto-check').checked,autoDownload:$('oot-auto-download').checked,autoInstall:$('oot-auto-install').checked,updateChannel:$('oot-update-channel').value};
 const result=await operation('preferences',request,t('正在保存更新偏好…'));if(result){setSnapshot(result);if(changed==='updateChannel'){release=null;await window.__TAURI__.event.emit('check-update',currentLocale());}toast(t('更新偏好已保存。'));}else if(snapshot)setSnapshot(snapshot);
}
for(const [key,id] of [['autoCheck','oot-auto-check'],['autoDownload','oot-auto-download'],['autoInstall','oot-auto-install']])$(id).addEventListener('change',()=>savePreferences(key));
 $('oot-update-channel').addEventListener('change',()=>savePreferences('updateChannel'));
async function checkUpdate(automatic=false){
 const generation=updateGeneration;
 if(!automatic){try{await invoke('open_update_window',{language:currentLocale()});}catch(error){toast(String(error),true);}return;}
 let result;
 if(automatic){try{result=await invoke('service',{action:'check-update',request:{}});}catch(error){automaticCheckFailed=true;renderUpdateResult();return;}}
 else result=await operation('check-update',{},t('正在检查更新…'));
 if(!result||generation!==updateGeneration)return;
 release=result;automaticCheckFailed=false;updatePhase='checked';renderUpdateResult();toast('');
 if(automatic&&result.available&&snapshot?.preferences.autoDownload){const downloaded=await operation('download-update',{version:result.version},t('正在下载并校验更新…'));if(downloaded){updatePhase='downloaded';renderUpdateResult();if(snapshot.preferences.autoInstall)await installUpdate();}}
}
async function installUpdate(){
 if(!release?.available)return;const result=await operation('apply-update',{version:release.version},t('正在下载、校验并安装更新…'));if(!result)return;
 waitingUpdate=Boolean(result.waitingForOffice);updatePhase=waitingUpdate?'waiting':'installed';renderUpdateResult();toast(waitingUpdate?t('已保留经过校验的更新。'):t('安装版本已核对。'));
}
$('oot-check-update').addEventListener('click',()=>checkUpdate());$('oot-install-update').addEventListener('click',()=>checkUpdate());
$('oot-uninstall').addEventListener('click',()=>{$('oot-uninstall-dialog').hidden=false;$('oot-uninstall-cancel').focus();});
$('oot-uninstall-cancel').addEventListener('click',()=>{$('oot-uninstall-dialog').hidden=true;$('oot-uninstall').focus();});
$('oot-uninstall-confirm').addEventListener('click',async()=>{const result=await operation('uninstall',{removeSettings:$('oot-remove-history').checked},t('正在准备卸载…'));if(result?.exit){$('oot-uninstall-dialog').hidden=true;await frame.close();}});
root.addEventListener('keydown',event=>{if($('oot-uninstall-dialog').hidden)return;if(event.key==='Escape'&&!busy)$('oot-uninstall-cancel').click();if(event.key==='Tab'){const targets=[...$('oot-uninstall-dialog').querySelectorAll('button,input')];const index=targets.indexOf(document.activeElement);if(event.shiftKey&&index<=0){event.preventDefault();targets.at(-1).focus();}else if(!event.shiftKey&&index===targets.length-1){event.preventDefault();targets[0].focus();}}});
for(const [id,action] of [['review-minimize','minimize'],['review-maximize','toggleMaximize'],['review-close','close']])$(id).addEventListener('click',()=>frame[action]().catch(error=>toast(String(error),true)));
for(const part of root.querySelectorAll('.o-bar,.o-bar>img,.o-bar-name,.o-bar-caption'))part.setAttribute('data-tauri-drag-region','');
root.querySelectorAll('a[href]').forEach(link=>link.addEventListener('click',event=>{event.preventDefault();invoke('open_external',{url:link.href}).catch(error=>toast(String(error),true));}));
createIcons({icons:{ArrowUpRight,Check,CircleArrowUp,CircleCheck,CodeXml,House,Info,Minus,PackageCheck,SlidersHorizontal,Square,Star,Wrench,X}});
async function initialize(){
renderPreview();setBusy(true);
try{
 const boot=await invoke('boot');languageOverride=boot.language;state.host=boot.host;state.route=boot.page;snapshot=await invoke('service',{action:'state',request:{}});setSnapshot(snapshot,true);setBusy(false);route(boot.page);
 await window.__TAURI__.event.listen('open-page',event=>{if(busy)return;const context=typeof event.payload==='string'?{page:event.payload}:event.payload;if(context.language!=null){languageOverride=context.language;setSnapshot(snapshot);}route(context.page);});
 if(boot.uninstall)$('oot-uninstall').click();
 if(snapshot.preferences.autoCheck&&boot.page!=='installer'&&!busy)void checkUpdate(true);
}catch(error){setLocale(resolveLanguage(({auto:0,'zh-CN':1,en:2}[languageOverride]??0),navigator.language));translateStatic();refreshCaption();renderPreview();setBusy(false);root.dataset.languageReady='true';toast(error,true);}
}
void initialize();
window.addEventListener('focus',async()=>{if(!snapshot||busy)return;try{const value=await invoke('service',{action:'state',request:{}});if(value.language.mode!==snapshot.language.mode||value.language.locale!==snapshot.language.locale)setSnapshot(value);}catch{}});
setInterval(async()=>{if(!waitingUpdate||busy)return;try{const current=await invoke('service',{action:'state',request:{}});snapshot=current;if(!current.officeRunning)await installUpdate();}catch(error){toast(String(error),true);}},15000);
