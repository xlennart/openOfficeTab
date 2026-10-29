import {t,setLocale,currentLocale,errorText} from './localization.js';
import {createIcons,PackageCheck,X} from 'lucide';

const root=document.getElementById('oot-design'),$=id=>document.getElementById(id);
const frame=window.__TAURI__.window.getCurrentWindow();
let phase='checking',release=null,currentVersion='',problem='',busy=false,closed=false,polling=false;
setLocale(new URLSearchParams(location.search).get('language'));
const service=(action,request={})=>window.__TAURI__.core.invoke('service',{action,request:{...request,language:currentLocale()}});
function render(){
 root.dataset.phase=phase;root.dataset.ready='true';document.documentElement.lang=currentLocale();
 document.title=t('openOfficeTab 软件更新');frame.setTitle(document.title).catch(()=>{});
 $('u-caption').textContent=t('软件更新');$('u-close').setAttribute('aria-label',t('关闭更新窗口'));
 const titles={checking:'正在检查更新…',available:'发现新版本',latest:'当前已是最新版',downloading:'正在下载更新…',installing:'正在安装更新…',waiting:'等待 Office 退出',installed:'更新安装完成',error:'更新未完成'};
 $('u-title').textContent=t(titles[phase]);
 $('u-version').hidden=!release;$('u-new-version').textContent=release?.available?`openOfficeTab ${release.version}`:`openOfficeTab ${currentVersion}`;
 $('u-current-version').textContent=release?.available?t('当前版本 {version}',{version:currentVersion}):'';
 $('u-release').hidden=phase!=='available';$('u-notes-heading').textContent=t('本次更新');
 $('u-notes').textContent=release?.notes?.[currentLocale()]||t('此版本未提供更新说明。');
 const status={downloading:'正在下载并校验更新…',waiting:'保存并退出 Office 后，保持此窗口打开将自动继续更新。',installed:'下次打开管理中心即可使用新版。'};
 $('u-status').textContent=status[phase]?t(status[phase]):'';
 $('u-progress').hidden=!['checking','downloading','installing'].includes(phase);$('u-progress').setAttribute('aria-label',t('更新进度'));
 $('u-error').hidden=!problem;$('u-error').textContent=problem?errorText(problem):'';
 $('u-safety').textContent=t('更新前请保存并退出 Office。现有设置将保留。');$('u-safety').hidden=!release?.available||phase==='installed';
 $('u-later').textContent=t('暂不更新');$('u-later').hidden=phase!=='available';
 const actions={available:'立即更新',latest:'确定',waiting:'关闭',installed:'完成',error:'重试'};
 $('u-action').textContent=t(actions[phase]||'请稍候…');$('u-action').disabled=busy;
 $('u-close').disabled=phase==='installing';$('u-later').disabled=busy;$('u-action').hidden=['checking','downloading','installing'].includes(phase);
 root.querySelector('main').setAttribute('aria-busy',String(busy));
}
async function check(){
 if(busy||closed)return;busy=true;phase='checking';problem='';render();
 try{const result=await service('check-update');if(closed)return;release=result;currentVersion=result.currentVersion||currentVersion;phase=result.available?'available':'latest';}
 catch(error){problem=String(error);phase='error';}finally{busy=false;if(!closed)render();}
}
async function install(){
 phase='installing';render();
 const result=await service('apply-update',{version:release.version});
 if(!result.installed&&!result.waitingForOffice)throw new Error('更新版本已经变化，请重新检查。');
 phase=result.waitingForOffice?'waiting':'installed';
}
async function update(){
 if(busy||!release?.available)return;busy=true;problem='';phase='downloading';render();
 try{const result=await service('download-update',{version:release.version});if(closed)return;if(!result.downloaded)throw new Error('更新版本已经变化，请重新检查。');await install();}
 catch(error){problem=String(error);phase='error';}finally{busy=false;if(!closed)render();}
}
async function close(){if(phase==='installing')return;try{await frame.close();closed=true;}catch(error){problem=String(error);render();}}
$('u-close').addEventListener('click',close);$('u-later').addEventListener('click',close);
$('u-action').addEventListener('click',()=>phase==='available'?update():phase==='error'?check():close());
document.addEventListener('keydown',event=>{if(event.key==='Escape')void close();});
createIcons({icons:{PackageCheck,X}});
async function initialize(){
 render();
 try{const boot=await window.__TAURI__.core.invoke('boot');currentVersion=boot.version;await window.__TAURI__.event.listen('check-update',event=>{setLocale(event.payload);if(busy)render();else void check();});await window.__TAURI__.event.listen('update-language',event=>{setLocale(event.payload);render();});await check();}
 catch(error){problem=String(error);phase='error';busy=false;render();}
}
void initialize();
setInterval(async()=>{
 if(phase!=='waiting'||busy||closed||polling)return;polling=true;
 try{const state=await service('state');if(!state.officeRunning&&!closed){busy=true;await install();}}
 catch(error){problem=String(error);phase='error';}finally{polling=false;busy=false;if(!closed)render();}
},5000);
