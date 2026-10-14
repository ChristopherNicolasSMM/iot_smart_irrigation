import {readFileSync} from 'node:fs';import assert from 'node:assert/strict';
import {contract,contractError,configurationHash} from '../src/contracts.js';import {validateConfig} from '../src/engine.js';
const manifest=JSON.parse(readFileSync(new URL('../contracts/fixtures/manifest.json',import.meta.url),'utf8'));
function references(value){if(value&&typeof value==='object'){if(value.$ref){const name=value.$ref.match(/^#\/\$defs\/([A-Za-z0-9]+)$/)?.[1];assert.ok(name&&contract.$defs[name],'Referência desconhecida');}for(const v of Object.values(value))references(v);}}
references(contract);
for(const {file,contract:name,valid} of manifest){const data=JSON.parse(readFileSync(new URL('../contracts/fixtures/'+file,import.meta.url),'utf8')),error=contractError(name,data);assert.equal(!error,valid,file+': '+error);if(valid&&name==='DeviceConfiguration')validateConfig(data);if(valid&&name==='ConfigurationResponse'){validateConfig(data.configuration);assert.equal(configurationHash(data.configuration),data.configuration_hash);}}
console.log(manifest.length+' fixtures verificadas; referências, limites e hashes coerentes');
