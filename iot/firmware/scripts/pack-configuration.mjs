import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {validateConfig} from '../../../src/engine.js';
const [path,guid,rev]=process.argv.slice(2);
if(!path||!/^[0-9a-f]{8}(-[0-9a-f]{4}){3}-[0-9a-f]{12}$/i.test(guid||'')||!Number.isSafeInteger(Number(rev))||Number(rev)<1)throw Error('Uso: node scripts/pack-configuration.mjs configuration.json GUID revision');
const configuration=validateConfig(JSON.parse(readFileSync(path,'utf8')));
process.stdout.write(JSON.stringify({schema_version:1,device_guid:guid,revision:Number(rev),configuration,configuration_hash:createHash('sha256').update(JSON.stringify(configuration)).digest('hex')}));
