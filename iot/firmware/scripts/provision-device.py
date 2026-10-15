"""USB bootstrap/recovery. Requires pyserial (included with PlatformIO).
No secrets are printed or written to files. Close any serial monitor first.
"""
import argparse
import getpass
import json
import time


def request(port, command, prefix, timeout=12):
    port.write((command+'\n').encode('utf-8'))
    port.flush()
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        line=port.readline().decode('utf-8',errors='replace').strip()
        if line.startswith(prefix):
            return json.loads(line[len(prefix):])
    raise RuntimeError('Sem resposta USB. Reinicie a placa, feche o monitor Serial e repita dentro de 3 minutos.')


def main():
    parser=argparse.ArgumentParser(description='Provisionar ESP32 por USB sem recompilar; preserva GUID e dados.')
    parser.add_argument('--port',required=True,help='Ex.: COM5 ou /dev/ttyUSB0')
    parser.add_argument('--status',action='store_true',help='Consultar IP e revisão, sem alterar')
    parser.add_argument('--file',help='JSON privado com objeto settings; não versionar')
    args=parser.parse_args()
    settings=None
    if not args.status:
        if args.file:
            with open(args.file,encoding='utf-8') as file:
                settings=json.load(file)
            if not isinstance(settings,dict):raise ValueError('Arquivo deve conter objeto settings, sem envelope.')
        else:
            print('Campos de senha vazios mantêm os valores atuais. No primeiro cadastro, informe senha admin de 12+ caracteres.')
            settings={'ssid':input('Nome da rede Wi-Fi (2,4 GHz): ').strip()}
            for name,label in [('wifi_password','Senha Wi-Fi'),('admin_password','Senha admin'),('operator_password','Senha operador (opcional)')]:
                value=getpass.getpass(label+': ')
                if value:settings[name]=value
            print('Para rede aberta, use arquivo JSON privado com wifi_password vazio. Integração central e vínculos podem ser definidos depois pelo portal.')
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError('Instale pyserial no mesmo Python: python -m pip install pyserial') from exc
    with serial.Serial(args.port,115200,timeout=.25,write_timeout=5) as port:
        # Opening many DevKits resets the board. Wait for setup, without echoing logs.
        time.sleep(2)
        port.reset_input_buffer()
        status=request(port,'PROVISION?','PROVISION_STATUS ')
        print('GUID:',status['guid'],'| revisão:',status['profile_revision'],'| IP:',status.get('ip') or 'sem conexão')
        if args.status:return
        if not status.get('usb_window_open'):raise RuntimeError('Janela USB fechada. Reinicie a placa e repita.')
        packet={'expected_revision':status['profile_revision'],'settings':settings}
        encoded=json.dumps(packet,ensure_ascii=False,separators=(',',':'))
        if len(encoded.encode('utf-8'))>15872:raise ValueError('Provisionamento excede o limite de bytes.')
        result=request(port,'PROVISION '+encoded,'PROVISION_RESULT ')
        if not result.get('saved'):raise RuntimeError('Recusado pela placa: '+result.get('error','erro desconhecido'))
        print('Salvo. A placa reiniciará; aguarde conexão e consulte o IP com --status ou no roteador.')


if __name__=='__main__':
    try:main()
    except (RuntimeError,ValueError,OSError) as error:
        raise SystemExit(str(error)) from None
