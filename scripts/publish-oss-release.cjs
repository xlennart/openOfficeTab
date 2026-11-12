// Operator-only tool. Credentials come from the process environment and never
// enter the client, release assets, receipts, or command-line arguments.
const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const {createRequire} = require('node:module');
const root = path.resolve(__dirname, '..');
const OSS = createRequire(path.join(root, 'tools/release/package.json'))('ali-oss');
const options = Object.fromEntries(process.argv.slice(2).map(arg => {
  const [key,...value] = arg.replace(/^--/, '').split('='); return [key,value.join('=')];
}));
const version = options.version;
if (!/^\d+\.\d+\.\d+(?:-(?:beta|rc)\.[1-9]\d*)?$/.test(version??'')) throw Error('Invalid release version.');
const beta = version.includes('-');
if (options.latest === 'true' && beta) throw Error('A prerelease cannot replace stable latest.');
let client,base,objectPrefix;
function configureUpload(){
 for(const key of ['OSS_RELEASE_BUCKET','OSS_RELEASE_REGION','OSS_RELEASE_PUBLIC_BASE','OSS_ACCESS_KEY_ID','OSS_ACCESS_KEY_SECRET']){
  if(!process.env[key])throw Error(`Missing ${key}.`);
 }
 base=new URL(process.env.OSS_RELEASE_PUBLIC_BASE);
 if(base.protocol!=='https:'||base.username||base.password||base.search||base.hash||!base.pathname.endsWith('/'))throw Error('Invalid public release base.');
 objectPrefix=base.pathname.replace(/^\//,'');
 client=new OSS({bucket:process.env.OSS_RELEASE_BUCKET,region:process.env.OSS_RELEASE_REGION,secure:true,
  accessKeyId:process.env.OSS_ACCESS_KEY_ID,accessKeySecret:process.env.OSS_ACCESS_KEY_SECRET,
  stsToken:process.env.OSS_SECURITY_TOKEN,timeout:60000});
}
const directory = path.resolve(options.directory ?? path.join(root,'delivery',`stable-${version}`));
const names = [`openOfficetab-${version}-setup-x64.exe`,`openOfficetab-${version}-win-x64.zip`,'SHA256SUMS.txt',beta?'openofficetab-update-beta.json':'openofficetab-update.json'];
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
async function anonymous(key, expected) {
  if(!key.startsWith(objectPrefix))throw Error('Unexpected release object prefix.');
  const response = await fetch(new URL(key.slice(objectPrefix.length),base),{signal:AbortSignal.timeout(60000)});
  if(!response.ok)throw Error(`Anonymous mirror response: ${response.status}`);
  const bytes = Buffer.from(await response.arrayBuffer());
  if(bytes.length!==expected.length || hash(bytes)!==hash(expected))throw Error('Anonymous mirror checksum mismatch.');
}
async function upload(key, bytes, immutable) {
  if(immutable){
    try {
      const current=await client.get(key);
      if(hash(current.content)!==hash(bytes))throw Error('Existing immutable release differs; preserved.');
      await anonymous(key,bytes);return;
    }catch(error){if(error.status!==404)throw error;}
  }
  await client.put(key,bytes,{headers:{'Cache-Control':immutable?'public, max-age=31536000, immutable':'no-cache',
    ...(immutable?{'x-oss-forbid-overwrite':'true'}:{})}});
  await anonymous(key,bytes);
}
async function main(){
  const assets=[];
  for(const name of names){
    const file=path.join(directory,name);const stat=await fs.lstat(file);
    if(!stat.isFile()||stat.isSymbolicLink())throw Error('Release inputs must be ordinary files.');
    assets.push({name,bytes:await fs.readFile(file)});
  }
  const sums=assets[2].bytes.toString('ascii');
  for(const asset of assets.slice(0,2))if(!sums.split(/\r?\n/).includes(`${hash(asset.bytes)}  ${asset.name}`))throw Error('Local checksum manifest mismatch.');
  const envelope=JSON.parse(assets[3].bytes.toString('utf8'));
  const publicXml=await fs.readFile(path.join(root,'desktop/service/update-public-key.xml'),'utf8');
  const modulus=publicXml.match(/<Modulus>([^<]+)<\/Modulus>/)?.[1];
  const exponent=publicXml.match(/<Exponent>([^<]+)<\/Exponent>/)?.[1];
  const key=crypto.createPublicKey({key:{kty:'RSA',n:Buffer.from(modulus,'base64').toString('base64url'),e:Buffer.from(exponent,'base64').toString('base64url')},format:'jwk'});
  const payload=Buffer.from(envelope.payload,'base64');
  if(!crypto.verify('RSA-SHA256',payload,key,Buffer.from(envelope.signature,'base64')))throw Error('Invalid update manifest signature.');
  const metadata=JSON.parse(payload.toString('utf8'));
  if(metadata.version!==version||metadata.tag!==`v${version}`||metadata.name!==names[0]||metadata.size!==assets[0].bytes.length||metadata.sha256!==hash(assets[0].bytes))throw Error('Signed metadata does not match the release.');
  if(options['verify-only']==='true'){
    console.log(JSON.stringify({version,status:'verified',uploadPerformed:false,assets:assets.map(a=>({name:a.name,sha256:hash(a.bytes),size:a.bytes.length}))}));return;
  }
  configureUpload();
  const prefix=objectPrefix;
  const receipts=[];
  for(const asset of assets){
    const relative=`v${version}/${asset.name}`;
    // SDK object names include the public base's prefix; anonymous URLs do not repeat it.
    await upload(prefix+relative,asset.bytes,true);
    receipts.push({name:asset.name,sha256:hash(asset.bytes),size:asset.bytes.length});
  }
  if(options.latest==='true'){
    await upload(prefix+'latest/SHA256SUMS.txt',assets[2].bytes,false);
    // Publish the pointer only after every versioned byte and checksum passed.
    await upload(prefix+'latest/openofficetab-update.json',assets[3].bytes,false);
  }
  console.log(JSON.stringify({version,status:'passed',latest:options.latest==='true',assets:receipts}));
}
main().catch(error=>{
  // SDK error objects may contain authenticated request headers. Never log them.
  const safeCode=typeof error.code==='string'&&/^[A-Za-z0-9_-]{1,64}$/.test(error.code)?error.code:'ReleaseMirrorError';
  console.error(JSON.stringify({status:'failed',code:safeCode,statusCode:Number.isInteger(error.status)?error.status:null,
    message:'Release mirror operation failed; request details are intentionally omitted.'}));process.exitCode=1;
});
