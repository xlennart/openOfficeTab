import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {t,setLocale,resolveLanguage,errorText} from '../desktop/src/localization.js';

for(const [mode,system,expected] of [[0,'zh-CN','zh-CN'],[0,'zh-TW','zh-CN'],[0,'en-GB','en'],[0,'de-DE','en'],[1,'en-US','zh-CN'],[2,'zh-CN','en']])assert.equal(resolveLanguage(mode,system),expected);
setLocale('en');
assert.equal(t('诊断与修复'),'Diagnostics & repair');
assert.equal(t('发现新版本 {version}',{version:'0.2.1'}),'New version 0.2.1 available');
assert.equal(t('会议纪要.docx'),'Meeting notes.docx');
assert.equal(t('user-provided-file.docx'),'user-provided-file.docx');
for(const bits of [32,64])assert.equal(t('已确认 {bits} 位 Office，可安装所选组件。',{bits}),`${bits}-bit Office verified. The selected components can be installed.`);
assert.equal(t('32 位 · 匹配'),'32-bit · matches');
assert.equal(errorText('更新清单签名无效。'),'The update manifest signature is invalid.');
assert.equal(errorText('请先安装加载组件。 (0x80004005)'),'Install the add-in components first. (0x80004005)');
assert(!errorText('private-path-or-server-response').includes('private-path'));
setLocale('zh-CN');
assert.equal(t('Diagnostics & repair'),'诊断与修复');
assert.equal(errorText('Installer checksum mismatch. (0x80004005)'),'安装器校验和不匹配。 (0x80004005)');
assert.equal(t('{hosts} 组件已安装',{hosts:'Word, Excel'}),'Word, Excel 组件已安装');
assert.equal(t('已确认 {bits} 位 Office，可安装所选组件。',{bits:32}),'已确认 32 位 Office，可安装所选组件。');

const messages=JSON.parse(await readFile(new URL('../desktop/src/messages.json',import.meta.url),'utf8'));
for(const [key,value] of Object.entries(messages)){
 assert(!/\p{Script=Han}/u.test(value.replaceAll('简体中文','')),`Chinese text in English translation: ${key}`);
 assert.deepEqual([...key.matchAll(/\{\w+\}/g)].map(m=>m[0]).sort(),[...value.matchAll(/\{\w+\}/g)].map(m=>m[0]).sort(),`Translation placeholders differ: ${key}`);
}
for(const name of ['README.md','docs/installation.en.md']){
 const text=await readFile(new URL('../'+name,import.meta.url),'utf8');
 assert(!/\p{Script=Han}/u.test(text.replace(/\]\([^\n)]*\)/g,']').replaceAll('简体中文','')),`Chinese interface labels in English documentation: ${name}`);
}
for(const name of ['center.js','index.html','update.js','update.html']){
 const source=await readFile(new URL('../desktop/src/'+name,import.meta.url),'utf8');
 const strings=name.endsWith('.js')?[...source.matchAll(/'([^'\n]*[\p{Script=Han}][^'\n]*)'/gu)].map(m=>m[1]):[...source.matchAll(/>([^<>]*[\p{Script=Han}][^<>]*)</gu)].map(m=>m[1].trim()).concat([...source.matchAll(/(?:alt|title|aria-label)="([^"]*[\p{Script=Han}][^"]*)"/gu)].map(m=>m[1]));
 for(const text of strings)assert(Object.hasOwn(messages,text),`Missing desktop translation: ${text}`);
}
const centerSource=await readFile(new URL('../desktop/src/center.js',import.meta.url),'utf8');
assert(!/t\('管理中心 \d/.test(centerSource),'Management center version must come from runtime state, not a stale translated literal');
setLocale('en');
assert.equal(t('管理中心 {version}',{version:'0.2.3-beta.1'}),'Management center 0.2.3-beta.1');
console.log('Desktop localization: language resolution, templates, errors, sample content and static/dynamic translation coverage passed.');
