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
from settings import VERSION, load_settings, save_settings, save_voice_setting, settings_path, valid_key, valid_model
from voices import VoiceReviews,REVIEW_REASONS
from voice_preview import decode_audio, generate_sample, play_pcm
from network import local_addresses, hotspot_networks, is_lan
from wireless import run_wireless, runtime_node, PORT

DRIVER_URL='https://docs.espressif.com/projects/esp-iot-solution/en/release-v2.0/usb/usb_overview/usb_serial_jtag.html'

def runtime_root():
    if getattr(sys,'frozen',False): return Path(sys._MEIPASS) / 'runtime'
    return Path(__file__).resolve().parents[1] / '.packaging/stage/runtime'

def runtime_env(key='',model='gemini-3.8-live',voice=''):
    env=os.environ.copy()
    for name in ('NODE_OPTIONS','NODE_PATH','GEMINI_API_KEY','GOOGLE_API_KEY','GEMINI_VOICE'): env.pop(name,None)
    env['GEMINI_MODEL']=model
    if key: env['GEMINI_API_KEY']=key
    if voice: env['GEMINI_VOICE']=voice
    return env

class HostApp:
    def __init__(self,root,*,discover=True,config_path=None):
        self.root=root; self.path=config_path or settings_path()
        self.stop_event=threading.Event(); self.worker=None; self.closing=False
        self.events=queue.Queue(maxsize=256)
        self.commands=queue.Queue(maxsize=32);self.request_id=0;self.session_busy=False
        self.preview_job=None;self.pc_heard=set();self.device_heard=set();self.evaluation_voice=''
        self.reviews=VoiceReviews(self.path.with_name('voice-reviews.json'))
        self.voice=tk.StringVar();self.voice_status=tk.StringVar(value='日本語の声を確認して採用してください。')
        self.preview_status=tk.StringVar(value='試聴はPCの再生先から流れ、Gemini APIを使用します。')
        self.key=tk.StringVar(); self.model=tk.StringVar(value='gemini-3.8-live')
        self.transport=tk.StringVar(value='Wi-Fi（USB不要）')
        self.port=tk.StringVar(); self.host=tk.StringVar(); self.remember=tk.BooleanVar(value=False)
        self.status=tk.StringVar(value='PCのホットスポットを有効にし、接続先とGeminiキーを確認してください。')
        root.title('スタックちゃん ホスト'); root.geometry('860x980'); root.minsize(800,940)
        root.configure(background='#edf2f7')
        style=ttk.Style(root); style.theme_use('clam')
        style.configure('TFrame',background='#edf2f7'); style.configure('TLabel',background='#edf2f7',font=('Yu Gothic UI',10))
        style.configure('Title.TLabel',font=('Yu Gothic UI',21,'bold'),foreground='#15324f')
        style.configure('Action.TButton',font=('Yu Gothic UI',12,'bold'),padding=(18,11))
        outer=ttk.Frame(root,padding=26); outer.pack(fill='both',expand=True)
        ttk.Label(outer,text='スタックちゃん ホスト',style='Title.TLabel').pack(anchor='w')
        ttk.Label(outer,text='PCのホットスポットにつないだ端末を自動で受け付けます。登録コードは不要です。').pack(anchor='w',pady=(2,19))
        form=ttk.Frame(outer); form.pack(fill='x'); form.columnconfigure(1,weight=1)
        self.transport_box=ttk.Combobox(form,textvariable=self.transport,values=['Wi-Fi（USB不要）','USB診断（従来）'],state='readonly')
        self.port_box=ttk.Combobox(form,textvariable=self.port,state='readonly',width=25)
        self.host_box=ttk.Combobox(form,textvariable=self.host,state='readonly')
        self.key_box=ttk.Entry(form,textvariable=self.key,show='●')
        self.model_box=ttk.Entry(form,textvariable=self.model)
        self.transport_box.bind('<<ComboboxSelected>>',lambda _event:self.scan())
        for row,(caption,widget) in enumerate([('接続方式',self.transport_box),('USBポート（診断時のみ）',self.port_box),('PCホットスポットのアドレス',self.host_box),('Gemini APIキー',self.key_box),('モデル',self.model_box)]):
            ttk.Label(form,text=caption).grid(row=row,column=0,sticky='w',padx=(0,16),pady=8)
            widget.grid(row=row,column=1,sticky='ew',pady=8)
        self.scan_button=ttk.Button(form,text='再検出',command=self.scan)
        self.scan_button.grid(row=0,column=2,padx=(10,0))
        self.voice_box=ttk.Combobox(form,textvariable=self.voice,state='readonly')
        ttk.Label(form,text='声（日本語・採用済み）').grid(row=5,column=0,sticky='w',pady=8)
        self.voice_box.grid(row=5,column=1,sticky='ew',pady=8)
        self.voice_box.bind('<<ComboboxSelected>>',lambda _event:self.choose_voice())
        ttk.Button(form,text='声を確認・管理',command=self.review_voices).grid(row=5,column=2,padx=10)
        self.model_box.bind('<FocusOut>',lambda _event:self.refresh_voices())
        ttk.Label(outer,textvariable=self.voice_status,wraplength=740).pack(anchor='w',pady=3)
        sample_controls=ttk.Frame(outer);sample_controls.pack(fill='x',pady=5)
        self.sleep_confirmed=tk.BooleanVar(value=False)
        ttk.Checkbutton(sample_controls,text='端末をSleepingにしました（試聴時）',variable=self.sleep_confirmed).pack(side='left')
        ttk.Button(sample_controls,text='サンプルを聴く',command=lambda:self.preview(self.voice.get())).pack(side='left',padx=5)
        ttk.Button(sample_controls,text='試聴を停止',command=self.stop_preview).pack(side='left',padx=5)
        ttk.Label(outer,textvariable=self.preview_status,wraplength=740).pack(anchor='w')
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
        ttk.Label(outer,text='開始後：短押し → 呼びかけ → 開始音の終了から約0.7秒後に質問。\nWi-Fi方式は停止するまで動作します。USB診断は約10分で終了します。',wraplength=680).pack(anchor='w')
        ttk.Label(outer,text=f'Windows 10/11 x64  ·  {VERSION}',foreground='#64778a').pack(anchor='w',pady=(10,0))
        ttk.Button(buttons,text='使い方・ライセンス',command=self.show_help).pack(side='right',padx=8)
        try:
            saved=load_settings(self.path)
            for name in ('model','port','host'):
                if isinstance(saved.get(name),str): getattr(self,name).set(saved[name])
            self.key.set(saved.get('key','')); self.remember.set(bool(saved.get('key')))
            self.voice.set(saved.get('voice',''))
        except Exception: self.status.set('保存設定を読み込めませんでした。キーを再入力してください。')
        self.refresh_voices()
        if self.reviews.load_error: self.preview_status.set('声の確認記録を読み込めません。原本を保持し、声を再確認できます。')
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
        wireless=self.transport.get()=='Wi-Fi（USB不要）'
        def work():
            ports=[p.device for p in list_ports.comports()]
            preferred=[p.device for p in list_ports.comports() if p.vid==0x303A]
            try: hosts=sorted(hotspot_networks()) if wireless else local_addresses()
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
        self.transport_box.configure(state='disabled' if value else 'readonly')

    def start(self):
        if self.preview_job:
            self.preview_status.set('試聴を停止してからホストを開始してください。');return
        key=self.key.get().strip(); model=self.model.get().strip()
        wireless=self.transport.get()=='Wi-Fi（USB不要）'
        if (not wireless and not re.fullmatch(r'COM[1-9][0-9]*',self.port.get())) or not is_lan(self.host.get()):
            messagebox.showwarning('接続先の確認','接続先を選んでください。USB診断ではUSBポートも必要です。'); return
        if not valid_key(key) or not valid_model(model):
            messagebox.showwarning('設定の確認','Geminiキーとモデル名を確認してください。'); return
        self.refresh_voices()
        voice=self.evaluation_voice or self.voice.get()
        if voice not in self.reviews.candidates(model):
            messagebox.showwarning('日本語の声','「声を確認・管理」でPC試聴し、端末で確認する声を指定してください。');return
        selected_host=self.host.get()
        runtime=runtime_root(); node=runtime/'node.exe'
        if not node.is_file():
            messagebox.showerror('実行環境','同梱された実行環境が見つかりません。配布EXEを確認してください。'); return
        try: save_settings(self.path,model=model,port=self.port.get(),host=self.host.get(),key=key,remember=self.remember.get(),voice=self.voice.get())
        except Exception:
            messagebox.showerror('設定保存','設定を保存できませんでした。保存先へのアクセスを確認してください。'); return
        args=['--port',self.port.get(),'--host',self.host.get(),'--mode','gemini','--seconds','580']
        env=runtime_env(key,model,voice)
        while not self.commands.empty(): self.commands.get_nowait()
        self.stop_event.clear(); self.set_running(True); self.status.set('接続を準備しています…')
        def work():
            try:
                if wireless:
                    stable_node=runtime_node(runtime)
                    result=run_wireless(runtime,stable_node,selected_host,env,self.stop_event,lambda text:self.post('status',text),
                        self.commands,lambda event:self.post('voice-event',event))
                else:
                    result=run_device(args,runtime_root=runtime,node_path=node,stop_event=self.stop_event,
                        on_status=lambda text:self.post('status',text),child_env=env)
            except Exception:
                result=1; self.post('friendly','接続処理を完了できませんでした。設定とUSB接続を確認してください。')
            finally:
                env.pop('GEMINI_API_KEY',None)
            self.post('done',result)
        self.worker=threading.Thread(target=work,daemon=False); self.worker.start()

    def stop(self):
        self.stop_preview()
        self.stop_event.set(); self.stop_button.configure(state='disabled'); self.status.set('停止しています…')

    def close(self):
        self.closing=True
        self.stop_preview()
        if self.worker and self.worker.is_alive(): self.stop()
        else: self.root.destroy()

    def log(self,text):
        self.status.set(text); self.history.configure(state='normal')
        self.history.insert('end',text+'\n')
        if int(self.history.index('end-1c').split('.')[0])>150: self.history.delete('1.0','20.0')
        self.history.see('end'); self.history.configure(state='disabled')

    def friendly(self,text):
        match=re.fullmatch(r'WIRELESS turn-failed ([A-Z_]+) (0|[1-5][0-9]{2})',text)
        if match:
            messages={'AUTH':'Gemini APIキーの認証に失敗しました。キーを確認してください。',
                'QUOTA':'Geminiの利用制限に達しました。利用枠を確認してください。',
                'MODEL':'指定したGeminiモデルが見つかりません。モデル名を確認してください。',
                'PERMISSION':'Geminiモデルへのアクセスが拒否されました。キーの権限を確認してください。',
                'CONFIG':'Geminiへの要求設定が拒否されました。モデルと対応機能を確認してください。',
                'SERVER':'Gemini側で一時的なエラーが発生しました。',
                'TRANSPORT':'Geminiとの通信に失敗しました。PCのインターネット接続を確認してください。',
                'TIMEOUT':'Geminiの応答が時間内に完了しませんでした。',
                'INVALID_AUDIO':'Geminiからの音声形式を確認できませんでした。',
                'EMPTY_AUDIO':'Geminiから回答音声が返りませんでした。',
                'OUTPUT_LIMIT':'Geminiの回答音声が長さの上限を超えました。',
                'INTERRUPTED':'Geminiの回答音声が中断されました。',
                'EARLY_CLOSE':'回答前にGeminiとの接続が終了しました。'}
            message=messages.get(match[1],'Geminiへの問い合わせに失敗しました。')
            return message+(f'（HTTP {match[2]}）' if match[2]!='0' else '')
        if text.startswith('WIRELESS '):
            return {'ready':'PC中継を開始しました。端末の接続を待っています。',
                'device-online':'ホットスポット上の端末が接続しました。質問できます。',
                'turn-complete':'Geminiの回答を端末へ送っています。',
                'turn-failed':'Geminiへの問い合わせに失敗しました。',
                'failed':'起動できませんでした。ホットスポットのアドレスとポートの使用状態を確認してください。',
                'stale_address':'PCのアドレスが変わっています。「再検出」で選び直してください。',
                'hotspot_required':'PCのモバイルホットスポットをオンにして「再検出」で選び直してください。',
                'stopped':'無線中継を停止しました。'}.get(text.split(' ',1)[1],'')
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
                'wifi_failed':'端末がWi-Fiに接続できませんでした。端末のWi-Fi設定と2.4GHzネットワークを確認してください。',
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
                self.status.set('ホットスポットのアドレス・登録コード・Geminiキーを確認して開始してください。' if hosts else 'LANを検出できません。PCのモバイルホットスポットを有効にしてください。接続を確認して再検出してください。')
            elif kind=='status':
                friendly=self.friendly(value)
                if friendly: self.log(friendly)
            elif kind=='friendly': self.log(value)
            elif kind=='voice-event': self.voice_event(value)
            elif kind=='preview-play': self.play_preview(*value)
            elif kind=='preview-ended': self.preview_ended(*value)
            elif kind=='done':
                self.session_busy=False;self.evaluation_voice='';self.stop_preview()
                self.set_running(False)
                if self.closing: self.root.destroy(); return
        self.root.after(100,self.pump)

    def running(self):
        return bool(self.worker and self.worker.is_alive())

    def refresh_voices(self):
        approved=self.reviews.approved(self.model.get().strip())
        self.voice_box.configure(values=approved)
        if self.voice.get() not in approved: self.voice.set('')
        if not approved: self.voice_status.set('採用済みの声がありません。「声を確認・管理」で日本語を確認してください。')

    def next_id(self):
        self.request_id+=1;return self.request_id

    def send_voice_command(self,command):
        try: self.commands.put_nowait(command);return True
        except queue.Full: self.voice_status.set('処理待ちです。少し待ってから操作してください。');return False

    def choose_voice(self):
        voice=self.voice.get();model=self.model.get().strip()
        if voice not in self.reviews.approved(model): return
        try: save_voice_setting(self.path,voice)
        except Exception: self.voice_status.set('声の設定を保存できませんでした。');return
        self.evaluation_voice=''
        if self.running():
            if self.transport.get()!='Wi-Fi（USB不要）':
                self.voice_status.set('USB診断では停止・再開後に声が変わります。');return
            self.send_voice_command({'type':'voice','id':self.next_id(),'voice':voice})
            self.voice_status.set('変更待ち：'+voice+'（進行中の回答は変更しません）')
        else: self.voice_status.set('選択した声：'+voice)

    def review_voices(self):
        dialog=tk.Toplevel(self.root);dialog.title('日本語の声を確認・管理');dialog.geometry('760x620')
        frame=ttk.Frame(dialog,padding=20);frame.pack(fill='both',expand=True)
        model=self.model.get().strip();candidate=tk.StringVar()
        box=ttk.Combobox(frame,textvariable=candidate,values=self.reviews.candidates(model),state='readonly');box.pack(fill='x',pady=8)
        previous=tk.StringVar(value='候補を選ぶと前回の評価を表示します。')
        def show_previous(_event=None):
            record=self.reviews.records.get(model+':'+candidate.get(),{})
            previous.set(('前回：'+record.get('status','')+' / '+record.get('reason','')+' / '+record.get('date','')) if record else 'このモデルでは未評価です。')
        box.bind('<<ComboboxSelected>>',show_previous)
        ttk.Label(frame,textvariable=previous,wraplength=680).pack(anchor='w',pady=3)
        ttk.Label(frame,text='ここは評価候補です。未確認の声は通常の一覧へ出ません。\nPC試聴 → 端末で質問・回答を確認 → 日本語が適切なら採用します。',wraplength=650).pack(anchor='w',pady=8)
        ttk.Checkbutton(frame,text='端末をSleepingにしました（試聴時）',variable=self.sleep_confirmed).pack(anchor='w',pady=5)
        reason=tk.StringVar(value=REVIEW_REASONS['approved'][0])
        ttk.Label(frame,text='判定理由（採用・除外の前に選択）').pack(anchor='w')
        ttk.Combobox(frame,textvariable=reason,values=REVIEW_REASONS['approved']+REVIEW_REASONS['excluded'],state='readonly').pack(fill='x',pady=5)
        ttk.Button(frame,text='候補の日本語サンプルを聴く',command=lambda:self.preview(candidate.get())).pack(anchor='w',pady=5)
        ttk.Button(frame,text='この声を端末で確認',command=lambda:self.review_device(model,candidate.get())).pack(anchor='w',pady=5)
        ttk.Button(frame,text='日本語に適する：採用',command=lambda:self.decide_voice(model,candidate.get(),True,reason.get())).pack(anchor='w',pady=5)
        ttk.Button(frame,text='日本語に適さない：除外',command=lambda:self.decide_voice(model,candidate.get(),False,reason.get())).pack(anchor='w',pady=5)
        ttk.Button(frame,text='試聴を停止',command=self.stop_preview).pack(anchor='w',pady=5)
        ttk.Label(frame,textvariable=self.preview_status,wraplength=650).pack(anchor='w',pady=8)

    def review_device(self,model,voice):
        if self.running() and self.transport.get()!='Wi-Fi（USB不要）':
            self.preview_status.set('USB診断を停止し、Wi-Fi方式で端末の声を確認してください。');return
        if (model,voice) not in self.pc_heard:
            self.preview_status.set('先に候補のPCサンプルを最後まで試聴してください。');return
        if self.preview_job:
            self.preview_status.set('試聴を停止してから端末で確認してください。');return
        if model!=self.model.get().strip() or voice not in self.reviews.candidates(model): return
        self.evaluation_voice=voice
        if self.running():
            self.send_voice_command({'type':'voice','id':self.next_id(),'voice':voice})
        else: self.start()
        self.voice_status.set('評価中の声：'+voice+'。端末を起こして質問し、日本語の回答を確認してください。')
        self.sleep_confirmed.set(False)

    def decide_voice(self,model,voice,approved,reason):
        if model!=self.model.get().strip() or voice not in self.reviews.candidates(model): return
        if reason not in REVIEW_REASONS['approved' if approved else 'excluded']:
            self.preview_status.set('採用・除外に合う判定理由を選択してください。');return
        if approved and ((model,voice) not in self.pc_heard or (model,voice) not in self.device_heard):
            self.preview_status.set('PC試聴と、この声の端末回答の確認が両方必要です。');return
        try: self.reviews.decide(model,voice,approved,pc_confirmed=(model,voice) in self.pc_heard,
            device_confirmed=(model,voice) in self.device_heard,app_version=VERSION,reason=reason)
        except Exception: self.preview_status.set('声の評価を保存できませんでした。');return
        was_active=self.evaluation_voice==voice or self.voice.get()==voice
        self.refresh_voices()
        if approved:
            self.voice.set(voice);self.choose_voice();self.preview_status.set(voice+'を採用しました。')
        else:
            if was_active:
                self.evaluation_voice='';self.voice.set('')
                if self.running(): self.stop()
            self.preview_status.set(voice+'を通常の一覧から除外しました。')

    def preview(self,voice):
        model=self.model.get().strip();key=self.key.get().strip()
        if self.preview_job: self.preview_status.set('試聴中です。別の声は停止後に選んでください。');return
        if self.session_busy: self.preview_status.set('端末の会話が終わってから試聴してください。');return
        if not self.sleep_confirmed.get(): self.preview_status.set('端末をSleepingにして、試聴用のチェックを入れてください。');return
        if voice not in self.reviews.candidates(model) or not valid_key(key):
            self.preview_status.set('声とGemini APIキーを確認してください。');return
        if self.running() and self.transport.get()!='Wi-Fi（USB不要）':
            self.preview_status.set('USB診断を停止してから試聴してください。');return
        job={'id':self.next_id(),'voice':voice,'model':model,'cancel':threading.Event(),'live':self.running(),'playing':False}
        self.preview_job=job;self.preview_status.set('日本語サンプルを生成しています…')
        if job['live']:
            if not self.send_voice_command({'type':'preview','id':job['id'],'voice':voice}): self.preview_job=None
        else:
            runtime=runtime_root();env=runtime_env(key,model,voice)
            def generate():
                try: pcm=generate_sample(runtime,runtime/'node.exe',env,voice,job['id'],job['cancel']);self.post('preview-play',(job['id'],pcm))
                except Exception as error: self.post('preview-ended',(job['id'],False,str(error) if isinstance(error,ValueError) else 'TRANSPORT'))
                finally: env.pop('GEMINI_API_KEY',None)
            threading.Thread(target=generate,daemon=False).start()

    def play_preview(self,request_id,pcm):
        job=self.preview_job
        if not job or job['id']!=request_id or job['cancel'].is_set():
            pcm[:]=b'\0'*len(pcm);return
        job['playing']=True;self.preview_status.set('PCで日本語サンプルを再生しています…')
        def play():
            ok=False;reason=''
            try: ok=play_pcm(pcm,job['cancel'])
            except Exception: reason='PC_AUDIO'
            finally:
                pcm[:]=b'\0'*len(pcm);self.post('preview-ended',(request_id,ok,reason))
        threading.Thread(target=play,daemon=False).start()

    def stop_preview(self):
        job=self.preview_job
        if not job: return
        job['cancel'].set()
        if not job['playing']:
            if job['live']: self.send_voice_command({'type':'preview-stop','id':job['id']})
            self.preview_job=None;self.preview_status.set('試聴を停止しました。')

    def preview_ended(self,request_id,ok,reason):
        job=self.preview_job
        if not job or job['id']!=request_id: return
        if job['live']: self.send_voice_command({'type':'preview-done','id':request_id})
        if ok and not job['cancel'].is_set():
            self.pc_heard.add((job['model'],job['voice']));self.preview_status.set('試聴完了：'+job['voice'])
        elif reason=='PC_AUDIO': self.preview_status.set('PCで再生できません。Windowsの再生先を確認してください。')
        elif job['cancel'].is_set() or reason=='CANCELLED': self.preview_status.set('試聴を停止しました。')
        else: self.preview_status.set('試聴できませんでした。'+self.friendly('WIRELESS turn-failed '+(reason if reason in ('AUTH','QUOTA','MODEL','CONFIG','PERMISSION','TIMEOUT','EMPTY_AUDIO','OUTPUT_LIMIT') else 'TRANSPORT')+' 0'))
        self.preview_job=None

    def voice_event(self,event):
        kind=event.get('event')
        if kind=='session-busy':
            self.session_busy=True;self.sleep_confirmed.set(False)
            self.voice_status.set('この会話の声：'+str(event.get('voice','')))
        elif kind=='session-idle': self.session_busy=False
        elif kind=='voice-selected': self.voice_status.set('次の質問の声：'+str(event.get('voice','')))
        elif kind=='voice-played': self.device_heard.add((event.get('model'),event.get('voice')))
        elif kind=='voice-failed': self.voice_status.set('声を適用できませんでした。モデルと声の対応を確認してください。')
        elif kind=='preview-conflict': self.preview_status.set('試聴中に端末の質問が来ました。試聴を停止し、端末をSleepingにしてください。')
        elif kind=='preview-audio':
            try: pcm=decode_audio(event)
            except Exception: self.preview_ended(event.get('id'),False,'INVALID_AUDIO');return
            self.play_preview(event.get('id'),pcm)
        elif kind=='preview-failed': self.preview_ended(event.get('id'),False,event.get('code','TRANSPORT'))


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
        hotspot_script="""import {pathToFileURL} from 'node:url';
const {createGeminiServer}=await import(pathToFileURL(process.argv[1]+'/gemini-server.js'));
const {runGeminiProtocolProbe}=await import(pathToFileURL(process.argv[1]+'/gemini-probe-client.js'));
const {once}=await import('node:events');
const app=createGeminiServer({deviceToken:'',trustedSubnet:'127.0.0.1/32',runTurn:async()=>({pcm:Buffer.alloc(2),sampleRate:24000,chunks:1,elapsedMs:0})});
try {app.server.listen(0,'127.0.0.1');await once(app.server,'listening');
const result=await runGeminiProtocolProbe('ws://127.0.0.1:'+app.server.address().port+'/live','',Buffer.alloc(640));
console.log(JSON.stringify({ok:result.pcm.length===2}));} finally {await app.close();}
"""
        result=subprocess.run([str(node),'--input-type=module','-e',hotspot_script,str(runtime/'server/dist')],
            capture_output=True,text=True,timeout=20,creationflags=flags,env=runtime_env())
        checks['hotspotTrust']=result.returncode==0 and json.loads(result.stdout.strip()).get('ok') is True
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
                checks['noManualPairing']=not hasattr(app,'pair_box') and not hasattr(app,'pair_code') and not hasattr(app,'forget_button')
                checks['voiceCatalog']=bool(app.reviews.candidates('gemini-3.8-live')) and not app.reviews.approved('gemini-3.8-live')
                checks['voiceUi']=str(app.voice_box.cget('state'))=='readonly' and not app.sleep_confirmed.get()
                app.review_voices();root.update()
                review=[w for w in root.winfo_children() if isinstance(w,tk.Toplevel)][0]
                def controls(widget):
                    result=[]
                    for child in widget.winfo_children():
                        if child.winfo_class() in ('TButton','TEntry','TCombobox','TCheckbutton'): result.append(child)
                        result.extend(controls(child))
                    return result
                checks['guiLayout']=all(w.winfo_width()>10 and w.winfo_height()>10 and
                    0<=w.winfo_rooty()-root.winfo_rooty() and
                    w.winfo_rooty()-root.winfo_rooty()+w.winfo_height()<=root.winfo_height() for w in controls(root) if w.winfo_toplevel()==root)
                checks['voiceReviewLayout']=all(w.winfo_width()>10 and w.winfo_height()>10 and
                    0<=w.winfo_rooty()-review.winfo_rooty() and w.winfo_rooty()-review.winfo_rooty()+w.winfo_height()<=review.winfo_height() for w in controls(review))
                root.destroy()
        checks['noProjectPrivateFiles']=not (runtime/'data').exists() and not (runtime/'server/.env').exists()
        checks['serialModule']=bool(list_ports.comports is not None)
        checks['wirelessModule']=(runtime/'server/dist/wireless-host.js').is_file()
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
