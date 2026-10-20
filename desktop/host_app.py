"""Portable Windows host. No development runtime or project secrets required."""
import argparse
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import tempfile
import threading
import tkinter as tk
from tkinter import messagebox, ttk
import webbrowser

sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'tools'))
from run_gemini_device import main as run_device
from serial.tools import list_ports
from settings import VERSION, load_settings, save_settings, settings_path, valid_key, valid_model
from network import local_addresses, is_lan

DRIVER_URL='https://docs.espressif.com/projects/esp-iot-solution/en/release-v2.0/usb/usb_overview/usb_serial_jtag.html'

def runtime_root():
    if getattr(sys,'frozen',False): return Path(sys._MEIPASS) / 'runtime'
    return Path(__file__).resolve().parents[1] / '.packaging/stage/runtime'

def runtime_env(key='',model='gemini-3.8-live'):
    env=os.environ.copy()
    for name in ('NODE_OPTIONS','NODE_PATH','GEMINI_API_KEY','GOOGLE_API_KEY'): env.pop(name,None)
    env['GEMINI_MODEL']=model
    if key: env['GEMINI_API_KEY']=key
    return env

class HostApp:
    def __init__(self,root,*,discover=True,config_path=None):
        self.root=root; self.path=config_path or settings_path()
        self.stop_event=threading.Event(); self.worker=None; self.closing=False
        self.events=queue.Queue(maxsize=256)
        self.key=tk.StringVar(); self.model=tk.StringVar(value='gemini-3.8-live')
        self.port=tk.StringVar(); self.host=tk.StringVar(); self.remember=tk.BooleanVar(value=False)
        self.status=tk.StringVar(value='USB端末を接続し、接続先とGeminiキーを確認してください。')
        root.title('スタックちゃん ホスト'); root.geometry('760x640'); root.minsize(680,590)
        root.configure(background='#edf2f7')
        style=ttk.Style(root); style.theme_use('clam')
        style.configure('TFrame',background='#edf2f7'); style.configure('TLabel',background='#edf2f7',font=('Yu Gothic UI',10))
        style.configure('Title.TLabel',font=('Yu Gothic UI',21,'bold'),foreground='#15324f')
        style.configure('Action.TButton',font=('Yu Gothic UI',12,'bold'),padding=(18,11))
        outer=ttk.Frame(root,padding=26); outer.pack(fill='both',expand=True)
        ttk.Label(outer,text='スタックちゃん ホスト',style='Title.TLabel').pack(anchor='w')
        ttk.Label(outer,text='USBで端末を設定して、同じLANからGeminiにつなぎます。').pack(anchor='w',pady=(2,19))
        form=ttk.Frame(outer); form.pack(fill='x'); form.columnconfigure(1,weight=1)
        self.port_box=ttk.Combobox(form,textvariable=self.port,state='readonly',width=25)
        self.host_box=ttk.Combobox(form,textvariable=self.host,state='readonly')
        self.key_box=ttk.Entry(form,textvariable=self.key,show='●')
        self.model_box=ttk.Entry(form,textvariable=self.model)
        for row,(caption,widget) in enumerate([('USBポート',self.port_box),('このPCのLANアドレス',self.host_box),('Gemini APIキー',self.key_box),('モデル',self.model_box)]):
            ttk.Label(form,text=caption).grid(row=row,column=0,sticky='w',padx=(0,16),pady=8)
            widget.grid(row=row,column=1,sticky='ew',pady=8)
        self.scan_button=ttk.Button(form,text='再検出',command=self.scan)
        self.scan_button.grid(row=0,column=2,padx=(10,0))
        self.remember_box=ttk.Checkbutton(outer,text='このWindowsユーザーでキーを暗号化して保存する',variable=self.remember)
        self.remember_box.pack(anchor='w',pady=(12,4))
        ttk.Label(outer,text='未選択ならキーは今回の起動中だけ使用します。EXEや端末には保存しません。',foreground='#4c6175').pack(anchor='w')
        buttons=ttk.Frame(outer); buttons.pack(fill='x',pady=(19,12))
        self.start_button=ttk.Button(buttons,text='接続して開始',style='Action.TButton',command=self.start)
        self.start_button.pack(side='left')
        self.stop_button=ttk.Button(buttons,text='停止',style='Action.TButton',command=self.stop,state='disabled')
        self.stop_button.pack(side='left',padx=12)
        ttk.Button(buttons,text='USB認識の案内',command=lambda:webbrowser.open(DRIVER_URL)).pack(side='right')
        ttk.Label(outer,textvariable=self.status,wraplength=670,foreground='#115873',font=('Yu Gothic UI',11,'bold')).pack(anchor='w',fill='x',pady=8)
        self.history=tk.Text(outer,height=8,state='disabled',font=('Yu Gothic UI',10),background='#ffffff',foreground='#243b53',relief='flat',padx=12,pady=8)
        self.history.pack(fill='both',expand=True,pady=(4,10))
        ttk.Label(outer,text='開始後：短押し → 呼びかけ → 合図とSpeak nowを待って質問。\n現行の検証用サービスは約10分で停止します。続けるときは再度開始してください。',wraplength=680).pack(anchor='w')
        ttk.Label(outer,text=f'Windows 10/11 x64  ·  {VERSION}',foreground='#64778a').pack(anchor='w',pady=(10,0))
        ttk.Button(buttons,text='使い方・ライセンス',command=self.show_help).pack(side='right',padx=8)
        try:
            saved=load_settings(self.path)
            for name in ('model','port','host'):
                if isinstance(saved.get(name),str): getattr(self,name).set(saved[name])
            self.key.set(saved.get('key','')); self.remember.set(bool(saved.get('key')))
        except Exception: self.status.set('保存設定を読み込めませんでした。キーを再入力してください。')
        root.protocol('WM_DELETE_WINDOW',self.close)
        root.after(100,self.pump)
        if discover: root.after(150,self.scan)

    def show_help(self):
        dialog=tk.Toplevel(self.root); dialog.title('使い方・ライセンス'); dialog.geometry('820x620')
        tabs=ttk.Notebook(dialog); tabs.pack(fill='both',expand=True)
        for title,filename in [('使い方','README.ja.md'),('ライセンス','THIRD_PARTY_NOTICES.txt')]:
            frame=ttk.Frame(tabs); tabs.add(frame,text=title)
            text=tk.Text(frame,wrap='word',font=('Yu Gothic UI',10)); text.pack(side='left',fill='both',expand=True)
            bar=ttk.Scrollbar(frame,command=text.yview); bar.pack(side='right',fill='y'); text.configure(yscrollcommand=bar.set)
            try: content=(runtime_root()/filename).read_text(encoding='utf-8')
            except OSError: content='配布資料を開けませんでした。'
            text.insert('1.0',content); text.configure(state='disabled')

    def post(self,kind,value):
        try: self.events.put_nowait((kind,value))
        except queue.Full: pass

    def scan(self):
        if self.worker and self.worker.is_alive(): return
        self.scan_button.configure(state='disabled'); self.status.set('USBポートとLANを確認しています…')
        def work():
            ports=[p.device for p in list_ports.comports()]
            preferred=[p.device for p in list_ports.comports() if p.vid==0x303A]
            try: hosts=local_addresses()
            except Exception: hosts=[]
            self.post('scan',(ports,preferred,hosts))
        threading.Thread(target=work,daemon=True).start()

    def set_running(self,value):
        self.start_button.configure(state='disabled' if value else 'normal')
        self.stop_button.configure(state='normal' if value else 'disabled')
        self.scan_button.configure(state='disabled' if value else 'normal')
        self.port_box.configure(state='disabled' if value else 'readonly')
        self.host_box.configure(state='disabled' if value else 'readonly')
        self.key_box.configure(state='disabled' if value else 'normal')
        self.model_box.configure(state='disabled' if value else 'normal')
        self.remember_box.configure(state='disabled' if value else 'normal')

    def start(self):
        key=self.key.get().strip(); model=self.model.get().strip()
        if not re.fullmatch(r'COM[1-9][0-9]*',self.port.get()) or not is_lan(self.host.get()):
            messagebox.showwarning('接続先の確認','USBポートとLANアドレスを選んでください。'); return
        if not valid_key(key) or not valid_model(model):
            messagebox.showwarning('設定の確認','Geminiキーとモデル名を確認してください。'); return
        runtime=runtime_root(); node=runtime/'node.exe'
        if not node.is_file():
            messagebox.showerror('実行環境','同梱された実行環境が見つかりません。配布EXEを確認してください。'); return
        try: save_settings(self.path,model=model,port=self.port.get(),host=self.host.get(),key=key,remember=self.remember.get())
        except Exception:
            messagebox.showerror('設定保存','設定を保存できませんでした。保存先へのアクセスを確認してください。'); return
        args=['--port',self.port.get(),'--host',self.host.get(),'--mode','gemini','--seconds','580']
        env=runtime_env(key,model)
        self.stop_event.clear(); self.set_running(True); self.status.set('接続を準備しています…')
        def work():
            try:
                result=run_device(args,runtime_root=runtime,node_path=node,stop_event=self.stop_event,
                    on_status=lambda text:self.post('status',text),child_env=env)
            except Exception:
                result=1; self.post('friendly','接続処理を完了できませんでした。設定とUSB接続を確認してください。')
            finally:
                env.pop('GEMINI_API_KEY',None)
            self.post('done',result)
        self.worker=threading.Thread(target=work,daemon=False); self.worker.start()

    def stop(self):
        self.stop_event.set(); self.stop_button.configure(state='disabled'); self.status.set('停止しています…')

    def close(self):
        self.closing=True
        if self.worker and self.worker.is_alive(): self.stop()
        else: self.root.destroy()

    def log(self,text):
        self.status.set(text); self.history.configure(state='normal')
        self.history.insert('end',text+'\n')
        if int(self.history.index('end-1c').split('.')[0])>150: self.history.delete('1.0','20.0')
        self.history.see('end'); self.history.configure(state='disabled')

    def friendly(self,text):
        if text.startswith('GEM WIFI CONNECTED'): return '端末がWi-Fiへ接続しました。'
        if text.startswith('GEM STATE configured=1 wifi=1 pc=1'): return '接続できました。端末を短押しして、呼びかけてください。'
        if text=='GEM WAKE MATCH': return '呼びかけを検出しました。合図とSpeak now表示を待って質問してください。'
        if text.startswith('GEM CAPTURE DONE'): return '質問を送っています。'
        if text.startswith('GEM RESULT PASS'): return '端末での回答再生が完了しました。'
        if text=='GEM CAPTURE NOISY': return '周囲が騒がしいため受付を見送りました。'
        if text.startswith('GEM CAPTURE EMPTY'): return '発話がなかったため待受へ戻りました。'
        if text.startswith('GEM RESULT FAIL'): return '回答を完了できませんでした。端末とPCの接続を確認してください。'
        if text.startswith('Gemini device session exit '):
            try: reason=json.loads(text.split(' exit ',1)[1])['reason']
            except Exception: return ''
            return {'duration_limit':'約10分の検証時間が終了しました。再度開始できます。',
                'user_stop':'停止しました。','serial_error':'USB接続を利用できなくなりました。端末とケーブルを確認してください。',
                'server_not_ready':'中継の準備ができませんでした。Geminiキーと同梱ファイルを確認してください。',
                'firmware_no_response':'端末から応答がありません。端末のリセットとUSB接続を確認してください。',
                'device_stop_unconfirmed':'PC側は終了しましたが、端末の停止応答を確認できませんでした。'}.get(reason,'接続を終了しました。設定と端末の状態を確認してください。')
        return ''

    def pump(self):
        while not self.events.empty():
            kind,value=self.events.get_nowait()
            if kind=='scan':
                ports,preferred,hosts=value
                self.port_box.configure(values=ports); self.host_box.configure(values=hosts)
                if self.port.get() not in ports: self.port.set((preferred or ports or [''])[0])
                if self.host.get() not in hosts: self.host.set((hosts or [''])[0])
                self.scan_button.configure(state='normal')
                self.status.set('接続先とGeminiキーを確認し、「接続して開始」を押してください。' if ports and hosts else 'USBポートまたはLANを検出できません。接続を確認して再検出してください。')
            elif kind=='status':
                friendly=self.friendly(value)
                if friendly: self.log(friendly)
            elif kind=='friendly': self.log(value)
            elif kind=='done':
                self.set_running(False)
                if self.closing: self.root.destroy(); return
        self.root.after(100,self.pump)


def self_test(report,ui=False):
    runtime=runtime_root(); checks={}; report=Path(report).resolve(); report.parent.mkdir(parents=True,exist_ok=True)
    try:
        node=runtime/'node.exe'
        flags=getattr(subprocess,'CREATE_NO_WINDOW',0)
        result=subprocess.run([str(node),'--version'],capture_output=True,text=True,timeout=15,creationflags=flags)
        checks['node']=result.returncode==0 and result.stdout.strip()=='v24.15.0'
        result=subprocess.run([str(node),str(runtime/'server/dist/host-self-check.js')],cwd=runtime/'server',
            capture_output=True,text=True,timeout=45,creationflags=flags,env=runtime_env())
        details=json.loads(result.stdout.strip())
        checks['offlinePcmAndWorker']=result.returncode==0 and details.get('status')=='PASS' and details.get('apiCalls')==0
        with tempfile.TemporaryDirectory(dir=report.parent) as folder:
            config=Path(folder)/'settings.json'; sample='packaging-test-key-0123456789'
            save_settings(config,model='gemini-3.8-live',port='COM4',host='192.168.1.2',key=sample,remember=True)
            checks['protectedSettings']=sample not in config.read_text() and load_settings(config)['key']==sample
            save_settings(config,model='gemini-3.8-live',port='COM4',host='192.168.1.2',key=sample,remember=False)
            checks['sessionOnlyKey']=not load_settings(config).get('key') and sample not in config.read_text()
            if ui:
                root=tk.Tk(); root.attributes('-alpha',0.0); root.withdraw(); app=HostApp(root,discover=False,config_path=Path(folder)/'ui.json')
                root.deiconify()
                root.update(); checks['gui']=app.start_button.cget('text')=='接続して開始' and app.remember.get() is False
                def controls(widget):
                    result=[]
                    for child in widget.winfo_children():
                        if child.winfo_class() in ('TButton','TEntry','TCombobox','TCheckbutton'): result.append(child)
                        result.extend(controls(child))
                    return result
                checks['guiLayout']=all(w.winfo_width()>10 and w.winfo_height()>10 and
                    0<=w.winfo_rooty()-root.winfo_rooty() and
                    w.winfo_rooty()-root.winfo_rooty()+w.winfo_height()<=root.winfo_height() for w in controls(root))
                root.destroy()
        checks['noProjectPrivateFiles']=not (runtime/'data').exists() and not (runtime/'server/.env').exists()
        checks['serialModule']=bool(list_ports.comports is not None)
    except Exception:
        checks['completed']=False
    data={'version':VERSION,'status':'PASS' if checks and all(checks.values()) else 'FAIL','checks':checks,'apiCalls':0}
    report.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    return 0 if data['status']=='PASS' else 1


def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--ui-self-test',action='store_true'); parser.add_argument('--report')
    args=parser.parse_args()
    if args.self_test or args.ui_self_test:
        if not args.report: return 2
        return self_test(args.report,args.ui_self_test)
    root=tk.Tk(); HostApp(root); root.mainloop(); return 0

if __name__=='__main__':
    try: raise SystemExit(main())
    except Exception:
        # Frozen windowed processes must never dump environment/credential errors.
        try: messagebox.showerror('スタックちゃん ホスト','起動できませんでした。配布ファイルとWindowsの設定を確認してください。')
        except Exception: pass
        raise SystemExit(1)
