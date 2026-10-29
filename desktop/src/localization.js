import messages from './messages.json' with {type:'json'};

let locale='zh-CN';
const reverse=new Map(Object.entries(messages).map(([zh,en])=>[en,zh]));
export function resolveLanguage(mode,systemLocale='en'){
 return mode===1?'zh-CN':mode===2?'en':/^zh(?:-|$)/i.test(systemLocale)?'zh-CN':'en';
}
export function setLocale(value){locale=value==='zh-CN'?'zh-CN':'en';}
export function currentLocale(){return locale;}
export function t(key,values={}){
 const source=reverse.get(key)||key;
 const value=locale==='en'?(messages[source]??source):source;
 return value.replace(/\{([a-zA-Z]+)\}/g,(match,name)=>Object.hasOwn(values,name)?String(values[name]):match);
}
export function errorText(error){
 const text=String(error),code=text.match(/\s*\(0x[0-9a-f]{8}\)$/i)?.[0]||'';
 const source=text.slice(0,text.length-code.length);
 const known=Object.hasOwn(messages,source)||reverse.has(source);
 return t(known?source:'操作未完成，请重试或导出诊断与日志。')+code;
}
export function bindStaticTranslations(root){
 const bindings=[];
 const walker=document.createTreeWalker(root,NodeFilter.SHOW_TEXT);
 while(walker.nextNode()){
  const node=walker.currentNode,key=node.textContent.trim();
  if(Object.hasOwn(messages,key))bindings.push(()=>{node.textContent=node.textContent.replace(node.textContent.trim(),t(key));});
 }
 for(const element of [root,...root.querySelectorAll('*')])for(const attribute of ['alt','title','aria-label']){
  const key=element.getAttribute(attribute);if(Object.hasOwn(messages,key))bindings.push(()=>element.setAttribute(attribute,t(key)));
 }
 return ()=>{bindings.forEach(apply=>apply());document.documentElement.lang=locale;document.title=t('openOfficeTab 管理中心');};
}
