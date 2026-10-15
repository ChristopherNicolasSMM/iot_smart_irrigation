"""Encrypted configuration/provisioning backup via physical USB, never plaintext files.
Requires pyserial and cryptography. Restore is same-GUID and starts in quarantine.
"""
import argparse
import base64
import getpass
import hashlib
import json
import os
from pathlib import Path
import time
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.exceptions import InvalidTag

ITERATIONS=600000
MAX_SNAPSHOT=32256
MAX_FILE=65536
MAGIC='irrigation-esp32-backup'


def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode('utf-8')
def encode(value):return base64.b64encode(value).decode('ascii')
def decode(value):return base64.b64decode(value,validate=True)


def seal(snapshot,password):
    raw=canonical(snapshot)
    if len(password)<12:raise ValueError('Senha do backup deve ter pelo menos 12 caracteres.')
    if len(raw)>MAX_SNAPSHOT:raise ValueError('Snapshot excede o limite de recuperação.')
    salt=os.urandom(16);nonce=os.urandom(12)
    header={'magic':MAGIC,'version':1,'kdf':'PBKDF2-HMAC-SHA256','iterations':ITERATIONS,'salt':encode(salt),'nonce':encode(nonce),'device_guid':snapshot['device_guid']}
    key=hashlib.pbkdf2_hmac('sha256',password.encode(),salt,ITERATIONS,32)
    return canonical({'header':header,'ciphertext':encode(AESGCM(key).encrypt(nonce,raw,canonical(header)))})


def unseal(data,password):
    if len(data)>MAX_FILE:raise ValueError('Arquivo excede o limite.')
    try:
        wrapper=json.loads(data);header=wrapper['header']
        if set(wrapper)!={'header','ciphertext'} or set(header)!={'magic','version','kdf','iterations','salt','nonce','device_guid'}:raise ValueError()
        if header['magic']!=MAGIC or header['version']!=1 or header['kdf']!='PBKDF2-HMAC-SHA256' or header['iterations']!=ITERATIONS:raise ValueError()
        salt=decode(header['salt']);nonce=decode(header['nonce']);ciphertext=decode(wrapper['ciphertext'])
        if len(salt)!=16 or len(nonce)!=12 or not 16<=len(ciphertext)<=MAX_SNAPSHOT+16:raise ValueError()
        key=hashlib.pbkdf2_hmac('sha256',password.encode(),salt,ITERATIONS,32)
        raw=AESGCM(key).decrypt(nonce,ciphertext,canonical(header));snapshot=json.loads(raw)
        if snapshot['schema_version']!=1 or snapshot['device_guid']!=header['device_guid']:raise ValueError()
        return snapshot
    except (InvalidTag,ValueError,KeyError,TypeError,UnicodeError) as error:
        raise ValueError('Backup inválido, adulterado ou senha incorreta.') from None


def write_private(path,data):
    descriptor=os.open(path,os.O_CREAT|os.O_EXCL|os.O_WRONLY,0o600)
    try:
        with os.fdopen(descriptor,'wb') as output:
            output.write(data);output.flush();os.fsync(output.fileno())
    except BaseException:
        Path(path).unlink(missing_ok=True)
        raise


def request(port,command,prefix,timeout=20):
    port.write((command+'\n').encode());port.flush();until=time.monotonic()+timeout
    pending=bytearray()
    while time.monotonic()<until:
        # readline can return a partial line when its timeout expires. A full
        # snapshot takes seconds at 115200 baud; parse only after newline.
        pending.extend(port.readline())
        if len(pending)>MAX_FILE:raise ValueError('Resposta USB excede o limite.')
        while b'\n' in pending:
            raw,_,rest=pending.partition(b'\n');pending=bytearray(rest)
            line=raw.decode('utf-8',errors='strict').strip()
            if line.startswith(prefix):return json.loads(line[len(prefix):])
            if line.startswith('RECOVERY_RESULT '):
                result=json.loads(line[len('RECOVERY_RESULT '):]);raise ValueError('Placa recusou: '+result.get('error','erro'))
    raise ValueError('Sem resposta USB. Feche o monitor, reinicie a placa e repita dentro de três minutos.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['backup','restore','accept','abort'])
    parser.add_argument('--port',required=True)
    parser.add_argument('--file',help='Arquivo .irrfw cifrado; backup nunca sobrescreve arquivo existente')
    args=parser.parse_args()
    snapshot=None;password=None
    if args.action in ['backup','restore']:
        if not args.file:parser.error('--file é obrigatório para backup/restore')
        password=getpass.getpass('Senha externa do backup: ')
        if args.action=='backup':
            if Path(args.file).exists():raise ValueError('Arquivo já existe; escolha outro nome.')
            if len(password)<12:raise ValueError('Senha exige 12+ caracteres.')
            if password!=getpass.getpass('Repita a senha do backup: '):raise ValueError('Senhas diferentes.')
        else:
            if Path(args.file).stat().st_size>MAX_FILE:raise ValueError('Arquivo excede o limite.')
            snapshot=unseal(Path(args.file).read_bytes(),password)
    import serial
    with serial.Serial(args.port,115200,timeout=.25,write_timeout=10) as port:
        time.sleep(2);port.reset_input_buffer();status=request(port,'PROVISION?','PROVISION_STATUS ')
        if not status.get('usb_window_open') or status.get('usb_recovery_allowed') is False:raise ValueError('Recuperação USB indisponível: reinicie ou confira o modo de produção.')
        if args.action=='backup':
            snapshot=request(port,'RECOVERY_BACKUP','RECOVERY_BACKUP ')
            if snapshot.get('schema_version')!=1 or snapshot.get('device_guid')!=status['guid']:raise ValueError('Snapshot inválido.')
            write_private(args.file,seal(snapshot,password));print('Backup cifrado salvo. Guarde a senha separadamente.')
        elif args.action=='restore':
            if snapshot['device_guid']!=status['guid']:raise ValueError('GUID diferente: restauração recusada sem alterar a placa.')
            packet={'expected_profile_revision':status['profile_revision'],'expected_configuration_revision':status['configuration_revision'],'backup':snapshot}
            raw=canonical(packet).decode()
            if len(raw.encode())>MAX_SNAPSHOT:raise ValueError('Snapshot excede o limite da placa.')
            result=request(port,'RECOVERY_RESTORE '+raw,'RECOVERY_RESULT ')
            if not result.get('saved'):raise ValueError('Recusado: '+result.get('error','erro'))
            print('Restauração preparada. A placa reiniciará em quarentena; confira dados e renove o token central antes de ativar.')
        else:
            result=request(port,'RECOVERY_'+args.action.upper(),'RECOVERY_RESULT ')
            if not result.get('saved'):raise ValueError('Recusado: '+result.get('error','erro'))
            print('Operação registrada; a placa reiniciará. Abort mantém quarentena e não desfaz gravações parciais.')


if __name__=='__main__':
    try:main()
    except (ValueError,OSError) as error:raise SystemExit(str(error)) from None
