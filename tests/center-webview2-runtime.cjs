const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.argv[2]);
const port=Number(process.argv[3]),jobRoot=process.argv[4],jobId=process.argv[5],version=process.argv[6];
const report={jobId,startedUtc:new Date().toISOString(),status:'running',driver:'playwright-webview2-cdp',checks:[]};
const result=path.join(jobRoot,'ui-result.json');
const save=()=>fs.writeFileSync(result,JSON.stringify(report,null,2));
async function until(query,timeout=30000){const end=Date.now()+timeout;while(Date.now()<end){const value=await query();if(value)return value;await new Promise(r=>setTimeout(r,100));}throw Error('Timed out waiting for this job condition');}
let page,browser;
(async()=>{
 browser=await chromium.connectOverCDP('http://127.0.0.1:'+port);
 page=await until(()=>browser.contexts().flatMap(context=>context.pages()).find(candidate=>{try{return new URL(candidate.url()).hostname==='tauri.localhost';}catch{return false;}}));
 await page.locator('#oot-design[data-language-ready="true"]').waitFor();
 const idle=()=>page.waitForFunction(()=>document.querySelector('#oot-design').getAttribute('aria-busy')==='false');
 const state=()=>page.evaluate(()=>window.__TAURI__.core.invoke('service',{action:'state',request:{}}));
 for(const size of ['default','minimum']){
  if(size==='minimum'){
   report.phase='native-minimum-resize';save();
   fs.writeFileSync(path.join(jobRoot,'resize-request.json'),JSON.stringify({jobId,width:420,height:480}));
   await until(()=>{const file=path.join(jobRoot,'resize-result.json');return fs.existsSync(file)&&JSON.parse(fs.readFileSync(file)).jobId===jobId;});
   await page.waitForFunction(()=>innerWidth>=416&&innerWidth<=422&&innerHeight>=476&&innerHeight<=482);
  }
  for(const language of ['en','zh-CN']){
   report.phase=size+'-'+language;save();
   await idle();await page.locator('#oot-language').selectOption(language==='en'?'2':'1');
   await page.waitForFunction(expected=>document.documentElement.lang===expected,language);await idle();
   assert.equal(await page.locator('#oot-update-description').textContent(),(language==='en'?'Management center ':'管理中心 ')+version);
   const channel=page.locator('#oot-update-channel');
   assert.deepEqual(await channel.locator('option').allTextContents(),language==='en'?['Stable','Beta']:['正式版','Beta 版']);
   const saved=[];
   for(const choice of ['beta','stable']){
    await channel.selectOption(choice);await idle();const snapshot=await state();
    assert.equal(snapshot.preferences.updateChannel,choice,'UI choice must reach shared persisted preferences');
    assert.equal(snapshot.version,version);saved.push(choice);
   }
   await channel.scrollIntoViewIfNeeded();
   const geometry=await page.evaluate(()=>{
    const rect=selector=>{const b=document.querySelector(selector).getBoundingClientRect();return {x:b.x,y:b.y,width:b.width,height:b.height};};
    const channel=document.querySelector('#oot-update-channel'),box=channel.getBoundingClientRect();
    return {width:innerWidth,height:innerHeight,scrollWidth:document.documentElement.scrollWidth,content:rect('#oot-page-updates>.o-page-scroll'),channel:rect('#oot-update-channel'),label:rect('label[for="oot-update-channel"]'),close:rect('#review-close'),channelReceivesPointer:document.elementFromPoint(box.x+box.width/2,box.y+box.height/2)===channel};
   });
   report.lastGeometry=geometry;save();
   assert(geometry.scrollWidth<=geometry.width+1,'No horizontal page overflow');
   for(const control of [geometry.channel,geometry.label,geometry.close])assert(control.width>0&&control.height>0&&control.x>=-1&&control.y>=-1&&control.x+control.width<=geometry.width+1&&control.y+control.height<=geometry.height+1,'Required control must fit the actual native viewport');
   assert(geometry.channelReceivesPointer,'The fixed footer must not intercept channel input');
   const tip=await page.locator('#oot-update-channel').locator('..').locator('[title]').getAttribute('title');
   assert(language==='en'?!/\p{Script=Han}/u.test(tip):/\p{Script=Han}/u.test(tip));
   await page.screenshot({path:path.join(jobRoot,`${size}-${language}.png`)});
   report.checks.push({size,language,saved,geometry,passed:true});save();
  }
 }
 await page.locator('#oot-language').selectOption('0');await idle();const automatic=await state();
 assert.equal(automatic.language.mode,0);assert.equal(await page.evaluate(()=>document.documentElement.lang),automatic.language.locale);
 report.autoLanguagePassed=true;
 const closed=new Promise(resolve=>page.once('close',resolve));
 await page.locator('#review-close').click().catch(error=>{if(!page.isClosed())throw error;});await closed;
 report.windowClosed=true;report.status='ui_passed';
})().catch(async error=>{report.status='failed';report.error=String(error);report.stack=error.stack;if(page&&!page.isClosed()){report.failureViewport=await page.evaluate(()=>({width:innerWidth,height:innerHeight})).catch(()=>null);await page.screenshot({path:path.join(jobRoot,'failure.png')}).catch(()=>{});}process.exitCode=1;}).finally(()=>{report.finishedUtc=new Date().toISOString();save();process.exit(process.exitCode||0);});
